#include <doctest/doctest.h>

#include <greeter/TriangularMeshMagnet.h>
#include <greeter/io/CylinderSegmentMagnetIO.h>
#include <greeter/io/MethodFactoryIO.h>
#include <greeter/service/SimulationService.h>

#include <cmath>
#include <memory>


namespace {

  std::unique_ptr<greeter::Magnet> sector(const uint32_t& segments,
                                          const float& first = -30.0f,
                                          const float& second = 30.0f) {

    nlohmann::json magnet = nlohmann::json::parse(R"({
      "type": "cylinder_segment",
      "parameters": { "dimensions": [0.02, 0.03, 0.01, -30, 30],
                      "magnetization": [0, 0, 1] }
    })");

    magnet["parameters"]["segments"] = segments;
    magnet["parameters"]["dimensions"][3] = first;
    magnet["parameters"]["dimensions"][4] = second;

    return greeter::MethodFactoryIO::getInstance().createMagnet(
      "cylinder_segment", magnet);
  }

  /* The largest difference from the exact field, over the probe points. */
  float worstError(const greeter::Magnet& magnet,
                   const std::vector<std::vector<float>>& points,
                   const std::vector<std::vector<double>>& exact,
                   const float& scale) {

    float worst = 0.0f;

    for (size_t i = 0; i < points.size(); i++) {

      const std::vector<float> field =
        magnet.computeMagneticField(points[i][0], points[i][1], points[i][2]);

      for (size_t axis = 0; axis < 3; axis++) {
        worst = std::max(worst,
          (float) std::fabs((double) field[axis] - exact[i][axis]) / scale);
      }
    }

    return worst;
  }

  /*
    magpylib 5, CylinderSegment(dimension=(0.02, 0.03, 0.01, -30, 30),
    polarization=(0, 0, 1)), which is the closed form this faceted body is an
    approximation of.
  */
  const std::vector<std::vector<float>> PROBES = {
    {0.0f,   0.0f,  0.0f},
    {0.05f,  0.0f,  0.0f},
    {0.025f, 0.0f,  0.0f},
    {0.0f,   0.04f, 0.02f}
  };

  const std::vector<std::vector<double>> EXACT = {
    { 0.0,           0.0,          -0.013022773},
    { 0.0,           0.0,          -0.011041888},
    { 0.0,           0.0,           0.540155584},
    {-0.001033833,   0.001562684,  -0.000823839}
  };

  const float SCALE = 0.540155584f;

}  // namespace


TEST_CASE("A ring sector is the closed form, to the fineness it was cut at") {

  // The type is a faceted body and not a closed form, so what it owes is not
  // exactness but a stated and honest error. This is that statement.
  const std::unique_ptr<greeter::Magnet> coarse = sector(8);
  const std::unique_ptr<greeter::Magnet> fine = sector(32);
  const std::unique_ptr<greeter::Magnet> finer = sector(64);

  const float coarse_error = worstError(*coarse, PROBES, EXACT, SCALE);
  const float fine_error = worstError(*fine, PROBES, EXACT, SCALE);
  const float finer_error = worstError(*finer, PROBES, EXACT, SCALE);

  INFO("8 facets: " << coarse_error << ", 32: " << fine_error
       << ", 64: " << finer_error);

  // The default of 32 facets is far below any tolerance a magnet is made to.
  CHECK(fine_error < 0.0005f);
  CHECK(finer_error < 0.0002f);

  // And the error falls as the facets get smaller, which is what says the
  // faceting is the only thing wrong rather than the arithmetic.
  CHECK(fine_error < coarse_error);
  CHECK(finer_error < fine_error);

  // Falling with the square of the facet size: four times as fine, sixteen
  // times as close. Allowing plenty of room, doubling the facets should at
  // least halve the error.
  CHECK(finer_error < 0.6f * fine_error);
}


TEST_CASE("A sector spanning a whole turn is a closed ring") {

  // magpylib 5, CylinderSegment(dimension=(0.02, 0.03, 0.01, -180, 180)).
  const std::unique_ptr<greeter::Magnet> ring = sector(64, -180.0f, 180.0f);

  struct Case {
    float point[3];
    double expected;
  };

  const std::vector<Case> cases = {
    {{0.0f,  0.0f, 0.0f },  -0.078136638},
    {{0.0f,  0.0f, 0.02f},   0.006050997},
    {{0.05f, 0.0f, 0.0f },  -0.019043880}
  };

  for (const auto& item : cases) {

    const std::vector<float> field =
      ring->computeMagneticField(item.point[0], item.point[1], item.point[2]);

    CHECK(field[2] == doctest::Approx(item.expected).epsilon(0.002));
  }

  // A whole turn joins up, so it has no end caps: four faces per facet
  // instead of four plus the two caps at each end.
  const greeter::TriangularMeshMagnet* body =
    dynamic_cast<const greeter::TriangularMeshMagnet*>(ring.get());

  REQUIRE(body != nullptr);
  CHECK(body->getNumOfFaces() == 8 * 64);

  const std::unique_ptr<greeter::Magnet> partial = sector(64);

  const greeter::TriangularMeshMagnet* open_ends =
    dynamic_cast<const greeter::TriangularMeshMagnet*>(partial.get());

  REQUIRE(open_ends != nullptr);
  CHECK(open_ends->getNumOfFaces() == 8 * 64 + 4);
}


TEST_CASE("A ring sector encloses what a ring sector should") {

  const std::unique_ptr<greeter::Magnet> fine = sector(128);

  const greeter::TriangularMeshMagnet* body =
    dynamic_cast<const greeter::TriangularMeshMagnet*>(fine.get());

  REQUIRE(body != nullptr);

  // (phi2 - phi1) / 2 * (r2^2 - r1^2) * h, approached from below because a
  // chord cuts inside the arc it stands for.
  const double exact = (M_PI / 3.0) / 2.0 * (0.03 * 0.03 - 0.02 * 0.02) * 0.01;

  CHECK(body->getVolume() == doctest::Approx(exact).epsilon(0.001));
  CHECK(body->getVolume() < (float) exact);

  const std::unique_ptr<greeter::Magnet> coarse = sector(8);

  const greeter::TriangularMeshMagnet* rough =
    dynamic_cast<const greeter::TriangularMeshMagnet*>(coarse.get());

  REQUIRE(rough != nullptr);
  CHECK(rough->getVolume() < body->getVolume());
}


TEST_CASE("A ring sector that could not be made is refused") {

  auto withDimensions = [](const std::string& dimensions) {
    return nlohmann::json::parse(
      "{\"type\": \"cylinder_segment\", \"parameters\": {\"dimensions\": " +
      dimensions + ", \"magnetization\": [0, 0, 1]}}");
  };

  // A sector reaching the axis needs a different triangulation, and a whole
  // solid cylinder is its own type.
  CHECK_THROWS_AS(greeter::CylinderSegmentMagnetIO::readDimensions(
    withDimensions("[0, 0.03, 0.01, -30, 30]")), std::invalid_argument);

  CHECK_THROWS_AS(greeter::CylinderSegmentMagnetIO::readDimensions(
    withDimensions("[0.03, 0.02, 0.01, -30, 30]")), std::invalid_argument);

  CHECK_THROWS_AS(greeter::CylinderSegmentMagnetIO::readDimensions(
    withDimensions("[0.02, 0.03, 0, -30, 30]")), std::invalid_argument);

  CHECK_THROWS_AS(greeter::CylinderSegmentMagnetIO::readDimensions(
    withDimensions("[0.02, 0.03, 0.01, 30, -30]")), std::invalid_argument);

  CHECK_THROWS_AS(greeter::CylinderSegmentMagnetIO::readDimensions(
    withDimensions("[0.02, 0.03, 0.01, -200, 200]")), std::invalid_argument);

  CHECK_THROWS_AS(greeter::CylinderSegmentMagnetIO::readDimensions(
    withDimensions("[0.02, 0.03, 0.01]")), std::invalid_argument);

  nlohmann::json too_coarse = withDimensions("[0.02, 0.03, 0.01, -30, 30]");
  too_coarse["parameters"]["segments"] = 1;

  CHECK_THROWS_AS(greeter::CylinderSegmentMagnetIO::readSegments(too_coarse),
                  std::invalid_argument);
}


TEST_CASE("Sectors laid side by side are the ring they were cut from") {

  /*
    How a segmented ring is actually built: every sector sits at the middle of
    the ring and spans its own slice of the turn, because an arc is measured
    about the axis of the ring and not about anything of its own.

    Note what this means for the arrangements. `halbach_ring` places its
    members at a radius, which is right for a cuboid or a cylinder and wrong
    for a sector: a sector placed out on the circle is a small arc about a
    point on the circle, not a piece of the ring. Building a segmented ring
    through an arrangement would need one that turns its members about the
    axis without moving them out to it. Written out by hand it works today,
    which is what this checks.
  */
  greeter::service::SimulationService service;

  nlohmann::json data = nlohmann::json::parse(R"({
    "magnets": [],
    "field_of_view": {
      "x": {"min": 0, "max": 0, "n": 1},
      "y": {"min": 0, "max": 0, "n": 1},
      "z": {"min": 0.004, "max": 0.004, "n": 1}
    }
  })");

  const int count = 8;

  for (int i = 0; i < count; i++) {

    const float from = -180.0f + 360.0f * (float) i / (float) count;
    const float to = -180.0f + 360.0f * (float) (i + 1) / (float) count;

    data["magnets"].push_back({
      {"id", i + 1},
      {"type", "cylinder_segment"},
      {"parameters", {
        {"dimensions", {0.02, 0.03, 0.01, from, to}},
        {"magnetization", {0, 0, 1}},
        {"segments", 8}
      }}
    });
  }

  service.loadJSON(data, "test");

  greeter::service::FieldRequest request;
  REQUIRE(service.getFieldRequest(request));

  const greeter::view::FieldGrid pieces = service.simulateField(request);

  REQUIRE(pieces.size() == 1);

  // The same ring in one piece, cut at the same fineness so that the two are
  // the same faceted shape and not merely close.
  greeter::service::SimulationService whole;

  nlohmann::json one = data;
  one["magnets"] = nlohmann::json::array();
  one["magnets"].push_back({
    {"id", 1},
    {"type", "cylinder_segment"},
    {"parameters", {
      {"dimensions", {0.02, 0.03, 0.01, -180, 180}},
      {"magnetization", {0, 0, 1}},
      {"segments", 8 * count}
    }}
  });

  whole.loadJSON(one, "test");

  const greeter::view::FieldGrid single = whole.simulateField(request);

  REQUIRE(single.size() == 1);

  for (size_t axis = 0; axis < 3; axis++) {
    CHECK(pieces.field[axis] ==
          doctest::Approx(single.field[axis]).epsilon(0.001));
  }

  // And each piece is a body the viewer draws without having been taught what
  // a ring sector is.
  const greeter::view::SceneSnapshot scene = service.getScene();

  REQUIRE(scene.magnets.size() == (size_t) count);

  for (const auto& magnet : scene.magnets) {
    CHECK(magnet.shape.kind == greeter::view::ShapeKind::Mesh);
    CHECK(magnet.shape.isValid());
  }
}


TEST_CASE("A ring sector can be an arrangement element, placed as one") {

  // Placed at a radius it is a small arc about a point on the circle rather
  // than a piece of a ring, which is a perfectly good magnet and simply not a
  // segmented ring. What matters here is that the type composes with the
  // arrangements at all.
  greeter::service::SimulationService service;

  service.loadJSON(nlohmann::json::parse(R"({
    "arrangements": [
      { "id": 100, "type": "linear_array", "parameters": {
          "count": [1, 1, 3], "spacing": [0.05, 0.05, 0.03],
          "element": {
            "type": "cylinder_segment",
            "parameters": {
              "dimensions": [0.02, 0.03, 0.01, -30, 30],
              "magnetization": [0, 0, 1],
              "segments": 8
            }
          } } }
    ],
    "force": { "targets": "all", "meshing": 8 }
  })"), "test");

  const greeter::view::SceneSnapshot scene = service.getScene();

  REQUIRE(scene.magnets.size() == 3);

  for (const auto& magnet : scene.magnets) {
    CHECK(magnet.arrangement_id == 100);
    CHECK(magnet.shape.kind == greeter::view::ShapeKind::Mesh);
  }

  // And it can be pushed about like any other body.
  const greeter::view::ForceReport report = service.simulateForces();

  REQUIRE(report.entries.size() == 3);

  for (const auto& entry : report.entries) {
    CHECK(entry.cells >= 1);
    CHECK(std::isfinite(entry.force[0]));
  }

  /*
    Stacked along z, and a sector is symmetric about its own mid height, so
    the three of them are a mirror image of themselves about the middle one.
    The forces have to obey that: the ends equal and opposite, the middle held.

    Note that stacking them along x would not do, however natural it looks. A
    sector spanning -30 to 30 degrees bulges towards its own +x and is not its
    own mirror image in x, so a row of them along x has no symmetry to check
    against.

    With no magnets listed of its own, the file numbers the generated ones
    from zero.
  */
  const greeter::view::ForceEntry* first = report.findById(0);
  const greeter::view::ForceEntry* middle = report.findById(1);
  const greeter::view::ForceEntry* last = report.findById(2);

  REQUIRE(first != nullptr);
  REQUIRE(middle != nullptr);
  REQUIRE(last != nullptr);

  CHECK(first->force[2] == doctest::Approx(-last->force[2]).epsilon(0.01));
  CHECK(std::fabs(first->force[2]) > 0.0f);
  CHECK(std::fabs(middle->force[2]) < 0.01f * std::fabs(first->force[2]));
}
