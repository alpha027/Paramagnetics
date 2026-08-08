#include <doctest/doctest.h>

#include <greeter/CubicMagnet.h>
#include <greeter/MagnetCollection.h>
#include <greeter/TetrahedronMagnet.h>
#include <greeter/TriangleMagnet.h>
#include <greeter/TriangularMeshMagnet.h>
#include <greeter/io/MethodFactoryIO.h>
#include <greeter/io/TriangularMeshMagnetIO.h>
#include <greeter/service/SimulationService.h>

#include <cmath>


namespace {

  /* The eight corners of a cube of half side `a`, centred on the origin. */
  std::vector<std::vector<float>> cubeCorners(const float& a) {
    return {
      {-a, -a, -a}, { a, -a, -a}, { a,  a, -a}, {-a,  a, -a},
      {-a, -a,  a}, { a, -a,  a}, { a,  a,  a}, {-a,  a,  a}
    };
  }

  /*
    A cube as twelve triangles, wound outwards. Two per face, from the six
    quads taken anticlockwise seen from outside.
  */
  std::vector<float> cubeTriangles(const float& a) {

    const std::vector<std::vector<float>> corner = cubeCorners(a);

    const int quad[6][4] = {
      {0, 3, 2, 1}, {4, 5, 6, 7}, {0, 1, 5, 4},
      {2, 3, 7, 6}, {0, 4, 7, 3}, {1, 2, 6, 5}
    };

    std::vector<float> triangles;

    for (const auto& face : quad) {

      const int split[2][3] = {
        {face[0], face[1], face[2]}, {face[0], face[2], face[3]}
      };

      for (const auto& triangle : split) {
        for (const auto& which : triangle) {
          for (size_t axis = 0; axis < 3; axis++) {
            triangles.push_back(corner[which][axis]);
          }
        }
      }
    }

    return triangles;
  }

  /* The four faces of a tetrahedron on the given vertices, wound outwards. */
  std::vector<float> tetrahedronTriangles(const std::vector<float>& vertices) {

    const int faces[4][3] = {
      {0, 2, 1}, {0, 1, 3}, {1, 2, 3}, {0, 3, 2}
    };

    std::vector<float> triangles;

    for (const auto& face : faces) {
      for (const auto& which : face) {
        for (size_t axis = 0; axis < 3; axis++) {
          triangles.push_back(vertices[3 * (size_t) which + axis]);
        }
      }
    }

    return triangles;
  }

}  // namespace


TEST_CASE("A cube built from twelve triangles is a cuboid") {

  // The point of the type. Two kernels that share no arithmetic beyond the
  // charged triangle have to agree on the same body, inside and out.
  //
  // The numbers are magpylib 5, TriangularMesh over the same twelve faces
  // with polarization (0.3, -0.2, 1), which also agrees with its own Cuboid
  // to fourteen digits away from the surface.
  const float a = 0.01f;

  const std::vector<float> magnetization = {0.3f, -0.2f, 1.0f};

  greeter::TriangularMeshMagnet mesh(
    {0.0f, 0.0f, 0.0f}, cubeTriangles(a), {1.0f, 0.0f, 0.0f, 0.0f}, magnetization);

  greeter::CuboidMagnet cuboid(
    {0.0f, 0.0f, 0.0f}, {2 * a, 2 * a, 2 * a}, {1.0f, 0.0f, 0.0f, 0.0f},
    magnetization);

  CHECK(mesh.getNumOfFaces() == 12);
  CHECK(mesh.getVolume() == doctest::Approx(8 * a * a * a));

  struct Case {
    float point[3];
    double expected[3];
  };

  const std::vector<Case> cases = {
    {{ 0.000f,  0.000f, 0.000f}, { 0.2,          -0.133333333,  0.666666667}},
    {{ 0.004f, -0.003f, 0.005f}, { 0.25562071,   -0.182236638,  0.657280259}},
    {{ 0.030f,  0.000f, 0.000f}, { 0.013607787,   0.004535929, -0.022679645}},
    {{-0.020f,  0.015f, 0.025f}, {-0.015710893,   0.011328832, -0.000436949}}
  };

  for (const auto& item : cases) {

    const std::vector<float> from_mesh =
      mesh.computeMagneticField(item.point[0], item.point[1], item.point[2]);

    const std::vector<float> from_cuboid =
      cuboid.computeMagneticField(item.point[0], item.point[1], item.point[2]);

    for (size_t axis = 0; axis < 3; axis++) {
      CHECK(from_mesh[axis] == doctest::Approx(item.expected[axis]).epsilon(1e-4));
      CHECK(from_mesh[axis] == doctest::Approx(from_cuboid[axis]).epsilon(1e-4));
    }
  }
}


TEST_CASE("A tetrahedron given as a mesh is the tetrahedron") {

  const std::vector<float> vertices = {
    0.0f, 0.0f, 0.0f,
    0.02f, 0.0f, 0.0f,
    0.0f, 0.02f, 0.0f,
    0.0f, 0.0f, 0.02f
  };

  const std::vector<float> magnetization = {0.3f, -0.2f, 1.0f};

  greeter::TetrahedronMagnet solid(
    {0.0f, 0.0f, 0.0f}, vertices, {1.0f, 0.0f, 0.0f, 0.0f}, magnetization);

  greeter::TriangularMeshMagnet mesh(
    {0.0f, 0.0f, 0.0f}, tetrahedronTriangles(vertices),
    {1.0f, 0.0f, 0.0f, 0.0f}, magnetization);

  CHECK(mesh.getVolume() == doctest::Approx(0.02f * 0.02f * 0.02f / 6.0f));

  // Points inside, outside and well away, but none on the surface, where the
  // field of a magnet has no single value to agree about.
  const std::vector<std::vector<float>> points = {
    {0.004f, 0.004f, 0.004f},
    {0.002f, 0.001f, 0.003f},
    {0.05f, 0.0f, 0.0f},
    {-0.03f, 0.02f, 0.01f},
    {0.011f, 0.011f, 0.011f}
  };

  for (const auto& point : points) {

    const std::vector<float> from_solid =
      solid.computeMagneticField(point[0], point[1], point[2]);

    const std::vector<float> from_mesh =
      mesh.computeMagneticField(point[0], point[1], point[2]);

    for (size_t axis = 0; axis < 3; axis++) {
      CHECK(from_mesh[axis] == doctest::Approx(from_solid[axis]).epsilon(1e-4));
    }
  }
}


TEST_CASE("The faces of a closed body add up to the body") {

  // What the Triangle type is for: the pieces every polyhedron is built from.
  // Summed over a closed surface, and with the polarization added inside,
  // they are the body.
  const std::vector<float> vertices = {
    0.0f, 0.0f, 0.0f,
    0.02f, 0.0f, 0.0f,
    0.0f, 0.02f, 0.0f,
    0.0f, 0.0f, 0.02f
  };

  const std::vector<float> magnetization = {0.0f, 0.0f, 1.0f};

  const std::vector<float> triangles = tetrahedronTriangles(vertices);

  greeter::TetrahedronMagnet solid(
    {0.0f, 0.0f, 0.0f}, vertices, {1.0f, 0.0f, 0.0f, 0.0f}, magnetization);

  const std::vector<std::vector<float>> points = {
    {0.05f, 0.01f, 0.0f},
    {0.004f, 0.004f, 0.004f}
  };

  for (const auto& point : points) {

    float summed[3] = {0.0f, 0.0f, 0.0f};

    for (size_t face = 0; face < 4; face++) {

      const std::vector<float> corners(
        triangles.begin() + (long) (9 * face),
        triangles.begin() + (long) (9 * face + 9));

      greeter::TriangleMagnet facet(
        {0.0f, 0.0f, 0.0f}, corners, {1.0f, 0.0f, 0.0f, 0.0f}, magnetization);

      const std::vector<float> field =
        facet.computeMagneticField(point[0], point[1], point[2]);

      for (size_t axis = 0; axis < 3; axis++) {
        summed[axis] += field[axis];
      }
    }

    // Inside the body the polarization adds to the charges of the faces.
    const bool inside =
      point[0] >= 0.0f && point[1] >= 0.0f && point[2] >= 0.0f &&
      point[0] + point[1] + point[2] <= 0.02f;

    if (inside) {
      for (size_t axis = 0; axis < 3; axis++) {
        summed[axis] += magnetization[axis];
      }
    }

    const std::vector<float> expected =
      solid.computeMagneticField(point[0], point[1], point[2]);

    for (size_t axis = 0; axis < 3; axis++) {
      CHECK(summed[axis] == doctest::Approx(expected[axis]).epsilon(1e-4));
    }
  }
}


TEST_CASE("A surface that is not closed is refused") {

  std::vector<float> triangles = cubeTriangles(0.01f);

  // Take one face away, leaving a hole.
  triangles.resize(triangles.size() - 9);

  CHECK_THROWS_AS(greeter::TriangularMeshMagnet(
    {0.0f, 0.0f, 0.0f}, triangles, {1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}),
    std::invalid_argument);

  // Too few faces to close anything.
  CHECK_THROWS_AS(greeter::TriangularMeshMagnet(
    {0.0f, 0.0f, 0.0f}, {0, 0, 0, 1, 0, 0, 0, 1, 0},
    {1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}),
    std::invalid_argument);

  // Not whole triangles.
  CHECK_THROWS_AS(greeter::TriangularMeshMagnet(
    {0.0f, 0.0f, 0.0f}, {0, 0, 0, 1, 0}, {1.0f, 0.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 1.0f}),
    std::invalid_argument);
}


TEST_CASE("Faces wound inconsistently are refused") {

  std::vector<float> triangles = cubeTriangles(0.01f);

  // Turn one face round, so that it walks two of its edges the same way as
  // its neighbours do instead of the opposite way.
  for (size_t axis = 0; axis < 3; axis++) {
    std::swap(triangles[3 + axis], triangles[6 + axis]);
  }

  CHECK_THROWS_AS(greeter::TriangularMeshMagnet(
    {0.0f, 0.0f, 0.0f}, triangles, {1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}),
    std::invalid_argument);
}


TEST_CASE("A body given inside out is turned the right way round") {

  const float a = 0.01f;

  std::vector<float> triangles = cubeTriangles(a);

  // Every face reversed, which is a consistent winding and a closed surface,
  // and encloses the same cube seen from the inside. Which way round a mesh
  // comes out of a program that made it is not worth refusing over.
  std::vector<float> reversed = triangles;

  for (size_t face = 0; face < 12; face++) {
    for (size_t axis = 0; axis < 3; axis++) {
      std::swap(reversed[9 * face + 3 + axis], reversed[9 * face + 6 + axis]);
    }
  }

  const std::vector<float> magnetization = {0.0f, 0.0f, 1.0f};

  greeter::TriangularMeshMagnet outwards(
    {0.0f, 0.0f, 0.0f}, triangles, {1.0f, 0.0f, 0.0f, 0.0f}, magnetization);

  greeter::TriangularMeshMagnet inwards(
    {0.0f, 0.0f, 0.0f}, reversed, {1.0f, 0.0f, 0.0f, 0.0f}, magnetization);

  CHECK(inwards.getVolume() > 0.0f);
  CHECK(inwards.getVolume() == doctest::Approx(outwards.getVolume()));

  const std::vector<float> one = outwards.computeMagneticField(0.03f, 0.01f, 0.0f);
  const std::vector<float> other = inwards.computeMagneticField(0.03f, 0.01f, 0.0f);

  for (size_t axis = 0; axis < 3; axis++) {
    CHECK(other[axis] == doctest::Approx(one[axis]));
  }
}


TEST_CASE("A body knows what is inside it") {

  const float a = 0.01f;

  const std::vector<float> triangles = cubeTriangles(a);

  CHECK(greeter::TriangularMeshMagnet::isInside(
    triangles.data(), 12, std::vector<float>{0.0f, 0.0f, 0.0f}.data()));

  CHECK(greeter::TriangularMeshMagnet::isInside(
    triangles.data(), 12, std::vector<float>{0.009f, -0.009f, 0.009f}.data()));

  CHECK_FALSE(greeter::TriangularMeshMagnet::isInside(
    triangles.data(), 12, std::vector<float>{0.011f, 0.0f, 0.0f}.data()));

  CHECK_FALSE(greeter::TriangularMeshMagnet::isInside(
    triangles.data(), 12, std::vector<float>{0.0f, 0.0f, 5.0f}.data()));

  // Just outside a corner, which is where a test that cast a ray would be at
  // its most delicate. The solid angle has no direction to be unlucky in.
  CHECK_FALSE(greeter::TriangularMeshMagnet::isInside(
    triangles.data(), 12, std::vector<float>{0.0101f, 0.0101f, 0.0101f}.data()));
}


TEST_CASE("A body is meshed into cells that carry what it carries") {

  const float a = 0.01f;

  greeter::TriangularMeshMagnet mesh(
    {0.0f, 0.0f, 0.0f}, cubeTriangles(a), {1.0f, 0.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 1.2f});

  std::vector<float> parameters;

  for (const auto& value : mesh.getPosition()) parameters.push_back(value);
  for (const auto& value : mesh.getOrientation()) parameters.push_back(value);
  for (const auto& value : mesh.getDimensions()) parameters.push_back(value);
  for (const auto& value : mesh.getMagnetization()) parameters.push_back(value);

  const float volume = 8 * a * a * a;
  const float expected_moment = volume * 1.2f / greeter::MU0;

  for (const uint32_t wanted : {1u, 8u, 200u}) {

    greeter::MeshingSpec meshing;
    meshing.total = wanted;

    const greeter::TargetMeshData cells =
      greeter::TriangularMeshMagnet::generateTargetMesh(parameters.data(), meshing);

    REQUIRE_FALSE(cells.empty());

    float total = 0.0f;

    for (const auto& cell : cells) {

      total += cell.moment[2];

      // Every cell stands for a piece of the body, so every cell is in it.
      CHECK(greeter::TriangularMeshMagnet::isInside(
        &parameters[8], 12, cell.point));
    }

    // However badly a grid tiles a body, the cells together carry exactly
    // what the body carries.
    INFO("cells wanted: " << wanted);
    CHECK(total == doctest::Approx(expected_moment).epsilon(1e-4));
  }
}


TEST_CASE("A mesh is read from vertices and faces, or from triangles") {

  const nlohmann::json by_index = nlohmann::json::parse(R"({
    "type": "triangular_mesh",
    "parameters": {
      "vertices": [[0,0,0], [0.02,0,0], [0,0.02,0], [0,0,0.02]],
      "faces": [[0,2,1], [0,1,3], [1,2,3], [0,3,2]],
      "magnetization": [0, 0, 1]
    }
  })");

  const std::unique_ptr<greeter::Magnet> magnet =
    greeter::MethodFactoryIO::getInstance().createMagnet("triangular_mesh", by_index);

  REQUIRE(magnet != nullptr);
  CHECK(magnet->getTypeID() == greeter::TriangularMeshMagnet::getStaticTypeID());

  // The face count leads the geometry, so a kernel given a pointer can tell
  // where the faces stop.
  CHECK(magnet->getDimensions()[0] == doctest::Approx(4.0f));
  CHECK(magnet->getDimensions().size() == 1 + 36);
  CHECK(magnet->getNumOfParameters() == 11 + 36);

  const nlohmann::json by_triangle = nlohmann::json::parse(R"({
    "type": "triangular_mesh",
    "parameters": {
      "triangles": [
        [[0,0,0], [0,0.02,0], [0.02,0,0]],
        [[0,0,0], [0.02,0,0], [0,0,0.02]],
        [[0.02,0,0], [0,0.02,0], [0,0,0.02]],
        [[0,0,0], [0,0,0.02], [0,0.02,0]]
      ],
      "magnetization": [0, 0, 1]
    }
  })");

  const std::unique_ptr<greeter::Magnet> same =
    greeter::MethodFactoryIO::getInstance().createMagnet("triangular_mesh", by_triangle);

  REQUIRE(same != nullptr);

  const std::vector<float> one = magnet->computeMagneticField(0.05, 0.01, 0.0);
  const std::vector<float> other = same->computeMagneticField(0.05, 0.01, 0.0);

  for (size_t axis = 0; axis < 3; axis++) {
    CHECK(other[axis] == doctest::Approx(one[axis]));
  }

  // A face naming a vertex that is not there.
  const nlohmann::json broken = nlohmann::json::parse(R"({
    "type": "triangular_mesh",
    "parameters": {
      "vertices": [[0,0,0], [1,0,0], [0,1,0], [0,0,1]],
      "faces": [[0,2,9]],
      "magnetization": [0, 0, 1]
    }
  })");

  CHECK_THROWS_AS(greeter::TriangularMeshMagnetIO::readTriangles(broken),
                  std::invalid_argument);
}


TEST_CASE("A magnet of any shape is drawn and takes part in a simulation") {

  greeter::service::SimulationService service;

  service.loadJSON(nlohmann::json::parse(R"({
    "magnets": [
      { "id": 1, "type": "triangular_mesh", "parameters": {
          "vertices": [[0,0,0], [0.02,0,0], [0,0.02,0], [0,0,0.02]],
          "faces": [[0,2,1], [0,1,3], [1,2,3], [0,3,2]],
          "magnetization": [0, 0, 1] } },
      { "id": 2, "type": "cuboid", "parameters": {
          "dimensions": [0.01, 0.01, 0.01], "magnetization": [0, 0, 1],
          "position": [0.05, 0, 0], "orientation": [1, 0, 0, 0] } }
    ],
    "field_of_view": {
      "x": {"min": 0.03, "max": 0.03, "n": 1},
      "y": {"min": 0, "max": 0, "n": 1},
      "z": {"min": 0.02, "max": 0.02, "n": 1}
    },
    "force": { "targets": [1], "meshing": 64 }
  })"), "test");

  const greeter::view::SceneSnapshot scene = service.getScene();

  const greeter::view::MagnetView* body = scene.findById(1);

  REQUIRE(body != nullptr);

  // Drawn as the triangles it is, which the viewer already understood before
  // this type existed.
  CHECK(body->shape.kind == greeter::view::ShapeKind::Mesh);
  CHECK(body->shape.type_name == "triangular_mesh");
  CHECK(body->shape.parameters.size() == 36);
  CHECK(body->shape.isValid());

  greeter::service::FieldRequest request;
  REQUIRE(service.getFieldRequest(request));

  const greeter::view::FieldGrid field = service.simulateField(request);

  REQUIRE(field.size() == 1);
  CHECK(std::isfinite(field.field[2]));

  // And it can be pushed about like any other body.
  const greeter::view::ForceReport report = service.simulateForces();

  REQUIRE(report.entries.size() == 1);
  CHECK(report.entries[0].id == 1);
  CHECK(report.entries[0].cells > 1);
  CHECK(std::isfinite(report.entries[0].force[0]));
}


TEST_CASE("A charged surface cannot be pushed") {

  greeter::service::SimulationService service;

  // A triangle carries charge but encloses no volume, so there is no moment
  // for a force to act on. Better to say so than to report zero.
  service.loadJSON(nlohmann::json::parse(R"({
    "magnets": [
      { "id": 1, "type": "triangle", "parameters": {
          "vertices": [[0,0,0], [0.01,0,0], [0,0.01,0]],
          "magnetization": [0, 0, 1] } },
      { "id": 2, "type": "cuboid", "parameters": {
          "dimensions": [0.01, 0.01, 0.01], "magnetization": [0, 0, 1],
          "position": [0.05, 0, 0], "orientation": [1, 0, 0, 0] } }
    ],
    "force": { "targets": [1] }
  })"), "test");

  CHECK_THROWS_AS(service.simulateForces(), std::invalid_argument);

  // As a source of field it is perfectly usable.
  const greeter::view::SceneSnapshot scene = service.getScene();

  const greeter::view::MagnetView* facet = scene.findById(1);

  REQUIRE(facet != nullptr);
  CHECK(facet->shape.kind == greeter::view::ShapeKind::Mesh);
  CHECK(facet->shape.parameters.size() == 9);
}
