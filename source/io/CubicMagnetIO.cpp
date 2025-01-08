#include <greeter/Quaternion.h>
#include <greeter/CubicMagnet.h>
#include <greeter/io/CubicMagnetIO.h>

greeter::CubicMagnetIO::CubicMagnetIO() {}
greeter::CubicMagnetIO::~CubicMagnetIO() {}

std::vector<float> greeter::CubicMagnetIO::readPosition(const nlohmann::json& magnet) {
  std::vector<float> position = magnet["parameters"]["position"].get<std::vector<float>>();
  return position;
}

std::vector<float> greeter::CubicMagnetIO::readDimensions(const nlohmann::json& magnet) {
  std::vector<float> dimensions = magnet["parameters"]["dimensions"].get<std::vector<float>>();
  return dimensions;
}

std::vector<float> greeter::CubicMagnetIO::readOrientation(const nlohmann::json& magnet) {
  std::vector<float> orientation = magnet["parameters"]["orientation"].get<std::vector<float>>();
  return orientation;
}

std::vector<float> greeter::CubicMagnetIO::readMagnetization(const nlohmann::json& magnet) {
  std::vector<float> magnetization = magnet["parameters"]["magnetization"].get<std::vector<float>>();
  return magnetization;
}

std::unique_ptr<greeter::Magnet> greeter::CubicMagnetIO::createMagnet(const nlohmann::json& magnet) {
  std::cout << "Reading position" << std::endl;  
  std::vector<float> position = readPosition(magnet);
  std::cout << "position read" << std::endl;  
  std::vector<float> dimensions = readDimensions(magnet);
  std::cout << "dimensions read" << std::endl;  
  std::vector<float> orientation = readOrientation(magnet);
  std::cout << "orientation read" << std::endl;  
  std::vector<float> magnetization = readMagnetization(magnet);
  std::cout << "extracted all data from JSON" << std::endl;
  return std::make_unique<greeter::CuboidMagnet>(position, dimensions, orientation, magnetization);
}