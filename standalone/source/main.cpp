#include <greeter/greeter.h>
#include <greeter/CubicMagnet.h>
#include <greeter/version.h>
#include <greeter/KokkosDefines.h>

#include <greeter/MagneticFieldSimulator_i.h>
#include <greeter/MagnetCollection.h>

#include <cxxopts.hpp>
#include <iostream>
#include <string>
#include <unordered_map>

//#include <Kokkos_Core.hpp>

auto main(int argc, char** argv) -> int {
  Kokkos::initialize(argc, argv);

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

  double x = magnetic_cube.computeMagneticField(0, 0, 0);
  std::cout << "Magnetic field at (0, 0, 0): " << x << std::endl;

  std::cout << "Kokkos related stuff"  << std::endl;
  int atomCount = 100000000;
  Float3VectorView forces( "forces", atomCount );
  Float3VectorView positions("positions", atomCount);
  Float4VectorView orientations("orientations", atomCount);
  Float3VectorView magnetizations("magnetizations", atomCount);
  Float3VectorView radii("radii", atomCount);
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

  the_simulator.printPosition(15000);

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

  std::vector<std::vector<float>> results = magnet_collection.simulate(fov);

  std::cout << "Finished simulating" << std::endl;


  return 0;
  Kokkos::finalize();
}
