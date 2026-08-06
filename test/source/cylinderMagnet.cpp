#include <doctest/doctest.h>
#include <greeter/greeter.h>
#include <greeter/version.h>
#include <greeter/MagnetCollection.h>
#include <greeter/CubicMagnet.h>
#include <greeter/SphericalMagnet.h>
#include <greeter/TetrahedronMagnet.h>
#include <greeter/CylinderMagnet.h>
#include <greeter/TargetMeshFactory.h>
#include <greeter/io/MagnetIO.h>

#include <nlohmann/json.hpp>
#include <cmath>

/*
  Reference values are produced with magpylib (see the magpylib repository),
  with the "magnetization" of this library given to magpylib as "polarization"
  and the "dimensions" given as the (diameter, height) of a magpylib Cylinder.

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

  const std::vector<float> NO_ROTATION = {1.0f, 0.0f, 0.0f, 0.0f};

  // 35 degrees about the normalized axis (0.3, -0.5, 0.81), as [w, x, y, z].
  const std::vector<float> ROTATION = {
    0.9537169507f, 0.090388169f, -0.1506469483f, 0.2440480562f};

  // A cylinder of diameter 2 and height 1, so that the dimensionless radius of
  // the closed forms is the radius itself.
  const std::vector<float> UNIT_CYLINDER = {2.0f, 1.0f};

  const float OUTSIDE_POINTS[4][3] = {
    { 3.0f,  1.0f,  2.0f},
    {-2.0f,  4.0f, -1.5f},
    { 0.7f, -0.3f,  0.9f},
    { 0.0f,  0.0f,  4.0f}
  };

  std::vector<float> fieldOf(const std::vector<float>& position,
                             const std::vector<float>& orientation,
                             const std::vector<float>& dimensions,
                             const std::vector<float>& magnetization,
                             const float* observation_point) {

    const float parameters[12] = {
      position[0], position[1], position[2],
      orientation[0], orientation[1], orientation[2], orientation[3],
      dimensions[0], dimensions[1],
      magnetization[0], magnetization[1], magnetization[2]
    };

    float bx, by, bz;
    greeter::CylinderMagnet::computeMagneticFieldForCylinder(
      parameters, observation_point, bx, by, bz);

    return {bx, by, bz};
  }

}  // namespace


TEST_CASE("The complete elliptic integral reproduces the Legendre forms") {

  // K(m) = cel(kc, 1, 1, 1) and E(m) = cel(kc, 1, 1, kc^2) with kc = sqrt(1-m),
  // which is how the two field expressions get their elliptic integrals without
  // a separate implementation of each.
  const double m[5] = {-0.5, -4.0, -100.0, 0.0, 0.5};

  // scipy.special.ellipk / ellipe at those parameters
  const double expected_k[5] = {
    1.4157372084260, 1.0094529099892, 0.3682192486091,
    1.5707963267949, 1.8540746773014};
  const double expected_e[5] = {
    1.7517712756948, 2.6351835815956, 10.2092609198146,
    1.5707963267949, 1.3506438810477};

  for (int i = 0; i < 5; i++) {
    const double kc = std::sqrt(1.0 - m[i]);
    CHECK(greeter::CylinderMagnet::completeEllipticIntegral(kc, 1.0, 1.0, 1.0)
          == doctest::Approx(expected_k[i]).epsilon(1e-12));
    CHECK(greeter::CylinderMagnet::completeEllipticIntegral(kc, 1.0, 1.0, kc * kc)
          == doctest::Approx(expected_e[i]).epsilon(1e-12));
  }

  // kc = 0 is the one argument the algorithm cannot take.
  CHECK_THROWS_AS(
    greeter::CylinderMagnet::completeEllipticIntegral(0.0, 1.0, 1.0, 1.0),
    std::domain_error);
}


TEST_CASE("The field outside a cylinder matches magpylib") {

  SUBCASE("polarization along the axis") {

    const double expected[4][3] = {
      { 6.318233683e-03,  2.106077894e-03, -4.476165160e-04},
      { 1.005095192e-03, -2.010190384e-03, -1.667875628e-03},
      { 1.106542352e-01, -4.742324364e-02,  1.575160307e-01},
      { 0.000000000e+00,  0.000000000e+00,  7.331556272e-03}
    };

    for (int i = 0; i < 4; i++) {
      checkVector(fieldOf({0, 0, 0}, NO_ROTATION, UNIT_CYLINDER, {0, 0, 1},
                          OUTSIDE_POINTS[i]),
                  expected[i], FIELD_TOLERANCE);
    }
  }

  SUBCASE("polarization across the axis") {

    const double expected[4][3] = {
      { 4.191124247e-03,  2.975486992e-03,  6.318233683e-03},
      {-1.103569149e-03, -2.583342617e-03,  1.005095192e-03},
      {-6.637759062e-02, -1.299944599e-02,  1.106542352e-01},
      {-3.665778136e-03,  0.000000000e+00,  0.000000000e+00}
    };

    for (int i = 0; i < 4; i++) {
      checkVector(fieldOf({0, 0, 0}, NO_ROTATION, UNIT_CYLINDER, {1, 0, 0},
                          OUTSIDE_POINTS[i]),
                  expected[i], FIELD_TOLERANCE);
    }
  }

  SUBCASE("oblique polarization, both parts at once") {

    const double expected[4][3] = {
      { 4.824180725e-03,  4.449262278e-03,  4.843379449e-04},
      { 1.764676717e-03, -3.768877480e-03, -2.767675299e-05},
      { 7.510983394e-02,  3.730791347e-03,  1.829207170e-01},
      {-1.099733441e-03,  1.832889068e-03,  5.865245017e-03}
    };

    for (int i = 0; i < 4; i++) {
      checkVector(fieldOf({0, 0, 0}, NO_ROTATION, UNIT_CYLINDER,
                          {0.3f, -0.5f, 0.8f}, OUTSIDE_POINTS[i]),
                  expected[i], FIELD_TOLERANCE);
    }
  }
}


TEST_CASE("The field inside a cylinder matches magpylib") {

  // Inside the body the diametral part returns an H-field, so the polarization
  // it leaves out has to be added back.
  const float points[3][3] = {
    { 0.2f,  0.1f,  0.1f},
    {-0.4f,  0.3f, -0.2f},
    { 0.9f,  0.0f,  0.0f}
  };

  const double expected[3][3] = {
    {2.396264393e-01, -3.845187982e-01, 3.632788940e-01},
    {2.518960732e-01, -4.012804810e-01, 4.129966219e-01},
    {1.828629706e-01, -3.622502436e-01, 5.327650220e-01}
  };

  for (int i = 0; i < 3; i++) {
    checkVector(fieldOf({0, 0, 0}, NO_ROTATION, UNIT_CYLINDER,
                        {0.3f, -0.5f, 0.8f}, points[i]),
                expected[i], FIELD_TOLERANCE);
  }
}


TEST_CASE("The field near the axis takes the series branch and still matches") {

  // Below a dimensionless radius of 0.05 the general diametral expression is
  // replaced by a series, so these points exercise a different code path.
  const float points[4][3] = {
    {0.0f,   0.0f,  2.0f},
    {0.001f, 0.0f,  0.8f},
    {0.02f,  0.01f, -3.0f},
    {0.0f,   0.0f,  0.1f}
  };

  const double expected[4][3] = {
    {-7.231979741e-03,  1.205329957e-02, 3.857055862e-02},
    {-3.776528026e-02,  6.315950674e-02, 2.021593078e-01},
    {-2.600284412e-03,  4.069665700e-03, 1.320945240e-02},
    { 2.335585176e-01, -3.892641960e-01, 3.543545727e-01}
  };

  for (int i = 0; i < 4; i++) {
    checkVector(fieldOf({0, 0, 0}, NO_ROTATION, UNIT_CYLINDER,
                        {0.3f, -0.5f, 0.8f}, points[i]),
                expected[i], FIELD_TOLERANCE);
  }
}


TEST_CASE("A rotated and translated cylinder matches magpylib") {

  const std::vector<float> dimensions = {1.4f, 2.2f};

  const double expected[4][3] = {
    { 3.598930642e-03, -2.054854743e-03,  5.771094597e-03},
    {-1.884916121e-03,  1.613476471e-03, -3.874970492e-04},
    { 1.365005540e-01,  5.040501046e-01, -4.406501171e-01},
    {-5.266456687e-04, -5.598583261e-03, -2.079802956e-03}
  };

  for (int i = 0; i < 4; i++) {
    checkVector(fieldOf({0.5f, -0.7f, 0.3f}, ROTATION, dimensions,
                        {0.4f, 0.6f, -0.7f}, OUTSIDE_POINTS[i]),
                expected[i], FIELD_TOLERANCE);
  }
}


TEST_CASE("The rim of a cylinder is excluded rather than infinite") {

  // Where the hull meets a base the closed form diverges, and magpylib returns
  // zero there instead of an infinity.
  const float on_rim[3] = {1.0f, 0.0f, 0.5f};

  const std::vector<float> field =
    fieldOf({0, 0, 0}, NO_ROTATION, UNIT_CYLINDER, {0.3f, -0.5f, 0.8f}, on_rim);

  CHECK(field[0] == 0.0f);
  CHECK(field[1] == 0.0f);
  CHECK(field[2] == 0.0f);

  // Just off the rim the field is large but finite.
  const float near_rim[3] = {1.0f, 0.0f, 0.55f};
  const std::vector<float> nearby =
    fieldOf({0, 0, 0}, NO_ROTATION, UNIT_CYLINDER, {0.3f, -0.5f, 0.8f}, near_rim);

  for (int i = 0; i < 3; i++) {
    CHECK(std::isfinite(nearby[i]));
  }
}


TEST_CASE("The cylinder mesh matches magpylib cell for cell") {

  const float parameters[12] = {
    0, 0, 0, 1, 0, 0, 0, 2.0f, 1.0f, 0.0f, 0.0f, 1.0f};

  // magpylib _target_mesh_cylinder(0, 1, 1, 0, 360, n) cell counts. A request
  // of one cell gives two, because the apportioning never drops a dimension
  // below one division.
  const uint32_t requested[7] = {1, 5, 20, 100, 500, 1000, 5000};
  const size_t expected_cells[7] = {2, 12, 30, 124, 570, 1134, 5208};

  for (int i = 0; i < 7; i++) {

    greeter::MeshingSpec meshing;
    meshing.total = requested[i];

    greeter::TargetMeshData mesh =
      greeter::CylinderMagnet::generateTargetMesh(parameters, meshing);

    CHECK(mesh.size() == expected_cells[i]);

    // The cells fill the body exactly, whatever the split.
    double total_moment = 0.0;
    for (const auto& cell : mesh) {
      total_moment += (double) cell.moment[2];
    }

    const double exact_volume = M_PI * 1.0 * 1.0 * 1.0;
    CHECK(total_moment * (double) greeter::MU0
          == doctest::Approx(exact_volume).epsilon(1e-5));
  }

  SUBCASE("a cylinder that is not the unit one") {

    const float other[12] = {
      0, 0, 0, 1, 0, 0, 0, 1.4f, 2.2f, 0.0f, 0.0f, 1.0f};

    const uint32_t requested_other[3] = {1, 20, 500};
    const size_t expected_other[3] = {2, 27, 550};

    for (int i = 0; i < 3; i++) {

      greeter::MeshingSpec meshing;
      meshing.total = requested_other[i];

      greeter::TargetMeshData mesh =
        greeter::CylinderMagnet::generateTargetMesh(other, meshing);

      CHECK(mesh.size() == expected_other[i]);

      double total_moment = 0.0;
      for (const auto& cell : mesh) {
        total_moment += (double) cell.moment[2];
      }

      CHECK(total_moment * (double) greeter::MU0
            == doctest::Approx(M_PI * 0.7 * 0.7 * 2.2).epsilon(1e-5));
    }
  }

  SUBCASE("every cell sits inside the body") {

    greeter::MeshingSpec meshing;
    meshing.total = 1000;

    greeter::TargetMeshData mesh =
      greeter::CylinderMagnet::generateTargetMesh(parameters, meshing);

    for (const auto& cell : mesh) {
      const double radius =
        std::sqrt((double) cell.point[0] * cell.point[0]
                  + (double) cell.point[1] * cell.point[1]);
      CHECK(radius <= 1.0);
      CHECK(std::fabs((double) cell.point[2]) <= 0.5);
    }
  }
}


TEST_CASE("The force on a cylinder target matches magpylib") {

  greeter::MagnetCollection collection;

  collection.addMagnet(std::make_unique<greeter::CuboidMagnet>(
    std::vector<float>{0.0f, 0.0f, 3.0f},
    std::vector<float>{1.0f, 1.0f, 1.0f},
    NO_ROTATION,
    std::vector<float>{0.0f, 0.0f, 1.0f}));

  collection.addMagnet(std::make_unique<greeter::CylinderMagnet>(
    std::vector<float>{0.0f, 0.0f, 0.0f}, NO_ROTATION, UNIT_CYLINDER,
    std::vector<float>{0.3f, -0.5f, 0.8f}));

  const uint32_t meshings[3] = {20, 500, 5000};

  const double expected_force[3][3] = {
    {-1.837094328e+03, 3.061823360e+03, 9.797834634e+03},
    {-1.819724572e+03, 3.032874286e+03, 9.705197717e+03},
    {-1.814864746e+03, 3.024774577e+03, 9.679278646e+03}
  };

  const double expected_torque[3][3] = {
    {-5.901670653e+03, -3.541003228e+03, 0.0},
    {-5.831912503e+03, -3.499147502e+03, 0.0},
    {-5.812590305e+03, -3.487554183e+03, 0.0}
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


TEST_CASE("The force from a cylinder source matches magpylib") {

  greeter::MagnetCollection collection;

  collection.addMagnet(std::make_unique<greeter::CylinderMagnet>(
    std::vector<float>{0.0f, 0.0f, 0.0f}, NO_ROTATION, UNIT_CYLINDER,
    std::vector<float>{0.3f, -0.5f, 0.8f}));

  collection.addMagnet(std::make_unique<greeter::CuboidMagnet>(
    std::vector<float>{0.0f, 0.0f, 3.0f},
    std::vector<float>{1.0f, 1.0f, 1.0f},
    NO_ROTATION,
    std::vector<float>{0.0f, 0.0f, 1.0f}));

  greeter::ForceConfig config;
  config.targets = {1};
  config.meshing.resize(1);
  config.meshing[0].total = 1000;

  std::vector<greeter::ForceResult> results = collection.computeForces(config);

  REQUIRE(results.size() == 1);

  const double expected_force[3] = {
    1.813922344e+03, -3.023203907e+03, -9.674252496e+03};
  const double expected_torque[3] = {
    -3.260774639e+03, -1.956464783e+03, 0.0};

  checkVector(results[0].force, expected_force, FORCE_TOLERANCE);
  checkVector(results[0].torque, expected_torque, FORCE_TOLERANCE);
}


TEST_CASE("A collection of all four magnet types agrees with magpylib") {

  greeter::MagnetCollection collection;

  collection.addMagnet(std::make_unique<greeter::CuboidMagnet>(
    std::vector<float>{0.0f, 0.0f, 3.0f},
    std::vector<float>{1.0f, 1.0f, 1.0f},
    NO_ROTATION,
    std::vector<float>{0.0f, 0.0f, 1.0f}));

  collection.addMagnet(std::make_unique<greeter::SphereMagnet>(
    std::vector<float>{2.5f, 0.0f, 0.0f}, NO_ROTATION, 0.5f, 1.0f));

  collection.addMagnet(std::make_unique<greeter::TetrahedronMagnet>(
    std::vector<float>{-2.0f, -2.0f, 1.0f},
    std::vector<float>{0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1},
    NO_ROTATION,
    std::vector<float>{0.0f, 0.0f, 1.0f}));

  collection.addMagnet(std::make_unique<greeter::CylinderMagnet>(
    std::vector<float>{0.5f, -3.0f, 0.3f}, NO_ROTATION,
    std::vector<float>{1.4f, 2.2f},
    std::vector<float>{0.4f, 0.6f, -0.7f}));

  REQUIRE(collection.get_num_magnets() == 4);

  // The four types carry different parameter counts, which the simulator has to
  // lay out correctly for the superposition to come out right.
  CHECK(collection.getMagnetParameters(0).size() == 13);
  CHECK(collection.getMagnetParameters(1).size() == 11);
  CHECK(collection.getMagnetParameters(2).size() == 22);
  CHECK(collection.getMagnetParameters(3).size() == 12);

  const std::vector<std::vector<float>> points = {
    { 3.0f, 1.0f,  2.0f},
    {-2.0f, 4.0f, -1.5f},
    { 0.0f, 0.0f,  4.0f}
  };

  const double expected[3][3] = {
    { 8.192348958e-04,  4.418210007e-03, 5.157155731e-03},
    {-2.761486292e-04,  3.216039598e-05, 1.669685384e-04},
    {-1.131719251e-03, -2.145066850e-03, 1.360972420e-01}
  };

  std::vector<std::vector<float>> fields = collection.simulate(points);

  REQUIRE(fields.size() == 3);

  for (int i = 0; i < 3; i++) {
    checkVector(fields[i], expected[i], FIELD_TOLERANCE);
  }
}


TEST_CASE("Force on a rotated cylinder from all four source types") {

  greeter::MagnetCollection collection;

  collection.addMagnet(std::make_unique<greeter::CuboidMagnet>(
    std::vector<float>{0.0f, 0.0f, 3.0f},
    std::vector<float>{1.0f, 1.0f, 1.0f},
    NO_ROTATION,
    std::vector<float>{0.0f, 0.0f, 1.0f}));

  collection.addMagnet(std::make_unique<greeter::SphereMagnet>(
    std::vector<float>{2.5f, 0.0f, 0.0f}, NO_ROTATION, 0.5f, 1.0f));

  collection.addMagnet(std::make_unique<greeter::TetrahedronMagnet>(
    std::vector<float>{-2.0f, -2.0f, 1.0f},
    std::vector<float>{0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1},
    NO_ROTATION,
    std::vector<float>{0.0f, 0.0f, 1.0f}));

  collection.addMagnet(std::make_unique<greeter::CylinderMagnet>(
    std::vector<float>{0.5f, -3.0f, 0.3f}, NO_ROTATION,
    std::vector<float>{1.4f, 2.2f},
    std::vector<float>{0.4f, 0.6f, -0.7f}));

  collection.addMagnet(std::make_unique<greeter::CylinderMagnet>(
    std::vector<float>{0.0f, 0.0f, 0.0f}, ROTATION, UNIT_CYLINDER,
    std::vector<float>{0.3f, -0.5f, 0.8f}));

  greeter::ForceConfig config;
  config.targets = {4};
  config.meshing.resize(1);
  config.meshing[0].total = 2000;

  std::vector<greeter::ForceResult> results = collection.computeForces(config);

  REQUIRE(results.size() == 1);
  CHECK(results[0].target_index == 4);

  const double expected_force[3] = {
    -1.647705553e+04, 1.134315765e+04, 2.243990558e+04};
  const double expected_torque[3] = {
    -2.815448446e+04, -2.021366118e+04, -1.975689997e+03};

  checkVector(results[0].force, expected_force, FORCE_TOLERANCE);
  checkVector(results[0].torque, expected_torque, FORCE_TOLERANCE);
}


TEST_CASE("A cylinder is read from JSON") {

  const char* JSON = R"({
    "magnets": [
      { "id": 1, "type": "cylinder", "parameters": {
          "dimensions": [2, 1], "magnetization": [0.3, -0.5, 0.8],
          "position": [0, 0, 0], "orientation": [1, 0, 0, 0] } }
    ],
    "field_of_view": {
      "x": { "min": 0, "max": 1, "n": 2 },
      "y": { "min": 0, "max": 1, "n": 2 },
      "z": { "min": 0, "max": 1, "n": 2 }
    }
  })";

  greeter::MagnetCollection collection =
    greeter::MagnetIO::read(nlohmann::json::parse(JSON));

  REQUIRE(collection.get_num_magnets() == 1);

  std::vector<float> parameters = collection.getMagnetParameters(0);
  REQUIRE(parameters.size() == 12);

  CHECK(parameters[7] == doctest::Approx(2.0f));
  CHECK(parameters[8] == doctest::Approx(1.0f));

  const std::vector<std::vector<float>> points = {{3.0f, 1.0f, 2.0f}};
  const double expected[3] = {4.824180725e-03, 4.449262278e-03, 4.843379449e-04};

  std::vector<std::vector<float>> fields = collection.simulate(points);
  checkVector(fields[0], expected, FIELD_TOLERANCE);

  SUBCASE("a magnetization given as one number is read as axial") {

    nlohmann::json data = nlohmann::json::parse(JSON);
    data["magnets"][0]["parameters"]["magnetization"] = 1.0;

    greeter::MagnetCollection axial =
      greeter::MagnetIO::read(data);

    std::vector<float> read = axial.getMagnetParameters(0);
    CHECK(read[9] == doctest::Approx(0.0f));
    CHECK(read[10] == doctest::Approx(0.0f));
    CHECK(read[11] == doctest::Approx(1.0f));
  }

  SUBCASE("a bad geometry is rejected") {

    nlohmann::json one_number = nlohmann::json::parse(JSON);
    one_number["magnets"][0]["parameters"]["dimensions"] = {2};
    CHECK_THROWS_AS(greeter::MagnetIO::read(one_number), std::invalid_argument);

    nlohmann::json three_numbers = nlohmann::json::parse(JSON);
    three_numbers["magnets"][0]["parameters"]["dimensions"] = {2, 1, 1};
    CHECK_THROWS_AS(greeter::MagnetIO::read(three_numbers), std::invalid_argument);

    nlohmann::json negative = nlohmann::json::parse(JSON);
    negative["magnets"][0]["parameters"]["dimensions"] = {2, -1};
    CHECK_THROWS_AS(greeter::MagnetIO::read(negative), std::invalid_argument);

    nlohmann::json zero_diameter = nlohmann::json::parse(JSON);
    zero_diameter["magnets"][0]["parameters"]["dimensions"] = {0, 1};
    CHECK_THROWS_AS(greeter::MagnetIO::read(zero_diameter), std::invalid_argument);
  }
}


TEST_CASE("The cylinder is reachable through every registry") {

  const u_int16_t type_id = greeter::CylinderMagnet::getStaticTypeID();

  CHECK(greeter::MagneticFieldMethodFactory::getInstance()
          .getNumberOfParameters(type_id) == 12);

  const bool kernel_is_registered =
    greeter::MagneticFieldMethodFactory::getInstance()
      .getComputeMagneticField(type_id) != nullptr;
  CHECK(kernel_is_registered);

  const float parameters[12] = {
    0, 0, 0, 1, 0, 0, 0, 2.0f, 1.0f, 0.0f, 0.0f, 1.0f};

  greeter::MeshingSpec meshing;
  meshing.total = 20;

  CHECK(greeter::TargetMeshFactory::getInstance()
          .generateTargetMesh(type_id, parameters, meshing).size() == 30);
}
