#include <doctest/doctest.h>

#include <greeter/DipoleMagnet.h>
#include <greeter/MagnetCollection.h>
#include <greeter/SphericalMagnet.h>
#include <greeter/TargetMesh.h>
#include <greeter/io/DipoleMagnetIO.h>
#include <greeter/io/MagnetIO.h>
#include <greeter/io/MethodFactoryIO.h>
#include <greeter/service/SimulationService.h>

#include <cmath>


TEST_CASE("The field of a point dipole") {

  // magpylib 5, Dipole(moment=(0.3, -0.2, 0.5), position=(0.01, 0, 0)).
  greeter::DipoleMagnet dipole(
    {0.01f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f}, {0.3f, -0.2f, 0.5f});

  struct Case {
    float point[3];
    double expected[3];
  };

  const std::vector<Case> cases = {
    {{ 0.05f, 0.00f, 0.00f}, { 0.0009375,     0.0003125,    -0.00078125}},
    {{ 0.00f, 0.05f, 0.00f}, {-0.000113144,  -0.000414861,  -0.000377146}},
    {{ 0.00f, 0.00f, 0.05f}, {-0.000417762,   0.000150859,   0.000580225}},
    {{-0.02f, 0.03f, 0.04f}, {-0.000218082,   0.000167641,  -0.000163191}}
  };

  for (const auto& item : cases) {

    const std::vector<float> field =
      dipole.computeMagneticField(item.point[0], item.point[1], item.point[2]);

    for (size_t axis = 0; axis < 3; axis++) {
      CHECK(field[axis] == doctest::Approx(item.expected[axis]).epsilon(1e-4));
    }
  }
}


TEST_CASE("Far away, a sphere is a dipole of the moment it carries") {

  // This is what the type is for. A magnet of volume V and polarization J has
  // the moment m = V * J / mu0, and beyond a few of its own diameters nothing
  // else about its shape shows. An array of a thousand magnets seen from a
  // metre away is a thousand dipoles, and costs far less evaluated as such.
  const float radius = 0.005f;
  const float polarization = 1.2f;

  const float volume = 4.0f / 3.0f * (float) M_PI * radius * radius * radius;
  const float moment = volume * polarization / greeter::MU0;

  CHECK(moment == doctest::Approx(0.5f).epsilon(1e-4));

  greeter::SphereMagnet sphere(radius, polarization);

  greeter::DipoleMagnet dipole(
    {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, moment});

  const std::vector<std::vector<float>> far = {
    {0.0f, 0.0f, 0.2f},
    {0.15f, 0.0f, 0.1f},
    {-0.08f, 0.12f, -0.05f}
  };

  for (const auto& point : far) {

    const std::vector<float> from_sphere =
      sphere.computeMagneticField(point[0], point[1], point[2]);

    const std::vector<float> from_dipole =
      dipole.computeMagneticField(point[0], point[1], point[2]);

    for (size_t axis = 0; axis < 3; axis++) {
      CHECK(from_dipole[axis] == doctest::Approx(from_sphere[axis]).epsilon(1e-4));
    }
  }

  // The equivalence is exact for a sphere at every distance outside it, which
  // is the one shape for which that is true.
  const std::vector<float> near_sphere = sphere.computeMagneticField(0.0f, 0.0f, 0.006f);
  const std::vector<float> near_dipole = dipole.computeMagneticField(0.0f, 0.0f, 0.006f);

  CHECK(near_dipole[2] == doctest::Approx(near_sphere[2]).epsilon(1e-4));
}


TEST_CASE("A dipole is turned with its own frame") {

  // The moment is read in the frame of the dipole and carried by its
  // orientation, the convention every magnet here follows and the one the
  // arrangements rely on.
  const float half = std::sqrt(0.5f);

  greeter::DipoleMagnet along_z(
    {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f});

  // A quarter turn about y takes the local z axis onto the global x axis.
  greeter::DipoleMagnet turned(
    {0.0f, 0.0f, 0.0f}, {half, 0.0f, half, 0.0f}, {0.0f, 0.0f, 1.0f});

  // On the axis of the moment the field is twice as strong as across it, and
  // points along the moment. So the turned dipole read along x has to give
  // what the unturned one gives along z.
  const std::vector<float> reference = along_z.computeMagneticField(0.0f, 0.0f, 0.1f);
  const std::vector<float> rotated = turned.computeMagneticField(0.1f, 0.0f, 0.0f);

  CHECK(rotated[0] == doctest::Approx(reference[2]));
  CHECK(rotated[1] == doctest::Approx(0.0f).epsilon(1e-5));
  CHECK(rotated[2] == doctest::Approx(0.0f).epsilon(1e-5));
}


TEST_CASE("A dipole is infinite where it sits") {

  greeter::DipoleMagnet dipole(
    {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f});

  const std::vector<float> at_the_point = dipole.computeMagneticField(0.0f, 0.0f, 0.0f);

  // A real singularity rather than a gap in the implementation, and magpylib
  // says the same. Returning zero would make a field of view asked for right
  // on top of a source look like a sensible answer.
  CHECK(std::isinf(at_the_point[2]));
  CHECK(at_the_point[2] > 0.0f);

  // The components the moment has nothing in stay finite. Getting this right
  // means turning the moment into the world frame first and putting the
  // infinity on afterwards: rotating a field that already holds one gives
  // not-a-number wherever the matrix has a zero, and "unknown" is a worse
  // answer than "nothing".
  CHECK(at_the_point[0] == doctest::Approx(0.0f));
  CHECK(at_the_point[1] == doctest::Approx(0.0f));

  // And it is infinite along the moment wherever the moment has been turned
  // to point, not along whichever axis it started on.
  const float half = std::sqrt(0.5f);

  greeter::DipoleMagnet turned(
    {0.0f, 0.0f, 0.0f}, {half, 0.0f, half, 0.0f}, {0.0f, 0.0f, 1.0f});

  const std::vector<float> singular = turned.computeMagneticField(0.0f, 0.0f, 0.0f);

  CHECK(std::isinf(singular[0]));
  CHECK(singular[0] > 0.0f);
  CHECK(singular[1] == doctest::Approx(0.0f));
  CHECK(singular[2] == doctest::Approx(0.0f));
}


TEST_CASE("A dipole is meshed as the one cell it already is") {

  const float parameters[10] = {
    0.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 0.0f, 0.0f,
    0.3f, -0.2f, 0.5f
  };

  greeter::MeshingSpec meshing;
  meshing.total = 64;

  const greeter::TargetMeshData mesh =
    greeter::DipoleMagnet::generateTargetMesh(parameters, meshing);

  // Splitting a point is not a thing, so the meshing input is ignored, as it
  // is for a sphere.
  REQUIRE(mesh.size() == 1);

  CHECK(mesh[0].point[0] == doctest::Approx(0.0f));

  // The moment of the cell is the moment given, not a volume times a
  // polarization: a dipole is stated directly in the unit a cell carries.
  CHECK(mesh[0].moment[0] == doctest::Approx(0.3f));
  CHECK(mesh[0].moment[2] == doctest::Approx(0.5f));
}


TEST_CASE("A dipole is read from a file, and a moment is not a magnetization") {

  const nlohmann::json good = nlohmann::json::parse(R"({
    "type": "dipole",
    "parameters": { "moment": [0.3, -0.2, 0.5], "position": [0.01, 0, 0] }
  })");

  const std::unique_ptr<greeter::Magnet> magnet =
    greeter::MethodFactoryIO::getInstance().createMagnet("dipole", good);

  REQUIRE(magnet != nullptr);
  CHECK(magnet->getTypeID() == greeter::DipoleMagnet::getStaticTypeID());
  CHECK(magnet->getDimensions().empty());
  CHECK(magnet->getMagnetization()[2] == doctest::Approx(0.5f));

  // An orientation and a position may be left out.
  CHECK(magnet->getOrientation()[0] == doctest::Approx(1.0f));

  // A moment along the local axis may be given as one number.
  const nlohmann::json single = nlohmann::json::parse(
    R"({"type": "dipole", "parameters": {"moment": 0.7}})");

  CHECK(greeter::DipoleMagnetIO::readMoment(single)[2] == doctest::Approx(0.7f));

  // Writing the moment as a "magnetization" is refused rather than read.
  // The two are different quantities in different units, and quietly reading
  // one as the other turns a units mistake into a plausible answer.
  const nlohmann::json wrong = nlohmann::json::parse(
    R"({"type": "dipole", "parameters": {"magnetization": [0, 0, 1]}})");

  CHECK_THROWS_AS(greeter::DipoleMagnetIO::readMoment(wrong), std::invalid_argument);

  const nlohmann::json missing = nlohmann::json::parse(
    R"({"type": "dipole", "parameters": {"position": [0, 0, 0]}})");

  CHECK_THROWS_AS(greeter::DipoleMagnetIO::readMoment(missing), std::invalid_argument);
}


TEST_CASE("A dipole says that it carries a moment and not a polarization") {

  greeter::service::SimulationService service;

  service.loadJSON(nlohmann::json::parse(R"({
    "magnets": [
      { "id": 1, "type": "dipole", "parameters": {
          "moment": [0, 0, 0.5], "position": [0, 0, 0] } },
      { "id": 2, "type": "cuboid", "parameters": {
          "dimensions": [0.01, 0.01, 0.01], "magnetization": [0, 0, 1],
          "position": [0.05, 0, 0], "orientation": [1, 0, 0, 0] } }
    ],
    "field_of_view": {
      "x": {"min": 0, "max": 0, "n": 1},
      "y": {"min": 0, "max": 0, "n": 1},
      "z": {"min": 0.1, "max": 0.1, "n": 1}
    }
  })"), "test");

  const greeter::view::SceneSnapshot scene = service.getScene();

  REQUIRE(scene.magnets.size() == 2);

  const greeter::view::MagnetView* dipole = scene.findById(1);
  const greeter::view::MagnetView* cuboid = scene.findById(2);

  REQUIRE(dipole != nullptr);
  REQUIRE(cuboid != nullptr);

  // A viewer would otherwise put Tesla on a number in ampere metre squared.
  CHECK(dipole->moment_kind == greeter::view::MomentKind::Moment);
  CHECK(cuboid->moment_kind == greeter::view::MomentKind::Polarization);

  CHECK(greeter::view::getUnit(dipole->moment_kind) == "A m^2");
  CHECK(greeter::view::getUnit(cuboid->moment_kind) == "T");

  // It has no extent, so it is drawn as a marker where it sits.
  CHECK(dipole->shape.kind == greeter::view::ShapeKind::Point);
  CHECK(dipole->shape.isValid());
  CHECK(dipole->shape.getLocalHalfExtent()[0] == doctest::Approx(0.0f));
}


TEST_CASE("A dipole takes part in a force simulation") {

  greeter::service::SimulationService service;

  // Two dipoles pointing the same way and stacked along their common axis
  // attract, exactly as two magnets do.
  service.loadJSON(nlohmann::json::parse(R"({
    "magnets": [
      { "id": 1, "type": "dipole", "parameters": {
          "moment": [0, 0, 1], "position": [0, 0, 0] } },
      { "id": 2, "type": "dipole", "parameters": {
          "moment": [0, 0, 1], "position": [0, 0, 0.05] } }
    ],
    "force": { "targets": "all" }
  })"), "test");

  const greeter::view::ForceReport report = service.simulateForces();

  REQUIRE(report.entries.size() == 2);

  const greeter::view::ForceEntry* lower = report.findById(1);
  const greeter::view::ForceEntry* upper = report.findById(2);

  REQUIRE(lower != nullptr);
  REQUIRE(upper != nullptr);

  CHECK(lower->force[2] > 0.0f);
  CHECK(upper->force[2] < 0.0f);
  CHECK(lower->force[2] == doctest::Approx(-upper->force[2]).epsilon(0.001));

  // The exact answer for two coaxial dipoles is F = 3 mu0 m1 m2 / (2 pi z^4).
  const double expected = 3.0 * (double) greeter::MU0 /
                          (2.0 * M_PI * std::pow(0.05, 4));

  CHECK(lower->force[2] == doctest::Approx(expected).epsilon(0.01));
}
