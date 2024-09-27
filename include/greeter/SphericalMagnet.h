#ifndef SPHERICAL_MAGNET_H
#define SPHERICAL_MAGNET_H

#include <greeter/Magnet.h>
#include <greeter/MagneticFieldMethodFactory.h>

namespace greeter {

class SphereMagnet: public Magnet {

    std::vector<float> position;
    float radius;
    float magnetization;

    public:
      SphereMagnet(float radius, float magnetization);
      SphereMagnet();
      virtual ~SphereMagnet();
      double computeMagneticField(double x, double y, double z) const override;
      void computeMagneticField(const float* parameters, const float* observation_point,
                                  float& b_x, float& b_y, float& b_z) const override;

      static std::vector<float> calculateMagneticFieldForSphere(
        std::vector<float> position, float radius, float magnetization,
        std::vector<float> observation_point
      );

      static void calculateMagneticFieldForSphere(
        std::vector<float> position, float radius, float magnetization,
        std::vector<float> observation_point,
        float& result_x, float& result_y, float& result_z
      );

       static void computeMagneticFieldForSphere(
        const float* parameters,
        const float* observation_point,
        float& result_x, float& result_y, float& result_z
      );

      static std::string getStaticTypeName();
      static u_int16_t getStaticType();
  };

} // namespace greeter

#endif