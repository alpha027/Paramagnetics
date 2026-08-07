#ifndef TARGET_MESH_H
#define TARGET_MESH_H

#include <cstdint>
#include <vector>

namespace greeter {

/*
  Vacuum permeability [T*m/A].

  The "magnetization" of every magnet of this library is a magnetic
  polarization J [T]: the field kernels carry no mu_0 prefactor, see
  CuboidMagnet::calculateMagneticFieldForAxisAlignedCube. A mesh cell of
  volume V therefore carries the magnetic moment m = V * J / MU0 [A*m^2].
*/
constexpr float MU0 = 1.25663706212e-6f;

/* How a target magnet is split into cells for the force computation. */
struct MeshingSpec {
  uint32_t total = 1;         // target number of cells, used when !explicit_split
  uint32_t n[3] = {1, 1, 1};  // explicit split along the local axes
  bool explicit_split = false;
};

/* One mesh cell: a point dipole expressed in the local frame of the magnet. */
struct MeshCell {
  float point[3];   // cell center [m]
  float moment[3];  // magnetic moment [A*m^2]
};

using TargetMeshData = std::vector<MeshCell>;

}  // namespace greeter

#endif  // TARGET_MESH_H
