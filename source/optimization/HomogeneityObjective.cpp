#include <greeter/optimization/HomogeneityObjective.h>

#include <cmath>
#include <limits>
#include <stdexcept>


namespace greeter {
namespace optimization {

namespace {

typedef Kokkos::TeamPolicy<ExecSpace> TeamPolicy;
typedef TeamPolicy::member_type TeamMember;

typedef Kokkos::View<uint32_t*, ExecSpace::scratch_memory_space,
                     Kokkos::MemoryUnmanaged> ScratchIndices;

/*
  A mean of zero would divide the homogeneity by nothing. It means the field
  averages out over the volume, which is a genome worth rejecting rather than
  one worth an infinity, so it is scored as the worst there is.
*/
KOKKOS_INLINE_FUNCTION
float partsPerMillion(const FieldExtent& extent, const double& count) {

    const double mean = extent.sum / count;
    const double magnitude = mean < 0.0 ? -mean : mean;

    if (!(magnitude > 0.0)) {
        return Kokkos::reduction_identity<float>::min();
    }

    return (float) (1e6 * (double) (extent.max - extent.min) / magnitude);
}

}  // namespace


HomogeneityObjective::HomogeneityObjective() {}

HomogeneityObjective::HomogeneityObjective(const HalbachBasis& basis,
                                           const Objective& _objective):
    fields(basis.getFields()),
    num_genes(basis.getNumGenes()),
    num_candidates(basis.getNumCandidates()),
    num_points(basis.getNumPoints()),
    objective(_objective) {}


void HomogeneityObjective::evaluate(const PopulationView& population,
                                    const FitnessView& fitness) const {

    const size_t num_individuals = population.extent(0);

    if (population.extent(1) != num_genes) {
        throw std::invalid_argument(
            "The population has " + std::to_string(population.extent(1)) +
            " genes an individual, the basis was built for " +
            std::to_string(num_genes));
    }

    if (fitness.extent(0) != num_individuals) {
        throw std::invalid_argument(
            "There is one fitness per individual");
    }

    // Copied into locals: a lambda that captured `this` would dereference a
    // host pointer inside the parallel region.
    const Kokkos::View<float***, Layout, MemSpace> the_fields = fields;
    const size_t the_num_genes = num_genes;
    const size_t the_num_candidates = num_candidates;
    const size_t the_num_points = num_points;

    const double count = (double) num_points;

    TeamPolicy policy((int) num_individuals, Kokkos::AUTO);

    policy.set_scratch_size(
        0, Kokkos::PerTeam((int) ScratchIndices::shmem_size(the_num_genes)));

    const bool magnitude = objective == Objective::BMagnitude;

    Kokkos::parallel_for(
        "homogeneity", policy,
        KOKKOS_LAMBDA(const TeamMember& team) {

            const int individual = team.league_rank();

            /*
              Where in the basis each gene of this individual points. Worked
              out once for the team rather than once per observation point,
              which is the difference between one multiply-add per gene and
              one per gene per point.
            */
            ScratchIndices configuration(team.team_scratch(0), the_num_genes);

            Kokkos::parallel_for(
                Kokkos::TeamThreadRange(team, the_num_genes),
                [&](const size_t& gene) {
                    configuration(gene) = (uint32_t)
                        (gene * the_num_candidates + population(individual, gene));
                });

            team.team_barrier();

            FieldExtent extent;

            /*
              One pass over the volume. The peak, the trough and the total are
              all the homogeneity needs, and reading the basis three times to
              get them separately would cost three times the memory traffic of
              the only expensive step here.
            */
            Kokkos::parallel_reduce(
                Kokkos::TeamThreadRange(team, the_num_points),
                [&](const size_t& point, FieldExtent& local) {

                    float value = 0.0f;

                    if (magnitude) {

                        float b_x = 0.0f;
                        float b_y = 0.0f;
                        float b_z = 0.0f;

                        for (size_t gene = 0; gene < the_num_genes; gene++) {
                            const uint32_t c = configuration(gene);
                            b_x += the_fields(c, 0, point);
                            b_y += the_fields(c, 1, point);
                            b_z += the_fields(c, 2, point);
                        }

                        value = sqrt(b_x * b_x + b_y * b_y + b_z * b_z);

                    } else {

                        for (size_t gene = 0; gene < the_num_genes; gene++) {
                            value += the_fields(configuration(gene), 0, point);
                        }
                    }

                    if (value < local.min) { local.min = value; }
                    if (value > local.max) { local.max = value; }
                    local.sum += (double) value;
                },
                FieldExtentReducer<MemSpace>(extent));

            Kokkos::single(Kokkos::PerTeam(team), [&]() {
                fitness(individual) = partsPerMillion(extent, count);
            });
        });

    Kokkos::fence();
}


FieldMetrics HomogeneityObjective::evaluateGenome(
    const std::vector<uint16_t>& genome) const {

    if (genome.size() != num_genes) {
        throw std::invalid_argument(
            "The genome has " + std::to_string(genome.size()) +
            " genes, the objective was built for " + std::to_string(num_genes));
    }

    std::vector<float> values;
    values.reserve(num_points);

    for (size_t point = 0; point < num_points; point++) {

        float b[3] = {0.0f, 0.0f, 0.0f};

        for (size_t gene = 0; gene < num_genes; gene++) {

            if (genome[gene] >= num_candidates) {
                throw std::out_of_range("Gene value out of range");
            }

            const size_t configuration = gene * num_candidates + genome[gene];

            b[0] += fields(configuration, 0, point);
            b[1] += fields(configuration, 1, point);
            b[2] += fields(configuration, 2, point);
        }

        values.push_back(
            objective == Objective::BMagnitude
                ? std::sqrt(b[0] * b[0] + b[1] * b[1] + b[2] * b[2])
                : b[0]);
    }

    return summarise(values);
}


FieldMetrics HomogeneityObjective::summarise(const std::vector<float>& values) {

    FieldMetrics metrics;

    if (values.empty()) {
        return metrics;
    }

    metrics.min = values[0];
    metrics.max = values[0];

    // In double, because a sum of a hundred thousand single precision samples
    // of nearly the same size loses digits the homogeneity is measured in.
    double total = 0.0;

    for (const auto& value : values) {
        metrics.min = std::min(metrics.min, value);
        metrics.max = std::max(metrics.max, value);
        total += (double) value;
    }

    metrics.mean = (float) (total / (double) values.size());

    const float magnitude = std::fabs(metrics.mean);

    metrics.ppm = magnitude > 0.0f
                ? 1e6f * (metrics.max - metrics.min) / magnitude
                : std::numeric_limits<float>::max();

    return metrics;
}


size_t HomogeneityObjective::getNumGenes() const {
    return num_genes;
}

size_t HomogeneityObjective::getNumCandidates() const {
    return num_candidates;
}

size_t HomogeneityObjective::getNumPoints() const {
    return num_points;
}

}  // namespace optimization
}  // namespace greeter
