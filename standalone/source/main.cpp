#include <greeter/greeter.h>
#include <greeter/CubicMagnet.h>
#include <greeter/SphericalMagnet.h>
#include <greeter/version.h>
#include <greeter/KokkosDefines.h>

#include <greeter/MagneticFieldSimulator_i.h>
#include <greeter/MagnetCollection.h>

#include <cxxopts.hpp>
#include <iostream>
#include <string>
#include <unordered_map>

#include <greeter/io/CubicMagnetIO.h>

#include <nlohmann/json.hpp>
#include <fstream>

//#include <Kokkos_Core.hpp>
//using json = nlohmann::json;

auto main(int argc, char** argv) -> int {
  Kokkos::initialize(argc, argv);

  std::ifstream f("/home/anas/Documents/Master eth/my python project/Magnetics/magnets.json");
  nlohmann::json data = nlohmann::json::parse(f);

  greeter::MagnetCollection junk_magnet_collection;

  // std::cout << data.dump(4) << std::endl;
  std::cout << "x position of first magnet: " << data["magnets"][0]["cuboid"]["parameters"]["position"] << std::endl;
  std::cout << "Number of magnets: " << data["magnets"].size() << std::endl;
  std::cout << "number of keys: " << data.size() << std::endl;

  std::vector<float> position = data["magnets"][0]["cuboid"]["parameters"]["position"].get<std::vector<float>>();
  std::cout << "The position from JSON is :" << position[0] << ", " << position[1] << ", " << position[2] << std::endl;
  nlohmann::json sub_data = data["magnets"][0]["cuboid"];

  greeter::CubicMagnetIO cubic_magnet_reader;
  std::cout << "About to read using IO class" << std::endl;
  std::unique_ptr<greeter::Magnet> cuboid_magnet_read = cubic_magnet_reader.createMagnet(sub_data);
  std::cout << "There is no problem here:" << std::endl;
  std::vector<std::string> keys;

  std::ifstream g("/home/anas/Documents/Master eth/my python project/Magnetics/magnets.json");

  bool is_valid_file = junk_magnet_collection.validJsonFile(g);

  for (auto it = data["magnets"][0]["cuboid"]["parameters"].begin(); it != data["magnets"][0]["cuboid"]["parameters"].end(); ++it) {
      keys.push_back(it.key());
      std::cout << "Ze Key : " << it.key() << " : " << it.value() << std::endl;
  }

  std::vector<std::string> magnet_type_names = {"cuboid", "sphere"};

   for (auto it = data["magnets"].begin(); it != data["magnets"].end(); ++it) {
      bool does_not_exist = true;
      for (auto& magnet_type : magnet_type_names) {
        if (it->contains(magnet_type)) {
          std::cout << "This is a CUBOID YAAAY " << std::endl;
        }
      }
    }

  std::cout << "keys 0: " << keys[0] << std::endl;
  std::cout << "keys 1: " << keys[1] << std::endl;
  std::cout << "keys 2: " << keys[2] << std::endl; 


  std::cout << "x position of first magnet: " << position[0] << std::endl;
  std::cout << "y position of first magnet: " << position[1] << std::endl;
  std::cout << "z position of first magnet: " << position[2] << std::endl;

  // typedef Kokkos::Cuda ExecSpace;
  // typedef Kokkos::OpenMP ExecSpace;
  // typedef Kokkos::OpenMP MemSpace;
  // typedef Kokkos::LayoutLeft Layout;

  const std::unordered_map<std::string, greeter::LanguageCode> languages{
      {"en", greeter::LanguageCode::EN},
      {"de", greeter::LanguageCode::DE},
      {"es", greeter::LanguageCode::ES},
      {"fr", greeter::LanguageCode::FR},
  };

  cxxopts::Options options(*argv, "A program to welcome the world!");

  std::string language;
  std::string name;

  // clang-format off
  options.add_options()
    ("h,help", "Show help")
    ("v,version", "Print the current version number")
    ("n,name", "Name to greet", cxxopts::value(name)->default_value("World"))
    ("l,lang", "Language code to use", cxxopts::value(language)->default_value("en"))
  ;

  // clang-format on
  auto result = options.parse(argc, argv);

  if (result["help"].as<bool>()) {
    std::cout << options.help() << std::endl;
    return 0;
  }

  if (result["version"].as<bool>()) {
    std::cout << "Greeter, version " << GREETER_VERSION << std::endl;
    return 0;
  }

  auto langIt = languages.find(language);
  if (langIt == languages.end()) {
    std::cerr << "unknown language code: " << language << std::endl;
    return 1;
  }

  greeter::Greeter greeter(name);
  std::cout << greeter.greet(langIt->second) << std::endl;

  greeter::CuboidMagnet magnetic_cube;

  double x = 0.0; //magnetic_cube.computeMagneticField(0, 0, 0);
  std::cout << "Magnetic field at (0, 0, 0): " << x << std::endl;

  std::cout << "Kokkos related stuff"  << std::endl;
  int atomCount = 10;
  Float3VectorView forces( "forces", atomCount );
  Float3VectorView positions("positions", atomCount);
  Float4VectorView orientations("orientations", atomCount);
  Float3VectorView magnetizations("magnetizations", atomCount);
  FloatVectorView radii("radii", 3*atomCount);
  Float3VectorView observation_points("observation_points", atomCount);
  UInt32VectorView magnet_types("magnet_types", atomCount);

  std::cout << "About to initialize Magnet collection simulator"  << std::endl;

  greeter::MagneticFieldSimulator the_simulator(observation_points, orientations, magnetizations, radii, magnet_types, positions);

  // simulator.simulate();

  // simulator.printValue(0);

  // simulator.printValue(254704);

  std::cout << "Finished simulating"  << std::endl;

  std::cout << "fill positions " << std::endl;

  std::vector<std::vector<float>> _positions ={ (size_t) atomCount, {1.0, 2.0, 3.0} };

  // simulator.fillMagnetPositions(_positions);

  std::cout << "finished positions ! " << std::endl;

  the_simulator.printPosition(1);

  greeter::CuboidMagnet cuboid_magnet;

  cuboid_magnet.setPosition(0.0, 0.0, 0.0);
  cuboid_magnet.setMagnetization(0.0, 1.0, 0.0);

  std::cout << "about to display cuboid magnet" << std::endl;
  cuboid_magnet.display();

  greeter::MagnetCollection magnet_collection;

  magnet_collection.addMagnet(std::make_unique<greeter::CuboidMagnet>(cuboid_magnet));

  std::cout << "magnet collection count " << magnet_collection.get_num_magnets() << std::endl;

  std::vector<std::vector<float>> fov = { (size_t) 1e6, {0.0, 0.0, 0.0} };

  float x_min, x_max, y_min, y_max, z_min, z_max;

  x_min = 2.0;
  x_max = 4.0;

  y_min = 2.0;
  y_max = 4.0;

  z_min = 2.0;
  z_max = 4.0;

  u_int32_t n_x, n_y, n_z = 1000;
  size_t index = 0;

  for (u_int32_t i = 0; i < n_x; i++) {
    float x = x_min + (x_max - x_min) * i / (n_x - 1);
    for (u_int32_t j = 0; j < n_y; j++) {
      float y = y_min + (y_max - y_min) * j / (n_y - 1);
      for (u_int32_t k = 0; k < n_z; k++) {
        float z = z_min + (z_max - z_min) * k / (n_z - 1);
        fov[index][0] = x;
        fov[index][1] = y;
        fov[index][2] = z;
        index++;
      }
    }
  }

  std::cout << "Filled FOV" << std::endl;

  magnet_collection.display(0);

  std::cout << "About to create simulator" << std::endl;

  //std::vector<std::vector<float>> results = magnet_collection.simulate(fov);

  std::cout << "Finished simulating" << std::endl;

  std::cout << "===============================================" << std::endl;
  std::cout << "FINALY SOME REAL TESTS !" << std::endl;
  std::cout << "===============================================" << std::endl;


  float halbach_array_radius = 0.3;
  size_t halbach_array_num_magnets = 4;
  std::vector<float> halbach_magnet_dimensions = {0.12, 0.12, 0.12};
  std::vector<float> halbach_magnetization = {0.0, 1.0, 0.0};

  greeter::MagnetCollection circular_halbach_array_test(greeter::MagnetCollection::
                  generateCircularHalbachArray( halbach_array_radius,
                    halbach_magnet_dimensions, halbach_array_num_magnets,
                    halbach_magnetization ));

  std::cout << "Circular Halbach Array created" << std::endl;

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

  circular_halbach_array_test.display();

  std::vector<std::vector<float>> results_test = circular_halbach_array_test.simulate(fov_test);

  std::cout << "Finished simulating" << std::endl;

  std::cout << "===============================================" << std::endl;

  for(int i = 0; i < fov_test.size(); i++) {
    std::cout << results_test[i][0] << ", " << results_test[i][1] << ", " << results_test[i][2] << std::endl;
  }

  float quaternion_rotation[4];

  greeter::Quaternion::set_rotation_from_axis_angle(
      "z", 2.0 * M_PI/4.0, quaternion_rotation
  );

  std::cout << "Quaternion Rotation: " << quaternion_rotation[0] << ", " << quaternion_rotation[1] << ", " << quaternion_rotation[2] << ", " << quaternion_rotation[3] << std::endl;

  std::cout << "log(10) : " << log(10) << std::endl;

  // SPHERE TESTS$
  greeter::SphereMagnet spherical_magnet(0.8f, 1.0f);

  std::vector<std::vector<float>> sphere_position = {
  {-2.3, 0., 1.},
  {-0.76666667, 0., 1.},
  { 0.76666667, 0., 1.},
  { 2.3, 0., 1.}
  };

  std::vector<float> sphere_mag_result;
  sphere_mag_result = spherical_magnet.computeMagneticField(-2.3, 0.0, 1.0);

  std::cout << "Sphere Magnetic Field: " << sphere_mag_result[0] << ", " << sphere_mag_result[1] << ", " << sphere_mag_result[2] << std::endl;

  sphere_mag_result = spherical_magnet.computeMagneticField(-0.76666667, 0., 1.);

  std::cout << "Sphere Magnetic Field: " << sphere_mag_result[0] << ", " << sphere_mag_result[1] << ", " << sphere_mag_result[2] << std::endl;
  return 0;
  Kokkos::finalize();
}
