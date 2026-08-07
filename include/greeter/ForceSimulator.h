#ifndef FORCE_SIMULATOR_H
#define FORCE_SIMULATOR_H

#include <greeter/KokkosDefines.h>
#include <greeter/ForceConfig.h>


namespace greeter {

/* Force and torque acting on one target magnet. */
struct ForceResult {
  uint32_t target_index;  // index of the target magnet in the collection
  float force[3];         // [N]
  float torque[3];        // [N*m]
  float pivot[3];         // the point the torque refers to
  uint32_t cells;         // how finely the target was meshed
};

/*
  Parallel force and torque simulator.

  Every target magnet is meshed into cells that carry a magnetic moment. For
  each cell the source field is evaluated at the cell center and at six finite
  difference points, which gives the field gradient. The cell then contributes

      F = (grad B) . m          and          T = m x B + (r - pivot) x F

  and the cell contributions are summed per target. This is the scheme of the
  magpylib getFT function, restricted to magnet targets.

  The parallelization mirrors MagneticFieldSimulator: the parallel index is a
  mesh cell (instead of an observation point) and the inner serial loop runs
  over the source magnets.
*/
class ForceSimulator {

  // Source magnets, same structure-of-arrays layout as MagneticFieldSimulator
  Float3VectorView positions;
  Float4VectorView orientations;
  Float3VectorView magnetizations;
  FloatVectorView dimensions;
  UInt32VectorView magnet_types;
  UInt32VectorView geometry_offsets;  // start of each magnet inside `dimensions`
  size_t num_magnets;

  // Kernel and geometry layout of each magnet, resolved once from `magnet_types`.
  MagnetKernelView magnet_kernels;

  void resolveMagnetTypes();

  // Mesh cells of all targets, tiled into a single array
  Float3VectorView mesh_points;          // cell centers, global frame
  Float3VectorView mesh_moments;         // cell moments, global frame [A*m^2]
  UInt32VectorView cell_target;          // target a cell belongs to
  UInt32VectorView cell_owner;           // magnet a cell belongs to, never a source
  UInt32VectorView target_cell_offsets;  // cell range of each target, size num_targets + 1
  UInt8VectorView source_mask;           // (num_targets x num_magnets), row major
  Float3VectorView pivots;               // torque pivot of each target

  // Results
  Float3VectorView cell_forces;
  Float3VectorView cell_torques;
  Float3VectorView target_forces;
  Float3VectorView target_torques;

  size_t num_targets;
  size_t num_cells;
  float eps;  // finite difference step [m]

public:

    ForceSimulator(Float3VectorView positions, Float4VectorView orientations,
      Float3VectorView magnetizations, FloatVectorView dimensions,
      UInt32VectorView magnet_types, UInt32VectorView geometry_offsets
    );

    ~ForceSimulator();

    void operator()( u_int64_t cell_index ) const;

    void simulate();

    /* The same, without announcing itself, see MagneticFieldSimulator. */
    void simulate(const bool& verbose);

    void fillTargets(const std::vector<greeter::TargetSpec>& targets);

    void setFiniteDifferenceStep(const float& step);
    float getFiniteDifferenceStep() const;

    void reduceToTargets();

    std::vector<greeter::ForceResult> getResults() const;

    std::vector<std::vector<float>> getResultsAsMatrix() const;

    size_t getNumCells() const;
    size_t getNumTargets() const;

    void printMeshReport() const;
};

}  // namespace greeter

#endif  // FORCE_SIMULATOR_H
