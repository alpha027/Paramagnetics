#ifndef CUBIC_MAGNET_H
#define CUBIC_MAGNET_H

#include <greeter/Magnet.h>
#include <greeter/MagneticFieldMethodFactory.h>
#include <vector>

namespace greeter {

class CuboidMagnet: public Magnet {

    std::vector<float> position;
    std::vector<float> dimensions;
    std::vector<float> orientation;
    std::vector<float> magnetization;

    public:
      CuboidMagnet();
      CuboidMagnet(std::vector<float> position, std::vector<float> dimensions,
                   std::vector<float> orientation,
                   std::vector<float> magnetization);

      virtual ~CuboidMagnet();
      double computeMagneticField(double x, double y, double z) const override;
      void computeMagneticField(const float* parameters, const float* observation_point,
                                float& b_x, float& b_y, float& b_z) const override;

      static std::string getStaticTypeName();
      static u_int16_t getStaticType();

      static std::vector<float> calculateMagneticFieldForCube(
        const float* position, const float* orientation, 
        const float* dimensions, const float* magnetization,
        const float* observation_point
      );

      static void calculateMagneticFieldForCube(
        const float* position, const float* orientation, 
        const float* dimensions, const float* magnetization,
        const float* observation_point,
        float& result_x, float& result_y, float& result_z
      );

       static void computeMagneticFieldForCube(
        const float* parameters,
        const float* observation_point,
        float& result_x, float& result_y, float& result_z
      );
  };

} // namespace greeter


#endif