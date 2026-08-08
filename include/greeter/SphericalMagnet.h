#ifndef SPHERICAL_MAGNET_H
#define SPHERICAL_MAGNET_H

#include <greeter/Magnet.h>
#include <greeter/MagneticFieldMethodFactory.h>
#include <greeter/TargetMesh.h>
#include <greeter/view/ShapeDescriptor.h>

namespace greeter {

class SphereMagnet: public Magnet {

    /* Spherical magnet with default magnetization orientation according to e_z */

    private:
      float radius;
      float magnetization;
      std::vector<float> position;
      std::vector<float> orientation;

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

      size_t getNumOfParameters() const override;

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

      static void calculateMagneticFieldForAxisAlignedSphere(
        const float radius,
        const float* magnetization,
        const float* observation_point,
        float& result_x, float& result_y, float& result_z
      );

      static std::string getStaticTypeName();
      static u_int16_t getStaticTypeID();
      static size_t numberOfParameters();

      static greeter::TargetMeshData generateTargetMesh(
        const float* parameters,
        const greeter::MeshingSpec& meshing
      );


      /*
        The polarization J [T] at a point: what the magnet is made of where it
        is, and zero outside it. Together with B it gives H and M, see
        MagneticFieldMethodFactory.
      */
      static void computePolarizationForSphere(
        const float* parameters,
        const float* observation_point,
        float& j_x, float& j_y, float& j_z
      );

      /*
        A sphere of a diameter. The class and the input file both work in the
        radius, a descriptor always gives the extent across the shape so that
        a viewer has one rule for every type. See MagnetGeometryFactory.
      */
      static greeter::view::ShapeDescriptor describeShape(const float* parameters);
  };

} // namespace greeter

#endif