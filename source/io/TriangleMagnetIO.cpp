#include <greeter/TriangleMagnet.h>
#include <greeter/io/TriangleMagnetIO.h>

#include <stdexcept>


greeter::TriangleMagnetIO::TriangleMagnetIO() {}
greeter::TriangleMagnetIO::~TriangleMagnetIO() {}

std::string greeter::TriangleMagnetIO::getTypeName() {
  return "triangle";
}

std::vector<float> greeter::TriangleMagnetIO::readPosition(const nlohmann::json& magnet) {

  const nlohmann::json& parameters = magnet["parameters"];

  if (!parameters.contains("position")) {
    return {0.0f, 0.0f, 0.0f};
  }

  std::vector<float> position = parameters["position"].get<std::vector<float>>();

  if (position.size() != 3) {
    throw std::invalid_argument("A triangle position must have three components");
  }

  return position;
}

std::vector<float> greeter::TriangleMagnetIO::readOrientation(const nlohmann::json& magnet) {

  const nlohmann::json& parameters = magnet["parameters"];

  if (!parameters.contains("orientation")) {
    return {1.0f, 0.0f, 0.0f, 0.0f};
  }

  std::vector<float> orientation = parameters["orientation"].get<std::vector<float>>();

  if (orientation.size() != 4) {
    throw std::invalid_argument(
      "A triangle orientation must be a quaternion [w, x, y, z]");
  }

  return orientation;
}

std::vector<float> greeter::TriangleMagnetIO::readVertices(const nlohmann::json& magnet) {

  const nlohmann::json& parameters = magnet["parameters"];

  if (!parameters.contains("vertices") || !parameters["vertices"].is_array()) {
    throw std::invalid_argument(
      "A triangle is given by its \"vertices\", three points in its local frame");
  }

  const nlohmann::json& given = parameters["vertices"];

  if (given.size() != 3) {
    throw std::invalid_argument(
      "A triangle has three vertices, and this one has " +
      std::to_string(given.size()));
  }

  std::vector<float> vertices;
  vertices.reserve(9);

  for (const auto& corner : given) {

    if (!corner.is_array() || corner.size() != 3) {
      throw std::invalid_argument("Every vertex of a triangle is three numbers");
    }

    for (const auto& value : corner) {
      vertices.push_back(value.get<float>());
    }
  }

  return vertices;
}

std::vector<float> greeter::TriangleMagnetIO::readMagnetization(
    const nlohmann::json& magnet) {

  const nlohmann::json& parameters = magnet["parameters"];

  if (!parameters.contains("magnetization")) {
    throw std::invalid_argument("A triangle needs its magnetization");
  }

  const nlohmann::json& given = parameters["magnetization"];

  if (given.is_number()) {
    return {0.0f, 0.0f, given.get<float>()};
  }

  std::vector<float> magnetization = given.get<std::vector<float>>();

  if (magnetization.size() != 3) {
    throw std::invalid_argument("A triangle magnetization must have three components");
  }

  return magnetization;
}

std::unique_ptr<greeter::Magnet> greeter::TriangleMagnetIO::createMagnet(
    const nlohmann::json& magnet) {

  if (!magnet.contains("parameters") || !magnet["parameters"].is_object()) {
    throw std::invalid_argument("A triangle needs a \"parameters\" object");
  }

  return std::make_unique<greeter::TriangleMagnet>(
    readPosition(magnet), readVertices(magnet),
    readOrientation(magnet), readMagnetization(magnet));
}
