#ifndef HALBACH_SOLUTION_H
#define HALBACH_SOLUTION_H

#include <greeter/MagnetCollection.h>
#include <greeter/optimization/GeneticOptimizer.h>
#include <greeter/optimization/HalbachSpec.h>
#include <greeter/optimization/HomogeneityObjective.h>
#include <nlohmann/json.hpp>
#include <cstdint>
#include <vector>


namespace greeter {
namespace optimization {

/*
  What the optimizer settled on, and how good it turned out to be.

  There are two homogeneity figures here, and they answer different questions.
  `optimized` is what the genetic algorithm was steered by: the objective over
  the reduced volume, summed out of the precomputed basis. `verified` is the
  same figure taken again over the whole sphere, from the magnets themselves,
  through the simulator every other part of this library uses.

  They are kept apart on purpose. The reduction to an octant assumes a
  symmetry that a ring of a finite number of magnets only nearly has, and a
  figure that was optimized cannot also be the evidence that the optimization
  worked. The second number is the one to quote.
*/
struct HalbachSolution {

    HalbachSpec spec;

    /* One ring candidate index per ring position of the symmetric half. */
    std::vector<uint16_t> genome;

    FieldMetrics optimized;   // over the reduced volume, out of the basis
    size_t optimized_points = 0;

    FieldMetrics verified;    // over the whole sphere, out of the simulator
    size_t verified_points = 0;
    bool was_verified = false;

    size_t num_magnets = 0;

    /* The size of stage one: every candidate at every position, sampled. */
    size_t basis_configurations = 0;
    size_t basis_magnets = 0;

    std::vector<GenerationRecord> history;

    double basis_seconds = 0.0;
    double evolution_seconds = 0.0;
    double verification_seconds = 0.0;

    /* The rings the genome chose, in the schema an input file writes. */
    std::vector<nlohmann::json> buildArrangements() const;

    /* Those rings expanded into magnets, which is what a simulation runs. */
    greeter::MagnetCollection buildCollection() const;

    /* The candidates the genome chose, for a report. */
    std::vector<RingCandidate> getChosenCandidates() const;
};

}  // namespace optimization
}  // namespace greeter

#endif  // HALBACH_SOLUTION_H
