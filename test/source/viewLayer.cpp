#include <doctest/doctest.h>

#include <greeter/MagnetGeometryFactory.h>
#include <greeter/MagneticFieldMethodFactory.h>
#include <greeter/io/MethodFactoryIO.h>
#include <greeter/service/SimulationService.h>
#include <greeter/view/ShapeMesh.h>
#include <greeter/view/SnapshotIO.h>
#include <greeter/io/ForceIO.h>
#include <greeter/io/MagnetIO.h>
#include <greeter/io/SceneIO.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>


namespace {

  /* A cuboid, a sphere, a cylinder and a tetrahedron, one of each. */
  const char* ONE_OF_EACH = R"({
    "magnets": [
      { "id": 1, "type": "cuboid", "parameters": {
          "dimensions": [0.02, 0.03, 0.04], "magnetization": [0, 0, 1],
          "position": [0.1, 0, 0], "orientation": [1, 0, 0, 0] } },
      { "id": 2, "type": "sphere", "parameters": {
          "dimensions": 0.01, "magnetization": 1.0,
          "position": [0, 0.1, 0], "orientation": [1, 0, 0, 0] } },
      { "id": 3, "type": "cylinder", "parameters": {
          "dimensions": [0.02, 0.05], "magnetization": [0, 0, 1],
          "position": [0, 0, 0.1], "orientation": [1, 0, 0, 0] } },
      { "id": 4, "type": "tetrahedron", "parameters": {
          "vertices": [[0, 0, 0], [0.01, 0, 0], [0, 0.01, 0], [0, 0, 0.01]],
          "magnetization": [0, 0, 1],
          "position": [0, 0, 0], "orientation": [1, 0, 0, 0] } }
    ],
    "field_of_view": {
      "x": {"min": -0.02, "max": 0.02, "n": 3},
      "y": {"min": -0.02, "max": 0.02, "n": 3},
      "z": {"min": -0.02, "max": 0.02, "n": 3}
    }
  })";

  greeter::service::SimulationService serviceFor(const char* text) {
    greeter::service::SimulationService service;
    service.loadJSON(nlohmann::json::parse(text), "test");
    return service;
  }

  float lengthOf(const float* vector) {
    return std::sqrt(vector[0] * vector[0] + vector[1] * vector[1] +
                     vector[2] * vector[2]);
  }

}  // namespace


TEST_CASE("Every magnet type that has a field can also be drawn") {

  /*
    The point of the registry: a magnet type added to the field kernels and
    forgotten here would be a magnet the viewer draws as nothing. Walking the
    kernels is what turns that from a thing to remember into a failing test.

    Walking the type ids rather than the reader names, because the two are not
    in step: a reader may be an alias that builds a magnet of some other type.
    "cylinder_segment" is one, and produces a triangular mesh. What has to be
    drawable is whatever can produce a field.
  */
  const greeter::MagnetGeometryFactory& shapes =
    greeter::MagnetGeometryFactory::getInstance();

  const std::vector<uint16_t> types =
    greeter::MagneticFieldMethodFactory::getInstance().getRegisteredTypes();

  CHECK(types.size() >= 7);

  for (const auto& type : types) {
    INFO("magnet type id: " << type);
    CHECK(shapes.isRegistered(type));
    CHECK_FALSE(shapes.getTypeName(type).empty());
  }

  // And every reader builds something, whether or not it is a type of its own.
  for (const auto& name : greeter::MethodFactoryIO::getInstance().getRegisteredTypes()) {
    INFO("reader: " << name.c_str());
    CHECK(greeter::MethodFactoryIO::getInstance().isRegistered(name));
  }
}


TEST_CASE("A magnet describes its own shape") {

  const greeter::service::SimulationService service = serviceFor(ONE_OF_EACH);

  const greeter::view::SceneSnapshot scene = service.getScene();

  REQUIRE(scene.magnets.size() == 4);

  const greeter::view::MagnetView* cuboid = scene.findById(1);
  REQUIRE(cuboid != nullptr);
  CHECK(cuboid->shape.kind == greeter::view::ShapeKind::Box);
  CHECK(cuboid->shape.type_name == "cuboid");
  REQUIRE(cuboid->shape.parameters.size() == 3);
  CHECK(cuboid->shape.parameters[0] == doctest::Approx(0.02f));
  CHECK(cuboid->shape.parameters[2] == doctest::Approx(0.04f));

  // The input file and the class both work in the radius of a sphere, a
  // descriptor always gives the extent across the shape.
  const greeter::view::MagnetView* sphere = scene.findById(2);
  REQUIRE(sphere != nullptr);
  CHECK(sphere->shape.kind == greeter::view::ShapeKind::Sphere);
  REQUIRE(sphere->shape.parameters.size() == 1);
  CHECK(sphere->shape.parameters[0] == doctest::Approx(0.02f));

  const greeter::view::MagnetView* cylinder = scene.findById(3);
  REQUIRE(cylinder != nullptr);
  CHECK(cylinder->shape.kind == greeter::view::ShapeKind::Cylinder);
  REQUIRE(cylinder->shape.parameters.size() == 2);
  CHECK(cylinder->shape.parameters[0] == doctest::Approx(0.02f));
  CHECK(cylinder->shape.parameters[1] == doctest::Approx(0.05f));

  const greeter::view::MagnetView* tetrahedron = scene.findById(4);
  REQUIRE(tetrahedron != nullptr);
  CHECK(tetrahedron->shape.kind == greeter::view::ShapeKind::Tetrahedron);
  CHECK(tetrahedron->shape.parameters.size() == 12);

  // The polarization is the last three parameters whatever the shape put in
  // front of it, which is what makes reading it shape-blind.
  CHECK(cuboid->magnetization[2] == doctest::Approx(1.0f));
  CHECK(sphere->magnetization[2] == doctest::Approx(1.0f));
}


TEST_CASE("A shape nobody has described is still placed") {

  const greeter::MagnetGeometryFactory& shapes =
    greeter::MagnetGeometryFactory::getInstance();

  const float parameters[13] = {0};

  CHECK_FALSE(shapes.isRegistered(60000));

  // A scene holding a magnet the viewer was never taught about still opens,
  // and the magnet still has a place in it.
  const greeter::view::ShapeDescriptor unknown =
    shapes.describeShape(60000, parameters);

  CHECK(unknown.kind == greeter::view::ShapeKind::Unknown);
  CHECK_FALSE(unknown.isValid());
  CHECK(greeter::view::buildMesh(unknown).empty());
}


TEST_CASE("A descriptor turns into triangles that face outwards") {

  struct Case {
    greeter::view::ShapeKind kind;
    std::vector<float> parameters;
  };

  const std::vector<Case> cases = {
    {greeter::view::ShapeKind::Box, {2.0f, 4.0f, 6.0f}},
    {greeter::view::ShapeKind::Cylinder, {2.0f, 5.0f}},
    {greeter::view::ShapeKind::Sphere, {3.0f}}
  };

  for (const auto& item : cases) {

    greeter::view::ShapeDescriptor shape;
    shape.kind = item.kind;
    shape.parameters = item.parameters;

    REQUIRE(shape.isValid());

    const greeter::view::ShapeMesh mesh = greeter::view::buildMesh(shape, 16);

    INFO("shape: " << greeter::view::getName(item.kind).c_str());

    REQUIRE_FALSE(mesh.empty());
    CHECK(mesh.normals.size() == mesh.vertices.size());
    CHECK(mesh.triangles.size() % 3 == 0);

    for (const auto& index : mesh.triangles) {
      CHECK(index < mesh.getVertexCount());
    }

    for (size_t i = 0; i < mesh.getVertexCount(); i++) {

      const float* normal = &mesh.normals[3 * i];
      const float* vertex = &mesh.vertices[3 * i];

      CHECK(lengthOf(normal) == doctest::Approx(1.0f).epsilon(0.001));

      // These shapes are convex and sit around their own origin, so the
      // outward normal never points back towards the middle.
      const float outward = normal[0] * vertex[0] + normal[1] * vertex[1] +
                            normal[2] * vertex[2];

      CHECK(outward > -1e-5f);
    }
  }
}


TEST_CASE("A tetrahedron is wound outwards whichever way its vertices were given") {

  // The four points of a tetrahedron may be given in either handedness, and
  // the normals have to come out pointing away from the body regardless.
  const std::vector<std::vector<float>> orderings = {
    {0, 0, 0,  1, 0, 0,  0, 1, 0,  0, 0, 1},
    {0, 0, 0,  0, 1, 0,  1, 0, 0,  0, 0, 1}
  };

  for (const auto& vertices : orderings) {

    greeter::view::ShapeDescriptor shape;
    shape.kind = greeter::view::ShapeKind::Tetrahedron;
    shape.parameters = vertices;

    const greeter::view::ShapeMesh mesh = greeter::view::buildMesh(shape);

    REQUIRE(mesh.getTriangleCount() == 4);

    const float centroid[3] = {0.25f, 0.25f, 0.25f};

    for (size_t i = 0; i < mesh.getVertexCount(); i++) {

      const float* normal = &mesh.normals[3 * i];
      const float* vertex = &mesh.vertices[3 * i];

      const float outward =
        normal[0] * (vertex[0] - centroid[0]) +
        normal[1] * (vertex[1] - centroid[1]) +
        normal[2] * (vertex[2] - centroid[2]);

      CHECK(outward > 0.0f);
    }
  }
}


TEST_CASE("A shape of no known kind still draws, as triangles it carries") {

  // The escape hatch: a future shape that is none of the named ones says so
  // in triangles and needs no change anywhere in the viewer.
  greeter::view::ShapeDescriptor shape;
  shape.kind = greeter::view::ShapeKind::Mesh;
  shape.parameters = {0, 0, 0,  1, 0, 0,  0, 1, 0};

  CHECK(shape.isValid());

  const greeter::view::ShapeMesh mesh = greeter::view::buildMesh(shape);

  CHECK(mesh.getTriangleCount() == 1);

  // Half a triangle is not a triangle.
  shape.parameters.pop_back();
  CHECK_FALSE(shape.isValid());
}


TEST_CASE("A scene knows which arrangement generated which magnet") {

  const greeter::service::SimulationService service = serviceFor(R"({
    "magnets": [
      { "id": 1, "type": "cuboid", "parameters": {
          "dimensions": [0.01, 0.01, 0.01], "magnetization": [0, 0, 1],
          "position": [0, 0, 0], "orientation": [1, 0, 0, 0] } }
    ],
    "arrangements": [
      { "id": 100, "type": "linear_array", "parameters": {
          "count": 3, "spacing": 0.02,
          "element": { "type": "cuboid", "parameters": {
            "dimensions": [0.01, 0.01, 0.01], "magnetization": [0, 0, 1] } } } }
    ],
    "field_of_view": {
      "x": {"min": 0, "max": 0, "n": 1},
      "y": {"min": 0, "max": 0, "n": 1},
      "z": {"min": 0.05, "max": 0.05, "n": 1}
    }
  })");

  const greeter::view::SceneSnapshot scene = service.getScene();

  REQUIRE(scene.magnets.size() == 4);
  REQUIRE(scene.arrangements.size() == 1);

  CHECK(scene.arrangements[0].id == 100);
  CHECK(scene.arrangements[0].type == "linear_array");
  CHECK(scene.arrangements[0].members.size() == 3);

  // The magnet listed on its own belongs to no arrangement, the three the
  // arrangement generated name it.
  CHECK(scene.magnets[0].arrangement_id == 0);
  CHECK(scene.magnets[1].arrangement_id == 100);
  CHECK(scene.magnets[3].arrangement_id == 100);

  // Generated magnets are numbered on from the highest magnet id, which the
  // id of the arrangement itself has no part in: the listed magnet is 1, so
  // the three the arrangement generated are 2, 3 and 4.
  CHECK(scene.magnets[0].id == 1);
  CHECK(scene.magnets[1].id == 2);
  CHECK(scene.magnets[3].id == 4);

  const greeter::view::ArrangementView* found = scene.findArrangementById(100);
  REQUIRE(found != nullptr);
  CHECK(found->members[0] == 1);
}


TEST_CASE("A scene says how big it is, so a camera can frame it") {

  const greeter::service::SimulationService service = serviceFor(ONE_OF_EACH);

  const greeter::view::SceneSnapshot scene = service.getScene();

  float minimum[3];
  float maximum[3];

  REQUIRE(scene.getBounds(minimum, maximum));

  // The cuboid sits at x = 0.1 and is 0.02 across, so the box reaches 0.11.
  CHECK(maximum[0] == doctest::Approx(0.11f));

  // The sphere sits at y = 0.1 and is given by a radius of 0.01, so it too
  // reaches 0.11: the descriptor has already turned that radius into the
  // diameter the extent is read off.
  CHECK(maximum[1] == doctest::Approx(0.11f));

  const greeter::view::SceneSnapshot nothing;
  CHECK_FALSE(nothing.getBounds(minimum, maximum));
}


TEST_CASE("A simulated field remembers where it was sampled") {

  const greeter::service::SimulationService service = serviceFor(ONE_OF_EACH);

  greeter::service::FieldRequest request;

  REQUIRE(service.getFieldRequest(request));

  CHECK(request.counts[0] == 3);
  CHECK(request.getSampleCount() == 27);

  const greeter::view::FieldGrid field = service.simulateField(request);

  REQUIRE(field.size() == 27);
  CHECK(field.grid);

  // This is the gap the whole layer exists to close: three numbers per sample
  // and nothing saying where any of them was measured.
  float point[3];
  field.getPoint(field.getIndex(0, 0, 0), point);

  CHECK(point[0] == doctest::Approx(-0.02f));
  CHECK(point[2] == doctest::Approx(-0.02f));

  field.getPoint(field.getIndex(2, 2, 2), point);
  CHECK(point[0] == doctest::Approx(0.02f));

  // x slowest and z fastest, matching FieldOfView.
  CHECK(field.getIndex(0, 0, 1) == 1);
  CHECK(field.getIndex(1, 0, 0) == 9);
}


TEST_CASE("A chunked field is the field") {

  // The viewer runs the samples in chunks so that it can report progress and
  // be stopped. That must not change a single number.
  const greeter::service::SimulationService service = serviceFor(ONE_OF_EACH);

  greeter::service::FieldRequest request;
  REQUIRE(service.getFieldRequest(request));

  greeter::service::RunOptions whole;
  whole.chunk = 1000000;

  greeter::service::RunOptions tiny;
  tiny.chunk = 4;

  const greeter::view::FieldGrid one = service.simulateField(request, nullptr, whole);
  const greeter::view::FieldGrid many = service.simulateField(request, nullptr, tiny);

  REQUIRE(one.size() == many.size());

  for (size_t i = 0; i < one.field.size(); i++) {
    CHECK(one.field[i] == doctest::Approx(many.field[i]));
  }
}


namespace {

  /* Stops the run after it has been told about `after` samples. */
  class StopAfter: public greeter::service::ProgressSink {

    public:

      explicit StopAfter(const size_t& after): stop_after(after) {}

      bool onProgress(const size_t& done, const size_t& total) override {
        calls++;
        last_done = done;
        last_total = total;
        return done < stop_after;
      }

      size_t stop_after;
      size_t calls = 0;
      size_t last_done = 0;
      size_t last_total = 0;
  };

}  // namespace


TEST_CASE("A field run reports progress and can be stopped") {

  const greeter::service::SimulationService service = serviceFor(ONE_OF_EACH);

  greeter::service::FieldRequest request;
  REQUIRE(service.getFieldRequest(request));

  greeter::service::RunOptions options;
  options.chunk = 4;

  StopAfter watcher(8);

  const greeter::view::FieldGrid field =
    service.simulateField(request, &watcher, options);

  CHECK(watcher.calls > 0);
  CHECK(watcher.last_total == 27);

  // A run that was stopped hands back nothing, so that half a field is never
  // mistaken for a whole one.
  CHECK(field.empty());

  StopAfter never(1000);

  const greeter::view::FieldGrid whole =
    service.simulateField(request, &never, options);

  CHECK(whole.size() == 27);
  CHECK(never.last_done == 27);
}


TEST_CASE("A field interpolates between its samples") {

  greeter::view::FieldGrid field;

  field.grid = true;
  field.bounds[0] = 0.0f; field.bounds[1] = 1.0f;
  field.bounds[2] = 0.0f; field.bounds[3] = 1.0f;
  field.bounds[4] = 0.0f; field.bounds[5] = 1.0f;
  field.counts[0] = 2; field.counts[1] = 2; field.counts[2] = 2;

  // Bx rises linearly with x, so interpolating it has an answer to check.
  field.field.assign(8 * 3, 0.0f);

  for (uint32_t i = 0; i < 2; i++) {
    for (uint32_t j = 0; j < 2; j++) {
      for (uint32_t k = 0; k < 2; k++) {
        field.field[3 * field.getIndex(i, j, k)] = (float) i;
      }
    }
  }

  float b[3];

  const float middle[3] = {0.5f, 0.5f, 0.5f};
  REQUIRE(field.sample(middle, b));
  CHECK(b[0] == doctest::Approx(0.5f));

  const float quarter[3] = {0.25f, 0.9f, 0.1f};
  REQUIRE(field.sample(quarter, b));
  CHECK(b[0] == doctest::Approx(0.25f));

  // The far face belongs to the last cell rather than to nothing.
  const float corner[3] = {1.0f, 1.0f, 1.0f};
  REQUIRE(field.sample(corner, b));
  CHECK(b[0] == doctest::Approx(1.0f));

  const float outside[3] = {1.5f, 0.5f, 0.5f};
  CHECK_FALSE(field.sample(outside, b));
}


TEST_CASE("A plane interpolates across the axis it was not sampled along") {

  // A slice is one sample thick, and a streamline drawn on it still has to
  // find a value everywhere on it.
  greeter::view::FieldGrid field;

  field.grid = true;
  field.bounds[0] = 0.0f; field.bounds[1] = 1.0f;
  field.bounds[2] = 0.0f; field.bounds[3] = 1.0f;
  field.bounds[4] = 0.0f; field.bounds[5] = 0.0f;
  field.counts[0] = 2; field.counts[1] = 2; field.counts[2] = 1;

  field.field.assign(4 * 3, 0.0f);

  for (uint32_t i = 0; i < 2; i++) {
    for (uint32_t j = 0; j < 2; j++) {
      field.field[3 * field.getIndex(i, j, 0) + 1] = (float) j;
    }
  }

  float b[3];

  const float point[3] = {0.5f, 0.75f, 0.0f};

  REQUIRE(field.sample(point, b));
  CHECK(b[1] == doctest::Approx(0.75f));
}


TEST_CASE("A field reports the range its colours have to cover") {

  const greeter::service::SimulationService service = serviceFor(ONE_OF_EACH);

  greeter::service::FieldRequest request;
  REQUIRE(service.getFieldRequest(request));

  const greeter::view::FieldGrid field = service.simulateField(request);

  float minimum = 0.0f;
  float maximum = 0.0f;

  REQUIRE(field.getMagnitudeRange(minimum, maximum));

  CHECK(minimum >= 0.0f);
  CHECK(maximum >= minimum);

  // A quantile is what keeps a handful of samples pressed against a magnet
  // from taking the whole scale.
  const float median = field.getMagnitudeQuantile(0.5f);

  CHECK(median >= minimum);
  CHECK(median <= maximum);
  CHECK(field.getMagnitudeQuantile(1.0f) == doctest::Approx(maximum));

  // Out of range fractions are clamped rather than read off the end.
  CHECK(field.getMagnitudeQuantile(-1.0f) == doctest::Approx(minimum));
  CHECK(field.getMagnitudeQuantile(5.0f) == doctest::Approx(maximum));
}


TEST_CASE("A snapshot survives a trip through JSON") {

  const greeter::service::SimulationService service = serviceFor(ONE_OF_EACH);

  const greeter::view::Snapshot snapshot = service.run();

  REQUIRE(snapshot.hasField());

  const greeter::view::Snapshot read =
    greeter::view::SnapshotIO::fromJSON(greeter::view::SnapshotIO::toJSON(snapshot));

  REQUIRE(read.scene.magnets.size() == snapshot.scene.magnets.size());

  for (size_t i = 0; i < read.scene.magnets.size(); i++) {

    const greeter::view::MagnetView& before = snapshot.scene.magnets[i];
    const greeter::view::MagnetView& after = read.scene.magnets[i];

    CHECK(after.id == before.id);
    CHECK(after.shape.kind == before.shape.kind);
    CHECK(after.shape.type_name == before.shape.type_name);
    CHECK(after.position[0] == doctest::Approx(before.position[0]));
    CHECK(after.magnetization[2] == doctest::Approx(before.magnetization[2]));
  }

  REQUIRE(read.field.size() == snapshot.field.size());
  CHECK(read.field.counts[0] == snapshot.field.counts[0]);

  for (size_t i = 0; i < read.field.field.size(); i++) {
    CHECK(read.field.field[i] == doctest::Approx(snapshot.field.field[i]));
  }
}


TEST_CASE("A snapshot survives a trip through the binary form") {

  const greeter::service::SimulationService service = serviceFor(ONE_OF_EACH);

  const greeter::view::Snapshot snapshot = service.run();

  std::stringstream stream(std::ios::in | std::ios::out | std::ios::binary);

  greeter::view::SnapshotIO::writeBinary(snapshot, stream);

  const greeter::view::Snapshot read =
    greeter::view::SnapshotIO::readBinary(stream);

  REQUIRE(read.scene.magnets.size() == snapshot.scene.magnets.size());
  REQUIRE(read.field.size() == snapshot.field.size());

  CHECK(read.scene.magnets[2].shape.kind == greeter::view::ShapeKind::Cylinder);
  CHECK(read.field.grid == snapshot.field.grid);

  // The samples themselves go through as raw floats, so they come back
  // exactly rather than nearly.
  for (size_t i = 0; i < read.field.field.size(); i++) {
    CHECK(read.field.field[i] == snapshot.field.field[i]);
  }
}


TEST_CASE("Something that is not a snapshot is not read as one") {

  std::stringstream stream(std::ios::in | std::ios::out | std::ios::binary);

  stream << "this is not a snapshot, it is a sentence";

  CHECK_THROWS_AS(greeter::view::SnapshotIO::readBinary(stream),
                  std::invalid_argument);

  CHECK_THROWS_AS(greeter::view::SnapshotIO::fromJSON(nlohmann::json::object()),
                  std::invalid_argument);

  CHECK(greeter::view::SnapshotIO::isJSONPath("run.json"));
  CHECK(greeter::view::SnapshotIO::isJSONPath("RUN.JSON"));
  CHECK_FALSE(greeter::view::SnapshotIO::isJSONPath("run.pmsnap"));
}


TEST_CASE("A snapshot written to a file opens again") {

  const greeter::service::SimulationService service = serviceFor(ONE_OF_EACH);

  const greeter::view::Snapshot snapshot = service.run();

  const std::string path = "test_snapshot.pmsnap";

  greeter::view::SnapshotIO::write(snapshot, path);

  const greeter::view::Snapshot read = greeter::view::SnapshotIO::read(path);

  CHECK(read.scene.magnets.size() == snapshot.scene.magnets.size());
  CHECK(read.field.size() == snapshot.field.size());
  CHECK(read.scene.source == snapshot.scene.source);

  std::remove(path.c_str());

  CHECK_THROWS_AS(greeter::view::SnapshotIO::read("no_such_snapshot.pmsnap"),
                  std::invalid_argument);
}


TEST_CASE("Forces come back named by magnet id") {

  greeter::service::SimulationService service;

  service.loadJSON(nlohmann::json::parse(R"({
    "magnets": [
      { "id": 7, "type": "cuboid", "parameters": {
          "dimensions": [0.01, 0.01, 0.01], "magnetization": [0, 0, 1],
          "position": [0, 0, 0], "orientation": [1, 0, 0, 0] } },
      { "id": 9, "type": "cuboid", "parameters": {
          "dimensions": [0.01, 0.01, 0.01], "magnetization": [0, 0, 1],
          "position": [0, 0, 0.02], "orientation": [1, 0, 0, 0] } }
    ],
    "force": { "targets": "all", "meshing": 8 }
  })"), "test");

  CHECK(service.hasForceSection());
  CHECK_FALSE(service.hasFieldSection());

  const greeter::view::ForceReport report = service.simulateForces();

  REQUIRE(report.entries.size() == 2);

  const greeter::view::ForceEntry* seven = report.findById(7);
  const greeter::view::ForceEntry* nine = report.findById(9);

  REQUIRE(seven != nullptr);
  REQUIRE(nine != nullptr);

  // Two magnets polarized the same way and stacked along z attract, so the
  // lower one is pulled up and the upper one down, equally.
  CHECK(seven->force[2] > 0.0f);
  CHECK(nine->force[2] < 0.0f);
  CHECK(seven->force[2] == doctest::Approx(-nine->force[2]).epsilon(0.001));

  // The mesh the numbers rest on, and the point the torque turns about, are
  // carried out with them.
  CHECK(seven->cells == 8);
  CHECK(nine->pivot[2] == doctest::Approx(0.02f));
}


TEST_CASE("A service without a scene says so rather than guessing") {

  greeter::service::SimulationService service;

  CHECK_FALSE(service.isLoaded());
  CHECK_FALSE(service.hasFieldSection());
  CHECK_FALSE(service.hasForceSection());

  CHECK_THROWS_AS(service.getScene(), std::logic_error);
  CHECK_THROWS_AS(service.getInput(), std::logic_error);

  CHECK_THROWS_AS(service.loadFile("no_such_file.json"), std::invalid_argument);

  // A file that failed to load leaves the service as it was rather than half
  // loaded.
  CHECK_FALSE(service.isLoaded());

  service.loadJSON(nlohmann::json::parse(ONE_OF_EACH), "test");

  CHECK(service.isLoaded());
  CHECK(service.getSource() == "test");

  CHECK_THROWS_AS(service.simulateForces(), std::invalid_argument);
}


TEST_CASE("A field of view that cannot be sampled is refused") {

  const greeter::service::SimulationService service = serviceFor(ONE_OF_EACH);

  greeter::service::FieldRequest request;

  request.counts[0] = 0;
  CHECK_THROWS_AS(service.simulateField(request), std::invalid_argument);

  request.counts[0] = 2;
  request.bounds[0] = 1.0f;
  request.bounds[1] = 0.0f;
  CHECK_THROWS_AS(service.simulateField(request), std::invalid_argument);
}


TEST_CASE("The service computes what the collection computes") {

  // The viewer runs the simulation through a facade instead of calling the
  // collection, and the facade cuts a field into chunks. Neither is allowed
  // to change a number. Both are run here in one process, so the comparison
  // is not confused by a parallel sum landing in a different order on a
  // different number of threads.
  const nlohmann::json data = nlohmann::json::parse(R"({
    "magnets": [
      { "id": 1, "type": "cuboid", "parameters": {
          "dimensions": [0.02, 0.02, 0.02], "magnetization": [0, 0, 1],
          "position": [0, 0, 0], "orientation": [1, 0, 0, 0] } },
      { "id": 2, "type": "cylinder", "parameters": {
          "dimensions": [0.02, 0.03], "magnetization": [0, 1, 0],
          "position": [0.03, 0, 0], "orientation": [1, 0, 0, 0] } },
      { "id": 3, "type": "sphere", "parameters": {
          "dimensions": 0.01, "magnetization": 1.0,
          "position": [0, 0.04, 0.01], "orientation": [1, 0, 0, 0] } }
    ],
    "field_of_view": {
      "x": {"min": -0.05, "max": 0.05, "n": 7},
      "y": {"min": -0.05, "max": 0.05, "n": 5},
      "z": {"min": -0.05, "max": 0.05, "n": 3}
    },
    "force": { "targets": "all", "meshing": 27 }
  })");

  greeter::service::SimulationService service;
  service.loadJSON(data, "test");

  const greeter::Scene scene = greeter::SceneIO::read(data);

  SUBCASE("the field") {

    const greeter::FieldOfView fov =
      greeter::MagnetIO::readFieldOfView(data["field_of_view"]);

    const std::vector<std::vector<float>> direct = scene.collection.simulate(fov);

    greeter::service::FieldRequest request;
    REQUIRE(service.getFieldRequest(request));

    greeter::service::RunOptions options;
    options.chunk = 7;  // deliberately not a divisor of 105

    const greeter::view::FieldGrid through =
      service.simulateField(request, nullptr, options);

    REQUIRE(through.size() == direct.size());

    for (size_t i = 0; i < direct.size(); i++) {
      for (size_t axis = 0; axis < 3; axis++) {
        CHECK(through.field[3 * i + axis] == direct[i][axis]);
      }
    }
  }

  SUBCASE("the forces") {

    const greeter::ForceConfig config =
      greeter::ForceIO::read(data, scene.magnet_ids, scene.arrangements);

    const std::vector<greeter::ForceResult> direct =
      scene.collection.computeForces(config, false);

    const greeter::view::ForceReport through = service.simulateForces();

    REQUIRE(through.entries.size() == direct.size());

    for (size_t i = 0; i < direct.size(); i++) {
      for (size_t axis = 0; axis < 3; axis++) {
        CHECK(through.entries[i].force[axis] == direct[i].force[axis]);
        CHECK(through.entries[i].torque[axis] == direct[i].torque[axis]);
      }
    }
  }
}
