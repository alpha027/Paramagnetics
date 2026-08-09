#include <greeter/optimization/HalbachOptimizer.h>

#include <greeter/optimization/HalbachBasis.h>

#include <greeter/MagneticFieldSimulator_i.h>

#include <cmath>
#include <iostream>


namespace greeter {
namespace optimization {

FieldMetrics HalbachOptimizer::measure(
    const greeter::MagnetCollection& collection, const HalbachSpec& spec,
    const Objective& objective, size_t& num_points,
    std::vector<FieldSample>* samples) {

    /*
      Over the whole sphere, whatever symmetry the search was reduced to. An
      eighth of a sphere is a fair place to search, because the choice that is
      best there is very nearly the choice that is best everywhere, but it is
      not a fair place to report from: the ring has a finite number of magnets
      and so is only nearly symmetric under the rotations the reduction
      assumes.
    */
    HalbachSpec full = spec;
    full.symmetry = Symmetry::Full;

    const std::vector<std::vector<float>> points = makeSamplePoints(full);

    num_points = points.size();

    // The simulator is driven directly rather than through
    // MagnetCollection::simulate, which announces every run it makes. This is
    // one step of several and has its own line to print.
    std::unique_ptr<greeter::MagneticFieldSimulator> simulator =
        collection.createSimulator();

    simulator->fillObservationPoints(points);
    simulator->simulate(false);

    const std::vector<float> fields = simulator->getMagneticFieldsFlat();

    std::vector<float> values;
    values.reserve(num_points);

    if (samples != nullptr) {
        samples->clear();
        samples->reserve(num_points);
    }

    for (size_t i = 0; i < num_points; i++) {

        const float b_x = fields[3 * i + 0];
        const float b_y = fields[3 * i + 1];
        const float b_z = fields[3 * i + 2];

        values.push_back(
            objective == Objective::BMagnitude
                ? std::sqrt(b_x * b_x + b_y * b_y + b_z * b_z)
                : b_x);

        if (samples != nullptr) {
            FieldSample sample;
            sample.position[0] = points[i][0];
            sample.position[1] = points[i][1];
            sample.position[2] = points[i][2];
            sample.b[0] = b_x;
            sample.b[1] = b_y;
            sample.b[2] = b_z;
            samples->push_back(sample);
        }
    }

    return HomogeneityObjective::summarise(values);
}


HalbachSolution HalbachOptimizer::run(const HalbachSpec& spec,
                                      GenerationSink* sink,
                                      const RunOptions& options) {

    spec.validate();

    HalbachSolution solution;
    solution.spec = spec;

    Kokkos::Timer timer;

    if (options.verbose) {
        std::cout << "Sampling every ring candidate at every ring position"
                  << std::endl;
    }

    const HalbachBasis basis = HalbachBasis::build(spec, options.verbose);

    solution.basis_seconds = timer.seconds();
    solution.optimized_points = basis.getNumPoints();
    solution.basis_magnets = basis.getNumMagnets();
    solution.basis_configurations = basis.getNumGenes() * basis.getNumCandidates();

    const HomogeneityObjective objective(basis, spec.objective);

    GeneticOptimizer optimizer(
        spec.genetic, basis.getNumGenes(), basis.getNumCandidates());

    if (options.verbose) {
        std::cout << "Evolving " << spec.genetic.population << " individuals of "
                  << basis.getNumGenes() << " genes over "
                  << spec.genetic.generations << " generations" << std::endl;
    }

    timer.reset();

    solution.genome = optimizer.run(objective, sink, options.verbose);

    solution.evolution_seconds = timer.seconds();
    solution.history = optimizer.getHistory();

    solution.optimized = objective.evaluateGenome(solution.genome);

    const greeter::MagnetCollection collection = solution.buildCollection();

    solution.num_magnets = collection.get_num_magnets();

    if (options.verify) {

        if (options.verbose) {
            std::cout << "Measuring the chosen magnet over the whole sphere"
                      << std::endl;
        }

        timer.reset();

        solution.verified = measure(
            collection, spec, spec.objective, solution.verified_points,
            options.keep_field ? &solution.verified_field : nullptr);

        solution.verification_seconds = timer.seconds();
        solution.was_verified = true;
    }

    return solution;
}

}  // namespace optimization
}  // namespace greeter
