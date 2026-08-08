#ifndef TRIANGULAR_MESH_MAGNET_H
#define TRIANGULAR_MESH_MAGNET_H

#include <greeter/Magnet.h>
#include <greeter/MagneticFieldMethodFactory.h>
#include <greeter/TargetMesh.h>
#include <greeter/view/ShapeDescriptor.h>
#include <string>
#include <vector>

namespace greeter {

/*
  A homogeneously polarized body of any shape, given as a closed surface of
  triangles.

      [0 .. 2]          position
      [3 .. 6]          orientation, as a quaternion (w, x, y, z)
      [7]               F, the number of faces
      [8 .. 7 + 9F]     F triangles, three vertices each, local frame [m]
      [8 + 9F .. 10+9F] the polarization J [T]

  The number of parameters therefore depends on the magnet and not only on its
  type, which is why nothing here registers a fixed count and why the
  simulators hand a kernel a pointer rather than a copy; see
  MagnetParameters.h.

  The face count is kept in the block itself because a kernel is given nothing
  but a pointer, and so has no other way to know where the triangles stop and
  the polarization begins. It is a float like everything else in the block,
  which represents a whole number exactly up to sixteen million faces.

  The field is the sum of the fields of the charged faces, plus the
  polarization itself inside the body, which is the same decomposition the
  tetrahedron uses. A tetrahedron is exactly this with four faces, and the two
  agree to the last digit on the same shape.

  Whether a point is inside is decided by adding up the solid angles the faces
  subtend at it. Seen from inside a closed surface the faces cover the whole
  sphere and the total is 4 pi; from outside they cancel to zero. That needs
  no ray to be cast in some arbitrary direction and so has no direction that
  makes it go wrong.

  The surface has to be closed and consistently wound, and both are checked
  when one is built: an open surface encloses no volume, and a face wound the
  wrong way round would carry the wrong sign of charge. A body given inside
  out is turned the right way round rather than refused, because which way
  round a mesh comes out of a CAD program is not something to make a user
  think about.
*/
class TriangularMeshMagnet: public Magnet {

    private:
      std::vector<float> position;
      std::vector<float> triangles;      // 9 floats per face, local frame
      std::vector<float> orientation;
      std::vector<float> magnetization;  // polarization J [T]

    public:
      TriangularMeshMagnet();

      /*
        `triangles` is nine floats per face. Throws when the surface is not
        closed or not consistently wound. A surface wound inwards is turned
        outwards instead of being refused.
      */
      TriangularMeshMagnet(std::vector<float> _position,
                           std::vector<float> _triangles,
                           std::vector<float> _orientation,
                           std::vector<float> _magnetization);

      TriangularMeshMagnet(const TriangularMeshMagnet& other);

      virtual ~TriangularMeshMagnet();

      std::vector<float> computeMagneticField(
                  double x, double y, double z) const override;
      void computeMagneticField(
                  const float* parameters,
                  const float* observation_point,
                  float& b_x, float& b_y, float& b_z) const override;

      std::vector<float> getPosition() const override;

      /* The centre of volume, which is what a torque turns about by default. */
      std::vector<float> getCentroid() const override;

      std::vector<float> getDimensions() const override;
      std::vector<float> getOrientation() const override;
      std::vector<float> getMagnetization() const override;

      std::unique_ptr<Magnet> clone() const override;

      uint16_t getTypeID() const override;

      void display() const override;

      void setPosition(const float& x, const float& y, const float& z) override;
      void translate(const float& x, const float& y, const float& z) override;

      /* 10 + 9 * the number of faces, so it differs between two of these. */
      size_t getNumOfParameters() const override;

      size_t getNumOfFaces() const;

      /* Volume of the body [m^3], from the divergence theorem. */
      float getVolume() const;

      static void computeMagneticFieldForTriangularMesh(
        const float* parameters,
        const float* observation_point,
        float& result_x, float& result_y, float& result_z
      );

      /*
        Whether the point, in the local frame, is inside the closed surface of
        `face_count` faces, by the sum of the solid angles they subtend.
      */
      static bool isInside(const float* triangles, const size_t& face_count,
                           const float* observation_point);

      /* Volume enclosed by a closed, outward wound surface [m^3]. */
      static double volumeOf(const float* triangles, const size_t& face_count);

      /*
        Throws when the surface is not closed or not consistently wound, and
        turns it outwards when it was wound inwards. Called by the constructor;
        exposed so that a reader can report the same complaint.
      */
      static void checkAndOrient(std::vector<float>& triangles);

      static std::string getStaticTypeName();
      static uint16_t getStaticTypeID();

      static greeter::TargetMeshData generateTargetMesh(
        const float* parameters,
        const greeter::MeshingSpec& meshing
      );


      /*
        The polarization J [T] at a point: what the magnet is made of where it
        is, and zero outside it. Together with B it gives H and M, see
        MagneticFieldMethodFactory.
      */
      static void computePolarizationForTriangularMesh(
        const float* parameters,
        const float* observation_point,
        float& j_x, float& j_y, float& j_z
      );

      static greeter::view::ShapeDescriptor describeShape(const float* parameters);
  };

} // namespace greeter

#endif  // TRIANGULAR_MESH_MAGNET_H
