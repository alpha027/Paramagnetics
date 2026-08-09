#include <greeter/Quaternion.h>
#include <greeter/SphericalMagnet.h>
#include <greeter/io/SphericalMagnetIO.h>

#include <cmath>
#include <stdexcept>


greeter::SphericalMagnetIO::SphericalMagnetIO() {}
greeter::SphericalMagnetIO::~SphericalMagnetIO() {}

std::string greeter::SphericalMagnetIO::getTypeName() {
  return "sphere";
}

std::vector<float> greeter::SphericalMagnetIO::readPosition(const nlohmann::json& magnet) {
  std::vector<float> position = magnet["parameters"]["position"].get<std::vector<float>>();
  if (position.size() != 3) {
    throw std::invalid_argument("A sphere position must have three components");
  }
  return position;
}

std::vector<float> greeter::SphericalMagnetIO::readDimensions(const nlohmann::json& magnet) {

  const nlohmann::json& dimensions = magnet["parameters"]["dimensions"];

  // A sphere is described by its radius alone, which may be given either as a
  // plain number or as a single element array for consistency with the cuboid.
  std::vector<float> radius;

  if (dimensions.is_number()) {
    radius.push_back(dimensions.get<float>());
  } else {
    radius = dimensions.get<std::vector<float>>();
  }

  if (radius.size() != 1) {
    throw std::invalid_argument(
      "A sphere has a single dimension, its radius");
  }

  if (!(radius[0] > 0.0f)) {
    throw std::invalid_argument("A sphere radius must be strictly positive");
  }

  return radius;
}

std::vector<float> greeter::SphericalMagnetIO::readOrientation(const nlohmann::json& magnet) {
  std::vector<float> orientation = magnet["parameters"]["orientation"].get<std::vector<float>>();
  if (orientation.size() != 4) {
    throw std::invalid_argument("A sphere orientation must be a quaternion [w, x, y, z]");
  }
  return orientation;
}

std::vector<float> greeter::SphericalMagnetIO::readMagnetization(const nlohmann::json& magnet) {

  const nlohmann::json& magnetization = magnet["parameters"]["magnetization"];

  // A sphere is magnetized along its local z axis, so the magnetization may be
  // given either as that single number or as the vector [0, 0, J].
  if (magnetization.is_number()) {
    return {0.0f, 0.0f, magnetization.get<float>()};
  }

  std::vector<float> polarization = magnetization.get<std::vector<float>>();

  if (polarization.size() != 3) {
    throw std::invalid_argument(
      "A sphere magnetization must be a number or have three components");
  }

  if (polarization[0] != 0.0f || polarization[1] != 0.0f) {
    throw std::invalid_argument(
      "A sphere is magnetized along its local z axis, so the x and y components "
      "of its magnetization must be zero. Use the orientation to point the "
      "magnetization in another direction.");
  }

  return polarization;
}

std::unique_ptr<greeter::Magnet> greeter::SphericalMagnetIO::createMagnet(const nlohmann::json& magnet) {
  std::vector<float> position = readPosition(magnet);
  std::vector<float> dimensions = readDimensions(magnet);
  std::vector<float> orientation = readOrientation(magnet);
  std::vector<float> magnetization = readMagnetization(magnet);
  return std::make_unique<greeter::SphereMagnet>(
    position, orientation, dimensions[0], magnetization[2]);
}
