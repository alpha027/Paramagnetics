#include <greeter/Quaternion.h>
#include <greeter/TetrahedronMagnet.h>
#include <greeter/io/TetrahedronMagnetIO.h>

#include <cmath>
#include <stdexcept>


greeter::TetrahedronMagnetIO::TetrahedronMagnetIO() {}
greeter::TetrahedronMagnetIO::~TetrahedronMagnetIO() {}

std::string greeter::TetrahedronMagnetIO::getTypeName() {
  return "tetrahedron";
}

std::vector<float> greeter::TetrahedronMagnetIO::readPosition(const nlohmann::json& magnet) {
  std::vector<float> position = magnet["parameters"]["position"].get<std::vector<float>>();
  if (position.size() != 3) {
    throw std::invalid_argument("A tetrahedron position must have three components");
  }
  return position;
}

std::vector<float> greeter::TetrahedronMagnetIO::readVertices(const nlohmann::json& magnet) {

  // The vertices are the geometry of a tetrahedron, so they are accepted under
  // the "dimensions" key of the other magnets as well.
  const nlohmann::json& parameters = magnet["parameters"];

  if (!parameters.contains("vertices") && !parameters.contains("dimensions")) {
    throw std::invalid_argument("A tetrahedron needs its four vertices");
  }

  const nlohmann::json& given =
    parameters.contains("vertices") ? parameters["vertices"] : parameters["dimensions"];

  std::vector<float> vertices;

  if (given.is_array() && !given.empty() && given[0].is_array()) {
    // A list of points, [[x, y, z], ...]
    for (const auto& vertex : given) {
      if (!vertex.is_array() || vertex.size() != 3) {
        throw std::invalid_argument("A tetrahedron vertex must have three components");
      }
      for (const auto& coordinate : vertex) {
        vertices.push_back(coordinate.get<float>());
      }
    }
  } else {
    // A flat list of twelve numbers
    vertices = given.get<std::vector<float>>();
  }

  if (vertices.size() != 12) {
    throw std::invalid_argument(
      "A tetrahedron is given by four vertices, either as four points or as "
      "twelve numbers");
  }

  if (!(greeter::TetrahedronMagnet::volumeOfTetrahedron(vertices.data()) > 0.0f)) {
    throw std::invalid_argument(
      "The four vertices of a tetrahedron must not be coplanar");
  }

  return vertices;
}

std::vector<float> greeter::TetrahedronMagnetIO::readOrientation(const nlohmann::json& magnet) {
  std::vector<float> orientation = magnet["parameters"]["orientation"].get<std::vector<float>>();
  if (orientation.size() != 4) {
    throw std::invalid_argument("A tetrahedron orientation must be a quaternion [w, x, y, z]");
  }
  return orientation;
}

std::vector<float> greeter::TetrahedronMagnetIO::readMagnetization(const nlohmann::json& magnet) {
  std::vector<float> magnetization =
    magnet["parameters"]["magnetization"].get<std::vector<float>>();
  if (magnetization.size() != 3) {
    throw std::invalid_argument("A tetrahedron magnetization must have three components");
  }
  return magnetization;
}

std::unique_ptr<greeter::Magnet> greeter::TetrahedronMagnetIO::createMagnet(
    const nlohmann::json& magnet) {

  std::vector<float> position = readPosition(magnet);
  std::vector<float> vertices = readVertices(magnet);
  std::vector<float> orientation = readOrientation(magnet);
  std::vector<float> magnetization = readMagnetization(magnet);

  return std::make_unique<greeter::TetrahedronMagnet>(
    position, vertices, orientation, magnetization);
}
