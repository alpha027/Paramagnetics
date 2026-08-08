#include <greeter/DipoleMagnet.h>
#include <greeter/io/DipoleMagnetIO.h>

#include <stdexcept>


greeter::DipoleMagnetIO::DipoleMagnetIO() {}
greeter::DipoleMagnetIO::~DipoleMagnetIO() {}

std::string greeter::DipoleMagnetIO::getTypeName() {
  return "dipole";
}

std::vector<float> greeter::DipoleMagnetIO::readPosition(const nlohmann::json& magnet) {

  const nlohmann::json& parameters = magnet["parameters"];

  if (!parameters.contains("position")) {
    return {0.0f, 0.0f, 0.0f};
  }

  std::vector<float> position = parameters["position"].get<std::vector<float>>();

  if (position.size() != 3) {
    throw std::invalid_argument("A dipole position must have three components");
  }

  return position;
}

std::vector<float> greeter::DipoleMagnetIO::readOrientation(const nlohmann::json& magnet) {

  const nlohmann::json& parameters = magnet["parameters"];

  if (!parameters.contains("orientation")) {
    return {1.0f, 0.0f, 0.0f, 0.0f};
  }

  std::vector<float> orientation = parameters["orientation"].get<std::vector<float>>();

  if (orientation.size() != 4) {
    throw std::invalid_argument("A dipole orientation must be a quaternion [w, x, y, z]");
  }

  return orientation;
}

std::vector<float> greeter::DipoleMagnetIO::readMoment(const nlohmann::json& magnet) {

  const nlohmann::json& parameters = magnet["parameters"];

  // A moment is not a polarization: one is measured in ampere metre squared
  // and the other in Tesla. Reading a field named "magnetization" as a moment
  // would turn a units mistake into a plausible looking answer, so it is
  // refused instead.
  if (!parameters.contains("moment") && parameters.contains("magnetization")) {
    throw std::invalid_argument(
      "A dipole carries a \"moment\" in ampere metre squared, not a "
      "\"magnetization\" in Tesla. A magnet of volume V and polarization J "
      "has the moment V * J / mu0");
  }

  if (!parameters.contains("moment")) {
    throw std::invalid_argument(
      "A dipole needs its \"moment\" in ampere metre squared");
  }

  const nlohmann::json& given = parameters["moment"];

  // A dipole pointing along its own axis is the common case, so a single
  // number is read as that, the way a sphere reads its magnetization.
  if (given.is_number()) {
    return {0.0f, 0.0f, given.get<float>()};
  }

  if (!given.is_array()) {
    throw std::invalid_argument(
      "The moment of a dipole is three numbers, or one for a moment along "
      "the local z axis");
  }

  std::vector<float> moment = given.get<std::vector<float>>();

  if (moment.size() != 3) {
    throw std::invalid_argument("A dipole moment must have three components");
  }

  return moment;
}

std::unique_ptr<greeter::Magnet> greeter::DipoleMagnetIO::createMagnet(
    const nlohmann::json& magnet) {

  if (!magnet.contains("parameters") || !magnet["parameters"].is_object()) {
    throw std::invalid_argument("A dipole needs a \"parameters\" object");
  }

  return std::make_unique<greeter::DipoleMagnet>(
    readPosition(magnet), readOrientation(magnet), readMoment(magnet));
}
