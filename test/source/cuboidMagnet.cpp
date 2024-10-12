#include <doctest/doctest.h>
#include <greeter/greeter.h>
#include <greeter/version.h>
//#include <greeter/MagnetCollection.h>
//#include <greeter/MagneticFieldSimulator.h>
#include <greeter/CubicMagnet.h>
//#include <greeter/SphericalMagnet.h>


TEST_CASE("Axis aligned Cuboid magnet magnetic field") {

  std::vector<float> position = {0.0, 0.0, 0.0};
  std::vector<float> dimensions = {1.92, 0.92, 0.92};
  std::vector<float> orientation = {1.0, 0.0, 0.0, 0.0};
  std::vector<float> magnetization = {0.0, 0.0, 1.0};

  greeter::CuboidMagnet cubic_magnet(
    position, dimensions,
    orientation, magnetization
  );

  std::vector<std::vector<float>> sphere_position = {
    {-2.3, 0., 1.},
    {-0.76666667, 0., 1.},
    { 0.76666667, 0., 1.},
    { 2.3, 0., 1.}
  };

  std::vector<float> cuboid_mag_result = cubic_magnet.computeMagneticField(
    sphere_position[0][0],
    sphere_position[0][1],
    sphere_position[0][2] 
  );

  CHECK(cuboid_mag_result[0] == doctest::Approx(-0.011591485));
  CHECK(cuboid_mag_result[1] == doctest::Approx(0.0));
  CHECK(cuboid_mag_result[2] == doctest::Approx(-0.0038766933));
}
