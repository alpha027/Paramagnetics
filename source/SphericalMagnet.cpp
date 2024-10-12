#include <greeter/SphericalMagnet.h>
#include <greeter/Quaternion.h>
#include <cmath>


greeter::SphereMagnet::SphereMagnet(float _radius, float _magnetization) :
    radius(_radius), magnetization(_magnetization),
    position({0.0f, 0.0f, 0.0f}),
    orientation({1.0f, 0.0f, 0.0f, 0.0f}) {}

greeter::SphereMagnet::SphereMagnet() :
    radius(0.1), magnetization(1.0),
    position({0.0f, 0.0f, 0.0f}),
    orientation({1.0f, 0.0f, 0.0f, 0.0f}) {}

greeter::SphereMagnet::SphereMagnet(
  std::vector<float> _position, std::vector<float> _orientation,
  float _radius, float _magnetization ) :
  position(_position), radius(_radius),
  orientation(_orientation), magnetization(_magnetization) {}

greeter::SphereMagnet::SphereMagnet(const SphereMagnet& other) :
    radius(other.radius), magnetization(other.magnetization) {}

greeter::SphereMagnet::~SphereMagnet() {}

std::vector<float> 
greeter::SphereMagnet::
  computeMagneticField(double x, double y, double z) const {

  std::vector<float> observation_point = {(float) x, (float) y, (float) z};
  std::vector<float> result = greeter::SphereMagnet::calculateMagneticFieldForSphere(
    position, orientation, radius, {0.0f, 0.0f, magnetization}, observation_point
  );

  return result;
}

std::unique_ptr<greeter::Magnet> greeter::SphereMagnet::clone() const {
  return std::make_unique<SphereMagnet>(*this);
}

std::vector<float> greeter::SphereMagnet::getPosition() const {
  std::vector<float> thePosition = {position[0], position[1], position[2]};
  return thePosition;
}

std::vector<float> greeter::SphereMagnet::getDimensions() const {
  std::vector<float> theDimensions = {radius};
  return theDimensions;
}

std::vector<float> greeter::SphereMagnet::getOrientation() const {
  std::vector<float> theOrientation = {
    orientation[0], orientation[1], orientation[2], orientation[3]
  };
  return theOrientation;
}

std::vector<float> greeter::SphereMagnet::getMagnetization() const {
  std::vector<float> theMagnetization = {magnetization};
  return theMagnetization;
}

size_t greeter::SphereMagnet::getNumOfParameters() const {
  return greeter::SphereMagnet::numberOfParameters();
}

size_t greeter::SphereMagnet::numberOfParameters() {
  return 11;
}

void greeter::SphereMagnet::display() const {
  std::cout << "------------------------------------------------------" << std::endl;
  std::cout << "SphereMagnet:" << std::endl;
  std::cout << "  position : (" << position[0] << ", " << position[1] << ", " << position[2] << ")" << std::endl;
  std::cout << "  radius : " << radius << std::endl;
  std::cout << "  magnetization : " << magnetization << std::endl;
  std::cout << "------------------------------------------------------" << std::endl;
}

void greeter::SphereMagnet::setPosition(const float& x, const float& y, const float& z) {
  position[0] = x;
  position[1] = y;
  position[2] = z;
}

void greeter::SphereMagnet::setMagnetization(const float& theMagnetization) {
  magnetization = theMagnetization;
}

void greeter::SphereMagnet::translate(const float& x, const float& y, const float& z) {
  position[0] += x;
  position[1] += y;
  position[2] += z;
}

void greeter::SphereMagnet::computeMagneticField(
    const float* parameters, const float* observation_point,
    float& b_x, float& b_y, float& b_z
  ) const {

  greeter::SphereMagnet::computeMagneticFieldForSphere(
    parameters, observation_point,
    b_x, b_y, b_z
  );

}

std::string greeter::SphereMagnet::getStaticTypeName() {
  return "sphere";
}

uint16_t greeter::SphereMagnet::getTypeID() const {
  return greeter::SphereMagnet::getStaticTypeID();
}

uint16_t greeter::SphereMagnet::getStaticTypeID() {
  return 1;
}


void greeter::SphereMagnet::
  calculateMagneticFieldForAxisAlignedSphere(
      const float radius,
      const float* magnetization,
      const float* observation_point,
      float& result_x, float& result_y, float& result_z
    )
{
    float x = observation_point[0];
    float y = observation_point[1];
    float z = observation_point[2];

    float magnetization_x = magnetization[0];
    float magnetization_y = magnetization[1];
    float magnetization_z = magnetization[2];

    float r = sqrt(x*x + y*y + z*z);

    float r5 = r*r*r*r*r;

    float radius3 = radius*radius*radius;

    float dot_product = x*magnetization_x + y*magnetization_y + z*magnetization_z;

    result_x = ((3.0f * dot_product * x - magnetization_x * r * r)
                / r5 * (radius3/3.0f));
    result_y = ((3.0f * dot_product * y - magnetization_y * r * r)
                / r5 * (radius3/3.0f));
    result_z = ((3.0f * dot_product * z - magnetization_z * r * r)
                / r5 * (radius3/3.0f));
}


void greeter::SphereMagnet::computeMagneticFieldForSphere(
        const float* parameters,
        const float* observation_point,
        float& result_x, float& result_y, float& result_z
      ) {

    // The function to fill in ---------------------------------------
    float position[3] = {parameters[0], parameters[1], parameters[2]};
    float orientation[4] = {parameters[3], parameters[4], parameters[5], parameters[6]};
    float radius = parameters[7];

    float magnetization[3] = {parameters[8], parameters[9], parameters[10]};

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

    greeter::SphereMagnet::calculateMagneticFieldForAxisAlignedSphere(
        radius, magnetization,
        rotated_observation_point,
        rotated_B_x, rotated_B_y, rotated_B_z
    );

    float rotated_field[3] = {rotated_B_x, rotated_B_y, rotated_B_z};

    float final_field[3];

    greeter::Quaternion::applyRotationFromQuaternion(
        orientation,
        rotated_field,
        final_field
    );

    result_x = final_field[0];
    result_y = final_field[1];
    result_z = final_field[2];
}


void greeter::SphereMagnet::calculateMagneticFieldForSphere(
        std::vector<float> position, std::vector<float> orientation, float radius, std::vector<float> magnetization,
        std::vector<float> observation_point,
        float& result_x, float& result_y, float& result_z
      ) {

        float parameters[11] = {
            position[0], position[1], position[2],
            orientation[0], orientation[1], orientation[2], orientation[3],
            radius, magnetization[0], magnetization[1], magnetization[2]
        };

        float the_observation_point[3] = {
            observation_point[0], observation_point[1], observation_point[2]};

        greeter::SphereMagnet::computeMagneticFieldForSphere(
            parameters, the_observation_point,
            result_x, result_y, result_z
        );
}

std::vector<float> greeter::SphereMagnet::calculateMagneticFieldForSphere(
      std::vector<float> position, std::vector<float> orientation,
      float radius, std::vector<float> magnetization,
      std::vector<float> observation_point
    ) {

    float theParameters[11] = {
        position[0], position[1], position[2],
        orientation[0], orientation[1], orientation[2], orientation[3],
        radius, magnetization[0], magnetization[1], magnetization[2]
    };

    float the_observation_point[3] = {
        observation_point[0], observation_point[1], observation_point[2]
    };

    float result_x = 0.0;
    float result_y = 0.0;
    float result_z = 0.0;

    greeter::SphereMagnet::computeMagneticFieldForSphere(
        theParameters, the_observation_point,
        result_x, result_y, result_z
    );

    return {result_x, result_y, result_z};
}


static bool registerCalculateMagneticFieldForSphereToFactory
    __attribute__((unused)) = greeter::MagneticFieldMethodFactory::getInstance().
    registerComputeMagneticField(
        greeter::SphereMagnet::getStaticTypeID(), 
        greeter::SphereMagnet::computeMagneticFieldForSphere );

static bool registerNumberOfParametersForSphereToFactory
    __attribute__((unused)) = greeter::MagneticFieldMethodFactory::getInstance().
    registerNumberOfParameters(
        greeter::SphereMagnet::getStaticTypeID(),
        greeter::SphereMagnet::numberOfParameters );