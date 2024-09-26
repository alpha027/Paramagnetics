#include <fmt/format.h>
#include <greeter/greeter.h>
#include <cmath>

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
CuboidMagnet::CuboidMagnet( std::vector<float> _position, std::vector<float> _dimensions, std::vector<float> _orientation,
                            std::vector<float> _magnetization
                          ) : 
position(std::move(_position)), dimensions(std::move(_dimensions)), orientation(std::move(_orientation)),
magnetization(std::move(_magnetization)) 
{}

CuboidMagnet::~CuboidMagnet() {}

std::vector<float> greeter::CuboidMagnet::calculateMagneticFieldForCube(
        std::vector<float> position, std::vector<float> dimensions,
        std::vector<float> orientation, std::vector<float> magnetization,
        std::vector<float> observation_point ) {

  std::vector<float> result = {0, 0, 0};
  std::vector<std::vector<float>> qsigns(3, std::vector<float>(3, 1.0));

  std::vector<std::vector<float>> qs_flipx = {
    {1.0, -1.0, -1.0},
    {-1.0, 1.0, 1.0},
    {-1.0, 1.0, 1.0}
  };

  std::vector<std::vector<float>> qs_flipy = {
    {1.0, -1.0, 1.0},
    {-1.0, 1.0, -1.0},
    {1.0, -1.0, 1.0}
  };

  std::vector<std::vector<float>> qs_flipz = {
    {1.0, 1.0, -1.0},
    {1.0, 1.0, -1.0},
    {-1.0, -1.0, 1.0}
  };

  if (observation_point[0] < 0.0) {
    observation_point[0] = -observation_point[0];
    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 3; j++) {
        qsigns[i][j] *= qs_flipx[i][j];
      }
    }
  }
  if (observation_point[1] < 0.0) {
    observation_point[1] = -observation_point[1];
    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 3; j++) {
        qsigns[i][j] *= qs_flipy[i][j];
      }
    }
  }
  if (observation_point[2] < 0.0) {
    observation_point[2] = -observation_point[2];
    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 3; j++) {
        qsigns[i][j] *= qs_flipz[i][j];
      }
    }
  }

  float xpa = observation_point[0] + dimensions[0]/2.0;
  float xma = observation_point[0] - dimensions[0]/2.0;
  float xma2 = xma * xma;
  float xpa2 = xpa * xpa;

  float ypb = observation_point[1] + dimensions[1]/2.0;
  float ymb = observation_point[1] - dimensions[1]/2.0;
  float ymb2 = ymb * ymb;
  float ypb2 = ypb * ypb;

  float zpc = observation_point[2] + dimensions[2]/2.0;
  float zmc = observation_point[2] - dimensions[2]/2.0;
  float zmc2 = zmc * zmc;
  float zpc2 = zpc * zpc;

  float mmm = sqrt(xma2 + ymb2 + zmc2);
  float mmp = sqrt(xma2 + ymb2 + zpc2);
  float mpm = sqrt(xma2 + ypb2 + zmc2);
  float mpp = sqrt(xma2 + ypb2 + zpc2);
  float pmm = sqrt(xpa2 + ymb2 + zmc2);
  float pmp = sqrt(xpa2 + ymb2 + zpc2);
  float ppm = sqrt(xpa2 + ypb2 + zmc2);
  float ppp = sqrt(xpa2 + ypb2 + zpc2);

  float ff2x = log((xma+mmm)*(xpa+ppm)*(xpa+pmp)*(xma+mpp)) -
               log((xpa+pmm)*(xma+mpm)*(xma+mmp)*(xpa+ppp));

  float ff2y = log((-ymb+mmm)*(-ypb+ppm)*(-ymb+pmp)*(-ypb+mpp)) -
               log((-ymb+pmm)*(-ypb+mpm)*(ymb-mmp)*(ypb-ppp));

  float ff2z = log((-zmc+mmm)*(-zmc+ppm)*(-zpc+pmp)*(-zpc+mpp)) -
               log((-zmc+pmm)*(zmc-mmp)*(-zpc+mmp)*(zpc-ppp));

  float ff1x = atan2(ymb*zmc, xma*mmm) - atan2(ymb*zmc, xpa*pmm) - atan2(ypb*zmc, xma*mpm) + 
               atan2(ypb*zmc, xpa*ppm) - atan2(ymb*zpc, xma*mmp) + atan2(ymb*zpc, xpa*pmp) +
               atan2(ypb*zpc, xma*mpp) + atan2(ypb*zpc, xpa*ppp);

  float ff1y = atan2(xma*zmc, ymb*mmm) - atan2(xpa*zmc, ymb*pmm) - atan2(xma*zmc, ypb*mpm) +
               atan2(xpa*zmc, ypb*ppm) - atan2(xma*zpc, ymb*mmp) + atan2(xpa*zpc, ymb*pmp) +
               atan2(xma*zpc, ypb*mpp) - atan2(xpa*zpc, ypb*ppp);

  float ff1z = atan2(xma*ymb, zmc*mmm) - atan2(xpa*ymb, zmc*pmm) + atan2(xma*ypb, zmc*mpm) -
               atan2(xpa*ypb, zmc*ppm) - atan2(xma*ymb, zpc*mmp) + atan2(xpa*ymb, zpc*pmp) +
               atan2(xma*ypb, zpc*mpp) - atan2(xpa*ypb, zpc*ppp);

  float bx_pol_x = magnetization[0] * ff1x * qsigns[0][0];
  float bx_pol_y = magnetization[1] * ff2z * qsigns[1][0];
  float bx_pol_z = magnetization[2] * ff2y * qsigns[2][0];

  float by_pol_x = magnetization[0] * ff2z * qsigns[0][1];
  float by_pol_y = magnetization[1] * ff1y * qsigns[1][1];
  float by_pol_z = magnetization[2] * ff2x * qsigns[2][1];

  float bz_pol_x = magnetization[0] * ff2y * qsigns[0][2];
  float bz_pol_y = magnetization[1] * ff2x * qsigns[1][2];
  float bz_pol_z = magnetization[2] * ff1z * qsigns[2][2];

  result[0] = (bx_pol_x + bx_pol_y + bx_pol_z)/(4.0 * M_PI);
  result[1] = (by_pol_x + by_pol_y + by_pol_z)/(4.0 * M_PI);
  result[2] = (bz_pol_x + bz_pol_y + bz_pol_z)/(4.0 * M_PI);

  return result;
}



double CuboidMagnet::computeMagneticField(double x, double y, double z) const {
  return 0;
}