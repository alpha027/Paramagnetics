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
#include <greeter/io/SphericalMagnetIO.h>

#include <nlohmann/json.hpp>
#include <fstream>

#include <greeter/io/MagnetIO.h>

//#include <Kokkos_Core.hpp>
//using json = nlohmann::json;

std::vector<std::filesystem::path> getJSONFiles();
std::string getDataPath();
void writeMatrixToCSV(const std::vector<std::vector<float>>& matrix, const std::string& filename);
std::string getRepoRoot();

auto main(int argc, char** argv) -> int {
  Kokkos::initialize(argc, argv);

  greeter::CubicMagnetIO cubic_magnet_reader;
  greeter::SphericalMagnetIO sphere_magnet_reader;

  std::filesystem::path JSON_path_str = getJSONFiles()[0];

  std::ifstream fileJson(JSON_path_str);
  nlohmann::json magnetics_data = nlohmann::json::parse(fileJson);

  greeter::MagnetCollection my_read_magnet_collection = greeter::MagnetIO::read(magnetics_data);

  greeter::FieldOfView my_read_fov = greeter::MagnetIO::readFieldOfView(magnetics_data["field_of_view"]);

  auto simulation_results_1 = my_read_magnet_collection.simulate(my_read_fov);

  std::string output_file_path = getRepoRoot() + "/output/simulation_results_1.csv";
  writeMatrixToCSV(simulation_results_1, output_file_path);

  return 0;
  Kokkos::finalize();
}

std::string getRepoRoot() {
  std::filesystem::path source_file = __FILE__;
  std::filesystem::path current_path = std::filesystem::current_path();
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