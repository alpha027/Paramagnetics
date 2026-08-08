#include <greeter/TriangularMeshMagnet.h>
#include <greeter/io/TriangularMeshMagnetIO.h>

#include <array>
#include <stdexcept>


greeter::TriangularMeshMagnetIO::TriangularMeshMagnetIO() {}
greeter::TriangularMeshMagnetIO::~TriangularMeshMagnetIO() {}

std::string greeter::TriangularMeshMagnetIO::getTypeName() {
  return "triangular_mesh";
}

std::vector<float> greeter::TriangularMeshMagnetIO::readPosition(
    const nlohmann::json& magnet) {

  const nlohmann::json& parameters = magnet["parameters"];

  if (!parameters.contains("position")) {
    return {0.0f, 0.0f, 0.0f};
  }

  std::vector<float> position = parameters["position"].get<std::vector<float>>();

  if (position.size() != 3) {
    throw std::invalid_argument(
      "A triangular mesh position must have three components");
  }

  return position;
}

std::vector<float> greeter::TriangularMeshMagnetIO::readOrientation(
    const nlohmann::json& magnet) {

  const nlohmann::json& parameters = magnet["parameters"];

  if (!parameters.contains("orientation")) {
    return {1.0f, 0.0f, 0.0f, 0.0f};
  }

  std::vector<float> orientation = parameters["orientation"].get<std::vector<float>>();

  if (orientation.size() != 4) {
    throw std::invalid_argument(
      "A triangular mesh orientation must be a quaternion [w, x, y, z]");
  }

  return orientation;
}

std::vector<float> greeter::TriangularMeshMagnetIO::readTriangles(
    const nlohmann::json& magnet) {

  const nlohmann::json& parameters = magnet["parameters"];

  std::vector<float> triangles;

  // Written out as triangles, which is easier to read for a small shape.
  if (parameters.contains("triangles")) {

    const nlohmann::json& given = parameters["triangles"];

    if (!given.is_array() || given.empty()) {
      throw std::invalid_argument(
        "The \"triangles\" of a mesh are a list of faces, three points each");
    }

    for (const auto& face : given) {

      if (!face.is_array() || face.size() != 3) {
        throw std::invalid_argument("Every face of a mesh has three corners");
      }

      for (const auto& corner : face) {

        if (!corner.is_array() || corner.size() != 3) {
          throw std::invalid_argument("Every corner of a face is three numbers");
        }

        for (const auto& value : corner) {
          triangles.push_back(value.get<float>());
        }
      }
    }

    return triangles;
  }

  // Vertices and faces, the way a mesh comes out of anything that makes one.
  if (!parameters.contains("vertices") || !parameters["vertices"].is_array()) {
    throw std::invalid_argument(
      "A triangular mesh is given by its \"vertices\" and \"faces\", or by its "
      "\"triangles\"");
  }

  if (!parameters.contains("faces") || !parameters["faces"].is_array()) {
    throw std::invalid_argument(
      "A triangular mesh needs \"faces\", each three indices into \"vertices\"");
  }

  std::vector<std::array<float, 3>> vertices;

  for (const auto& vertex : parameters["vertices"]) {

    if (!vertex.is_array() || vertex.size() != 3) {
      throw std::invalid_argument("Every vertex of a mesh is three numbers");
    }

    vertices.push_back({vertex[0].get<float>(), vertex[1].get<float>(),
                        vertex[2].get<float>()});
  }

  if (vertices.size() < 4) {
    throw std::invalid_argument(
      "A closed surface takes at least four vertices, and this one has " +
      std::to_string(vertices.size()));
  }

  const nlohmann::json& faces = parameters["faces"];

  if (faces.empty()) {
    throw std::invalid_argument("A triangular mesh needs at least one face");
  }

  for (const auto& face : faces) {

    if (!face.is_array() || face.size() != 3) {
      throw std::invalid_argument("Every face of a mesh is three vertex indices");
    }

    for (const auto& index : face) {

      if (!index.is_number_integer() || index.get<int64_t>() < 0 ||
          (size_t) index.get<int64_t>() >= vertices.size()) {
        throw std::invalid_argument(
          "A face of a triangular mesh names a vertex that does not exist");
      }

      const std::array<float, 3>& vertex = vertices[index.get<size_t>()];

      triangles.insert(triangles.end(), vertex.begin(), vertex.end());
    }
  }

  return triangles;
}

std::vector<float> greeter::TriangularMeshMagnetIO::readMagnetization(
    const nlohmann::json& magnet) {

  const nlohmann::json& parameters = magnet["parameters"];

  if (!parameters.contains("magnetization")) {
    throw std::invalid_argument("A triangular mesh needs its magnetization");
  }

  const nlohmann::json& given = parameters["magnetization"];

  if (given.is_number()) {
    return {0.0f, 0.0f, given.get<float>()};
  }

  std::vector<float> magnetization = given.get<std::vector<float>>();

  if (magnetization.size() != 3) {
    throw std::invalid_argument(
      "A triangular mesh magnetization must have three components");
  }

  return magnetization;
}

std::unique_ptr<greeter::Magnet> greeter::TriangularMeshMagnetIO::createMagnet(
    const nlohmann::json& magnet) {

  if (!magnet.contains("parameters") || !magnet["parameters"].is_object()) {
    throw std::invalid_argument("A triangular mesh needs a \"parameters\" object");
  }

  return std::make_unique<greeter::TriangularMeshMagnet>(
    readPosition(magnet), readTriangles(magnet),
    readOrientation(magnet), readMagnetization(magnet));
}
