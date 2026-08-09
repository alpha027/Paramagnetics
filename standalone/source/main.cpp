#include <greeter/greeter.h>
#include <greeter/version.h>
#include <greeter/KokkosDefines.h>

#include <cxxopts.hpp>
#include <iostream>
#include <string>

#include <nlohmann/json.hpp>
#include <fstream>

#include <greeter/service/SimulationService.h>
#include <greeter/view/SnapshotIO.h>


std::vector<std::filesystem::path> getJSONFiles();
std::string getDataPath();
void writeMatrixToCSV(const std::vector<std::vector<float>>& matrix, const std::string& filename);
std::string getRepoRoot();


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
    ("i,input", "Input file, the first JSON file of data/ by default",
     cxxopts::value<std::string>())
    ("s,snapshot", "Also write a snapshot of the run, for the viewer to open. "
                   "A .json extension writes the readable form, anything else "
                   "the compact one",
     cxxopts::value<std::string>())
    ("q,quiet", "Do not announce every step of the simulation")
    ("h,help", "Print this");

  cxxopts::ParseResult parsed;

  try {
    parsed = options.parse(argc, argv);
  } catch (const std::exception& error) {
    std::cerr << "Error: " << error.what() << std::endl;
    return 1;
  }

  if (parsed.count("help")) {
    std::cout << options.help() << std::endl;
    return 0;
  }

  Kokkos::initialize(argc, argv);

  int status = 0;

  // In its own scope: everything holding a Kokkos view has to be gone before
  // Kokkos::finalize, and a service that outlives it takes the process down.
  {
    try {

      std::string input_path;

      if (parsed.count("input")) {
        input_path = parsed["input"].as<std::string>();
      } else {
        const std::vector<std::filesystem::path> files = getJSONFiles();
        if (files.empty()) {
          throw std::invalid_argument("No JSON file in " + getDataPath());
        }
        input_path = files[0];
      }

      greeter::service::SimulationService service;

      service.loadFile(input_path);

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

        snapshot.field = service.simulateField(request, &progress, run_options);

        // The CSV keeps the shape it has always had, three numbers a sample.
        // Where each of them was measured is in the snapshot.
        std::vector<std::vector<float>> matrix;
        matrix.reserve(snapshot.field.size());

        for (size_t i = 0; i < snapshot.field.size(); i++) {
          float b[3];
          snapshot.field.getField(i, b);
          matrix.push_back({b[0], b[1], b[2]});
        }

        writeMatrixToCSV(matrix, getRepoRoot() + "/output/simulation_results_1.csv");
      }

      if (service.hasForceSection()) {

        if (!parsed.count("quiet")) {
          std::cout << "Computing forces" << std::endl;
        }

        snapshot.forces = service.simulateForces(nullptr, run_options);

        std::vector<std::vector<float>> matrix;
        matrix.reserve(snapshot.forces.entries.size());

        for (const auto& entry : snapshot.forces.entries) {
          matrix.push_back({
            (float) entry.id,
            entry.force[0], entry.force[1], entry.force[2],
            entry.torque[0], entry.torque[1], entry.torque[2]
          });
        }

        writeMatrixToCSV(matrix, getRepoRoot() + "/output/force_results.csv");
      }

      if (parsed.count("snapshot")) {

        const std::string snapshot_path = parsed["snapshot"].as<std::string>();

        greeter::view::SnapshotIO::write(snapshot, snapshot_path);

        std::cout << "Snapshot written to " << snapshot_path << std::endl;
      }

    } catch (const std::exception& error) {
      std::cerr << "Error: " << error.what() << std::endl;
      status = 1;
    }
  }

  Kokkos::finalize();

  return status;
}

std::string getRepoRoot() {
  std::filesystem::path source_file = __FILE__;
  std::filesystem::path repo_root = source_file.parent_path().parent_path().parent_path();
  return repo_root;
}

std::string getDataPath() {
  std::filesystem::path repo_root = getRepoRoot();
  return repo_root / "data";
}

std::vector<std::filesystem::path> getJSONFiles() {
  std::string data_path = getDataPath();
  std::vector<std::filesystem::path> json_files;
  for (const auto& entry : std::filesystem::directory_iterator(data_path)) {
    if (entry.path().extension() == ".json") {
      json_files.push_back(entry.path());
    }
  }
  return json_files;
}

void writeMatrixToCSV(const std::vector<std::vector<float>>& matrix, const std::string& filename) {
    // Open the file in write mode
    std::ofstream file(filename);

    // Check if the file is open
    if (!file.is_open()) {
        std::cerr << "Error: Could not open the file for writing!" << std::endl;
        return;
    }

    /*
      Nine significant digits, which is what a float needs to be written and
      read back unchanged. The default six is a part in a million of the value,
      and a homogeneous magnet is measured in exactly that: a field of 0.21 T
      flat to 90 ppm varies by 19 microtesla end to end, which the default
      writes in twenty steps. The homogeneity read back off such a file is then
      the file's rounding rather than the magnet's.
    */
    file.precision(9);

    // Write the matrix to the file
    for (const auto& row : matrix) {
        for (size_t i = 0; i < row.size(); ++i) {
            file << row[i];  // Write the element

            // Add a comma after all but the last element
            if (i < row.size() - 1) {
                file << ",";
            }
        }
        file << "\n"; // End of row
    }

    // Close the file
    file.close();

    std::cout << "Matrix successfully written to " << filename << std::endl;
}
