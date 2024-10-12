#include <greeter/CubicMagnet.h>
#include <greeter/Quaternion.h>
#include <cmath>

greeter::CuboidMagnet::CuboidMagnet() : position({0, 0, 0}), orientation({1.0, 0.0, 0.0, 0.0}), dimensions({1.0, 1.0, 1.0}), magnetization({0, 1.0, 0}) {}
greeter::CuboidMagnet::CuboidMagnet( std::vector<float> _position, std::vector<float> _dimensions, std::vector<float> _orientation,
                            std::vector<float> _magnetization
                          ) : 
position(std::move(_position)), dimensions(std::move(_dimensions)), orientation(std::move(_orientation)),
magnetization(std::move(_magnetization)) 
{}

greeter::CuboidMagnet::CuboidMagnet(const CuboidMagnet& other) :
position(other.position), dimensions(other.dimensions), orientation(other.orientation), magnetization(other.magnetization) {}

greeter::CuboidMagnet::~CuboidMagnet() {}

std::unique_ptr<greeter::Magnet> greeter::CuboidMagnet::clone() const {
  return std::make_unique<CuboidMagnet>(*this);
}

std::vector<float> greeter::CuboidMagnet::getPosition() const {
  std::vector<float> thePosition = {position[0], position[1], position[2]};
  return thePosition;
}

std::vector<float> greeter::CuboidMagnet::getDimensions() const {
  std::vector<float> theDimensions = {dimensions[0], dimensions[1], dimensions[2]};
  return theDimensions;
}

std::vector<float> greeter::CuboidMagnet::getOrientation() const {
  std::vector<float> theOrientation = {orientation[0], orientation[1], orientation[2], orientation[3]};
  return theOrientation;
}

std::vector<float> greeter::CuboidMagnet::getMagnetization() const {
  std::vector<float> theMagnetization = {magnetization[0], magnetization[1], magnetization[2]};
  return theMagnetization;
}

void greeter::CuboidMagnet::setPosition(const float& x, const float& y, const float& z) {
  position[0] = x;
  position[1] = y;
  position[2] = z;
}

void greeter::CuboidMagnet::setMagnetization(const float& x, const float& y, const float& z) {
  magnetization[0] = x;
  magnetization[1] = y;
  magnetization[2] = z;
}

void greeter::CuboidMagnet::translate(const float& x, const float& y, const float& z) {
  position[0] += x;
  position[1] += y;
  position[2] += z;
}

void greeter::CuboidMagnet::display() const {
  std::cout << "------------------------------------------------------" << std::endl;
  std::cout << "CuboidMagnet:" << std::endl;
  std::cout << "  position : (" << position[0] << ", " << position[1] << ", " << position[2] << ")" << std::endl;
  std::cout << "  dimensions : (" << dimensions[0] << ", " << dimensions[1] << ", " << dimensions[2] << ")" << std::endl;
  std::cout << "  orientation : (" << orientation[0] << ", " << orientation[1] << ", " << orientation[2] << ", " << orientation[3] << ")" << std::endl;
  std::cout << "  magnetization : (" << magnetization[0] << ", " << magnetization[1] << ", " << magnetization[2] << ")" << std::endl;
  std::cout << "------------------------------------------------------" << std::endl;
}

void greeter::CuboidMagnet::calculateMagneticFieldForCube(
        const float* position, const float* orientation, 
        const float* dimensions, const float* magnetization,
        const float* observation_point,
        float& result_x, float& result_y, float& result_z
      )
{

  float translated_observation_point[3] = {
    observation_point[0] - position[0],
    observation_point[1] - position[1],
    observation_point[2] - position[2]
  };

  float rotated_observation_point[3];

  greeter::Quaternion::applyInverseRotationFromQuaternion(
    orientation,
    translated_observation_point,
    rotated_observation_point
  );

  float rotated_B_x, rotated_B_y, rotated_B_z;

  greeter::CuboidMagnet::calculateMagneticFieldForAxisAlignedCube(
    position, dimensions, magnetization,
    rotated_observation_point,
    rotated_B_x, rotated_B_y, rotated_B_z
  );

  float rotated_field[3] = {rotated_B_x, rotated_B_y, rotated_B_z};

  float final_magnetic_field[3];

  greeter::Quaternion::applyRotationFromQuaternion(
    orientation,
    rotated_field,
    final_magnetic_field
  );

  result_x = final_magnetic_field[0];
  result_y = final_magnetic_field[1];
  result_z = final_magnetic_field[2];
}


void greeter::CuboidMagnet::calculateMagneticFieldForAxisAlignedCube(
        const float* position, const float* dimensions,
        const float* magnetization,
        const float* observation_point,
        float& result_x, float& result_y, float& result_z
      )
{

  float new_observation_point[3] = {observation_point[0], observation_point[1], observation_point[2]};

  float qsigns[3][3] = {
    {1.0, 1.0, 1.0},
    {1.0, 1.0, 1.0},
    {1.0, 1.0, 1.0}
  };

  float qs_flipx[3][3] = {
    {1.0, -1.0, -1.0},
    {-1.0, 1.0, 1.0},
    {-1.0, 1.0, 1.0}
  };

  float qs_flipy[3][3] = {
    {1.0, -1.0, 1.0},
    {-1.0, 1.0, -1.0},
    {1.0, -1.0, 1.0}
  };

  float qs_flipz[3][3] = {
    {1.0, 1.0, -1.0},
    {1.0, 1.0, -1.0},
    {-1.0, -1.0, 1.0}
  };

  if (observation_point[0] < 0.0) {
    new_observation_point[0] = -new_observation_point[0];
    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 3; j++) {
        qsigns[i][j] *= qs_flipx[i][j];
      }
    }
  }
  if (observation_point[1] > 0.0) {
    new_observation_point[1] = -new_observation_point[1];
    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 3; j++) {
        qsigns[i][j] *= qs_flipy[i][j];
      }
    }
  }
  if (observation_point[2] > 0.0) {
    new_observation_point[2] = - new_observation_point[2];
    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 3; j++) {
        qsigns[i][j] *= qs_flipz[i][j];
      }
    }
  }

  float xpa = new_observation_point[0] + dimensions[0]/2.0f;
  float xma = new_observation_point[0] - dimensions[0]/2.0f;
  float xma2 = xma * xma;
  float xpa2 = xpa * xpa;

  float ypb = new_observation_point[1] + dimensions[1]/2.0f;
  float ymb = new_observation_point[1] - dimensions[1]/2.0f;
  float ymb2 = ymb * ymb;
  float ypb2 = ypb * ypb;

  float zpc = new_observation_point[2] + dimensions[2]/2.0f;
  float zmc = new_observation_point[2] - dimensions[2]/2.0f;
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
               log((-zmc+pmm)*(zmc-mpm)*(-zpc+mmp)*(zpc-ppp));

  float ff1x = atan2(ymb*zmc, xma*mmm) - atan2(ymb*zmc, xpa*pmm) - 
               atan2(ypb*zmc, xma*mpm) + atan2(ypb*zmc, xpa*ppm) -
               atan2(ymb*zpc, xma*mmp) + atan2(ymb*zpc, xpa*pmp) +
               atan2(ypb*zpc, xma*mpp) - atan2(ypb*zpc, xpa*ppp);

  float ff1y = atan2(xma*zmc, ymb*mmm) - atan2(xpa*zmc, ymb*pmm) - 
               atan2(xma*zmc, ypb*mpm) + atan2(xpa*zmc, ypb*ppm) -
               atan2(xma*zpc, ymb*mmp) + atan2(xpa*zpc, ymb*pmp) +
               atan2(xma*zpc, ypb*mpp) - atan2(xpa*zpc, ypb*ppp);

  float ff1z = atan2(xma*ymb, zmc*mmm) - atan2(xpa*ymb, zmc*pmm) -
               atan2(xma*ypb, zmc*mpm) + atan2(xpa*ypb, zmc*ppm) -
               atan2(xma*ymb, zpc*mmp) + atan2(xpa*ymb, zpc*pmp) +
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

  result_x = (bx_pol_x + bx_pol_y + bx_pol_z)/(4.0f * M_PI);
  result_y = (by_pol_x + by_pol_y + by_pol_z)/(4.0f * M_PI);
  result_z = (bz_pol_x + bz_pol_y + bz_pol_z)/(4.0f * M_PI);
}


std::vector<float> greeter::CuboidMagnet::calculateMagneticFieldForCube(
       const float* position, const float* orientation, 
        const float* dimensions, const float* magnetization,
        const float* observation_point ) {

  float result_x = 0.0;
  float result_y = 0.0;
  float result_z = 0.0;

  greeter::CuboidMagnet::calculateMagneticFieldForCube(
    position, orientation, dimensions, magnetization, observation_point,
    result_x, result_y, result_z
  );

  return {result_x, result_y, result_z};
}

std::vector<float> greeter::CuboidMagnet::computeMagneticField(double x, double y, double z) const {

  float parameters[13] = {
    position[0], position[1], position[2],
    orientation[0], orientation[1], orientation[2], orientation[3],
    dimensions[0], dimensions[1], dimensions[2],
    magnetization[0], magnetization[1], magnetization[2]
  };

  float observation_point[3] = {(float) x, (float) y, (float) z};

  float b_x, b_y, b_z;

  greeter::CuboidMagnet::computeMagneticField(
    parameters, observation_point,
    b_x, b_y, b_z
  );

  return {b_x, b_y, b_z};
}

void greeter::CuboidMagnet::computeMagneticField(const float* parameters, const float* observation_point, 
                                        float& b_x, float& b_y, float& b_z) const {

  float position[3] = {parameters[0], parameters[1], parameters[2]};
  float orientation[4] = {parameters[3], parameters[4], parameters[5], parameters[6]};
  float dimensions[3] = {parameters[7], parameters[8], parameters[9]};
  float magnetization[3] = {parameters[10], parameters[11], parameters[12]};

  greeter::CuboidMagnet::calculateMagneticFieldForCube(
    position, orientation, dimensions, magnetization, observation_point,
    b_x, b_y, b_z
  );
}

void greeter::CuboidMagnet::computeMagneticFieldForCube(
    const float* parameters, const float* observation_point,
    float& result_x, float& result_y, float& result_z ) {

    float position[3] = {parameters[0], parameters[1], parameters[2]};
    float orientation[4] = {parameters[3], parameters[4], parameters[5], parameters[6]};
    float dimensions[3] = {parameters[7], parameters[8], parameters[9]};
    float magnetization[3] = {parameters[10], parameters[11], parameters[12]};

    greeter::CuboidMagnet::calculateMagneticFieldForCube(
      position, orientation, dimensions, magnetization, observation_point,
      result_x, result_y, result_z
    );
}

std::string greeter::CuboidMagnet::getStaticTypeName() {
  return "cuboid";
}

uint16_t greeter::CuboidMagnet::getTypeID() const {
  return greeter::CuboidMagnet::getStaticTypeID();
}

uint16_t greeter::CuboidMagnet::getStaticTypeID() {
  return 0;
}

static bool registerCalculateMagneticFieldForCubeToFactory 
__attribute__((unused)) =
 greeter::MagneticFieldMethodFactory::getInstance().
 registerComputeMagneticField(
  greeter::CuboidMagnet::getStaticTypeID(),
  greeter::CuboidMagnet::computeMagneticFieldForCube
);