#ifndef FORCE_CONFIG_H
#define FORCE_CONFIG_H

#include <greeter/TargetMesh.h>
#include <array>
#include <cstdint>
#include <vector>

namespace greeter {

/*
  Force computation request, as read from the JSON input file.

  All magnet references are indices into the MagnetCollection, the magnet "id"
  fields of the JSON file are resolved by ForceIO.
*/
struct ForceConfig {
  std::vector<uint32_t> targets;               // magnets the force acts on
  std::vector<std::vector<uint32_t>> sources;  // per target, empty -> all other magnets
  std::vector<MeshingSpec> meshing;            // per target
  std::vector<std::array<float, 3>> pivots;    // per target, used when !centroid_pivot
  bool centroid_pivot = true;                  // torque pivot is the target center
  float eps = 0.0f;                            // finite difference step [m], <= 0 -> auto
  bool mesh_report = false;                    // print the cell count of every target
};

/*
  A resolved force target: where it sits, which magnets act on it, and how it is
  meshed. Built by MagnetCollection and consumed by ForceSimulator.
*/
struct TargetSpec {
  uint32_t magnet_index = 0;
  float position[3] = {0.0f, 0.0f, 0.0f};
  float orientation[4] = {1.0f, 0.0f, 0.0f, 0.0f};
  float pivot[3] = {0.0f, 0.0f, 0.0f};
  std::vector<uint32_t> sources;  // empty -> all magnets except magnet_index
  TargetMeshData mesh;            // cells in the local frame of the target
};

}  // namespace greeter

#endif  // FORCE_CONFIG_H
