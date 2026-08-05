#include <doctest/doctest.h>
#include <greeter/greeter.h>
#include <greeter/version.h>
#include <greeter/MagnetCollection.h>
#include <greeter/MagneticFieldMethodFactory.h>
#include <greeter/TargetMeshFactory.h>
#include <greeter/io/MagnetIO.h>
#include <greeter/io/MethodFactoryIO.h>

/*
  The registries used to be filled by a static object in the translation unit of
  every magnet class. A linker drops a translation unit of a static library when
  nothing else in it is referenced, so the registries came up empty unless the
  program happened to name the class somewhere, which cost a segmentation fault
  in this test binary. The magnets of this library are registered by the
  constructor of each registry instead, and these tests hold that in place.

  Note that this file deliberately names no magnet class and no reader.
*/

namespace {

  const uint16_t CUBOID = 0;
  const uint16_t SPHERE = 1;
  const uint16_t TETRAHEDRON = 2;

}  // namespace


TEST_CASE("The field kernel of every magnet type is registered") {

  greeter::MagneticFieldMethodFactory& factory =
    greeter::MagneticFieldMethodFactory::getInstance();

  CHECK(factory.getNumberOfParameters(CUBOID) == 13);
  CHECK(factory.getNumberOfParameters(SPHERE) == 11);
  CHECK(factory.getNumberOfParameters(TETRAHEDRON) == 22);

  // A cuboid of unit side polarized along z, on its own axis
  const float parameters[13] = {
    0.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 0.0f, 0.0f,
    1.0f, 1.0f, 1.0f,
    0.0f, 0.0f, 1.0f
  };
  const float observation_point[3] = {0.0f, 0.0f, 2.0f};

  float b_x = 0.0f, b_y = 0.0f, b_z = 0.0f;
  factory.computeMagneticField(CUBOID, parameters, observation_point, b_x, b_y, b_z);

  CHECK(b_z != 0.0f);
}


TEST_CASE("The target mesher of every magnet type is registered") {

  greeter::TargetMeshFactory& factory = greeter::TargetMeshFactory::getInstance();

  greeter::MeshingSpec meshing;
  meshing.total = 8;

  const float cuboid[13] = {
    0.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 0.0f, 0.0f,
    1.0f, 1.0f, 1.0f,
    0.0f, 0.0f, 1.0f
  };

  const float sphere[11] = {
    0.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 0.0f, 0.0f,
    0.5f,
    0.0f, 0.0f, 1.0f
  };

  const float tetrahedron[22] = {
    0.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 1.0f,
    0.0f, 0.0f, 1.0f
  };

  CHECK(factory.generateTargetMesh(CUBOID, cuboid, meshing).size() == 8);
  CHECK(factory.generateTargetMesh(SPHERE, sphere, meshing).size() == 1);
  CHECK(factory.generateTargetMesh(TETRAHEDRON, tetrahedron, meshing).size() == 10);
}


TEST_CASE("The JSON reader of every magnet type is registered") {

  const char* JSON = R"({
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
    "force": { "targets": "all" }
  })";

  greeter::MagnetCollection collection =
    greeter::MagnetIO::read(nlohmann::json::parse(JSON));

  REQUIRE(collection.get_num_magnets() == 3);

  CHECK(collection.getMagnetParameters(0).size() == 13);
  CHECK(collection.getMagnetParameters(1).size() == 11);
  CHECK(collection.getMagnetParameters(2).size() == 22);
}


TEST_CASE("An unknown magnet type is reported instead of being built") {

  const char* JSON = R"({
    "magnets": [
      { "id": 1, "type": "cylinder", "parameters": {
          "dimensions": [1, 1], "magnetization": [0, 0, 1],
          "position": [0, 0, 0], "orientation": [1, 0, 0, 0] } }
    ],
    "force": { "targets": "all" }
  })";

  // A null magnet used to reach the collection and crash on its first use.
  CHECK_THROWS_AS(
    greeter::MethodFactoryIO::getInstance().createMagnet(
      "cylinder", nlohmann::json::parse(JSON)["magnets"][0]),
    std::invalid_argument);

  CHECK_FALSE(greeter::MagnetIO::validateJSON(nlohmann::json::parse(JSON)));
}
