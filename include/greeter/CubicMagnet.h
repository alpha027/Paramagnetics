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
      CuboidMagnet(const CuboidMagnet& other);

      virtual ~CuboidMagnet();
      double computeMagneticField(double x, double y, double z) const override;
      void computeMagneticField(const float* parameters, const float* observation_point,
                                float& b_x, float& b_y, float& b_z) const override;

      std::vector<float> getPosition() const override;
      std::vector<float> getDimensions() const;
      std::vector<float> getOrientation() const;
      std::vector<float> getMagnetization() const;

      std::unique_ptr<Magnet> clone() const override;

      void display() const override;

      void setPosition(const float& x, const float& y, const float& z) override;
      void setMagnetization(const float& x, const float& y, const float& z);

      uint16_t getTypeID() const override;

      static std::string getStaticTypeName();
      static uint16_t getStaticTypeID();

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