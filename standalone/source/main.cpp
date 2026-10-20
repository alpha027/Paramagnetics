#include <greeter/greeter.h>
#include <greeter/version.h>
#include <greeter/KokkosDefines.h>

#include <cxxopts.hpp>
#include <iostream>
#include <string>

#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <stdexcept>

#include <greeter/service/SimulationService.h>
#include <greeter/view/SnapshotIO.h>


namespace {

/* Says how far along a run is, on one line that rewrites itself. */
class ConsoleProgress: public greeter::service::ProgressSink {

  public:

    bool onProgress(const size_t& done, const size_t& total) override {

      if (total == 0) {
        return true;
      }

      const int percent = (int) (100 * done / total);

      // Only when it has moved, otherwise a large run spends its time
      // printing.
      if (percent != last_percent) {
        std::cout << "\r  " << percent << "% (" << done << " of " << total
                  << ")" << std::flush;
        last_percent = percent;
      }

      if (done == total) {
        std::cout << std::endl;
      }

      return true;
    }

  private:

    int last_percent = -1;
};

}  // namespace


auto main(int argc, char** argv) -> int {

  cxxopts::Options options("Greeter", "ParaMagneticS, parallel magnetic field and force simulation");

  options.add_options()
    ("i,input", "Input JSON file (required)",
     cxxopts::value<std::string>())
    ("s,snapshot", "Also write a snapshot of the run, for the viewer to open. "
                   "A .json extension writes the readable form, anything else "
                   "the compact one",
     cxxopts::value<std::string>())
    ("o,output-dir", "Directory for CSV results, relative to the working directory",
     cxxopts::value<std::string>()->default_value("output"))
    ("overwrite", "Allow replacing existing result files")
    ("legacy-csv", "Write the old headerless CSV layout without coordinates")
    ("q,quiet", "Do not announce every step of the simulation")
    ("h,help", "Print this");

  // Let Kokkos consume its own options while still rejecting misspelled CLI options.
  std::vector<char*> arguments;
  for (int i = 0; i < argc; ++i) {
    if (std::string(argv[i]).rfind("--kokkos-", 0) != 0) arguments.push_back(argv[i]);
  }
  cxxopts::ParseResult parsed;

  try {
    parsed = options.parse((int) arguments.size(), arguments.data());
  } catch (const std::exception& error) {
    std::cerr << "Error: " << error.what() << std::endl;
    return 1;
  }

  if (parsed.count("help")) {
    std::cout << options.help() << std::endl;
    return 0;
  }

  if (!parsed.count("input")) {
    std::cerr << "Error: --input is required\n" << options.help() << std::endl;
    return 1;
  }

  Kokkos::initialize(argc, argv);

  int status = 0;

  // In its own scope: everything holding a Kokkos view has to be gone before
  // Kokkos::finalize, and a service that outlives it takes the process down.
  {
    try {

      const std::string input_path = parsed["input"].as<std::string>();
      const bool quiet = parsed.count("quiet") > 0;
      const bool legacy = parsed.count("legacy-csv") > 0;
      const std::filesystem::path output_dir = parsed["output-dir"].as<std::string>();

      greeter::service::SimulationService service;

      service.loadFile(input_path);

      // Check every destination before running or replacing any existing result.
      std::vector<std::filesystem::path> destinations;
      if (service.hasFieldSection()) destinations.push_back(output_dir / "simulation_results_1.csv");
      if (service.hasForceSection()) destinations.push_back(output_dir / "force_results.csv");
      if (parsed.count("snapshot")) destinations.push_back(parsed["snapshot"].as<std::string>());
      std::vector<std::filesystem::path> resolved;
      for (const auto& path : destinations) {
        const auto canonical = std::filesystem::weakly_canonical(path);
        if (canonical == std::filesystem::weakly_canonical(input_path)) {
          throw std::invalid_argument("Output would replace the input: " + path.string());
        }
        if (std::find(resolved.begin(), resolved.end(), canonical) != resolved.end()) {
          throw std::invalid_argument("Two outputs share a destination: " + path.string());
        }
        resolved.push_back(canonical);
        if (std::filesystem::exists(path) && !parsed.count("overwrite")) {
          throw std::invalid_argument("Output already exists: " + path.string() +
                                      "; choose another directory or use --overwrite");
        }
        if (std::filesystem::is_directory(path)) {
          throw std::invalid_argument("Output is a directory: " + path.string());
        }
      }
      for (const auto& path : destinations) {
        if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
      }
      // Throw on open, write, flush, and close failures, including full disks.
      auto openCSV = [](const std::filesystem::path& path) {
        std::ofstream file;
        file.exceptions(std::ios::failbit | std::ios::badbit);
        file.open(path);
        file.precision(9);
        return file;
      };

      greeter::service::RunOptions run_options;
      run_options.verbose = false;

      ConsoleProgress progress;

      greeter::view::Snapshot snapshot;

      snapshot.scene = service.getScene();

      greeter::service::FieldRequest request;

      if (service.getFieldRequest(request)) {

        if (!parsed.count("quiet")) {
          std::cout << "Simulating the field at " << request.getSampleCount()
                    << " points" << std::endl;
        }

        snapshot.field = service.simulateField(request, quiet ? nullptr : &progress, run_options);

        auto file = openCSV(output_dir / "simulation_results_1.csv");
        if (!legacy) {
          const auto name = greeter::view::getName(snapshot.field.kind);
          const auto unit = greeter::view::getUnit(snapshot.field.kind);
          file << "x[m],y[m],z[m]," << name << "x[" << unit << "],"
               << name << "y[" << unit << "]," << name << "z[" << unit << "]\n";
        }
        for (size_t i = 0; i < snapshot.field.size(); i++) {
          float point[3], b[3];
          snapshot.field.getPoint(i, point);
          snapshot.field.getField(i, b);
          if (!legacy) file << point[0] << ',' << point[1] << ',' << point[2] << ',';
          file << b[0] << ',' << b[1] << ',' << b[2] << '\n';
        }
        file.close();
      }

      if (service.hasForceSection()) {

        if (!parsed.count("quiet")) {
          std::cout << "Computing forces" << std::endl;
        }

        snapshot.forces = service.simulateForces(nullptr, run_options);

        auto file = openCSV(output_dir / "force_results.csv");
        if (!legacy) file << "magnet_id,Fx[N],Fy[N],Fz[N],Tx[N*m],Ty[N*m],Tz[N*m]\n";
        for (const auto& entry : snapshot.forces.entries) {
          file << entry.id;
          for (const float value : entry.force) file << ',' << value;
          for (const float value : entry.torque) file << ',' << value;
          file << '\n';
        }
        file.close();
      }

      if (parsed.count("snapshot")) {

        const std::string snapshot_path = parsed["snapshot"].as<std::string>();

        greeter::view::SnapshotIO::write(snapshot, snapshot_path);

        if (!quiet) std::cout << "Snapshot written to " << snapshot_path << std::endl;
      }

    } catch (const std::exception& error) {
      std::cerr << "Error: " << error.what() << std::endl;
      status = 1;
    }
  }

  Kokkos::finalize();

  return status;
}
