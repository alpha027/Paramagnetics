#include <doctest/doctest.h>
#include <greeter/greeter.h>
#include <greeter/version.h>
#include <greeter/MagnetCollection.h>
#include <greeter/io/ArrangementFactoryIO.h>
#include <greeter/io/ForceIO.h>
#include <greeter/io/MagnetIO.h>
#include <greeter/io/SceneIO.h>

#include <nlohmann/json.hpp>
#include <cmath>
#include <string>

/*
  An arrangement is a parametrised group of magnets: the input file says how
  many, how far apart and what they are made of, and the reader turns that into
  ordinary magnets that nothing downstream can tell apart from ones the file
  listed one by one.

  What is worth testing about one is therefore not the physics, which belongs
  to the magnet it repeats, but the placement it works out and the ids the
  scene hands to what it generated.
*/

namespace {

  const float SQRT_HALF = 0.70710678f;

  // A few points off every symmetry plane of the arrangements below.
  const std::vector<std::vector<float>> PROBE_POINTS = {
    { 0.7f,  0.3f,  0.4f},
    {-0.6f,  0.9f, -0.2f},
    { 1.4f, -0.5f,  0.8f},
    {-1.1f, -0.7f,  1.3f}
  };

  const char* FIELD_OF_VIEW = R"({
    "x": { "min": 0, "max": 1, "n": 2 },
    "y": { "min": 0, "max": 1, "n": 2 },
    "z": { "min": 0, "max": 1, "n": 2 }
  })";

  // An input file holding one arrangement and nothing else.
  nlohmann::json fileWith(const nlohmann::json& parameters,
                          const std::string& type = "linear_array") {
    nlohmann::json data;
    data["arrangements"] = nlohmann::json::array();
    data["arrangements"].push_back({{"id", 100}, {"type", type}, {"parameters", parameters}});
    data["field_of_view"] = nlohmann::json::parse(FIELD_OF_VIEW);
    return data;
  }

  // A unit cuboid polarized along z, the element most of these repeat.
  nlohmann::json cuboidElement(const nlohmann::json& magnetization = {0, 0, 1}) {
    return {
      {"type", "cuboid"},
      {"parameters", {
        {"dimensions", {0.2, 0.2, 0.2}},
        {"magnetization", magnetization}
      }}
    };
  }

  std::vector<float> positionOf(const greeter::MagnetCollection& collection, size_t index) {
    const std::vector<float> parameters = collection.getMagnetParameters(index);
    return {parameters[0], parameters[1], parameters[2]};
  }

  std::vector<float> orientationOf(const greeter::MagnetCollection& collection, size_t index) {
    const std::vector<float> parameters = collection.getMagnetParameters(index);
    return {parameters[3], parameters[4], parameters[5], parameters[6]};
  }

  void checkVector(const std::vector<float>& given, const std::vector<float>& expected) {
    REQUIRE(given.size() == expected.size());
    for (size_t i = 0; i < expected.size(); i++) {
      CHECK(given[i] == doctest::Approx(expected[i]));
    }
  }

  void checkSameField(const greeter::MagnetCollection& one,
                      const greeter::MagnetCollection& other) {

    const std::vector<std::vector<float>> from_one = one.simulate(PROBE_POINTS);
    const std::vector<std::vector<float>> from_other = other.simulate(PROBE_POINTS);

    REQUIRE(from_one.size() == from_other.size());

    for (size_t i = 0; i < from_one.size(); i++) {
      REQUIRE(from_one[i].size() == 3);
      for (size_t j = 0; j < 3; j++) {
        CHECK(from_one[i][j] == doctest::Approx(from_other[i][j]).epsilon(1e-5));
      }
    }
  }

}  // namespace


TEST_CASE("A linear array lays its members out on a lattice") {

  SUBCASE("a row is centred on the position of the arrangement") {

    const greeter::Scene scene = greeter::SceneIO::read(fileWith({
      {"count", 4},
      {"spacing", 0.5},
      {"element", cuboidElement()}
    }));

    REQUIRE(scene.collection.get_num_magnets() == 4);

    checkVector(positionOf(scene.collection, 0), {-0.75f, 0.0f, 0.0f});
    checkVector(positionOf(scene.collection, 1), {-0.25f, 0.0f, 0.0f});
    checkVector(positionOf(scene.collection, 2), { 0.25f, 0.0f, 0.0f});
    checkVector(positionOf(scene.collection, 3), { 0.75f, 0.0f, 0.0f});

    // Nothing asked for a rotation, so nothing is turned.
    for (size_t i = 0; i < 4; i++) {
      checkVector(orientationOf(scene.collection, i), {1.0f, 0.0f, 0.0f, 0.0f});
    }
  }

  SUBCASE("a single count is a row along the first axis") {

    const greeter::Scene row = greeter::SceneIO::read(fileWith({
      {"count", 3}, {"spacing", 0.5}, {"element", cuboidElement()}
    }));

    const greeter::Scene spelled_out = greeter::SceneIO::read(fileWith({
      {"count", {3, 1, 1}}, {"spacing", {0.5, 0.5, 0.5}}, {"element", cuboidElement()}
    }));

    REQUIRE(row.collection.get_num_magnets() == 3);
    REQUIRE(spelled_out.collection.get_num_magnets() == 3);

    for (size_t i = 0; i < 3; i++) {
      checkVector(positionOf(row.collection, i), positionOf(spelled_out.collection, i));
    }
  }

  SUBCASE("x runs fastest and z slowest") {

    const greeter::Scene scene = greeter::SceneIO::read(fileWith({
      {"count", {2, 2, 2}},
      {"spacing", {1.0, 2.0, 4.0}},
      {"element", cuboidElement()}
    }));

    REQUIRE(scene.collection.get_num_magnets() == 8);

    // index = ix + nx * (iy + ny * iz)
    checkVector(positionOf(scene.collection, 0), {-0.5f, -1.0f, -2.0f});
    checkVector(positionOf(scene.collection, 1), { 0.5f, -1.0f, -2.0f});
    checkVector(positionOf(scene.collection, 2), {-0.5f,  1.0f, -2.0f});
    checkVector(positionOf(scene.collection, 3), { 0.5f,  1.0f, -2.0f});
    checkVector(positionOf(scene.collection, 4), {-0.5f, -1.0f,  2.0f});
    checkVector(positionOf(scene.collection, 7), { 0.5f,  1.0f,  2.0f});
  }

  SUBCASE("an array that is not centred starts at the position of the arrangement") {

    const greeter::Scene scene = greeter::SceneIO::read(fileWith({
      {"count", 3},
      {"spacing", 0.5},
      {"centered", false},
      {"position", {1.0, 2.0, 3.0}},
      {"element", cuboidElement()}
    }));

    checkVector(positionOf(scene.collection, 0), {1.0f, 2.0f, 3.0f});
    checkVector(positionOf(scene.collection, 1), {1.5f, 2.0f, 3.0f});
    checkVector(positionOf(scene.collection, 2), {2.0f, 2.0f, 3.0f});
  }
}


TEST_CASE("A linear array refuses a lattice it cannot lay out") {

  SUBCASE("a count has to be a whole number of at least one") {

    CHECK_THROWS_AS(greeter::SceneIO::read(fileWith({
      {"count", 0}, {"spacing", 0.5}, {"element", cuboidElement()}
    })), std::invalid_argument);

    CHECK_THROWS_AS(greeter::SceneIO::read(fileWith({
      {"count", 2.5}, {"spacing", 0.5}, {"element", cuboidElement()}
    })), std::invalid_argument);

    CHECK_THROWS_AS(greeter::SceneIO::read(fileWith({
      {"count", {2, 1}}, {"spacing", 0.5}, {"element", cuboidElement()}
    })), std::invalid_argument);
  }

  SUBCASE("neighbours may not sit on top of each other") {

    // Two members and no room between them is not a lattice, and the field
    // between two magnets in the same place does not exist.
    CHECK_THROWS_AS(greeter::SceneIO::read(fileWith({
      {"count", 2}, {"spacing", 0.0}, {"element", cuboidElement()}
    })), std::invalid_argument);

    // One member along an axis needs no room along it.
    CHECK_NOTHROW(greeter::SceneIO::read(fileWith({
      {"count", {2, 1, 1}}, {"spacing", {0.5, 0.0, 0.0}}, {"element", cuboidElement()}
    })));
  }

  SUBCASE("the count and the spacing are both needed") {

    CHECK_THROWS_AS(greeter::SceneIO::read(fileWith({
      {"spacing", 0.5}, {"element", cuboidElement()}
    })), std::invalid_argument);

    CHECK_THROWS_AS(greeter::SceneIO::read(fileWith({
      {"count", 2}, {"element", cuboidElement()}
    })), std::invalid_argument);
  }
}


TEST_CASE("An arrangement is built out of whatever magnet type it is given") {

  // The element goes through the reader of its own type, so an arrangement
  // knows nothing about the shapes it repeats and works with every one of them.
  SUBCASE("every registered magnet type can be repeated") {

    const std::vector<nlohmann::json> elements = {
      {{"type", "cuboid"}, {"parameters", {
        {"dimensions", {0.2, 0.2, 0.2}}, {"magnetization", {0, 0, 1}}}}},
      {{"type", "sphere"}, {"parameters", {
        {"dimensions", 0.1}, {"magnetization", 1.0}}}},
      {{"type", "cylinder"}, {"parameters", {
        {"dimensions", {0.2, 0.3}}, {"magnetization", {0, 0, 1}}}}},
      {{"type", "tetrahedron"}, {"parameters", {
        {"vertices", {{0, 0, 0}, {0.2, 0, 0}, {0, 0.2, 0}, {0, 0, 0.2}}},
        {"magnetization", {0, 0, 1}}}}}
    };

    for (const auto& element : elements) {

      const greeter::Scene scene = greeter::SceneIO::read(fileWith({
        {"count", 3}, {"spacing", 0.5}, {"element", element}
      }));

      CHECK(scene.collection.get_num_magnets() == 3);

      // The element really was built as its own type, not as some default.
      const std::vector<std::vector<float>> field = scene.collection.simulate(PROBE_POINTS);
      REQUIRE(field.size() == PROBE_POINTS.size());
      CHECK(std::isfinite(field[0][2]));
      CHECK(field[0][2] != 0.0f);
    }
  }

  SUBCASE("an element may not place itself") {

    // The arrangement is what decides where its members go, so an element that
    // carries a placement is a contradiction rather than something to overwrite.
    nlohmann::json placed = cuboidElement();
    placed["parameters"]["position"] = {0, 0, 0};

    CHECK_THROWS_AS(greeter::SceneIO::read(fileWith({
      {"count", 2}, {"spacing", 0.5}, {"element", placed}
    })), std::invalid_argument);

    nlohmann::json turned = cuboidElement();
    turned["parameters"]["orientation"] = {1, 0, 0, 0};

    CHECK_THROWS_AS(greeter::SceneIO::read(fileWith({
      {"count", 2}, {"spacing", 0.5}, {"element", turned}
    })), std::invalid_argument);
  }

  SUBCASE("an element of an unknown type is refused by the reader of the magnets") {

    CHECK_THROWS_AS(greeter::SceneIO::read(fileWith({
      {"count", 2}, {"spacing", 0.5},
      {"element", {{"type", "torus"}, {"parameters", {{"dimensions", {1, 2}}}}}}
    })), std::invalid_argument);
  }

  SUBCASE("an arrangement needs an element and parameters") {

    CHECK_THROWS_AS(greeter::SceneIO::read(fileWith({
      {"count", 2}, {"spacing", 0.5}
    })), std::invalid_argument);

    nlohmann::json data;
    data["arrangements"] = {{{"id", 1}, {"type", "linear_array"}}};
    data["field_of_view"] = nlohmann::json::parse(FIELD_OF_VIEW);

    CHECK_THROWS_AS(greeter::SceneIO::read(data), std::invalid_argument);
  }
}


TEST_CASE("An arrangement is placed as a whole") {

  SUBCASE("the members are turned about the position of the arrangement") {

    // A row along x, turned a quarter turn about z, is a row along y.
    const greeter::Scene scene = greeter::SceneIO::read(fileWith({
      {"count", 2},
      {"spacing", 1.0},
      {"orientation", {SQRT_HALF, 0.0, 0.0, SQRT_HALF}},
      {"position", {0.0, 0.0, 2.0}},
      {"element", cuboidElement()}
    }));

    checkVector(positionOf(scene.collection, 0), {0.0f, -0.5f, 2.0f});
    checkVector(positionOf(scene.collection, 1), {0.0f,  0.5f, 2.0f});

    for (size_t i = 0; i < 2; i++) {
      checkVector(orientationOf(scene.collection, i), {SQRT_HALF, 0.0f, 0.0f, SQRT_HALF});
    }
  }

  SUBCASE("a placed arrangement is the same thing as the magnets written out") {

    // Every part of the placement at once, against the same magnets listed one
    // by one. This is what says the composition is right rather than merely
    // self consistent.
    const greeter::Scene arranged = greeter::SceneIO::read(fileWith({
      {"count", {2, 2, 1}},
      {"spacing", {0.6, 0.4, 1.0}},
      {"orientation", {SQRT_HALF, 0.0, 0.0, SQRT_HALF}},
      {"position", {0.1, -0.2, 0.3}},
      {"element", cuboidElement({0, 0, 1})}
    }));

    // A quarter turn about z sends (x, y) to (-y, x).
    const std::vector<std::vector<float>> expected_positions = {
      {0.1f + 0.2f, -0.2f - 0.3f, 0.3f},
      {0.1f + 0.2f, -0.2f + 0.3f, 0.3f},
      {0.1f - 0.2f, -0.2f - 0.3f, 0.3f},
      {0.1f - 0.2f, -0.2f + 0.3f, 0.3f}
    };

    REQUIRE(arranged.collection.get_num_magnets() == 4);

    nlohmann::json listed;
    listed["magnets"] = nlohmann::json::array();
    listed["field_of_view"] = nlohmann::json::parse(FIELD_OF_VIEW);

    for (size_t i = 0; i < 4; i++) {

      checkVector(positionOf(arranged.collection, i), expected_positions[i]);

      listed["magnets"].push_back({
        {"id", (int) i},
        {"type", "cuboid"},
        {"parameters", {
          {"dimensions", {0.2, 0.2, 0.2}},
          {"magnetization", {0, 0, 1}},
          {"position", expected_positions[i]},
          {"orientation", {SQRT_HALF, 0.0, 0.0, SQRT_HALF}}
        }}
      });
    }

    checkSameField(arranged.collection, greeter::MagnetIO::read(listed));
  }

  SUBCASE("the orientation of an arrangement is a quaternion that turns") {

    CHECK_THROWS_AS(greeter::SceneIO::read(fileWith({
      {"count", 2}, {"spacing", 0.5},
      {"orientation", {1, 0, 0}}, {"element", cuboidElement()}
    })), std::invalid_argument);

    CHECK_THROWS_AS(greeter::SceneIO::read(fileWith({
      {"count", 2}, {"spacing", 0.5},
      {"orientation", {0, 0, 0, 0}}, {"element", cuboidElement()}
    })), std::invalid_argument);

    CHECK_THROWS_AS(greeter::SceneIO::read(fileWith({
      {"count", 2}, {"spacing", 0.5},
      {"position", {1, 2}}, {"element", cuboidElement()}
    })), std::invalid_argument);
  }
}


TEST_CASE("An alternating array turns every other member over") {

  SUBCASE("the members are turned, not their magnetization negated") {

    const greeter::Scene scene = greeter::SceneIO::read(fileWith({
      {"count", 3},
      {"spacing", 0.5},
      {"alternating", "x"},
      {"element", cuboidElement()}
    }));

    checkVector(orientationOf(scene.collection, 0), {1.0f, 0.0f, 0.0f, 0.0f});
    checkVector(orientationOf(scene.collection, 1), {0.0f, 1.0f, 0.0f, 0.0f});
    checkVector(orientationOf(scene.collection, 2), {1.0f, 0.0f, 0.0f, 0.0f});
  }

  SUBCASE("turning a cuboid over reverses the field it makes") {

    // A cube is unchanged by half a turn about x, so a turned one polarized
    // along +z has to be the same magnet as an unturned one polarized along -z.
    // Nothing of the arrangement is shared with the file it is checked against.
    const greeter::Scene alternating = greeter::SceneIO::read(fileWith({
      {"count", 2},
      {"spacing", 0.5},
      {"alternating", "x"},
      {"element", cuboidElement({0, 0, 1})}
    }));

    nlohmann::json listed;
    listed["field_of_view"] = nlohmann::json::parse(FIELD_OF_VIEW);
    listed["magnets"] = {
      {{"id", 1}, {"type", "cuboid"}, {"parameters", {
        {"dimensions", {0.2, 0.2, 0.2}}, {"magnetization", {0, 0, 1}},
        {"position", {-0.25, 0, 0}}, {"orientation", {1, 0, 0, 0}}}}},
      {{"id", 2}, {"type", "cuboid"}, {"parameters", {
        {"dimensions", {0.2, 0.2, 0.2}}, {"magnetization", {0, 0, -1}},
        {"position", {0.25, 0, 0}}, {"orientation", {1, 0, 0, 0}}}}}
    };

    checkSameField(alternating.collection, greeter::MagnetIO::read(listed));
  }

  SUBCASE("the parity is over the whole lattice, not one axis") {

    const greeter::Scene scene = greeter::SceneIO::read(fileWith({
      {"count", {2, 2, 1}},
      {"spacing", 0.5},
      {"alternating", "y"},
      {"element", cuboidElement()}
    }));

    // (0,0,0) and (1,1,0) are even, (1,0,0) and (0,1,0) are odd.
    checkVector(orientationOf(scene.collection, 0), {1.0f, 0.0f, 0.0f, 0.0f});
    checkVector(orientationOf(scene.collection, 1), {0.0f, 0.0f, 1.0f, 0.0f});
    checkVector(orientationOf(scene.collection, 2), {0.0f, 0.0f, 1.0f, 0.0f});
    checkVector(orientationOf(scene.collection, 3), {1.0f, 0.0f, 0.0f, 0.0f});
  }

  SUBCASE("the axis of the turn has to be named") {

    CHECK_THROWS_AS(greeter::SceneIO::read(fileWith({
      {"count", 2}, {"spacing", 0.5}, {"alternating", true}, {"element", cuboidElement()}
    })), std::invalid_argument);

    CHECK_THROWS_AS(greeter::SceneIO::read(fileWith({
      {"count", 2}, {"spacing", 0.5}, {"alternating", "w"}, {"element", cuboidElement()}
    })), std::invalid_argument);

    // Switched off explicitly, nothing is turned.
    const greeter::Scene scene = greeter::SceneIO::read(fileWith({
      {"count", 2}, {"spacing", 0.5}, {"alternating", false}, {"element", cuboidElement()}
    }));

    checkVector(orientationOf(scene.collection, 1), {1.0f, 0.0f, 0.0f, 0.0f});
  }
}


TEST_CASE("A scene numbers the magnets an arrangement generated") {

  nlohmann::json data;
  data["field_of_view"] = nlohmann::json::parse(FIELD_OF_VIEW);
  data["magnets"] = {
    {{"id", 7}, {"type", "cuboid"}, {"parameters", {
      {"dimensions", {0.2, 0.2, 0.2}}, {"magnetization", {0, 0, 1}},
      {"position", {0, 0, 3}}, {"orientation", {1, 0, 0, 0}}}}}
  };
  data["arrangements"] = {
    {{"id", 100}, {"type", "linear_array"}, {"parameters", {
      {"count", 3}, {"spacing", 0.5}, {"element", cuboidElement()}}}},
    {{"id", 200}, {"type", "linear_array"}, {"parameters", {
      {"count", 2}, {"spacing", 0.5}, {"position", {0, 2, 0}},
      {"element", cuboidElement()}}}}
  };

  const greeter::Scene scene = greeter::SceneIO::read(data);

  SUBCASE("the magnets the file lists come first and keep their ids") {

    REQUIRE(scene.collection.get_num_magnets() == 6);
    REQUIRE(scene.magnet_ids.size() == 6);

    CHECK(scene.magnet_ids[0] == 7);
    checkVector(positionOf(scene.collection, 0), {0.0f, 0.0f, 3.0f});
  }

  SUBCASE("the generated magnets are numbered on from the highest id in use") {

    // Nothing generated may take the id of a magnet the file named.
    CHECK(scene.magnet_ids[1] == 8);
    CHECK(scene.magnet_ids[2] == 9);
    CHECK(scene.magnet_ids[3] == 10);
    CHECK(scene.magnet_ids[4] == 11);
    CHECK(scene.magnet_ids[5] == 12);
  }

  SUBCASE("each arrangement knows which magnets are its own") {

    REQUIRE(scene.arrangements.size() == 2);

    CHECK(scene.arrangements[0].id == 100);
    CHECK(scene.arrangements[0].members == std::vector<uint32_t>{1, 2, 3});

    CHECK(scene.arrangements[1].id == 200);
    CHECK(scene.arrangements[1].members == std::vector<uint32_t>{4, 5});
  }

  SUBCASE("an id names one magnet") {

    nlohmann::json repeated = data;
    repeated["magnets"].push_back(repeated["magnets"][0]);

    CHECK_THROWS_AS(greeter::SceneIO::read(repeated), std::invalid_argument);

    nlohmann::json repeated_arrangement = data;
    repeated_arrangement["arrangements"][1]["id"] = 100;

    CHECK_THROWS_AS(greeter::SceneIO::read(repeated_arrangement), std::invalid_argument);
  }

  SUBCASE("a file may hold arrangements and no magnets at all") {

    nlohmann::json only_arrangements = data;
    only_arrangements.erase("magnets");

    const greeter::Scene scene_without = greeter::SceneIO::read(only_arrangements);

    CHECK(scene_without.collection.get_num_magnets() == 5);
    CHECK(scene_without.magnet_ids[0] == 0);
    CHECK(scene_without.magnet_ids[4] == 4);
  }

  SUBCASE("a file has to describe some magnets") {

    nlohmann::json empty;
    empty["field_of_view"] = nlohmann::json::parse(FIELD_OF_VIEW);

    CHECK_FALSE(greeter::MagnetIO::validateJSON(empty));
    CHECK_THROWS_AS(greeter::SceneIO::read(empty), std::invalid_argument);
  }
}


TEST_CASE("The force section reaches the magnets an arrangement generated") {

  nlohmann::json data;
  data["magnets"] = {
    {{"id", 1}, {"type", "cuboid"}, {"parameters", {
      {"dimensions", {0.2, 0.2, 0.2}}, {"magnetization", {0, 0, 1}},
      {"position", {0, 0, 1}}, {"orientation", {1, 0, 0, 0}}}}}
  };
  data["arrangements"] = {
    {{"id", 100}, {"type", "linear_array"}, {"parameters", {
      {"count", 3}, {"spacing", 0.5}, {"element", cuboidElement()}}}}
  };
  data["force"] = {{"targets", "all"}, {"meshing", 1}};

  const greeter::Scene scene = greeter::SceneIO::read(data);

  SUBCASE("\"all\" covers the generated magnets too") {

    nlohmann::json with_force = data;
    with_force["force"] = {{"targets", "all"}, {"meshing", 1}};

    const greeter::ForceConfig config = greeter::ForceIO::read(
      with_force, scene.magnet_ids, scene.arrangements);

    // Four magnets, not just the one the file listed.
    CHECK(config.targets.size() == 4);
    CHECK(config.targets == std::vector<uint32_t>{0, 1, 2, 3});
  }

  SUBCASE("an arrangement can be named as a target") {

    nlohmann::json with_force = data;
    with_force["force"] = {
      {"targets", {{{"arrangement", 100}, {"meshing", 8}}}},
      {"meshing", 1}
    };

    const greeter::ForceConfig config = greeter::ForceIO::read(
      with_force, scene.magnet_ids, scene.arrangements);

    CHECK(config.targets == std::vector<uint32_t>{1, 2, 3});

    // The settings of the entry are shared by every member it named.
    REQUIRE(config.meshing.size() == 3);
    for (const auto& meshing : config.meshing) {
      CHECK(meshing.total == 8);
    }
  }

  SUBCASE("an arrangement can be named as a source") {

    nlohmann::json with_force = data;
    with_force["force"] = {
      {"targets", {1}},
      {"sources", {{{"arrangement", 100}}}},
      {"meshing", 1}
    };

    const greeter::ForceConfig config = greeter::ForceIO::read(
      with_force, scene.magnet_ids, scene.arrangements);

    REQUIRE(config.targets == std::vector<uint32_t>{0});
    REQUIRE(config.sources.size() == 1);
    CHECK(config.sources[0] == std::vector<uint32_t>{1, 2, 3});
  }

  SUBCASE("a generated magnet can be named by the id it was given") {

    nlohmann::json with_force = data;
    with_force["force"] = {{"targets", {3}}, {"meshing", 1}};

    const greeter::ForceConfig config = greeter::ForceIO::read(
      with_force, scene.magnet_ids, scene.arrangements);

    CHECK(config.targets == std::vector<uint32_t>{2});
  }

  SUBCASE("an unknown arrangement is refused") {

    nlohmann::json with_force = data;
    with_force["force"] = {{"targets", {{{"arrangement", 999}}}}, {"meshing", 1}};

    CHECK_THROWS_AS(
      greeter::ForceIO::read(with_force, scene.magnet_ids, scene.arrangements),
      std::invalid_argument);
  }

  SUBCASE("a target object still needs to name something") {

    nlohmann::json with_force = data;
    with_force["force"] = {{"targets", {{{"meshing", 4}}}}, {"meshing", 1}};

    CHECK_THROWS_AS(
      greeter::ForceIO::read(with_force, scene.magnet_ids, scene.arrangements),
      std::invalid_argument);
  }

  SUBCASE("reading a force section against the file alone is refused") {

    // The ids of the "magnets" array do not cover what an arrangement
    // generated, so the reading that assumes they do has to say so rather than
    // quietly leave the generated magnets out.
    nlohmann::json with_force = data;
    with_force["force"] = {{"targets", "all"}, {"meshing", 1}};

    CHECK_THROWS_AS(greeter::ForceIO::read(with_force), std::invalid_argument);
  }

  SUBCASE("the force on a generated magnet is the force on the same magnet listed") {

    nlohmann::json with_force = data;
    with_force["force"] = {{"targets", {3}}, {"meshing", 64}};

    const greeter::ForceConfig config = greeter::ForceIO::read(
      with_force, scene.magnet_ids, scene.arrangements);

    const std::vector<greeter::ForceResult> arranged =
      scene.collection.computeForces(config);

    // The same four magnets, written out one by one.
    nlohmann::json listed;
    listed["magnets"] = {
      {{"id", 1}, {"type", "cuboid"}, {"parameters", {
        {"dimensions", {0.2, 0.2, 0.2}}, {"magnetization", {0, 0, 1}},
        {"position", {0, 0, 1}}, {"orientation", {1, 0, 0, 0}}}}},
      {{"id", 2}, {"type", "cuboid"}, {"parameters", {
        {"dimensions", {0.2, 0.2, 0.2}}, {"magnetization", {0, 0, 1}},
        {"position", {-0.5, 0, 0}}, {"orientation", {1, 0, 0, 0}}}}},
      {{"id", 3}, {"type", "cuboid"}, {"parameters", {
        {"dimensions", {0.2, 0.2, 0.2}}, {"magnetization", {0, 0, 1}},
        {"position", {0, 0, 0}}, {"orientation", {1, 0, 0, 0}}}}},
      {{"id", 4}, {"type", "cuboid"}, {"parameters", {
        {"dimensions", {0.2, 0.2, 0.2}}, {"magnetization", {0, 0, 1}},
        {"position", {0.5, 0, 0}}, {"orientation", {1, 0, 0, 0}}}}}
    };
    // Id 3 is the middle member of the row, the same magnet the arrangement
    // was asked about.
    listed["force"] = {{"targets", {3}}, {"meshing", 64}};

    const greeter::Scene listed_scene = greeter::SceneIO::read(listed);
    const std::vector<greeter::ForceResult> written_out =
      listed_scene.collection.computeForces(greeter::ForceIO::read(listed));

    REQUIRE(arranged.size() == 1);
    REQUIRE(written_out.size() == 1);

    for (size_t i = 0; i < 3; i++) {
      CHECK(arranged[0].force[i] == doctest::Approx(written_out[0].force[i]).epsilon(1e-5));
      CHECK(arranged[0].torque[i] == doctest::Approx(written_out[0].torque[i]).epsilon(1e-5));
    }
  }
}


TEST_CASE("An arrangement is reachable through the registry") {

  SUBCASE("the linear array is registered under its own name") {

    CHECK(greeter::ArrangementFactoryIO::getInstance().isRegistered("linear_array"));
    CHECK_FALSE(greeter::ArrangementFactoryIO::getInstance().isRegistered("no_such_arrangement"));
  }

  SUBCASE("an unknown arrangement type is refused by the schema and the registry") {

    nlohmann::json data = fileWith({
      {"count", 2}, {"spacing", 0.5}, {"element", cuboidElement()}
    }, "no_such_arrangement");

    CHECK_FALSE(greeter::MagnetIO::validateJSON(data));
    CHECK_THROWS_AS(greeter::SceneIO::read(data), std::invalid_argument);

    CHECK_THROWS_AS(
      greeter::ArrangementFactoryIO::getInstance().expand("no_such_arrangement", data),
      std::invalid_argument);
  }
}
