#include <greeter/optimization/HalbachBasis.h>

#include <greeter/MagnetCollection.h>
#include <greeter/MagnetParameters.h>
#include <greeter/MagneticFieldMethodFactory.h>
#include <greeter/arrangements/HalbachRingArrangement.h>

#include <cmath>
#include <iostream>
#include <stdexcept>


namespace greeter {
namespace optimization {

HalbachBasis::HalbachBasis() {}

HalbachBasis::~HalbachBasis() {}


std::vector<std::vector<float>> makeSamplePoints(const HalbachSpec& spec) {

    // The same grid numpy.linspace gives for the box of the DSV: the number of
    // steps that fit in it, and one more for the far edge.
    const int32_t steps = (int32_t) std::lround(spec.dsv / spec.resolution);

    if (steps < 1) {
        throw std::invalid_argument(
            "The \"resolution\" does not divide the \"dsv\" into any step");
    }

    const int32_t n = steps + 1;

    const float radius = 0.5f * spec.dsv;
    const float spacing = spec.dsv / (float) steps;

    // A point exactly on the surface of the sphere is inside it, and one a
    // rounding error outside would otherwise drop out of an octant along its
    // own axis. The tolerance is a thousandth of a step.
    const float tolerance = 1e-3f * spacing;
    const float radius_squared = radius * radius + tolerance * radius;

    std::vector<std::vector<float>> points;

    for (int32_t i = 0; i < n; i++) {

        const float x = -radius + (float) i * spacing;

        if (spec.symmetry == Symmetry::Octant && x < -tolerance) {
            continue;
        }

        for (int32_t j = 0; j < n; j++) {

            const float y = -radius + (float) j * spacing;

            if (spec.symmetry == Symmetry::Octant && y < -tolerance) {
                continue;
            }

            for (int32_t k = 0; k < n; k++) {

                const float z = -radius + (float) k * spacing;

                if (spec.symmetry != Symmetry::Full && z < -tolerance) {
                    continue;
                }

                if (x * x + y * y + z * z > radius_squared) {
                    continue;
                }

                points.push_back({x, y, z});
            }
        }
    }

    if (points.empty()) {
        throw std::invalid_argument(
            "The sampled volume came out empty, which means the \"resolution\" "
            "is too coarse for the \"dsv\"");
    }

    return points;
}


HalbachBasis HalbachBasis::build(const HalbachSpec& spec, const bool& verbose) {

    spec.validate();

    HalbachBasis basis;

    basis.points = makeSamplePoints(spec);
    basis.num_genes = spec.getNumGenes();
    basis.num_candidates = spec.getNumCandidates();

    const size_t num_points = basis.points.size();
    const size_t num_configurations = basis.num_genes * basis.num_candidates;

    /*
      Every configuration's magnets, one after the other in a single
      collection, with a note of where each configuration's run begins. The
      rings themselves are laid out by HalbachRingArrangement, which is what
      an input file asking for a "halbach_ring" goes through, so the magnet
      measured here is the magnet the solution writes out.
    */
    greeter::MagnetCollection all;

    std::vector<uint32_t> configuration_first(num_configurations, 0);
    std::vector<uint32_t> configuration_count(num_configurations, 0);

    for (size_t gene = 0; gene < basis.num_genes; gene++) {
        for (size_t candidate = 0; candidate < basis.num_candidates; candidate++) {

            const size_t configuration = gene * basis.num_candidates + candidate;

            configuration_first[configuration] = all.get_num_magnets();

            for (const auto& ring : spec.makeRingsForGene(gene, candidate)) {
                for (auto& member :
                     greeter::HalbachRingArrangement::expand(ring)) {
                    all.addMagnet(std::move(member));
                }
            }

            configuration_count[configuration] =
                all.get_num_magnets() - configuration_first[configuration];
        }
    }

    basis.num_magnets = all.get_num_magnets();

    if (verbose) {
        std::cout << "  " << num_configurations << " configurations, "
                  << basis.num_magnets << " magnets, " << num_points
                  << " observation points" << std::endl;
    }

    // The parameters of every magnet end to end, in the order the field
    // kernels read them. One array for all of them, as the simulators use.
    UInt32VectorView magnet_types("magnet_types", basis.num_magnets);
    UInt32VectorView parameter_offsets("parameter_offsets", basis.num_magnets);
    FloatVectorView magnet_parameters(
        "magnet_parameters", all.getTotalNumOfParameters());

    all.fillMagnetParameters(magnet_parameters, parameter_offsets, magnet_types);

    /*
      The kernel of every magnet, looked up once. Doing it inside the loop
      would put a hash lookup and a branch on the type in front of every one
      of the hundreds of millions of field evaluations below. This is what
      MagneticFieldSimulator::resolveMagnetTypes does, for the same reason.
    */
    MagnetKernelView kernels("magnet_kernels", basis.num_magnets);

    const greeter::MagneticFieldMethodFactory& factory =
        greeter::MagneticFieldMethodFactory::getInstance();

    for (size_t i = 0; i < basis.num_magnets; i++) {
        kernels(i).kernel =
            factory.getComputeMagneticField((u_int16_t) magnet_types(i));
        kernels(i).polarization = nullptr;
        kernels(i).parameter_offset = parameter_offsets(i);
    }

    Float3VectorView observation_points("observation_points", num_points);

    for (size_t i = 0; i < num_points; i++) {
        observation_points(i, 0) = basis.points[i][0];
        observation_points(i, 1) = basis.points[i][1];
        observation_points(i, 2) = basis.points[i][2];
    }

    UInt32VectorView first("configuration_first", num_configurations);
    UInt32VectorView count("configuration_count", num_configurations);

    for (size_t i = 0; i < num_configurations; i++) {
        first(i) = configuration_first[i];
        count(i) = configuration_count[i];
    }

    basis.fields = Kokkos::View<float***, Layout, MemSpace>(
        "basis_fields", num_configurations, 3, num_points);

    Kokkos::View<float***, Layout, MemSpace> fields = basis.fields;

    Kokkos::Timer timer;

    /*
      One parallel region for the whole precomputation, a configuration and an
      observation point wide. Every iteration is independent and writes three
      floats nobody else writes, so there is no reduction and no atomic here.
    */
    Kokkos::parallel_for(
        "halbach_basis",
        Kokkos::MDRangePolicy<ExecSpace, Kokkos::Rank<2>>(
            {0, 0}, {(int64_t) num_configurations, (int64_t) num_points}),
        KOKKOS_LAMBDA(const int64_t configuration, const int64_t point) {

            const float observation_point[3] = {
                observation_points(point, 0),
                observation_points(point, 1),
                observation_points(point, 2)
            };

            float sum_x = 0.0f;
            float sum_y = 0.0f;
            float sum_z = 0.0f;

            const uint32_t begin = first(configuration);
            const uint32_t end = begin + count(configuration);

            for (uint32_t m = begin; m < end; m++) {

                const MagnetKernel magnet = kernels(m);

                const float* parameters = greeter::magnetParameters(
                    magnet_parameters, magnet.parameter_offset);

                float b_x = 0.0f;
                float b_y = 0.0f;
                float b_z = 0.0f;

                magnet.kernel(parameters, observation_point, b_x, b_y, b_z);

                sum_x += b_x;
                sum_y += b_y;
                sum_z += b_z;
            }

            fields(configuration, 0, point) = sum_x;
            fields(configuration, 1, point) = sum_y;
            fields(configuration, 2, point) = sum_z;
        });

    Kokkos::fence();

    if (verbose) {
        std::cout << "  basis built in " << timer.seconds() << " s" << std::endl;
    }

    return basis;
}


const std::vector<std::vector<float>>& HalbachBasis::getPoints() const {
    return points;
}

size_t HalbachBasis::getNumPoints() const {
    return points.size();
}

size_t HalbachBasis::getNumGenes() const {
    return num_genes;
}

size_t HalbachBasis::getNumCandidates() const {
    return num_candidates;
}

size_t HalbachBasis::getNumMagnets() const {
    return num_magnets;
}

Kokkos::View<float***, Layout, MemSpace> HalbachBasis::getFields() const {
    return fields;
}


void HalbachBasis::evaluateAt(const std::vector<uint16_t>& genome,
                              const size_t& point, float* b) const {

    if (genome.size() != num_genes) {
        throw std::invalid_argument(
            "The genome has " + std::to_string(genome.size()) +
            " genes, the basis was built for " + std::to_string(num_genes));
    }

    if (point >= points.size()) {
        throw std::out_of_range("Observation point index out of range");
    }

    b[0] = 0.0f;
    b[1] = 0.0f;
    b[2] = 0.0f;

    for (size_t gene = 0; gene < num_genes; gene++) {

        if (genome[gene] >= num_candidates) {
            throw std::out_of_range("Gene value out of range");
        }

        const size_t configuration = gene * num_candidates + genome[gene];

        for (size_t component = 0; component < 3; component++) {
            b[component] += fields(configuration, component, point);
        }
    }
}

}  // namespace optimization
}  // namespace greeter
