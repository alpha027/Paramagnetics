#ifndef HALBACH_OPTIMIZER_H
#define HALBACH_OPTIMIZER_H

#include <greeter/optimization/GeneticOptimizer.h>
#include <greeter/optimization/HalbachSolution.h>
#include <greeter/optimization/HalbachSpec.h>


namespace greeter {
namespace optimization {

/* How a run is carried out, as against what it is looking for. */
struct RunOptions {

    /* Whether every step announces itself on the standard output. */
    bool verbose = true;

    /*
      Whether the samples the verification takes are kept in the solution
      rather than reduced to metrics and dropped. See HalbachSolution.
    */
    bool keep_field = false;

    /*
      Whether the answer is measured again over the whole sphere, from the
      magnets rather than from the basis.

      On by default. It costs one more field simulation, and it is the only
      part of a run that does not take the symmetry reduction on trust, so
      turning it off is for a smoke test and not for a result.
    */
    bool verify = true;
};

/*
  The whole optimization, from a specification to a magnet.

  Three stages, and the middle one is the only one that is a search:

    1. Every ring candidate at every ring position is simulated once, into
       HalbachBasis. This is where the magnets are, and it is a single
       parallel region a million iterations wide.

    2. A genetic algorithm picks one candidate per position. It never touches
       a magnet: superposition means the field of a choice is the sum of the
       precomputed fields, so a generation of ten thousand is a walk over
       memory rather than a simulation.

    3. The choice is turned back into "halbach_ring" arrangements and
       simulated over the whole sphere, which is the number the run reports.
*/
class HalbachOptimizer {

  public:

    static HalbachSolution run(const HalbachSpec& spec, GenerationSink* sink,
                               const RunOptions& options);

    /*
      Measures a magnet that already exists, the way stage three does.

      Given to the solution's own collection it is the verification, and given
      to any other it is the comparison that says whether the optimization was
      worth doing.
    */
    static FieldMetrics measure(const greeter::MagnetCollection& collection,
                                const HalbachSpec& spec,
                                const Objective& objective,
                                size_t& num_points,
                                std::vector<FieldSample>* samples = nullptr);
};

}  // namespace optimization
}  // namespace greeter

#endif  // HALBACH_OPTIMIZER_H
