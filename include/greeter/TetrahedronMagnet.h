#ifndef TETRAHEDRON_MAGNET_H
#define TETRAHEDRON_MAGNET_H

#include <greeter/Magnet.h>
#include <greeter/MagneticFieldMethodFactory.h>
#include <greeter/TargetMesh.h>
#include <greeter/view/ShapeDescriptor.h>
#include <vector>

namespace greeter {

/*
  Homogeneously magnetized tetrahedron, given by its four vertices in the local
  frame of the magnet.

  The field is the sum of the fields of the four magnetically charged triangular
  faces, plus the polarization itself inside the body. This is the decomposition
  used by magpylib, following Guptasarma, Geophysics 64(1), 1999.
*/
class TetrahedronMagnet: public Magnet {

    std::vector<float> position;
    std::vector<float> vertices;  // four local vertices, twelve values
    std::vector<float> orientation;
    std::vector<float> magnetization;

    public:
      TetrahedronMagnet();
      TetrahedronMagnet(std::vector<float> position, std::vector<float> vertices,
                        std::vector<float> orientation,
                        std::vector<float> magnetization);
      TetrahedronMagnet(const TetrahedronMagnet& other);

      virtual ~TetrahedronMagnet();

      std::vector<float> computeMagneticField(double x, double y, double z) const override;
      void computeMagneticField(const float* parameters, const float* observation_point,
                                float& b_x, float& b_y, float& b_z) const override;

      std::vector<float> getPosition() const override;
      std::vector<float> getCentroid() const override;
      std::vector<float> getDimensions() const override;
      std::vector<float> getOrientation() const override;
      std::vector<float> getMagnetization() const override;

      std::unique_ptr<Magnet> clone() const override;

      void display() const override;

      void setPosition(const float& x, const float& y, const float& z) override;
      void setMagnetization(const float& x, const float& y, const float& z);
      void translate(const float& x, const float& y, const float& z) override;

      uint16_t getTypeID() const override;
      size_t getNumOfParameters() const override;

      static std::string getStaticTypeName();
      static uint16_t getStaticTypeID();
      static size_t numberOfParameters();

      /* Volume of the tetrahedron spanned by four vertices [m^3]. */
      static float volumeOfTetrahedron(const float* vertices);

      /* B field of a single magnetically charged triangle, in the frame of the
         triangle. The vertex order fixes the sign of the surface normal. */
      static void calculateMagneticFieldForTriangle(
        const float* vertex_a, const float* vertex_b, const float* vertex_c,
        const float* magnetization,
        const float* observation_point,
        double& result_x, double& result_y, double& result_z
      );

      static void calculateMagneticFieldForAxisAlignedTetrahedron(
        const float* vertices,
        const float* magnetization,
        const float* observation_point,
        float& result_x, float& result_y, float& result_z
      );

      static void calculateMagneticFieldForTetrahedron(
        const float* position, const float* orientation,
        const float* vertices, const float* magnetization,
        const float* observation_point,
        float& result_x, float& result_y, float& result_z
      );

      static std::vector<float> calculateMagneticFieldForTetrahedron(
        const float* position, const float* orientation,
        const float* vertices, const float* magnetization,
        const float* observation_point
      );

      static void computeMagneticFieldForTetrahedron(
        const float* parameters,
        const float* observation_point,
        float& result_x, float& result_y, float& result_z
      );

      static greeter::TargetMeshData generateTargetMesh(
        const float* parameters,
        const greeter::MeshingSpec& meshing
      );

      /* The four local vertices, see MagnetGeometryFactory. */
      static greeter::view::ShapeDescriptor describeShape(const float* parameters);
  };

} // namespace greeter

#endif
