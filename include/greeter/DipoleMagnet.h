#ifndef DIPOLE_MAGNET_H
#define DIPOLE_MAGNET_H

#include <greeter/Magnet.h>
#include <greeter/MagneticFieldMethodFactory.h>
#include <greeter/TargetMesh.h>
#include <greeter/view/ShapeDescriptor.h>
#include <string>
#include <vector>

namespace greeter {

/*
  A point dipole.

  It has no shape, so it is described by ten numbers:

      [0 .. 2]    position
      [3 .. 6]    orientation, as a quaternion (w, x, y, z)
      [7 .. 9]    the magnetic moment m [A*m^2], in the local frame

  Note what sits in the last three: every other magnet of this library keeps a
  magnetic polarization J in Tesla there, and a dipole keeps a moment in
  ampere metre squared. The two are not the same quantity and not the same
  unit. A magnet of volume V and polarization J has the moment m = V * J / mu0,
  which is exactly what the target meshers here compute for a cell, so a
  dipole is the same thing a mesh cell already is. The registry is told which
  of the two a type carries, see MagnetGeometryFactory, so that a viewer can
  label it correctly rather than calling ampere metre squared Tesla.

  The field is

      B = mu0 / (4 pi) * (3 (m . r) r / |r|^5 - m / |r|^3)

  which is the far field of any of the shaped magnets, and is why this is
  worth having: an array of a thousand magnets seen from a metre away is a
  thousand dipoles, and evaluating it as one costs a fraction of the shaped
  kernels.

  At the dipole itself the field is infinite, and that is what this returns,
  as magpylib does. It is a real singularity rather than a gap in the
  implementation, and quietly returning zero there would make a wrong answer
  look like a right one. A field of view whose points land exactly on a
  dipole is the one thing to avoid.
*/
class DipoleMagnet: public Magnet {

    private:
      std::vector<float> position;
      std::vector<float> orientation;
      std::vector<float> moment;  // [A*m^2]

    public:
      DipoleMagnet();
      DipoleMagnet(std::vector<float> _position,
                   std::vector<float> _orientation,
                   std::vector<float> _moment);
      DipoleMagnet(const DipoleMagnet& other);

      virtual ~DipoleMagnet();

      std::vector<float> computeMagneticField(
                  double x, double y, double z) const override;
      void computeMagneticField(
                  const float* parameters,
                  const float* observation_point,
                  float& b_x, float& b_y, float& b_z) const override;

      std::vector<float> getPosition() const override;

      /* A point has no extent, so this is empty. */
      std::vector<float> getDimensions() const override;

      std::vector<float> getOrientation() const override;

      /* The moment [A*m^2], not a polarization. */
      std::vector<float> getMagnetization() const override;

      std::unique_ptr<Magnet> clone() const override;

      uint16_t getTypeID() const override;

      void display() const override;

      void setPosition(const float& x, const float& y, const float& z) override;
      void translate(const float& x, const float& y, const float& z) override;

      size_t getNumOfParameters() const override;

      /*
        The field of a dipole sitting at the origin with its moment given in
        the same frame as the observation point.
      */
      static void calculateMagneticFieldForAxisAlignedDipole(
        const float* moment,
        const float* observation_point,
        float& result_x, float& result_y, float& result_z
      );

      static void computeMagneticFieldForDipole(
        const float* parameters,
        const float* observation_point,
        float& result_x, float& result_y, float& result_z
      );

      static std::string getStaticTypeName();
      static uint16_t getStaticTypeID();
      static size_t numberOfParameters();

      /* One cell carrying the moment itself: a dipole is already one. */
      static greeter::TargetMeshData generateTargetMesh(
        const float* parameters,
        const greeter::MeshingSpec& meshing
      );


      /*
        The polarization J [T] at a point: what the magnet is made of where it
        is, and zero outside it. Together with B it gives H and M, see
        MagneticFieldMethodFactory.
      */
      static void computePolarizationForDipole(
        const float* parameters,
        const float* observation_point,
        float& j_x, float& j_y, float& j_z
      );

      static greeter::view::ShapeDescriptor describeShape(const float* parameters);
  };

} // namespace greeter

#endif  // DIPOLE_MAGNET_H
