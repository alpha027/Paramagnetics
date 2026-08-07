#include <doctest/doctest.h>
#include <greeter/greeter.h>
#include <greeter/version.h>
#include <greeter/io/ForceIO.h>
#include <greeter/io/MagnetIO.h>
#include <cmath>

namespace {

  const char* TWO_MAGNETS = R"({
    "magnets": [
      { "id": 7, "type": "cuboid", "parameters": {
          "dimensions": [1, 1, 1], "magnetization": [0, 0, 1],
          "position": [0, 0, 0], "orientation": [1, 0, 0, 0] } },
      { "id": 9, "type": "cuboid", "parameters": {
          "dimensions": [1, 1, 1], "magnetization": [0, 0, 1],
          "position": [0, 0, 2], "orientation": [1, 0, 0, 0] } }
    ]
  })";

  nlohmann::json withForce(const nlohmann::json& force) {
    nlohmann::json data = nlohmann::json::parse(TWO_MAGNETS);
    data["force"] = force;
    return data;
  }

}  // namespace


TEST_CASE("A JSON file without a force section is recognized") {

  nlohmann::json data = nlohmann::json::parse(TWO_MAGNETS);

  CHECK_FALSE(greeter::ForceIO::hasForceSection(data));
  CHECK_THROWS_AS(greeter::ForceIO::read(data), std::invalid_argument);

  // A field of view is only needed for a field simulation.
  CHECK(greeter::MagnetIO::validateJSON(withForce({{"targets", "all"}})));
}


TEST_CASE("Force targets are resolved from the magnet ids") {

  SUBCASE("a list of ids") {

    greeter::ForceConfig config = greeter::ForceIO::read(
      withForce({{"targets", {9}}, {"meshing", 8}}));

    REQUIRE(config.targets.size() == 1);
    CHECK(config.targets[0] == 1);  // the magnet with id 9 is the second one
    CHECK(config.meshing[0].total == 8);
    CHECK_FALSE(config.meshing[0].explicit_split);
    CHECK(config.centroid_pivot);
  }

  SUBCASE("all magnets") {

    greeter::ForceConfig config = greeter::ForceIO::read(
      withForce({{"targets", "all"}}));

    REQUIRE(config.targets.size() == 2);
    CHECK(config.targets[0] == 0);
    CHECK(config.targets[1] == 1);
  }

  SUBCASE("an unknown id is rejected") {

    CHECK_THROWS_AS(
      greeter::ForceIO::read(withForce({{"targets", {123}}})),
      std::invalid_argument);
  }

  SUBCASE("an empty target list is rejected") {

    CHECK_THROWS_AS(
      greeter::ForceIO::read(withForce({{"targets", nlohmann::json::array()}})),
      std::invalid_argument);
  }
}


TEST_CASE("Force meshing accepts a count and an explicit split") {

  SUBCASE("explicit split") {

    greeter::ForceConfig config = greeter::ForceIO::read(
      withForce({{"targets", {9}}, {"meshing", {2, 3, 4}}}));

    CHECK(config.meshing[0].explicit_split);
    CHECK(config.meshing[0].n[0] == 2);
    CHECK(config.meshing[0].n[1] == 3);
    CHECK(config.meshing[0].n[2] == 4);
  }

  SUBCASE("a non positive meshing is rejected") {

    CHECK_THROWS_AS(
      greeter::ForceIO::read(withForce({{"targets", {9}}, {"meshing", 0}})),
      std::invalid_argument);

    CHECK_THROWS_AS(
      greeter::ForceIO::read(withForce({{"targets", {9}}, {"meshing", {1, 2}}})),
      std::invalid_argument);
  }
}


TEST_CASE("Force target objects override the shared defaults") {

  nlohmann::json force = {
    {"meshing", 8},
    {"eps", 1e-4},
    {"targets", {
      {{"id", 9}, {"meshing", {2, 2, 2}}, {"sources", {7}}, {"pivot", {0, 0, 1}}},
      {{"id", 7}}
    }}
  };

  greeter::ForceConfig config = greeter::ForceIO::read(withForce(force));

  REQUIRE(config.targets.size() == 2);

  CHECK(config.eps == doctest::Approx(1e-4));

  CHECK(config.targets[0] == 1);
  CHECK(config.meshing[0].explicit_split);
  CHECK(config.meshing[0].n[0] == 2);
  REQUIRE(config.sources[0].size() == 1);
  CHECK(config.sources[0][0] == 0);
  CHECK(config.pivots[0][2] == doctest::Approx(1.0));

  // The second target falls back on the shared meshing, has no source
  // restriction, and keeps the centroid pivot.
  CHECK(config.targets[1] == 0);
  CHECK(config.meshing[1].total == 8);
  CHECK(config.sources[1].empty());
  CHECK(std::isnan(config.pivots[1][0]));

  CHECK_FALSE(config.centroid_pivot);
}
