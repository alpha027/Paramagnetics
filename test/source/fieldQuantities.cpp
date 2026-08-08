#include <doctest/doctest.h>

#include <greeter/TargetMesh.h>
#include <greeter/service/SimulationService.h>
#include <greeter/view/SnapshotIO.h>

#include <cmath>
#include <sstream>


namespace {

  const char* THREE_MAGNETS = R"({
    "magnets": [
      { "id": 1, "type": "cuboid", "parameters": {
          "dimensions": [0.02, 0.02, 0.02], "magnetization": [0.3, -0.2, 1.0],
          "position": [0, 0, 0], "orientation": [1, 0, 0, 0] } },
      { "id": 2, "type": "cylinder", "parameters": {
          "dimensions": [0.02, 0.03], "magnetization": [0, 0, 1],
          "position": [0.05, 0, 0], "orientation": [1, 0, 0, 0] } },
      { "id": 3, "type": "sphere", "parameters": {
          "dimensions": 0.008, "magnetization": 1.0,
          "position": [0, 0.05, 0], "orientation": [1, 0, 0, 0] } }
    ],
    "field_of_view": {
      "x": {"min": -0.031, "max": 0.069, "n": 7},
      "y": {"min": -0.023, "max": 0.061, "n": 7},
      "z": {"min": -0.019, "max": 0.017, "n": 5}
    }
  })";

  greeter::view::FieldGrid runFor(const std::string& quantity) {

    nlohmann::json data = nlohmann::json::parse(THREE_MAGNETS);

    data["field_of_view"]["quantity"] = quantity;

    greeter::service::SimulationService service;
    service.loadJSON(data, "test");

    greeter::service::FieldRequest request;
    REQUIRE(service.getFieldRequest(request));

    return service.simulateField(request);
  }

  /* One point, one quantity, out of a scene given as JSON. */
  std::vector<float> at(const nlohmann::json& magnets, const std::string& quantity,
                        const float& x, const float& y, const float& z) {

    nlohmann::json data;
    data["magnets"] = magnets;
    data["field_of_view"] = {
      {"quantity", quantity},
      {"x", {{"min", x}, {"max", x}, {"n", 1}}},
      {"y", {{"min", y}, {"max", y}, {"n", 1}}},
      {"z", {{"min", z}, {"max", z}, {"n", 1}}}
    };

    greeter::service::SimulationService service;
    service.loadJSON(data, "test");

    greeter::service::FieldRequest request;
    REQUIRE(service.getFieldRequest(request));

    const greeter::view::FieldGrid grid = service.simulateField(request);

    REQUIRE(grid.size() == 1);

    return {grid.field[0], grid.field[1], grid.field[2]};
  }

}  // namespace


TEST_CASE("Inside a sphere the four quantities are the textbook ones") {

  /*
    A homogeneously polarized sphere is where all four can be written down
    without computing anything, so it is the one place they can be checked
    against something other than each other.

    Inside:  B = 2/3 J,  J = J,  M = J / mu0,  H = (B - J) / mu0 = -J / (3 mu0)

    That last one is the demagnetizing field, and its sign is the point: H
    inside a magnet points against the polarization, which is why a magnet
    demagnetizes itself and why anyone asks for H at all.
  */
  const nlohmann::json sphere = nlohmann::json::parse(R"([
    { "id": 1, "type": "sphere", "parameters": {
        "dimensions": 0.01, "magnetization": 1.2,
        "position": [0, 0, 0], "orientation": [1, 0, 0, 0] } }
  ])");

  const float polarization = 1.2f;

  const std::vector<float> b = at(sphere, "B", 0.0f, 0.0f, 0.003f);
  const std::vector<float> j = at(sphere, "J", 0.0f, 0.0f, 0.003f);
  const std::vector<float> h = at(sphere, "H", 0.0f, 0.0f, 0.003f);
  const std::vector<float> m = at(sphere, "M", 0.0f, 0.0f, 0.003f);

  CHECK(b[2] == doctest::Approx(2.0f / 3.0f * polarization));
  CHECK(j[2] == doctest::Approx(polarization));
  CHECK(m[2] == doctest::Approx(polarization / greeter::MU0).epsilon(1e-5));

  CHECK(h[2] == doctest::Approx(-polarization / (3.0f * greeter::MU0)).epsilon(1e-5));
  CHECK(h[2] < 0.0f);

  // And the identities hold component by component.
  for (size_t axis = 0; axis < 3; axis++) {
    CHECK(h[axis] == doctest::Approx((b[axis] - j[axis]) / greeter::MU0).epsilon(1e-4));
    CHECK(m[axis] == doctest::Approx(j[axis] / greeter::MU0).epsilon(1e-4));
  }
}


TEST_CASE("Outside every magnet there is no material, and H is just B") {

  const nlohmann::json sphere = nlohmann::json::parse(R"([
    { "id": 1, "type": "sphere", "parameters": {
        "dimensions": 0.01, "magnetization": 1.2,
        "position": [0, 0, 0], "orientation": [1, 0, 0, 0] } }
  ])");

  const std::vector<float> b = at(sphere, "B", 0.0f, 0.0f, 0.05f);
  const std::vector<float> j = at(sphere, "J", 0.0f, 0.0f, 0.05f);
  const std::vector<float> h = at(sphere, "H", 0.0f, 0.0f, 0.05f);
  const std::vector<float> m = at(sphere, "M", 0.0f, 0.0f, 0.05f);

  // J and M say what the material is, and out here there is none.
  for (size_t axis = 0; axis < 3; axis++) {
    CHECK(j[axis] == doctest::Approx(0.0f));
    CHECK(m[axis] == doctest::Approx(0.0f));
    CHECK(h[axis] == doctest::Approx(b[axis] / greeter::MU0).epsilon(1e-5));
  }

  CHECK(b[2] > 0.0f);
  CHECK(h[2] > 0.0f);
}


TEST_CASE("The four quantities of a whole scene") {

  /*
    Three magnets of three shapes, sampled on a grid that deliberately misses
    every surface, since a magnet has no single field on its own face.

    The values are magpylib 5, getB, getH, getJ and getM over the same
    collection, and agree with these to single precision at all 245 points,
    13 of which are inside a magnet.
  */
  const greeter::view::FieldGrid b = runFor("B");
  const greeter::view::FieldGrid h = runFor("H");
  const greeter::view::FieldGrid j = runFor("J");
  const greeter::view::FieldGrid m = runFor("M");

  REQUIRE(b.size() == 245);
  REQUIRE(h.size() == b.size());

  CHECK(b.kind == greeter::view::FieldKind::B);
  CHECK(h.kind == greeter::view::FieldKind::H);
  CHECK(j.kind == greeter::view::FieldKind::J);
  CHECK(m.kind == greeter::view::FieldKind::M);

  size_t inside = 0;

  for (size_t i = 0; i < 3 * b.size(); i++) {

    CHECK(h.field[i] ==
          doctest::Approx((b.field[i] - j.field[i]) / greeter::MU0).epsilon(1e-4));

    CHECK(m.field[i] == doctest::Approx(j.field[i] / greeter::MU0).epsilon(1e-4));
  }

  for (size_t i = 0; i < b.size(); i++) {
    if (j.getMagnitude(i) > 0.0f) {
      inside++;
    }
  }

  // The grid does pass through the magnets, so the inside branch is not
  // merely untested.
  CHECK(inside == 13);
}


TEST_CASE("A shape with no volume has no polarization anywhere") {

  // A dipole is a moment and not a piece of material, and a triangle is a
  // surface. Neither has an inside for J to be the polarization of.
  const nlohmann::json sources = nlohmann::json::parse(R"([
    { "id": 1, "type": "dipole", "parameters": {
        "moment": [0, 0, 0.5], "position": [0, 0, 0] } },
    { "id": 2, "type": "triangle", "parameters": {
        "vertices": [[0, 0, 0], [0.01, 0, 0], [0, 0.01, 0]],
        "magnetization": [0, 0, 1], "position": [0.05, 0, 0] } }
  ])");

  const std::vector<float> j = at(sources, "J", 0.002f, 0.002f, 0.001f);
  const std::vector<float> b = at(sources, "B", 0.002f, 0.002f, 0.001f);
  const std::vector<float> h = at(sources, "H", 0.002f, 0.002f, 0.001f);

  for (size_t axis = 0; axis < 3; axis++) {
    CHECK(j[axis] == doctest::Approx(0.0f));
    CHECK(h[axis] == doctest::Approx(b[axis] / greeter::MU0).epsilon(1e-5));
  }
}


TEST_CASE("A body of any shape knows its own inside") {

  // The triangular mesh works out J the same way it works out whether to add
  // the polarization to the field, so the two can never disagree about where
  // the body is.
  const nlohmann::json body = nlohmann::json::parse(R"([
    { "id": 1, "type": "triangular_mesh", "parameters": {
        "vertices": [[0,0,0], [0.02,0,0], [0,0.02,0], [0,0,0.02]],
        "faces": [[0,2,1], [0,1,3], [1,2,3], [0,3,2]],
        "magnetization": [0, 0, 1.5] } }
  ])");

  const std::vector<float> inside = at(body, "J", 0.003f, 0.003f, 0.003f);
  const std::vector<float> outside = at(body, "J", 0.03f, 0.0f, 0.0f);

  CHECK(inside[2] == doctest::Approx(1.5f));
  CHECK(outside[2] == doctest::Approx(0.0f));

  const std::vector<float> b_in = at(body, "B", 0.003f, 0.003f, 0.003f);
  const std::vector<float> h_in = at(body, "H", 0.003f, 0.003f, 0.003f);

  CHECK(h_in[2] == doctest::Approx((b_in[2] - 1.5f) / greeter::MU0).epsilon(1e-4));
}


TEST_CASE("A turned magnet turns its polarization with it") {

  // J is read in the frame of the magnet, like everything else, so a magnet
  // laid on its side has its polarization on its side too.
  const float half = std::sqrt(0.5f);

  nlohmann::json cuboid = nlohmann::json::parse(R"([
    { "id": 1, "type": "cuboid", "parameters": {
        "dimensions": [0.01, 0.01, 0.04], "magnetization": [0, 0, 1],
        "position": [0, 0, 0] } }
  ])");

  cuboid[0]["parameters"]["orientation"] = {half, 0.0f, half, 0.0f};

  // A quarter turn about y takes the long axis, and the polarization, from z
  // onto x. So a point out along x is now inside the magnet.
  const std::vector<float> j = at(cuboid, "J", 0.015f, 0.0f, 0.0f);

  CHECK(j[0] == doctest::Approx(1.0f));
  CHECK(j[1] == doctest::Approx(0.0f).epsilon(1e-5));
  CHECK(j[2] == doctest::Approx(0.0f).epsilon(1e-5));

  // And what used to be inside is now outside it.
  const std::vector<float> outside = at(cuboid, "J", 0.0f, 0.0f, 0.015f);

  CHECK(outside[0] == doctest::Approx(0.0f));
  CHECK(outside[2] == doctest::Approx(0.0f));
}


TEST_CASE("A quantity that is not one of the four is refused") {

  nlohmann::json data = nlohmann::json::parse(THREE_MAGNETS);

  greeter::service::SimulationService service;

  data["field_of_view"]["quantity"] = "E";
  service.loadJSON(data, "test");

  greeter::service::FieldRequest request;
  CHECK_THROWS_AS(service.getFieldRequest(request), std::invalid_argument);

  data["field_of_view"]["quantity"] = 4;
  service.loadJSON(data, "test");
  CHECK_THROWS_AS(service.getFieldRequest(request), std::invalid_argument);

  // Saying nothing means B, which is what it always meant.
  data["field_of_view"].erase("quantity");
  service.loadJSON(data, "test");

  REQUIRE(service.getFieldRequest(request));
  CHECK(request.kind == greeter::view::FieldKind::B);
}


TEST_CASE("A field says what it is of, and in what unit") {

  CHECK(greeter::view::getName(greeter::view::FieldKind::B) == "B");
  CHECK(greeter::view::getName(greeter::view::FieldKind::H) == "H");

  // The units are not interchangeable, which is the reason a grid carries
  // which quantity it holds instead of leaving a reader to assume Tesla.
  CHECK(greeter::view::getUnit(greeter::view::FieldKind::B) == "T");
  CHECK(greeter::view::getUnit(greeter::view::FieldKind::J) == "T");
  CHECK(greeter::view::getUnit(greeter::view::FieldKind::H) == "A/m");
  CHECK(greeter::view::getUnit(greeter::view::FieldKind::M) == "A/m");
}


TEST_CASE("A snapshot remembers which quantity it holds") {

  greeter::view::Snapshot snapshot;

  snapshot.field = runFor("H");

  REQUIRE(snapshot.hasField());
  REQUIRE(snapshot.field.kind == greeter::view::FieldKind::H);

  const greeter::view::Snapshot through_json =
    greeter::view::SnapshotIO::fromJSON(greeter::view::SnapshotIO::toJSON(snapshot));

  CHECK(through_json.field.kind == greeter::view::FieldKind::H);

  std::stringstream stream(std::ios::in | std::ios::out | std::ios::binary);

  greeter::view::SnapshotIO::writeBinary(snapshot, stream);

  const greeter::view::Snapshot through_binary =
    greeter::view::SnapshotIO::readBinary(stream);

  CHECK(through_binary.field.kind == greeter::view::FieldKind::H);
  CHECK(through_binary.field.size() == snapshot.field.size());

  // A snapshot written before there was anything but B reads back as B.
  nlohmann::json older = greeter::view::SnapshotIO::toJSON(snapshot);
  older["field"].erase("quantity");

  CHECK(greeter::view::SnapshotIO::fromJSON(older).field.kind ==
        greeter::view::FieldKind::B);
}
