#include <doctest/doctest.h>
#include <greeter/greeter.h>
#include <greeter/version.h>
#include <greeter/MagnetCollection.h>
//#include <greeter/MagneticFieldSimulator.h>
//#include <greeter/CuboidMagnet.h>
#include <greeter/SphericalMagnet.h>
#include <greeter/io/ForceIO.h>
#include <greeter/io/MagnetIO.h>
#include <cmath>


TEST_CASE("Spherical magnet magnetic field") {

  greeter::SphereMagnet spherical_magnet(0.8f, 1.0f);

  std::vector<std::vector<float>> observation_points = {
    {-2.3, 0., 1.},
    {-0.76666667, 0., 1.},
    { 0.76666667, 0., 1.},
    { 2.3, 0., 1.}
  };

  std::vector<float> sphere_mag_result;
  sphere_mag_result = spherical_magnet.computeMagneticField(
    observation_points[0][0],
    observation_points[0][1],
    observation_points[0][2]
  );

  CHECK(sphere_mag_result[0] == doctest::Approx(-0.0118678));
  CHECK(sphere_mag_result[1] == doctest::Approx(0.0));
  CHECK(sphere_mag_result[2] == doctest::Approx(-0.005658));

  sphere_mag_result = spherical_magnet.computeMagneticField(
    observation_points[1][0],
    observation_points[1][1],
    observation_points[1][2]
  );

  CHECK(sphere_mag_result[0] == doctest::Approx(-0.1235667));
  CHECK(sphere_mag_result[1] == doctest::Approx(0.0));
  CHECK(sphere_mag_result[2] == doctest::Approx(0.0758713));
}


namespace {

  // A sphere of radius 0.5 in the origin and a unit cube two metres above it,
  // both polarized along z with 1 T.
  const char* SPHERE_AND_CUBE = R"({
    "magnets": [
      { "id": 1, "type": "sphere", "parameters": {
          "dimensions": [0.5], "magnetization": [0, 0, 1],
          "position": [0, 0, 0], "orientation": [1, 0, 0, 0] } },
      { "id": 2, "type": "cuboid", "parameters": {
          "dimensions": [1, 1, 1], "magnetization": [0, 0, 1],
          "position": [0, 0, 2], "orientation": [1, 0, 0, 0] } }
    ],
    "force": { "targets": [2], "meshing": 1 }
  })";

  nlohmann::json sphereWith(const nlohmann::json& parameters) {
    nlohmann::json data = nlohmann::json::parse(SPHERE_AND_CUBE);
    data["magnets"][0]["parameters"] = parameters;
    return data;
  }

}  // namespace


TEST_CASE("A sphere read from a JSON file is a sphere") {

  nlohmann::json data = nlohmann::json::parse(SPHERE_AND_CUBE);

  greeter::MagnetCollection collection = greeter::MagnetIO::read(data);

  REQUIRE(collection.get_num_magnets() == 2);

  // The reader used to build a cuboid out of the sphere parameters, which left
  // the collection with a magnet whose dimensions were read out of bounds.
  std::vector<float> parameters = collection.getMagnetParameters(0);
  CHECK(parameters.size() == greeter::SphereMagnet::numberOfParameters());
  CHECK(parameters[7] == doctest::Approx(0.5));

  SUBCASE("its field is the field of a directly constructed sphere") {

    greeter::SphereMagnet sphere(0.5f, 1.0f);

    std::vector<float> expected = sphere.computeMagneticField(0.3, -0.7, 1.1);

    float b_x, b_y, b_z;
    const float observation_point[3] = {0.3f, -0.7f, 1.1f};
    greeter::SphereMagnet::computeMagneticFieldForSphere(
      parameters.data(), observation_point, b_x, b_y, b_z);

    CHECK(b_x == doctest::Approx(expected[0]));
    CHECK(b_y == doctest::Approx(expected[1]));
    CHECK(b_z == doctest::Approx(expected[2]));
  }

  SUBCASE("the force it exerts matches magpylib") {

    greeter::ForceConfig config = greeter::ForceIO::read(data);

    std::vector<greeter::ForceResult> results = collection.computeForces(config);

    REQUIRE(results.size() == 1);

    // magpylib getFT of a Sphere(diameter=1, polarization=(0,0,1)) on a
    // Cuboid(dimension=(1,1,1), polarization=(0,0,1)) at (0, 0, 2), meshing 1
    // The tolerance is the accuracy of a single precision finite difference,
    // not of the field itself.
    CHECK(results[0].force[2] == doctest::Approx(-12433.98).epsilon(1e-3));
    CHECK(results[0].force[0] == doctest::Approx(0.0).epsilon(1e-3).scale(12433.98));
    CHECK(results[0].force[1] == doctest::Approx(0.0).epsilon(1e-3).scale(12433.98));
  }
}


TEST_CASE("A sphere accepts a scalar radius and magnetization") {

  nlohmann::json data = sphereWith({
    {"dimensions", 0.5}, {"magnetization", 1.0},
    {"position", {0, 0, 0}}, {"orientation", {1, 0, 0, 0}}});

  greeter::MagnetCollection collection = greeter::MagnetIO::read(data);

  std::vector<float> parameters = collection.getMagnetParameters(0);

  CHECK(parameters.size() == greeter::SphereMagnet::numberOfParameters());
  CHECK(parameters[7] == doctest::Approx(0.5));
  CHECK(parameters[10] == doctest::Approx(1.0));
}


TEST_CASE("Malformed sphere parameters are rejected") {

  SUBCASE("a cuboid dimension triple is not a radius") {

    CHECK_THROWS_AS(
      greeter::MagnetIO::read(sphereWith({
        {"dimensions", {1, 1, 1}}, {"magnetization", {0, 0, 1}},
        {"position", {0, 0, 0}}, {"orientation", {1, 0, 0, 0}}})),
      std::invalid_argument);
  }

  SUBCASE("a radius must be strictly positive") {

    CHECK_THROWS_AS(
      greeter::MagnetIO::read(sphereWith({
        {"dimensions", {0}}, {"magnetization", {0, 0, 1}},
        {"position", {0, 0, 0}}, {"orientation", {1, 0, 0, 0}}})),
      std::invalid_argument);
  }

  SUBCASE("a sphere is magnetized along its local z axis only") {

    // The direction has to come from the orientation, silently dropping the
    // transverse components would give a wrong field.
    CHECK_THROWS_AS(
      greeter::MagnetIO::read(sphereWith({
        {"dimensions", {0.5}}, {"magnetization", {1, 0, 0}},
        {"position", {0, 0, 0}}, {"orientation", {1, 0, 0, 0}}})),
      std::invalid_argument);
  }

  SUBCASE("an orientation is a quaternion") {

    CHECK_THROWS_AS(
      greeter::MagnetIO::read(sphereWith({
        {"dimensions", {0.5}}, {"magnetization", {0, 0, 1}},
        {"position", {0, 0, 0}}, {"orientation", {0, 0, 0}}})),
      std::invalid_argument);
  }
}


TEST_CASE("Inside a sphere the field is two thirds of the polarization") {

  // A homogeneously polarized sphere is the one shape whose interior field is
  // uniform, and it is exactly 2/3 of the polarization. The dipole expression
  // that gives the field outside runs away as the radius goes to zero, so the
  // two cases have to be told apart; using the outside one everywhere put
  // 83 T at 2 mm from the middle of a 1 T sphere.
  //
  // The numbers below are magpylib 5, Sphere(diameter=0.02,
  // polarization=(0, 0, 1)), which agrees with this to single precision.
  greeter::SphereMagnet sphere(0.01f, 1.0f);

  const float expected_inside = 2.0f / 3.0f;

  // Along the axis, across it and off to one side: uniform means uniform.
  const std::vector<std::vector<float>> inside = {
    {0.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 0.002f},
    {0.0f, 0.0f, -0.006f},
    {0.004f, 0.0f, 0.0f},
    {0.003f, -0.004f, 0.005f}
  };

  for (const auto& point : inside) {

    const std::vector<float> field =
      sphere.computeMagneticField(point[0], point[1], point[2]);

    CHECK(field[0] == doctest::Approx(0.0f).epsilon(1e-5));
    CHECK(field[1] == doctest::Approx(0.0f).epsilon(1e-5));
    CHECK(field[2] == doctest::Approx(expected_inside));
  }

  // The surface belongs to the inside, and the two expressions agree there:
  // on the axis the outside field at r = a is also 2/3 of the polarization.
  const std::vector<float> surface = sphere.computeMagneticField(0.0f, 0.0f, 0.01f);
  CHECK(surface[2] == doctest::Approx(expected_inside));

  // Outside is unchanged, and still the dipole field.
  const std::vector<float> outside = sphere.computeMagneticField(0.0f, 0.0f, 0.02f);
  CHECK(outside[2] == doctest::Approx(0.0833333f));

  const std::vector<float> across = sphere.computeMagneticField(0.02f, 0.0f, 0.0f);
  CHECK(across[2] == doctest::Approx(-0.0416667f));
}


TEST_CASE("A sphere polarized off axis is uniform inside along its own axis") {

  // The class holds a polarization along its local z and is turned to point
  // it elsewhere, so the interior field has to follow the turn rather than
  // staying along z.
  const float half = std::sqrt(0.5f);

  // A quarter turn about y takes the local z axis onto the global x axis.
  greeter::SphereMagnet sphere(
    {0.0f, 0.0f, 0.0f}, {half, 0.0f, half, 0.0f}, 0.01f, 1.0f);

  const std::vector<float> field = sphere.computeMagneticField(0.0f, 0.002f, 0.0f);

  CHECK(field[0] == doctest::Approx(2.0f / 3.0f));
  CHECK(field[1] == doctest::Approx(0.0f).epsilon(1e-5));
  CHECK(field[2] == doctest::Approx(0.0f).epsilon(1e-5));
}
