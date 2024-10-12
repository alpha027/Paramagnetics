#include <doctest/doctest.h>
#include <greeter/greeter.h>
#include <greeter/version.h>
//#include <greeter/MagnetCollection.h>
//#include <greeter/MagneticFieldSimulator.h>
//#include <greeter/CuboidMagnet.h>
#include <greeter/SphericalMagnet.h>


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
