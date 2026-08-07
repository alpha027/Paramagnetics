#include <doctest/doctest.h>
#include <greeter/greeter.h>
#include <greeter/version.h>
#include <greeter/MagnetCollection.h>
#include <greeter/CubicMagnet.h>
#include <greeter/SphericalMagnet.h>
#include <greeter/TetrahedronMagnet.h>
#include <greeter/io/MagnetIO.h>
#include <cmath>

/*
  Reference values are produced with magpylib (see the magpylib repository),
  with the "magnetization" of this library given to magpylib as "polarization"
  and the "vertices" given as the vertices of a magpylib Tetrahedron.

  The kernels of this library run in single precision, so the results are
  compared with a relative tolerance instead of the doctest default.
*/

namespace {

  const double FIELD_TOLERANCE = 1e-4;
  const double FORCE_TOLERANCE = 3e-3;

  void checkVector(const std::vector<float>& value, const double* expected,
                   double relative_tolerance) {

    REQUIRE(value.size() == 3);

    double norm = 0.0;
    for (int i = 0; i < 3; i++) {
      norm += expected[i] * expected[i];
    }
    norm = std::sqrt(norm);

    // Components that vanish by symmetry are compared against the magnitude of
    // the whole vector, a relative tolerance is meaningless for them.
    const double tolerance = relative_tolerance * norm;

    for (int i = 0; i < 3; i++) {
      CHECK(std::fabs((double)value[i] - expected[i]) <= tolerance);
    }
  }

  void checkVector(const float* value, const double* expected,
                   double relative_tolerance) {
    checkVector(std::vector<float>{value[0], value[1], value[2]}, expected,
                relative_tolerance);
  }

  // The unit tetrahedron spanned by the origin and the three axes.
  const std::vector<float> UNIT_TETRAHEDRON = {
    0.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 1.0f
  };

  // A tetrahedron with no symmetry left, to exercise the general case.
  const std::vector<float> IRREGULAR_TETRAHEDRON = {
    -0.4f, -0.3f, -0.2f,
     0.9f,  0.1f, -0.35f,
     0.1f,  1.2f,  0.05f,
     0.0f,  0.2f,  1.4f
  };

  // 35 degrees around the normalized axis (0.3, -0.5, 0.81), as [w, x, y, z]
  const std::vector<float> ROTATION = {
    0.9537169507f, 0.090388169f, -0.1506469483f, 0.2440480562f
  };

  const std::vector<float> NO_ROTATION = {1.0f, 0.0f, 0.0f, 0.0f};
  const std::vector<float> ORIGIN = {0.0f, 0.0f, 0.0f};

  const double OBSERVATION_POINTS[4][3] = {
    { 2.0, 1.0, 1.0},
    {-1.5, 0.7, 0.3},
    { 0.0, 0.0, -2.0},
    { 3.0, -2.0, 1.5}
  };

}  // namespace


TEST_CASE("Tetrahedron magnetic field outside the body") {

  SUBCASE("polarization along z") {

    greeter::TetrahedronMagnet magnet(
      ORIGIN, UNIT_TETRAHEDRON, NO_ROTATION, {0.0f, 0.0f, 1.0f});

    const double expected[4][3] = {
      { 0.0014147623,      0.0006322903,      -0.0008515171},
      {-1.5722750619e-04, -4.2802130121e-06,  -2.2318020645e-03},
      { 0.0003831007,      0.0003831007,       0.002118861},
      { 0.0001817764,     -0.0001495331,      -0.0001654571}
    };

    for (int p = 0; p < 4; p++) {
      checkVector(magnet.computeMagneticField(
        OBSERVATION_POINTS[p][0], OBSERVATION_POINTS[p][1], OBSERVATION_POINTS[p][2]),
        expected[p], FIELD_TOLERANCE);
    }
  }

  SUBCASE("a polarization with three components") {

    greeter::TetrahedronMagnet magnet(
      ORIGIN, UNIT_TETRAHEDRON, NO_ROTATION, {0.3f, -0.7f, 0.5f});

    const double expected[4][3] = {
      { 0.0002279578,      0.0013366358,     -0.000443933},
      { 0.0022243522,      0.000839504,      -0.0011600731},
      {-0.0001523982,      0.0009443457,      0.0009061902},
      { 3.6395390470e-04, -1.8344741590e-04,  7.6477524571e-05}
    };

    for (int p = 0; p < 4; p++) {
      checkVector(magnet.computeMagneticField(
        OBSERVATION_POINTS[p][0], OBSERVATION_POINTS[p][1], OBSERVATION_POINTS[p][2]),
        expected[p], FIELD_TOLERANCE);
    }
  }

  SUBCASE("a tetrahedron with no symmetry") {

    greeter::TetrahedronMagnet magnet(
      ORIGIN, IRREGULAR_TETRAHEDRON, NO_ROTATION, {0.3f, -0.7f, 0.5f});

    const double expected[4][3] = {
      { 0.0009516949,  0.0032975813, -0.0009040967},
      { 0.006604133,   0.002577437,  -0.0036294153},
      {-0.0006565273,  0.0028064818,  0.0025237301},
      { 0.0008928798, -0.0004378462,  0.0001771801}
    };

    for (int p = 0; p < 4; p++) {
      checkVector(magnet.computeMagneticField(
        OBSERVATION_POINTS[p][0], OBSERVATION_POINTS[p][1], OBSERVATION_POINTS[p][2]),
        expected[p], FIELD_TOLERANCE);
    }
  }

  SUBCASE("rotated and translated") {

    greeter::TetrahedronMagnet magnet(
      {0.4f, -0.2f, 1.1f}, UNIT_TETRAHEDRON, ROTATION, {0.3f, -0.7f, 0.5f});

    const double expected[4][3] = {
      {-0.0011089397,      0.001123231,        -0.0009806633},
      { 1.6162118154e-03, -3.6558830443e-05,    7.3689069945e-04},
      {-8.7036181483e-05,  2.0013502672e-04,    3.4810237504e-04},
      { 0.0004727857,     -0.0003037424,       -0.0001726668}
    };

    for (int p = 0; p < 4; p++) {
      checkVector(magnet.computeMagneticField(
        OBSERVATION_POINTS[p][0], OBSERVATION_POINTS[p][1], OBSERVATION_POINTS[p][2]),
        expected[p], FIELD_TOLERANCE);
    }
  }
}


TEST_CASE("Tetrahedron magnetic field inside the body") {

  // Inside the magnet the polarization adds to the field of the surface
  // charges, which is the term the four triangles alone do not carry.
  greeter::TetrahedronMagnet magnet(
    ORIGIN, UNIT_TETRAHEDRON, NO_ROTATION, {0.3f, -0.7f, 0.5f});

  const double points[3][3] = {
    {0.1, 0.1, 0.1}, {0.25, 0.25, 0.25}, {0.5, 0.2, 0.1}
  };

  const double expected[3][3] = {
    {0.1847258736, -0.405570161,  0.3027850805},
    {0.2182152175, -0.5395275367, 0.3697637684},
    {0.2602119605, -0.5369213557, 0.2571163279}
  };

  for (int p = 0; p < 3; p++) {
    checkVector(magnet.computeMagneticField(points[p][0], points[p][1], points[p][2]),
                expected[p], FIELD_TOLERANCE);
  }
}


TEST_CASE("The vertex order of a tetrahedron does not change its field") {

  // The four faces have to be wound so that their normals point out of the
  // body, which takes a chirality check when the vertices come the other way
  // around.
  const std::vector<float> flipped = {
    0.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f,
    0.0f, 1.0f, 0.0f
  };

  greeter::TetrahedronMagnet magnet(ORIGIN, flipped, NO_ROTATION, {0.3f, -0.7f, 0.5f});

  const double expected[2][3] = {
    {0.0002279578, 0.0013366358, -0.000443933},
    {0.0022243522, 0.000839504,  -0.0011600731}
  };

  for (int p = 0; p < 2; p++) {
    checkVector(magnet.computeMagneticField(
      OBSERVATION_POINTS[p][0], OBSERVATION_POINTS[p][1], OBSERVATION_POINTS[p][2]),
      expected[p], FIELD_TOLERANCE);
  }
}


TEST_CASE("A tetrahedron reports its volume and its barycenter") {

  CHECK(greeter::TetrahedronMagnet::volumeOfTetrahedron(UNIT_TETRAHEDRON.data())
        == doctest::Approx(1.0 / 6.0));

  greeter::TetrahedronMagnet magnet(
    {0.4f, -0.2f, 1.1f}, UNIT_TETRAHEDRON, ROTATION, {0.3f, -0.7f, 0.5f});

  // The barycenter is the pivot a torque refers to by default, and unlike a
  // cuboid or a sphere it is not the position of the magnet.
  const double expected[3] = {0.4248805411, 0.0642183838, 1.4421543575};

  checkVector(magnet.getCentroid(), expected, FIELD_TOLERANCE);
}


TEST_CASE("A tetrahedron is meshed on a barycentric grid") {

  std::vector<float> parameters;
  for (const auto& value : ORIGIN) parameters.push_back(value);
  for (const auto& value : NO_ROTATION) parameters.push_back(value);
  for (const auto& value : UNIT_TETRAHEDRON) parameters.push_back(value);
  for (const auto& value : std::vector<float>{0.0f, 0.0f, 1.0f}) parameters.push_back(value);

  REQUIRE(parameters.size() == greeter::TetrahedronMagnet::numberOfParameters());

  SUBCASE("a single cell sits in the barycenter and carries the whole moment") {

    greeter::MeshingSpec meshing;
    greeter::TargetMeshData mesh =
      greeter::TetrahedronMagnet::generateTargetMesh(parameters.data(), meshing);

    REQUIRE(mesh.size() == 1);

    CHECK(mesh[0].point[0] == doctest::Approx(0.25));
    CHECK(mesh[0].point[1] == doctest::Approx(0.25));
    CHECK(mesh[0].point[2] == doctest::Approx(0.25));

    // m = V * J / mu0
    CHECK(mesh[0].moment[2] == doctest::Approx((1.0 / 6.0) / greeter::MU0).epsilon(1e-5));
  }

  SUBCASE("the cell count follows the lattice of magpylib") {

    // A grid with n subdivisions holds (n+1)(n+2)(n+3)/6 points, and the
    // subdivision closest to the request is the one magpylib picks.
    const uint32_t requested[4] = {1, 20, 500, 1000};
    const size_t cells[4] = {1, 20, 455, 969};

    for (int i = 0; i < 4; i++) {
      greeter::MeshingSpec meshing;
      meshing.total = requested[i];
      CHECK(greeter::TetrahedronMagnet::generateTargetMesh(
              parameters.data(), meshing).size() == cells[i]);
    }
  }

  SUBCASE("the cells add up to the volume of the body") {

    greeter::MeshingSpec meshing;
    meshing.total = 500;

    greeter::TargetMeshData mesh =
      greeter::TetrahedronMagnet::generateTargetMesh(parameters.data(), meshing);

    double total_moment = 0.0;
    for (const auto& cell : mesh) {
      total_moment += cell.moment[2];
    }

    CHECK(total_moment == doctest::Approx((1.0 / 6.0) / greeter::MU0).epsilon(1e-4));
  }
}


TEST_CASE("A collection of three magnet types matches magpylib") {

  // A cuboid, a sphere and a rotated tetrahedron evaluated together, which is
  // the case the parallel field kernel has to dispatch on the magnet type for.
  greeter::MagnetCollection collection;

  collection.addMagnet(std::make_unique<greeter::CuboidMagnet>(
    std::vector<float>{2.0f, 0.0f, 0.0f},
    std::vector<float>{1.0f, 1.2f, 0.8f},
    NO_ROTATION,
    std::vector<float>{0.0f, 0.4f, 1.0f}));

  collection.addMagnet(std::make_unique<greeter::SphereMagnet>(
    std::vector<float>{-1.5f, 0.5f, 0.2f}, NO_ROTATION, 0.45f, 1.3f));

  collection.addMagnet(std::make_unique<greeter::TetrahedronMagnet>(
    std::vector<float>{0.4f, -0.2f, 1.1f}, IRREGULAR_TETRAHEDRON, ROTATION,
    std::vector<float>{0.3f, -0.7f, 0.5f}));

  const std::vector<std::vector<float>> observation_points = {
    { 0.0f,  0.0f,  0.0f},
    { 1.0f,  1.0f,  1.0f},
    {-2.0f, -1.0f,  2.5f},
    { 3.0f,  2.0f, -1.0f}
  };

  const double expected[4][3] = {
    {-0.003580736,   0.0050226613, -0.004447884},
    {-0.0426885758,  0.0071305837,  0.0004623999},
    {-0.0024240388, -0.0018890428,  0.0008595445},
    {-0.0014335044, -0.0033668219, -0.005012057}
  };

  std::vector<std::vector<float>> fields = collection.simulate(observation_points);

  REQUIRE(fields.size() == 4);

  for (int p = 0; p < 4; p++) {
    checkVector(fields[p], expected[p], FIELD_TOLERANCE);
  }
}


TEST_CASE("Force on a tetrahedron target") {

  greeter::MagnetCollection collection;

  collection.addMagnet(std::make_unique<greeter::CuboidMagnet>(
    std::vector<float>{0.0f, 0.0f, 0.0f},
    std::vector<float>{1.0f, 1.0f, 1.0f},
    NO_ROTATION,
    std::vector<float>{0.0f, 0.0f, 1.0f}));

  collection.addMagnet(std::make_unique<greeter::TetrahedronMagnet>(
    std::vector<float>{0.0f, 0.0f, 2.0f}, UNIT_TETRAHEDRON, NO_ROTATION,
    std::vector<float>{0.0f, 0.0f, 1.0f}));

  const uint32_t meshings[3] = {1, 20, 500};

  const double expected_force[3][3] = {
    {-485.4166253944, -485.4166251643, -2155.3792121102},
    {-510.5513998283, -510.5513999336, -1959.5844526317},
    {-512.66655576,   -512.6665557027, -2045.45964203}
  };

  const double expected_torque[3][3] = {
    {-285.8798711897, 285.8798711897, 0.0},
    {-367.8496436029, 367.8496436289, 0.0},
    {-356.8676820324, 356.8676820170, 0.0}
  };

  for (int i = 0; i < 3; i++) {

    greeter::ForceConfig config;
    config.targets = {1};
    config.meshing.resize(1);
    config.meshing[0].total = meshings[i];

    std::vector<greeter::ForceResult> results = collection.computeForces(config);

    REQUIRE(results.size() == 1);
    CHECK(results[0].target_index == 1);

    checkVector(results[0].force, expected_force[i], FORCE_TOLERANCE);
    checkVector(results[0].torque, expected_torque[i], FORCE_TOLERANCE);
  }
}


TEST_CASE("Force from a tetrahedron source on a cuboid target") {

  greeter::MagnetCollection collection;

  collection.addMagnet(std::make_unique<greeter::TetrahedronMagnet>(
    std::vector<float>{0.0f, 0.0f, 0.0f}, UNIT_TETRAHEDRON, NO_ROTATION,
    std::vector<float>{0.0f, 0.0f, 1.0f}));

  collection.addMagnet(std::make_unique<greeter::CuboidMagnet>(
    std::vector<float>{0.0f, 0.0f, 2.0f},
    std::vector<float>{1.0f, 1.0f, 1.0f},
    NO_ROTATION,
    std::vector<float>{0.0f, 0.0f, 1.0f}));

  greeter::ForceConfig config;
  config.targets = {1};
  config.meshing.resize(1);
  config.meshing[0].total = 8;

  std::vector<greeter::ForceResult> results = collection.computeForces(config);

  REQUIRE(results.size() == 1);

  const double expected_force[3] = {1430.4864111476, 1430.4864117257, -5956.6218650489};
  const double expected_torque[3] = {565.0457206016, -565.0457204523, 0.0};

  checkVector(results[0].force, expected_force, FORCE_TOLERANCE);
  checkVector(results[0].torque, expected_torque, FORCE_TOLERANCE);
}


TEST_CASE("Force of a cuboid and a sphere together on a rotated tetrahedron") {

  greeter::MagnetCollection collection;

  collection.addMagnet(std::make_unique<greeter::CuboidMagnet>(
    std::vector<float>{2.0f, 0.0f, 0.0f},
    std::vector<float>{1.0f, 1.2f, 0.8f},
    NO_ROTATION,
    std::vector<float>{0.0f, 0.4f, 1.0f}));

  collection.addMagnet(std::make_unique<greeter::SphereMagnet>(
    std::vector<float>{-1.5f, 0.5f, 0.2f}, NO_ROTATION, 0.45f, 1.3f));

  collection.addMagnet(std::make_unique<greeter::TetrahedronMagnet>(
    std::vector<float>{0.4f, -0.2f, 1.1f}, IRREGULAR_TETRAHEDRON, ROTATION,
    std::vector<float>{0.3f, -0.7f, 0.5f}));

  greeter::ForceConfig config;
  config.targets = {2};
  config.meshing.resize(1);
  config.meshing[0].total = 500;

  std::vector<greeter::ForceResult> results = collection.computeForces(config);

  REQUIRE(results.size() == 1);

  // The sum over both sources, which the kernel accumulates before it takes
  // the gradient.
  const double expected_force[3] = {-1811.8360600384, -1603.7521730338, 2824.9667700775};
  const double expected_torque[3] = {195.5044632952, -1974.0809275905, -2206.8884535067};

  checkVector(results[0].force, expected_force, FORCE_TOLERANCE);
  checkVector(results[0].torque, expected_torque, FORCE_TOLERANCE);
}


TEST_CASE("A tetrahedron is read from a JSON file") {

  const char* JSON = R"({
    "magnets": [
      { "id": 4, "type": "tetrahedron", "parameters": {
          "vertices": [[0, 0, 0], [1, 0, 0], [0, 1, 0], [0, 0, 1]],
          "magnetization": [0.3, -0.7, 0.5],
          "position": [0, 0, 0], "orientation": [1, 0, 0, 0] } }
    ],
    "force": { "targets": [4], "meshing": 20 }
  })";

  nlohmann::json data = nlohmann::json::parse(JSON);

  CHECK(greeter::MagnetIO::validateJSON(data));

  greeter::MagnetCollection collection = greeter::MagnetIO::read(data);

  REQUIRE(collection.get_num_magnets() == 1);

  std::vector<float> parameters = collection.getMagnetParameters(0);
  REQUIRE(parameters.size() == greeter::TetrahedronMagnet::numberOfParameters());

  const double expected[3] = {0.0002279578, 0.0013366358, -0.000443933};

  float b_x, b_y, b_z;
  const float observation_point[3] = {2.0f, 1.0f, 1.0f};

  greeter::TetrahedronMagnet::computeMagneticFieldForTetrahedron(
    parameters.data(), observation_point, b_x, b_y, b_z);

  checkVector(std::vector<float>{b_x, b_y, b_z}, expected, FIELD_TOLERANCE);

  SUBCASE("a flat list of twelve numbers is accepted too") {

    nlohmann::json flat = nlohmann::json::parse(JSON);
    flat["magnets"][0]["parameters"]["vertices"] =
      {0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1};

    greeter::MagnetCollection other = greeter::MagnetIO::read(flat);
    CHECK(other.getMagnetParameters(0) == parameters);
  }

  SUBCASE("coplanar vertices are rejected") {

    nlohmann::json flat = nlohmann::json::parse(JSON);
    flat["magnets"][0]["parameters"]["vertices"] =
      {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0}};

    CHECK_THROWS_AS(greeter::MagnetIO::read(flat), std::invalid_argument);
  }

  SUBCASE("a wrong number of vertices is rejected") {

    nlohmann::json flat = nlohmann::json::parse(JSON);
    flat["magnets"][0]["parameters"]["vertices"] = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};

    CHECK_THROWS_AS(greeter::MagnetIO::read(flat), std::invalid_argument);
  }
}
