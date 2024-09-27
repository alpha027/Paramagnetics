#include <greeter/greeter.h>
#include <greeter/version.h>
#include <greeter/KokkosDefines.h>

#include <greeter/MagnetCollection_i.h>

#include <cxxopts.hpp>
#include <iostream>
#include <string>
#include <unordered_map>

//#include <Kokkos_Core.hpp>

auto main(int argc, char** argv) -> int {
  Kokkos::initialize(argc, argv);

  //typedef Kokkos::Cuda ExecSpace;
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
  Float3VectorView orientations("orientations", atomCount);
  Float3VectorView magnetizations("magnetizations", atomCount);
  Float3VectorView radii("radii", atomCount);
  Float3VectorView observation_points("observation_points", atomCount);

  std::cout << "About to initialize Magnet collection simulator"  << std::endl;

  greeter::MagnetCollectionSimulator simulator(observation_points, orientations, magnetizations, radii, positions);

  simulator.simulate();

  simulator.printValue(0);

  simulator.printValue(254704);

  std::cout << "Finished simulating"  << std::endl;

  std::cout << "fill positions " << std::endl;

  std::vector<std::vector<float>> _positions ={ (size_t) atomCount, {1.0, 2.0, 3.0} };

  simulator.fillMagnetPositions(_positions);

  std::cout << "finished positions ! " << std::endl;
 
  return 0;
  Kokkos::finalize();
}
