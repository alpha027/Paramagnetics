#include <doctest/doctest.h>
#include <greeter/greeter.h>
#include <greeter/version.h>
#include <greeter/MagnetCollection.h>

#include <nlohmann/json.hpp>
#include <cstdio>
#include <filesystem>
#include <fstream>

/*
  MagnetCollection can be built from, and checked against, a JSON file. Both used
  to carry their own list of magnet types, which went stale as soon as a type was
  added, and both were dead code that could not have worked: the type check asked
  whether a magnet object had a key named "cuboid", which the schema never has,
  and the constructor left every magnet commented out.

  They now go through MagnetIO, which is the one place that knows the schema.
*/

namespace {

  const char* THREE_MAGNETS = R"({
    "magnets": [
      { "id": 1, "type": "cuboid", "parameters": {
          "dimensions": [1, 1, 1], "magnetization": [0, 0, 1],
          "position": [0, 0, 0], "orientation": [1, 0, 0, 0] } },
      { "id": 2, "type": "sphere", "parameters": {
          "dimensions": 0.5, "magnetization": 1.0,
          "position": [0, 0, 2], "orientation": [1, 0, 0, 0] } },
      { "id": 3, "type": "tetrahedron", "parameters": {
          "vertices": [[0, 0, 0], [1, 0, 0], [0, 1, 0], [0, 0, 1]],
          "magnetization": [0, 0, 1],
          "position": [0, 0, -2], "orientation": [1, 0, 0, 0] } }
    ],
    "field_of_view": {
      "x": { "min": 0, "max": 1, "n": 2 },
      "y": { "min": 0, "max": 1, "n": 2 },
      "z": { "min": 0, "max": 1, "n": 2 }
    }
  })";

  // A file the test owns, removed again when it goes out of scope.
  class TemporaryFile {

    std::filesystem::path path;

    public:

      TemporaryFile(const std::string& contents) {
        path = std::filesystem::temp_directory_path()
             / ("paramagnetics_test_" + std::to_string(std::rand()) + ".json");
        std::ofstream file(path);
        file << contents;
      }

      ~TemporaryFile() {
        std::error_code error;
        std::filesystem::remove(path, error);
      }

      std::ifstream open() const { return std::ifstream(path); }
  };

}  // namespace


TEST_CASE("A collection is built from a JSON file") {

  TemporaryFile file(THREE_MAGNETS);
  std::ifstream stream = file.open();

  greeter::MagnetCollection collection(stream);

  REQUIRE(collection.get_num_magnets() == 3);

  // Every type keeps its own parameter count, so the magnets are the ones the
  // file asked for and not three cuboids.
  CHECK(collection.getMagnetParameters(0).size() == 13);
  CHECK(collection.getMagnetParameters(1).size() == 11);
  CHECK(collection.getMagnetParameters(2).size() == 22);
}


TEST_CASE("A JSON file is checked against the schema") {

  SUBCASE("a file with all three magnet types is valid") {

    TemporaryFile file(THREE_MAGNETS);
    std::ifstream stream = file.open();

    greeter::MagnetCollection collection;
    CHECK(collection.validJsonFile(stream));
  }

  SUBCASE("a magnet type this library does not know is rejected") {

    nlohmann::json data = nlohmann::json::parse(THREE_MAGNETS);
    data["magnets"][1]["type"] = "cylinder";

    TemporaryFile file(data.dump());
    std::ifstream stream = file.open();

    greeter::MagnetCollection collection;
    CHECK_FALSE(collection.validJsonFile(stream));
  }

  SUBCASE("a file without magnets is rejected") {

    TemporaryFile file(R"({"field_of_view": {}})");
    std::ifstream stream = file.open();

    greeter::MagnetCollection collection;
    CHECK_FALSE(collection.validJsonFile(stream));
  }
}
