#ifndef TRIANGLE_MAGNET_H
#define TRIANGLE_MAGNET_H

#include <greeter/Magnet.h>
#include <greeter/MagneticFieldMethodFactory.h>
#include <greeter/TargetMesh.h>
#include <greeter/view/ShapeDescriptor.h>
#include <string>
#include <vector>

namespace greeter {

/*
  One magnetically charged triangle.

  This is a surface and not a body: it carries the charge that the face of a
  magnet would carry, sigma = J . n, and nothing else. It is described by
  nineteen numbers:

      [0 .. 2]    position
      [3 .. 6]    orientation, as a quaternion (w, x, y, z)
      [7 .. 15]   three vertices, in the local frame [m]
      [16 .. 18]  the polarization J [T] whose normal component charges it

  The winding of the vertices fixes which way the normal points and so the
  sign of the charge.

  On its own a charged triangle is not a magnet, and its field is not the
  field of anything physical: a real body is closed, and the fields of its
  faces only add up to something sensible together. It is here because it is
  the piece every polyhedron is built from, and because magpylib offers the
  same thing for the same reason. For a body, use TriangularMeshMagnet, which
  is a closed set of these plus the polarization inside.

  Having no volume, it also cannot be a force target: there is nothing to
  carry a moment.
*/
class TriangleMagnet: public Magnet {

    private:
      std::vector<float> position;
      std::vector<float> vertices;       // three local vertices, nine values
      std::vector<float> orientation;
      std::vector<float> magnetization;  // polarization J [T]

    public:
      TriangleMagnet();
      TriangleMagnet(std::vector<float> _position,
                     std::vector<float> _vertices,
                     std::vector<float> _orientation,
                     std::vector<float> _magnetization);
      TriangleMagnet(const TriangleMagnet& other);

      virtual ~TriangleMagnet();

      std::vector<float> computeMagneticField(
                  double x, double y, double z) const override;
      void computeMagneticField(
                  const float* parameters,
                  const float* observation_point,
                  float& b_x, float& b_y, float& b_z) const override;

      std::vector<float> getPosition() const override;

      /* The centre of the three vertices, which is what a torque turns about. */
      std::vector<float> getCentroid() const override;

      std::vector<float> getDimensions() const override;
      std::vector<float> getOrientation() const override;
      std::vector<float> getMagnetization() const override;

      std::unique_ptr<Magnet> clone() const override;

      uint16_t getTypeID() const override;

      void display() const override;

      void setPosition(const float& x, const float& y, const float& z) override;
      void translate(const float& x, const float& y, const float& z) override;

      size_t getNumOfParameters() const override;

      static void computeMagneticFieldForTriangle(
        const float* parameters,
        const float* observation_point,
        float& result_x, float& result_y, float& result_z
      );

      static std::string getStaticTypeName();
      static uint16_t getStaticTypeID();
      static size_t numberOfParameters();

      /*
        Empty: a surface has no volume, so there is no moment to put in a cell
        and a triangle cannot be a force target.
      */
      static greeter::TargetMeshData generateTargetMesh(
        const float* parameters,
        const greeter::MeshingSpec& meshing
      );


      /*
        The polarization J [T] at a point: what the magnet is made of where it
        is, and zero outside it. Together with B it gives H and M, see
        MagneticFieldMethodFactory.
      */
      static void computePolarizationForTriangle(
        const float* parameters,
        const float* observation_point,
        float& j_x, float& j_y, float& j_z
      );

      static greeter::view::ShapeDescriptor describeShape(const float* parameters);
  };

} // namespace greeter

#endif  // TRIANGLE_MAGNET_H
