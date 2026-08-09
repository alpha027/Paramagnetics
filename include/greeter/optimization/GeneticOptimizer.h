#ifndef GENETIC_OPTIMIZER_H
#define GENETIC_OPTIMIZER_H

#include <greeter/KokkosDefines.h>
#include <greeter/optimization/HalbachSpec.h>
#include <greeter/optimization/HomogeneityObjective.h>
#include <cstdint>
#include <vector>


namespace greeter {
namespace optimization {

/* What one generation came to, for a convergence curve. */
struct GenerationRecord {
    uint32_t generation = 0;
    float best = 0.0f;        // best of this generation, in ppm
    float best_ever = 0.0f;   // best seen since the run began
    float mean = 0.0f;        // mean of the generation, a spread indicator
    float seconds = 0.0f;
};

/* Told how a run is going, and asked whether to carry on. */
class GenerationSink {

  public:

    virtual ~GenerationSink() = default;

    /* Returns false to stop the run at the end of this generation. */
    virtual bool onGeneration(const GenerationRecord& record) = 0;
};

/*
  A genetic algorithm over genomes of small integers, run on the device.

  A genome is one index per gene, drawn from a common alphabet: for the
  Halbach problem, which ring candidate each ring position uses. This is the
  port of the DEAP loop of homogeneityOptimisation.py, with the same operators
  at the same rates, but with the population living in a Kokkos view and a
  whole generation produced by a single parallel region.

  The one operator that is not a port is the mutation. DEAP's mutFlipBit
  applies "not" to whatever it is given, which turns a gene of the nineteen
  candidates into a zero or a one and quietly collapses most of the search
  space. A mutated gene is redrawn from the alphabet here, which is what a
  mutation on an integer genome is meant to do.
*/
class GeneticOptimizer {

  public:

    GeneticOptimizer(const GeneticSettings& settings,
                     const size_t& num_genes, const size_t& num_candidates);

    ~GeneticOptimizer();

    /*
      Runs the evolution and returns the best genome it ever saw.

      "Ever" rather than "at the end": a generation can lose its best
      individual to a crossover, so the best of the last generation is not
      the best of the run. The elite carried over makes the two agree in
      practice, and this makes them agree by construction.
    */
    std::vector<uint16_t> run(const HomogeneityObjective& objective,
                              GenerationSink* sink, const bool& verbose);

    float getBestFitness() const;

    const std::vector<GenerationRecord>& getHistory() const;

    /* Whether a sink asked the run to stop before the last generation. */
    bool wasStopped() const;

  private:

    /*
      Selection, crossover and mutation of the whole population, in one
      parallel region over pairs of offspring. They fuse because selection
      only reads the fitness of the generation that is being replaced, so
      nothing here has to wait for anything else here.
    */
    void reproduce(const PopulationView& from, const PopulationView& into,
                   const FitnessView& fitness);

    GeneticSettings settings;

    size_t num_genes;
    size_t num_candidates;

    RandPoolType random_pool;

    std::vector<uint16_t> best_genome;
    float best_fitness;

    std::vector<GenerationRecord> history;

    bool stopped = false;
};

}  // namespace optimization
}  // namespace greeter

#endif  // GENETIC_OPTIMIZER_H
