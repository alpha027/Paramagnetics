#include <greeter/SphericalMagnet.h>
#include <cmath>

greeter::SphereMagnet::SphereMagnet(float _radius, float _magnetization) : radius(_radius), magnetization(_magnetization) {}

greeter::SphereMagnet::SphereMagnet() : radius(0), magnetization(0) {}

greeter::SphereMagnet::~SphereMagnet() {}

double greeter::SphereMagnet::computeMagneticField(double x, double y, double z) const {
  return 0;
}

void greeter::SphereMagnet::computeMagneticField(const float* parameters, const float* observation_point, 
                                        float& b_x, float& b_y, float& b_z) const {

  float position[3] = {parameters[0], parameters[1], parameters[2]};
  float orientation[3] = {parameters[3], parameters[4], parameters[5]};
  float dimensions[3] = {parameters[6], parameters[7], parameters[8]};
  float magnetization[3] = {parameters[9], parameters[10], parameters[11]};

  float result_x = 0.0;
  float result_y = 0.0;
  float result_z = 0.0;
}

std::string greeter::SphereMagnet::getStaticTypeName() {
  return "sphere";
}

u_int16_t greeter::SphereMagnet::getStaticType() {
  return 1;
}

void greeter::SphereMagnet::computeMagneticFieldForSphere(
        const float* parameters,
        const float* observation_point,
        float& result_x, float& result_y, float& result_z) {

    // The function to fill in ----------------------------------------------
    float position[3] = {parameters[0], parameters[1], parameters[2]};
    float radius = parameters[3];
    float magnetization = parameters[4];

    result_x = 0.0;
    result_y = 0.0;
    result_z = 0.0;
}


void greeter::SphereMagnet::calculateMagneticFieldForSphere(
        std::vector<float> position, float radius, float magnetization,
        std::vector<float> observation_point,
        float& result_x, float& result_y, float& result_z) {

        float parameters[5] = {position[0], position[1], position[2], radius, magnetization};

        float the_observation_point[3] = {observation_point[0], observation_point[1], observation_point[2]};
        
        greeter::SphereMagnet::computeMagneticFieldForSphere(
            parameters, the_observation_point,
            result_x, result_y, result_z
        );
}


static bool registerCalculateMagneticFieldForSphereToFactory __attribute__((unused)) = greeter::MagneticFieldMethodFactory::getInstance().registerComputeMagneticField(greeter::SphereMagnet::getStaticType(), greeter::SphereMagnet::computeMagneticFieldForSphere);