#include <doctest/doctest.h>
#include <greeter/greeter.h>
#include <greeter/version.h>
#include <greeter/MagnetCollection.h>
#include <greeter/CubicMagnet.h>
#include <greeter/SphericalMagnet.h>
#include <cmath>

/*
  Reference values are produced with the magpylib getFT function (see the
  magpylib repository), with the "magnetization" of this library given to
  magpylib as "polarization" and with the same finite difference step.

  The kernels of this library run in single precision, so the results are
  compared with a relative tolerance instead of the doctest default.
*/

namespace {

  const double RELATIVE_TOLERANCE = 2e-3;

  void checkVector(const float* value, const double* expected) {

    double norm = 0.0;
    for (int i = 0; i < 3; i++) {
      norm += expected[i] * expected[i];
    }
    norm = std::sqrt(norm);

    // Components that vanish by symmetry are compared against the magnitude of
    // the whole vector, a relative tolerance is meaningless for them.
    const double tolerance = RELATIVE_TOLERANCE * norm;

    for (int i = 0; i < 3; i++) {
      CHECK(std::fabs((double)value[i] - expected[i]) <= tolerance);
    }
  }

  greeter::ForceConfig makeConfig(uint32_t target, const greeter::MeshingSpec& meshing,
                                  float eps) {
    greeter::ForceConfig config;
    config.targets = {target};
    config.meshing = {meshing};
    config.eps = eps;
    return config;
  }

  greeter::MeshingSpec scalarMeshing(uint32_t total) {
    greeter::MeshingSpec meshing;
    meshing.total = total;
    return meshing;
  }

}  // namespace


TEST_CASE("Target meshing of a cuboid magnet") {

  // 1 x 1 x 1 cube, polarization 1 T along e_z
  const float parameters[13] = {
    0.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 0.0f, 0.0f,
    1.0f, 1.0f, 1.0f,
    0.0f, 0.0f, 1.0f
  };

  SUBCASE("a single cell carries the moment of the whole magnet") {

    greeter::TargetMeshData mesh = greeter::CuboidMagnet::generateTargetMesh(
      parameters, scalarMeshing(1));

    REQUIRE(mesh.size() == 1);

    CHECK(mesh[0].point[0] == doctest::Approx(0.0));
    CHECK(mesh[0].point[1] == doctest::Approx(0.0));
    CHECK(mesh[0].point[2] == doctest::Approx(0.0));

    // m = V * J / mu_0
    CHECK(mesh[0].moment[2] == doctest::Approx(1.0 / greeter::MU0).epsilon(1e-4));
  }

  SUBCASE("the scalar meshing input targets a cell aspect ratio of one") {

    greeter::TargetMeshData mesh = greeter::CuboidMagnet::generateTargetMesh(
      parameters, scalarMeshing(1000));

    CHECK(mesh.size() == 1000);
  }

  SUBCASE("the cell moments sum up to the moment of the whole magnet") {

    greeter::MeshingSpec meshing;
    meshing.explicit_split = true;
    meshing.n[0] = 2;
    meshing.n[1] = 3;
    meshing.n[2] = 4;

    greeter::TargetMeshData mesh = greeter::CuboidMagnet::generateTargetMesh(
      parameters, meshing);

    REQUIRE(mesh.size() == 24);

    double total_moment = 0.0;
    double centroid = 0.0;

    for (const auto& cell : mesh) {
      total_moment += cell.moment[2];
      centroid += cell.point[0];
    }

    CHECK(total_moment == doctest::Approx(1.0 / greeter::MU0).epsilon(1e-4));
    CHECK(centroid == doctest::Approx(0.0).scale(1.0));
  }
}


TEST_CASE("Target meshing of a spherical magnet") {

  // radius 0.3, polarization 1.2 T along e_z
  const float parameters[11] = {
    0.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 0.0f, 0.0f,
    0.3f,
    0.0f, 0.0f, 1.2f
  };

  greeter::TargetMeshData mesh = greeter::SphereMagnet::generateTargetMesh(
    parameters, scalarMeshing(1000));

  // A sphere is exactly a point dipole, it is never split.
  REQUIRE(mesh.size() == 1);

  const double volume = 4.0 / 3.0 * M_PI * 0.3 * 0.3 * 0.3;

  CHECK(mesh[0].moment[2] == doctest::Approx(volume * 1.2 / greeter::MU0).epsilon(1e-4));
}


TEST_CASE("Force between two coaxial cuboid magnets") {

  greeter::MagnetCollection collection;

  collection.addMagnet(std::make_unique<greeter::CuboidMagnet>(
    std::vector<float>{0.0f, 0.0f, 0.0f}, std::vector<float>{1.0f, 1.0f, 1.0f},
    std::vector<float>{1.0f, 0.0f, 0.0f, 0.0f}, std::vector<float>{0.0f, 0.0f, 1.0f}));

  collection.addMagnet(std::make_unique<greeter::CuboidMagnet>(
    std::vector<float>{0.0f, 0.0f, 2.0f}, std::vector<float>{1.0f, 1.0f, 1.0f},
    std::vector<float>{1.0f, 0.0f, 0.0f, 0.0f}, std::vector<float>{0.0f, 0.0f, 1.0f}));

  SUBCASE("single cell target") {

    const double expected_force[3] = {0.0, 0.0, -23049.784559481};

    std::vector<greeter::ForceResult> results =
      collection.computeForces(makeConfig(1, scalarMeshing(1), 1e-3f));

    REQUIRE(results.size() == 1);
    CHECK(results[0].target_index == 1);

    checkVector(results[0].force, expected_force);

    // The torque of a symmetric configuration vanishes, compare it against the
    // scale of the moment times the field instead.
    CHECK(std::fabs(results[0].torque[0]) <= 1e-2 * std::fabs(expected_force[2]));
    CHECK(std::fabs(results[0].torque[1]) <= 1e-2 * std::fabs(expected_force[2]));
    CHECK(std::fabs(results[0].torque[2]) <= 1e-2 * std::fabs(expected_force[2]));
  }

  SUBCASE("meshed target converges towards the exact force") {

    const double expected_force_8[3] = {0.0, 0.0, -22542.990236108};
    const double expected_force_1000[3] = {0.0, 0.0, -22510.20126};

    std::vector<greeter::ForceResult> results_8 =
      collection.computeForces(makeConfig(1, scalarMeshing(8), 1e-3f));
    checkVector(results_8[0].force, expected_force_8);

    std::vector<greeter::ForceResult> results_1000 =
      collection.computeForces(makeConfig(1, scalarMeshing(1000), 1e-3f));
    checkVector(results_1000[0].force, expected_force_1000);
  }
}


TEST_CASE("Force and torque on a rotated and offset cuboid magnet") {

  greeter::MagnetCollection collection;

  collection.addMagnet(std::make_unique<greeter::CuboidMagnet>(
    std::vector<float>{0.0f, 0.0f, 0.0f}, std::vector<float>{2.0f, 1.0f, 1.0f},
    std::vector<float>{1.0f, 0.0f, 0.0f, 0.0f}, std::vector<float>{0.2f, -0.3f, 0.9f}));

  // quaternion (w, x, y, z), the same rotation as the scipy (x, y, z, w)
  // quaternion (0.1833050421, 0.3174936514, 0.1833050421, 0.9121383143)
  collection.addMagnet(std::make_unique<greeter::CuboidMagnet>(
    std::vector<float>{1.5f, -0.8f, 2.2f}, std::vector<float>{1.0f, 2.0f, 0.5f},
    std::vector<float>{0.9121383143f, 0.1833050421f, 0.3174936514f, 0.1833050421f},
    std::vector<float>{-0.4f, 0.1f, 0.7f}));

  greeter::MeshingSpec meshing;
  meshing.explicit_split = true;
  meshing.n[0] = 2;
  meshing.n[1] = 3;
  meshing.n[2] = 4;

  const double expected_force[3] = {-5430.206455965, 1774.455216252, -4445.324232085};
  const double expected_torque[3] = {95.703130517, 3545.604843702, 1267.913389693};

  std::vector<greeter::ForceResult> results =
    collection.computeForces(makeConfig(1, meshing, 1e-3f));

  REQUIRE(results.size() == 1);

  checkVector(results[0].force, expected_force);
  checkVector(results[0].torque, expected_torque);
}


TEST_CASE("Force on a spherical target from a cuboid source") {

  greeter::MagnetCollection collection;

  collection.addMagnet(std::make_unique<greeter::CuboidMagnet>(
    std::vector<float>{0.0f, 0.0f, 0.0f}, std::vector<float>{1.0f, 1.0f, 1.0f},
    std::vector<float>{1.0f, 0.0f, 0.0f, 0.0f}, std::vector<float>{0.0f, 0.0f, 1.0f}));

  collection.addMagnet(std::make_unique<greeter::SphereMagnet>(
    std::vector<float>{0.0f, 1.0f, 1.5f}, std::vector<float>{1.0f, 0.0f, 0.0f, 0.0f},
    0.3f, 1.2f));

  const double expected_force[3] = {0.0, -3372.478288353, -973.571878387};
  const double expected_torque[3] = {-2029.418300349, 0.0, 0.0};

  std::vector<greeter::ForceResult> results =
    collection.computeForces(makeConfig(1, scalarMeshing(1), 1e-3f));

  REQUIRE(results.size() == 1);

  checkVector(results[0].force, expected_force);
  checkVector(results[0].torque, expected_torque);
}


TEST_CASE("Force on a cuboid target from a spherical source") {

  greeter::MagnetCollection collection;

  collection.addMagnet(std::make_unique<greeter::SphereMagnet>(
    std::vector<float>{0.0f, 0.0f, 0.0f}, std::vector<float>{1.0f, 0.0f, 0.0f, 0.0f},
    0.5f, 1.0f));

  collection.addMagnet(std::make_unique<greeter::CuboidMagnet>(
    std::vector<float>{0.8f, 0.0f, 1.2f}, std::vector<float>{0.5f, 0.5f, 0.5f},
    std::vector<float>{1.0f, 0.0f, 0.0f, 0.0f}, std::vector<float>{0.0f, 0.3f, 0.9f}));

  const double expected_force[3] = {-3539.221604658, 715.735051689, -1000.227976281};
  const double expected_torque[3] = {444.906733382, 1725.198574131, -572.588683488};

  std::vector<greeter::ForceResult> results =
    collection.computeForces(makeConfig(1, scalarMeshing(27), 1e-3f));

  REQUIRE(results.size() == 1);

  checkVector(results[0].force, expected_force);
  checkVector(results[0].torque, expected_torque);
}


TEST_CASE("Force of several sources adds up on a single target") {

  greeter::MagnetCollection collection;

  collection.addMagnet(std::make_unique<greeter::CuboidMagnet>(
    std::vector<float>{0.0f, 0.0f, 0.0f}, std::vector<float>{1.0f, 1.0f, 1.0f},
    std::vector<float>{1.0f, 0.0f, 0.0f, 0.0f}, std::vector<float>{0.0f, 0.0f, 1.0f}));

  collection.addMagnet(std::make_unique<greeter::CuboidMagnet>(
    std::vector<float>{3.0f, 0.0f, 0.0f}, std::vector<float>{1.0f, 1.0f, 1.0f},
    std::vector<float>{1.0f, 0.0f, 0.0f, 0.0f}, std::vector<float>{0.0f, 0.0f, -1.0f}));

  collection.addMagnet(std::make_unique<greeter::CuboidMagnet>(
    std::vector<float>{1.5f, 0.0f, 1.5f}, std::vector<float>{1.0f, 1.0f, 1.0f},
    std::vector<float>{1.0f, 0.0f, 0.0f, 0.0f}, std::vector<float>{0.0f, 0.0f, 1.0f}));

  // magpylib returns one force per source, this library returns their sum
  const double expected_force[3] = {-20307.891307, 0.0, 0.0};
  const double expected_torque[3] = {0.0, 20633.284511, 0.0};

  std::vector<greeter::ForceResult> results =
    collection.computeForces(makeConfig(2, scalarMeshing(8), 1e-3f));

  REQUIRE(results.size() == 1);
  CHECK(results[0].target_index == 2);

  checkVector(results[0].force, expected_force);
  checkVector(results[0].torque, expected_torque);

  SUBCASE("restricting the sources drops the contribution of the other magnets") {

    greeter::ForceConfig config = makeConfig(2, scalarMeshing(8), 1e-3f);
    config.sources = {{0}};

    const double expected_force_single[3] = {-10153.94565, 0.0, 3616.581816};

    std::vector<greeter::ForceResult> single =
      collection.computeForces(config);

    checkVector(single[0].force, expected_force_single);
  }
}


TEST_CASE("Several targets are simulated at once") {

  greeter::MagnetCollection collection;

  collection.addMagnet(std::make_unique<greeter::CuboidMagnet>(
    std::vector<float>{0.0f, 0.0f, 0.0f}, std::vector<float>{1.0f, 1.0f, 1.0f},
    std::vector<float>{1.0f, 0.0f, 0.0f, 0.0f}, std::vector<float>{0.0f, 0.0f, 1.0f}));

  collection.addMagnet(std::make_unique<greeter::CuboidMagnet>(
    std::vector<float>{0.0f, 0.0f, 2.0f}, std::vector<float>{1.0f, 1.0f, 1.0f},
    std::vector<float>{1.0f, 0.0f, 0.0f, 0.0f}, std::vector<float>{0.0f, 0.0f, 1.0f}));

  greeter::ForceConfig config;
  config.targets = {0, 1};
  config.meshing = {scalarMeshing(8), scalarMeshing(8)};
  config.eps = 1e-3f;

  std::vector<greeter::ForceResult> results = collection.computeForces(config);

  REQUIRE(results.size() == 2);
  CHECK(results[0].target_index == 0);
  CHECK(results[1].target_index == 1);

  // Newton's third law
  for (int i = 0; i < 3; i++) {
    CHECK(std::fabs(results[0].force[i] + results[1].force[i])
          <= 2e-3 * std::fabs(results[1].force[2]));
  }
}
