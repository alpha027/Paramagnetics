#include <doctest/doctest.h>
#include <greeter/greeter.h>
#include <greeter/version.h>
#include <greeter/MagnetCollection.h>


TEST_CASE("Circular Halbach Array made of cuboid magnets") {

  Kokkos::initialize();
  {

  float halbach_array_radius = 0.3;
  size_t halbach_array_num_magnets = 4;
  std::vector<float> halbach_magnet_dimensions = {0.12, 0.12, 0.12};
  std::vector<float> halbach_magnetization = {0.0, 5.0, 0.0};

  greeter::MagnetCollection circular_halbach_array_test(greeter::MagnetCollection::
                  generateCircularHalbachArray( halbach_array_radius,
                    halbach_magnet_dimensions, halbach_array_num_magnets,
                    halbach_magnetization ));

  std::vector<std::vector<float>> fov_test = {
    {-0.5, -0.5, 0.},
    {-0.16666667, -0.5, 0.},
    { 0.16666667, -0.5, 0.},
    { 0.5, -0.5, 0.},
    {-0.5, -0.16666667, 0.},
    {-0.16666667, -0.16666667, 0.},
    { 0.16666667, -0.16666667, 0.},
    { 0.5, -0.16666667, 0.},
    {-0.5, 0.16666667, 0.},
    {-0.16666667, 0.16666667, 0.},
    { 0.16666667, 0.16666667, 0.},
    { 0.5, 0.16666667, 0.},
    {-0.5, 0.5, 0.},
    {-0.16666667, 0.5, 0.},
    { 0.16666667, 0.5, 0.},
    { 0.5, 0.5, 0}
  };

   std::vector<std::vector<float>> results_test = circular_halbach_array_test.simulate(fov_test);

   
   CHECK(results_test[5][0] == doctest::Approx(0.0));
   CHECK(results_test[5][1] == doctest::Approx(0.03494169));
   CHECK(results_test[5][2] == doctest::Approx(0.0));

   CHECK(results_test[4][0] == doctest::Approx(0.05888743));
   CHECK(results_test[4][1] == doctest::Approx(0.0107326));
   CHECK(results_test[4][2] == doctest::Approx(0.0));
   }
   Kokkos::finalize();
}