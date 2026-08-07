#ifndef CYLINDER_MAGNET_H
#define CYLINDER_MAGNET_H

#include <greeter/Magnet.h>
#include <greeter/MagneticFieldMethodFactory.h>
#include <greeter/TargetMesh.h>
#include <greeter/view/ShapeDescriptor.h>

#include <string>

namespace greeter {

/*
  Homogeneously polarized cylinder.

  The axis of the cylinder is the local z axis and its geometric center is the
  origin of the local frame, as in magpylib. The shape takes two geometry
  parameters, a diameter and a height, so a magnet of this type is described by
  twelve numbers:

      [0 .. 2]    position
      [3 .. 6]    orientation, as a quaternion (w, x, y, z)
      [7]         diameter
      [8]         height
      [9 .. 11]   magnetization, a polarization J in Tesla

  The field is the superposition of two closed forms that are derived
  separately in the literature: the part of the polarization along the axis
  gives a B-field (Derby, American Journal of Physics 78(3) 2010, 229-235) and
  the part across it gives an H-field (Caciagli, Journal of Magnetism and
  Magnetic Materials 456 (2018) 423-432, with an unpublished extension by
  Ortner, Leitner and Rauber). Both reduce to Bulirsch's complete elliptic
  integral, which is the only special function this file needs.
*/
class CylinderMagnet: public Magnet {

    private:
      std::vector<float> position;
      std::vector<float> dimensions;     // diameter, height
      std::vector<float> orientation;
      std::vector<float> magnetization;  // polarization J [T]

    public:
      CylinderMagnet();
      CylinderMagnet(std::vector<float> _position,
                     std::vector<float> _orientation,
                     std::vector<float> _dimensions,
                     std::vector<float> _magnetization);
      CylinderMagnet(const CylinderMagnet& other);

      virtual ~CylinderMagnet();

      std::vector<float> computeMagneticField(
                  double x, double y, double z) const override;
      void computeMagneticField(
                  const float* parameters,
                  const float* observation_point,
                  float& b_x, float& b_y, float& b_z) const override;

      std::vector<float> getPosition() const override;
      std::vector<float> getDimensions() const override;
      std::vector<float> getOrientation() const override;
      std::vector<float> getMagnetization() const override;

      std::unique_ptr<Magnet> clone() const override;

      uint16_t getTypeID() const override;

      void display() const override;

      void setPosition(const float& x, const float& y, const float& z) override;
      void translate(const float& x, const float& y, const float& z) override;

      size_t getNumOfParameters() const override;

      /*
        Bulirsch's complete elliptic integral,

            cel(kc, p, c, s) = int_0^(pi/2)
                (c cos^2 t + s sin^2 t)
                / ((cos^2 t + p sin^2 t) sqrt(cos^2 t + kc^2 sin^2 t)) dt

        which covers the three Legendre integrals the two field expressions
        need: K(m) = cel(kc, 1, 1, 1), E(m) = cel(kc, 1, 1, kc^2) with
        kc = sqrt(1 - m), and Pi(n, m) = cel(kc, 1 - n, 1, 1). Undefined for
        kc = 0, which every caller here excludes beforehand.
      */
      static double completeEllipticIntegral(double kc, double p,
                                             double c, double s);

      /*
        B-field of an axially polarized cylinder of unit polarization, in
        cylindrical coordinates made dimensionless by the cylinder radius. The
        azimuthal component vanishes by symmetry.
      */
      static void calculateAxialBField(double z0, double r, double z,
                                       double& b_r, double& b_z);

      /*
        H-field of a diametrally polarized cylinder of unit polarization, in
        the same dimensionless cylindrical coordinates. `phi` is measured from
        the direction of the polarization.
      */
      static void calculateDiametralHField(double z0, double r, double z,
                                           double phi,
                                           double& h_r, double& h_phi,
                                           double& h_z);

      static void calculateMagneticFieldForAxisAlignedCylinder(
        const float diameter, const float height,
        const float* magnetization,
        const float* observation_point,
        float& result_x, float& result_y, float& result_z
      );

      static void computeMagneticFieldForCylinder(
        const float* parameters,
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

      /* Diameter and height, see MagnetGeometryFactory. */
      static greeter::view::ShapeDescriptor describeShape(const float* parameters);
  };

} // namespace greeter

#endif  // CYLINDER_MAGNET_H
