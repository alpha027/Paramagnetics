#include <fmt/format.h>
#include <greeter/greeter.h>

using namespace greeter;

Greeter::Greeter(std::string _name) : name(std::move(_name)) {}

std::string Greeter::greet(LanguageCode lang) const {
  switch (lang) {
    default:
    case LanguageCode::EN:
      return fmt::format("Hello, {}!", name);
    case LanguageCode::DE:
      return fmt::format("Hallo {}!", name);
    case LanguageCode::ES:
      return fmt::format("¡Hola {}!", name);
    case LanguageCode::FR:
      return fmt::format("Bonjour {}!", name);
  }
}

Magnet::~Magnet() {}

SphereMagnet::SphereMagnet(double _radius, double _magnetization) : radius(_radius), magnetization(_magnetization) {}

SphereMagnet::SphereMagnet() : radius(0), magnetization(0) {}

SphereMagnet::~SphereMagnet() {}

double SphereMagnet::computeMagneticField(double x, double y, double z) const {
  return 0;
}

CuboidMagnet::CuboidMagnet() : position({0, 0, 0}), orientation({0, 0, 0}), magnetization({0, 0, 0}) {}
CuboidMagnet::CuboidMagnet( std::vector<float> _position, std::vector<float> _orientation,
                            std::vector<float> _magnetization
                          ) : 
position(std::move(_position)), orientation(std::move(_orientation)),
magnetization(std::move(_magnetization)) 
{}

CuboidMagnet::~CuboidMagnet() {}

double CuboidMagnet::computeMagneticField(double x, double y, double z) const {
  return 0;
}