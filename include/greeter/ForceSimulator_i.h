#ifndef FORCESIMULATOR_I_H
#define FORCESIMULATOR_I_H

#include <greeter/ForceSimulator.h>
#include <greeter/MagneticFieldMethodFactory.h>
#include <greeter/MagnetParameters.h>
#include <greeter/Quaternion.h>

inline
greeter::ForceSimulator::ForceSimulator(
    Float3VectorView _positions, Float4VectorView _orientations,
    Float3VectorView _magnetizations, FloatVectorView _dimensions,
    UInt32VectorView _magnet_types, UInt32VectorView _geometry_offsets) :
    positions(_positions), orientations(_orientations),
    magnetizations(_magnetizations),
    dimensions(_dimensions),
    magnet_types(_magnet_types),
    geometry_offsets(_geometry_offsets),
    num_targets(0), num_cells(0), eps(1e-5f) {

        num_magnets = magnet_types.extent(0);

        resolveMagnetTypes();
    }

/*
  Look up, once, the kernel of every source magnet and how many geometry
  parameters its shape takes. The cell loop below runs this table instead of
  branching on the magnet type, which is what keeps a new magnet type out of it.
*/
inline
void greeter::ForceSimulator::resolveMagnetTypes() {

    const greeter::MagneticFieldMethodFactory& factory =
        greeter::MagneticFieldMethodFactory::getInstance();

    magnet_kernels = MagnetKernelView("magnet_kernels", num_magnets);

    for (size_t i = 0; i < num_magnets; i++) {

        const u_int16_t magnet_type = (u_int16_t) magnet_types(i);

        magnet_kernels(i).kernel = factory.getComputeMagneticField(magnet_type);
        magnet_kernels(i).geometry_offset = geometry_offsets(i);
        magnet_kernels(i).geometry_count =
            (u_int32_t) factory.getNumberOfParameters(magnet_type) - 10;
    }
}

inline
greeter::ForceSimulator::~ForceSimulator() {}

inline
void greeter::ForceSimulator::setFiniteDifferenceStep(const float& step) {
    eps = step;
}

inline
float greeter::ForceSimulator::getFiniteDifferenceStep() const {
    return eps;
}

inline
size_t greeter::ForceSimulator::getNumCells() const {
    return num_cells;
}

inline
size_t greeter::ForceSimulator::getNumTargets() const {
    return num_targets;
}

inline
void greeter::ForceSimulator::fillTargets(const std::vector<greeter::TargetSpec>& targets) {

    num_targets = targets.size();

    num_cells = 0;
    for (const auto& target : targets) {
        num_cells += target.mesh.size();
    }

    mesh_points = Float3VectorView("mesh_points", num_cells);
    mesh_moments = Float3VectorView("mesh_moments", num_cells);
    cell_target = UInt32VectorView("cell_target", num_cells);
    cell_owner = UInt32VectorView("cell_owner", num_cells);
    cell_forces = Float3VectorView("cell_forces", num_cells);
    cell_torques = Float3VectorView("cell_torques", num_cells);

    target_cell_offsets = UInt32VectorView("target_cell_offsets", num_targets + 1);
    source_mask = UInt8VectorView("source_mask", num_targets * num_magnets);
    pivots = Float3VectorView("pivots", num_targets);
    target_forces = Float3VectorView("target_forces", num_targets);
    target_torques = Float3VectorView("target_torques", num_targets);

    size_t cell_index = 0;

    for (size_t t = 0; t < num_targets; t++) {

        const greeter::TargetSpec& target = targets[t];

        target_cell_offsets(t) = (u_int32_t) cell_index;

        pivots(t, 0) = target.pivot[0];
        pivots(t, 1) = target.pivot[1];
        pivots(t, 2) = target.pivot[2];

        // A target never acts as a source on itself.
        if (target.sources.empty()) {
            for (size_t i = 0; i < num_magnets; i++) {
                source_mask(t * num_magnets + i) = (i == target.magnet_index) ? 0 : 1;
            }
        } else {
            for (size_t i = 0; i < num_magnets; i++) {
                source_mask(t * num_magnets + i) = 0;
            }
            for (const auto& source : target.sources) {
                if (source < num_magnets && source != target.magnet_index) {
                    source_mask(t * num_magnets + source) = 1;
                }
            }
        }

        // Apply the placement of the target to its local mesh.
        for (const auto& cell : target.mesh) {

            float rotated_point[3];
            float rotated_moment[3];

            greeter::Quaternion::applyRotationFromQuaternion(
                target.orientation, cell.point, rotated_point);
            greeter::Quaternion::applyRotationFromQuaternion(
                target.orientation, cell.moment, rotated_moment);

            mesh_points(cell_index, 0) = rotated_point[0] + target.position[0];
            mesh_points(cell_index, 1) = rotated_point[1] + target.position[1];
            mesh_points(cell_index, 2) = rotated_point[2] + target.position[2];

            mesh_moments(cell_index, 0) = rotated_moment[0];
            mesh_moments(cell_index, 1) = rotated_moment[1];
            mesh_moments(cell_index, 2) = rotated_moment[2];

            cell_target(cell_index) = (u_int32_t) t;
            cell_owner(cell_index) = target.magnet_index;

            cell_index++;
        }
    }

    target_cell_offsets(num_targets) = (u_int32_t) num_cells;
}

inline
void greeter::ForceSimulator::simulate() {
    simulate(true);
}

inline
void greeter::ForceSimulator::simulate(const bool& verbose) {

    if (verbose) {
        std::cout << "Start force simulation !" << std::endl;
    }

    Kokkos::parallel_for( range_policy( 0, num_cells ),
                          *this );
    Kokkos::fence();

    reduceToTargets();

    if (verbose) {
        std::cout << "End force simulation !" << std::endl;
    }
}

inline
void greeter::ForceSimulator::reduceToTargets() {

    // Local copies, a Kokkos lambda must not capture `this`.
    auto forces = cell_forces;
    auto torques = cell_torques;

    for (size_t t = 0; t < num_targets; t++) {

        const u_int64_t start = target_cell_offsets(t);
        const u_int64_t end = target_cell_offsets(t + 1);

        float fx = 0.0f, fy = 0.0f, fz = 0.0f;
        float tx = 0.0f, ty = 0.0f, tz = 0.0f;

        Kokkos::parallel_reduce( "target_reduction", range_policy( start, end ),
            KOKKOS_LAMBDA ( const u_int64_t i, float& lfx, float& lfy, float& lfz,
                            float& ltx, float& lty, float& ltz ) {
                lfx += forces(i, 0);
                lfy += forces(i, 1);
                lfz += forces(i, 2);
                ltx += torques(i, 0);
                lty += torques(i, 1);
                ltz += torques(i, 2);
            }, fx, fy, fz, tx, ty, tz);

        target_forces(t, 0) = fx;
        target_forces(t, 1) = fy;
        target_forces(t, 2) = fz;

        target_torques(t, 0) = tx;
        target_torques(t, 1) = ty;
        target_torques(t, 2) = tz;
    }

    Kokkos::fence();
}

inline
std::vector<greeter::ForceResult> greeter::ForceSimulator::getResults() const {

    std::vector<greeter::ForceResult> results;
    results.reserve(num_targets);

    for (size_t t = 0; t < num_targets; t++) {

        greeter::ForceResult result;

        // All cells of a target belong to the same magnet.
        result.target_index = cell_owner(target_cell_offsets(t));

        result.cells = target_cell_offsets(t + 1) - target_cell_offsets(t);

        // Carried out with the numbers, because a torque means nothing
        // without the point it turns about.
        result.pivot[0] = pivots(t, 0);
        result.pivot[1] = pivots(t, 1);
        result.pivot[2] = pivots(t, 2);

        result.force[0] = target_forces(t, 0);
        result.force[1] = target_forces(t, 1);
        result.force[2] = target_forces(t, 2);

        result.torque[0] = target_torques(t, 0);
        result.torque[1] = target_torques(t, 1);
        result.torque[2] = target_torques(t, 2);

        results.push_back(result);
    }

    return results;
}

inline
std::vector<std::vector<float>> greeter::ForceSimulator::getResultsAsMatrix() const {

    std::vector<std::vector<float>> matrix;
    matrix.reserve(num_targets);

    for (const auto& result : getResults()) {
        matrix.push_back({
            (float) result.target_index,
            result.force[0], result.force[1], result.force[2],
            result.torque[0], result.torque[1], result.torque[2]
        });
    }

    return matrix;
}

inline
void greeter::ForceSimulator::printMeshReport() const {
    std::cout << "Mesh report:" << std::endl;
    for (size_t t = 0; t < num_targets; t++) {
        std::cout << "  Target magnet " << cell_owner(target_cell_offsets(t)) << ": "
                  << target_cell_offsets(t + 1) - target_cell_offsets(t) << " cells"
                  << std::endl;
    }
    std::cout << "  Total: " << num_cells << " cells, finite difference step "
              << eps << " m" << std::endl;
}


KOKKOS_INLINE_FUNCTION
void greeter::ForceSimulator::operator()( u_int64_t cell_index ) const {

    const u_int32_t target_index = cell_target(cell_index);
    const u_int32_t owner_index = cell_owner(cell_index);

    const float cell_point[3] = {
        mesh_points(cell_index, 0),
        mesh_points(cell_index, 1),
        mesh_points(cell_index, 2)
    };

    const float moment[3] = {
        mesh_moments(cell_index, 0),
        mesh_moments(cell_index, 1),
        mesh_moments(cell_index, 2)
    };

    // Cell center followed by the six finite difference points.
    const float fd_offsets[7][3] = {
        { 0.0f, 0.0f, 0.0f},
        {  eps, 0.0f, 0.0f}, { -eps, 0.0f, 0.0f},
        { 0.0f,  eps, 0.0f}, { 0.0f, -eps, 0.0f},
        { 0.0f, 0.0f,  eps}, { 0.0f, 0.0f, -eps}
    };

    // Accumulated in double: the differences taken below are small compared to
    // the field values themselves.
    double b_field[7][3];
    for (int s = 0; s < 7; s++) {
        b_field[s][0] = 0.0;
        b_field[s][1] = 0.0;
        b_field[s][2] = 0.0;
    }

    for (size_t i = 0; i < num_magnets; i++) {

        if (i == owner_index) {
            continue;
        }
        if (source_mask((size_t) target_index * num_magnets + i) == 0) {
            continue;
        }

        const MagnetKernel magnet = magnet_kernels(i);

        float magnet_parameters[greeter::MAX_MAGNET_PARAMETERS];

        greeter::packMagnetParameters(
            positions, orientations, magnetizations, dimensions,
            i, magnet.geometry_offset, magnet.geometry_count, magnet_parameters);

        for (int s = 0; s < 7; s++) {

            const float observation_point[3] = {
                cell_point[0] + fd_offsets[s][0],
                cell_point[1] + fd_offsets[s][1],
                cell_point[2] + fd_offsets[s][2]
            };

            float bx = 0.0f, by = 0.0f, bz = 0.0f;

            magnet.kernel(magnet_parameters, observation_point, bx, by, bz);

            b_field[s][0] += bx;
            b_field[s][1] += by;
            b_field[s][2] += bz;
        }
    }

    // F_j = sum_k m_k * dB_k/dx_j , the central differences of the gradient are
    // stored in the pairs (1,2), (3,4) and (5,6) of b_field.
    const double inverse_step = 0.5 / (double) eps;

    float force[3];

    for (int j = 0; j < 3; j++) {
        double component = 0.0;
        for (int k = 0; k < 3; k++) {
            const double gradient =
                (b_field[2*j + 1][k] - b_field[2*j + 2][k]) * inverse_step;
            component += (double) moment[k] * gradient;
        }
        force[j] = (float) component;
    }

    // T = m x B + (r - pivot) x F
    const float b_center[3] = {
        (float) b_field[0][0], (float) b_field[0][1], (float) b_field[0][2]
    };

    const float lever[3] = {
        cell_point[0] - pivots(target_index, 0),
        cell_point[1] - pivots(target_index, 1),
        cell_point[2] - pivots(target_index, 2)
    };

    const float torque[3] = {
        moment[1]*b_center[2] - moment[2]*b_center[1] + lever[1]*force[2] - lever[2]*force[1],
        moment[2]*b_center[0] - moment[0]*b_center[2] + lever[2]*force[0] - lever[0]*force[2],
        moment[0]*b_center[1] - moment[1]*b_center[0] + lever[0]*force[1] - lever[1]*force[0]
    };

    cell_forces(cell_index, 0) = force[0];
    cell_forces(cell_index, 1) = force[1];
    cell_forces(cell_index, 2) = force[2];

    cell_torques(cell_index, 0) = torque[0];
    cell_torques(cell_index, 1) = torque[1];
    cell_torques(cell_index, 2) = torque[2];
}


#endif // FORCESIMULATOR_I_H
