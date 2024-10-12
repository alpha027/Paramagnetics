#ifndef SPHERICAL_MAGNET_H
#define SPHERICAL_MAGNET_H

#include <greeter/Magnet.h>
#include <greeter/MagneticFieldMethodFactory.h>

namespace greeter {

class SphereMagnet: public Magnet {

    /* Spherical magnet with default magnetization orientation according to e_z */

    std::vector<float> position;
    std::vector<float> orientation;
    float radius;
    float magnetization;

    public:
      SphereMagnet(float radius, float magnetization);
      SphereMagnet(float radius, float magnetization,
                   std::vector<float> _position);
      SphereMagnet(std::vector<float> _position,
                   std::vector<float> _orientation,
                   float _radius, float _magnetization);
      SphereMagnet();
      SphereMagnet(const SphereMagnet& other);

      virtual ~SphereMagnet();
      std::vector<float> computeMagneticField(
                  double x, double y, double z) const override;
      void computeMagneticField(
                  const float* parameters,
                  const float* observation_point,
                  float& b_x, float& b_y, float& b_z) const override;

      std::vector<float> getPosition() const override;
      std::vector<float> getDimensions() const;
      std::vector<float> getOrientation() const;
      std::vector<float> getMagnetization() const;

      std::unique_ptr<Magnet> clone() const override;

      uint16_t getTypeID() const override;

      void display() const override;

      void setPosition(const float& x, const float& y, const float& z) override;
      void setMagnetization(const float& x);
      void translate(const float& x, const float& y, const float& z) override;

      static std::vector<float> calculateMagneticFieldForSphere(
        std::vector<float> position, std::vector<float> orientation,
        float radius, std::vector<float> magnetization,
        std::vector<float> observation_point
      );

      static void calculateMagneticFieldForSphere(
        std::vector<float> position, std::vector<float> orientation,
        float radius, std::vector<float> magnetization,
        std::vector<float> observation_point,
        float& result_x, float& result_y, float& result_z
      );

      static void computeMagneticFieldForSphere(
        const float* parameters,
        const float* observation_point,
        float& result_x, float& result_y, float& result_z
      );

      static std::string getStaticTypeName();
      static u_int16_t getStaticTypeID();
  };

} // namespace greeter

#endif