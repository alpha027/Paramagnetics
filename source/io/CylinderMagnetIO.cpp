#include <greeter/CylinderMagnet.h>
#include <greeter/io/CylinderMagnetIO.h>

#include <stdexcept>


greeter::CylinderMagnetIO::CylinderMagnetIO() {}
greeter::CylinderMagnetIO::~CylinderMagnetIO() {}

std::string greeter::CylinderMagnetIO::getTypeName() {
  return "cylinder";
}

std::vector<float> greeter::CylinderMagnetIO::readPosition(const nlohmann::json& magnet) {
  std::vector<float> position = magnet["parameters"]["position"].get<std::vector<float>>();
  if (position.size() != 3) {
    throw std::invalid_argument("A cylinder position must have three components");
  }
  return position;
}

std::vector<float> greeter::CylinderMagnetIO::readDimensions(const nlohmann::json& magnet) {

  const nlohmann::json& parameters = magnet["parameters"];

  if (!parameters.contains("dimensions")) {
    throw std::invalid_argument("A cylinder needs its diameter and its height");
  }

  const nlohmann::json& given = parameters["dimensions"];

  if (!given.is_array()) {
    throw std::invalid_argument(
      "A cylinder is given by two numbers, a diameter and a height");
  }

  std::vector<float> dimensions = given.get<std::vector<float>>();

  if (dimensions.size() != 2) {
    throw std::invalid_argument(
      "A cylinder is given by two numbers, a diameter and a height");
  }

  if (!(dimensions[0] > 0.0f) || !(dimensions[1] > 0.0f)) {
    throw std::invalid_argument(
      "The diameter and the height of a cylinder must be strictly positive");
  }

  return dimensions;
}

std::vector<float> greeter::CylinderMagnetIO::readOrientation(const nlohmann::json& magnet) {
  std::vector<float> orientation = magnet["parameters"]["orientation"].get<std::vector<float>>();
  if (orientation.size() != 4) {
    throw std::invalid_argument("A cylinder orientation must be a quaternion [w, x, y, z]");
  }
  return orientation;
}

std::vector<float> greeter::CylinderMagnetIO::readMagnetization(const nlohmann::json& magnet) {

  const nlohmann::json& given = magnet["parameters"]["magnetization"];

  // A cylinder polarized along its axis is the common case, so a single number
  // is read as that, the way a sphere reads its magnetization.
  if (given.is_number()) {
    return {0.0f, 0.0f, given.get<float>()};
  }

  std::vector<float> magnetization = given.get<std::vector<float>>();

  if (magnetization.size() != 3) {
    throw std::invalid_argument("A cylinder magnetization must have three components");
  }

  return magnetization;
}

std::unique_ptr<greeter::Magnet> greeter::CylinderMagnetIO::createMagnet(
    const nlohmann::json& magnet) {

  std::vector<float> position = readPosition(magnet);
  std::vector<float> dimensions = readDimensions(magnet);
  std::vector<float> orientation = readOrientation(magnet);
  std::vector<float> magnetization = readMagnetization(magnet);

  return std::make_unique<greeter::CylinderMagnet>(
    position, orientation, dimensions, magnetization);
}
