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

  std::vector<std::vector<float>> observation_points = {
    {-2.3, 0., 1.},
    {-0.76666667, 0., 1.},
    { 0.76666667, 0., 1.},
    { 2.3, 0., 1.}
  };

  std::vector<float> cuboid_mag_result = cubic_magnet.computeMagneticField(
    observation_points[0][0],
    observation_points[0][1],
    observation_points[0][2] 
  );

  CHECK(cuboid_mag_result[0] == doctest::Approx(-0.011591485));
  CHECK(cuboid_mag_result[1] == doctest::Approx(0.0));
  CHECK(cuboid_mag_result[2] == doctest::Approx(-0.0038766933));
}


TEST_CASE("Cuboid magnet magnetic field off the symmetry planes") {

  // The observation points above lie in the y = 0 plane, where the y component
  // of the field of a z-magnetized cuboid vanishes. The points below probe the
  // components that are hidden by that symmetry. Reference values from magpylib.

  greeter::CuboidMagnet cubic_magnet(
    {0.0, 0.0, 0.0}, {1.0, 1.0, 1.0},
    {1.0, 0.0, 0.0, 0.0}, {0.0, 0.0, 1.0}
  );

  std::vector<float> result = cubic_magnet.computeMagneticField(0.7, 0.4, 1.2);

  CHECK(result[0] == doctest::Approx(0.031840754));
  CHECK(result[1] == doctest::Approx(0.017724238));
  CHECK(result[2] == doctest::Approx(0.029423465));

  greeter::CuboidMagnet y_magnetized_magnet(
    {0.0, 0.0, 0.0}, {1.0, 1.0, 1.0},
    {1.0, 0.0, 0.0, 0.0}, {0.0, 1.0, 0.0}
  );

  std::vector<float> y_result = y_magnetized_magnet.computeMagneticField(0.7, 0.4, 1.2);

  CHECK(y_result[0] == doctest::Approx(0.0099710666));
  CHECK(y_result[1] == doctest::Approx(-0.0205961516));
  CHECK(y_result[2] == doctest::Approx(0.0177242384));
}
