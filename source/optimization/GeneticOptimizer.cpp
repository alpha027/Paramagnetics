#include <greeter/optimization/GeneticOptimizer.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>


namespace greeter {
namespace optimization {

namespace {

typedef RandPoolType::generator_type Generator;

/*
  The best of a few individuals drawn at random.

  Tournament selection, at the size the Python script uses. It is the whole
  of the selection pressure here: a larger tournament converges sooner and
  explores less.
*/
KOKKOS_INLINE_FUNCTION
uint32_t tournament(Generator& generator, const FitnessView& fitness,
                    const uint32_t& population_size, const uint32_t& size) {

    uint32_t winner = Kokkos::rand<Generator, uint32_t>::draw(
        generator, population_size);

    for (uint32_t i = 1; i < size; i++) {

        const uint32_t challenger = Kokkos::rand<Generator, uint32_t>::draw(
            generator, population_size);

        if (fitness(challenger) < fitness(winner)) {
            winner = challenger;
        }
    }

    return winner;
}

}  // namespace


GeneticOptimizer::GeneticOptimizer(const GeneticSettings& _settings,
                                   const size_t& _num_genes,
                                   const size_t& _num_candidates):
    settings(_settings),
    num_genes(_num_genes),
    num_candidates(_num_candidates),
    random_pool(_settings.seed),
    best_fitness(std::numeric_limits<float>::max()) {

    if (num_genes == 0) {
        throw std::invalid_argument("A genome needs at least one gene");
    }

    if (num_candidates < 1) {
        throw std::invalid_argument("A gene needs at least one candidate value");
    }

    if (settings.population < 2) {
        throw std::invalid_argument("A population needs at least two individuals");
    }

    if (settings.elitism >= settings.population) {
        throw std::invalid_argument(
            "The elitism carries over the whole population");
    }
}


GeneticOptimizer::~GeneticOptimizer() {}


void GeneticOptimizer::reproduce(const PopulationView& from,
                                 const PopulationView& into,
                                 const FitnessView& fitness) {

    const uint32_t population_size = settings.population;
    const uint32_t the_num_genes = (uint32_t) num_genes;
    const uint32_t alphabet = (uint32_t) num_candidates;

    const uint32_t tournament_size = settings.tournament;

    const float cx_prob = settings.cx_prob;
    const float mut_prob = settings.mut_prob;
    const float gene_mut_prob = settings.gene_mut_prob;

    RandPoolType pool = random_pool;

    // Offspring are made two at a time because a crossover needs two of them,
    // which is also what lets the whole generation be one parallel region: a
    // pair is the unit nothing outside of it touches.
    const uint32_t pairs = (population_size + 1) / 2;

    Kokkos::parallel_for(
        "reproduce", Kokkos::RangePolicy<ExecSpace>(0, pairs),
        KOKKOS_LAMBDA(const uint32_t pair) {

            Generator generator = pool.get_state();

            const uint32_t first = 2 * pair;
            const uint32_t second = first + 1;
            const bool has_second = second < population_size;

            const uint32_t parent_a =
                tournament(generator, fitness, population_size, tournament_size);
            const uint32_t parent_b =
                tournament(generator, fitness, population_size, tournament_size);

            for (uint32_t gene = 0; gene < the_num_genes; gene++) {
                into(first, gene) = from(parent_a, gene);
                if (has_second) {
                    into(second, gene) = from(parent_b, gene);
                }
            }

            // Two point crossover: the stretch between two cut points changes
            // hands. With one gene there is nothing to cut.
            if (has_second && the_num_genes > 1
                && generator.frand() < cx_prob) {

                uint32_t cut_a = Kokkos::rand<Generator, uint32_t>::draw(
                    generator, the_num_genes);
                uint32_t cut_b = Kokkos::rand<Generator, uint32_t>::draw(
                    generator, the_num_genes);

                if (cut_a > cut_b) {
                    const uint32_t swap = cut_a;
                    cut_a = cut_b;
                    cut_b = swap;
                }

                for (uint32_t gene = cut_a; gene <= cut_b; gene++) {
                    const uint16_t swap = into(first, gene);
                    into(first, gene) = into(second, gene);
                    into(second, gene) = swap;
                }
            }

            /*
              Mutation, in two stages as DEAP applies it: an individual is
              picked at all with one probability, and then each of its genes
              moves with another. A gene that moves is redrawn from the
              alphabet, see the note on mutFlipBit in the header.
            */
            for (uint32_t child = 0; child < 2; child++) {

                if (child == 1 && !has_second) {
                    break;
                }

                if (generator.frand() >= mut_prob) {
                    continue;
                }

                const uint32_t row = child == 0 ? first : second;

                for (uint32_t gene = 0; gene < the_num_genes; gene++) {
                    if (generator.frand() < gene_mut_prob) {
                        into(row, gene) = (uint16_t)
                            Kokkos::rand<Generator, uint32_t>::draw(
                                generator, alphabet);
                    }
                }
            }

            pool.free_state(generator);
        });

    Kokkos::fence();
}


std::vector<uint16_t> GeneticOptimizer::run(
    const HomogeneityObjective& objective, GenerationSink* sink,
    const bool& verbose) {

    if (objective.getNumGenes() != num_genes
        || objective.getNumCandidates() != num_candidates) {
        throw std::invalid_argument(
            "The objective and the optimizer disagree about the shape of a genome");
    }

    history.clear();
    stopped = false;
    best_fitness = std::numeric_limits<float>::max();
    best_genome.assign(num_genes, 0);

    PopulationView population("population", settings.population, num_genes);
    PopulationView offspring("offspring", settings.population, num_genes);

    FitnessView fitness("fitness", settings.population);

    // A first generation drawn uniformly from the alphabet, which is what
    // toolbox.attr_bool does in the script this ports.
    {
        RandPoolType pool = random_pool;
        const uint32_t alphabet = (uint32_t) num_candidates;
        const uint32_t the_num_genes = (uint32_t) num_genes;

        Kokkos::parallel_for(
            "seed_population",
            Kokkos::RangePolicy<ExecSpace>(0, settings.population),
            KOKKOS_LAMBDA(const uint32_t individual) {

                Generator generator = pool.get_state();

                for (uint32_t gene = 0; gene < the_num_genes; gene++) {
                    population(individual, gene) = (uint16_t)
                        Kokkos::rand<Generator, uint32_t>::draw(generator, alphabet);
                }

                pool.free_state(generator);
            });

        Kokkos::fence();
    }

    objective.evaluate(population, fitness);

    typename FitnessView::HostMirror host_fitness =
        Kokkos::create_mirror_view(fitness);

    /*
      A host buffer of its own rather than a mirror of `population`.

      The two population buffers are swapped every generation, and a mirror
      would go on naming whichever one it was made from: on a host build it
      *is* that buffer, so after a swap the copy below would write the new
      offspring over the buffer they are being made in. Re-making the mirror
      each generation would work and would allocate once a generation.
    */
    Kokkos::View<uint16_t**, Layout, Kokkos::HostSpace> host_population(
        "host_population", settings.population, num_genes);

    // Where the elite of a generation are, kept on the device so that copying
    // them over is a parallel region rather than a row at a time.
    Kokkos::View<uint32_t*, Layout, MemSpace> elite_rows(
        "elite_rows", settings.elitism > 0 ? settings.elitism : 1);

    typename Kokkos::View<uint32_t*, Layout, MemSpace>::HostMirror
        host_elite_rows = Kokkos::create_mirror_view(elite_rows);

    std::vector<uint32_t> order(settings.population);

    Kokkos::Timer timer;

    // The clock is read at the top of each pass, so what a record holds is the
    // time from the start of the previous pass: the making and the scoring of
    // the generation it describes. Timing only the bookkeeping below it would
    // report tens of microseconds for work that took a second.
    double previous = 0.0;

    for (uint32_t generation = 0; generation <= settings.generations;
         generation++) {

        const double now = timer.seconds();
        const double elapsed = now - previous;
        previous = now;

        Kokkos::deep_copy(host_fitness, fitness);

        /*
          The generation is summed up on the host. It is a few thousand floats
          against a parallel region that has just read tens of millions, so
          the copy costs nothing, and it buys the mean, the best and the
          ranking the elite are taken from without three more reductions.
        */
        uint32_t best_index = 0;
        double total = 0.0;

        for (uint32_t i = 0; i < settings.population; i++) {
            total += (double) host_fitness(i);
            if (host_fitness(i) < host_fitness(best_index)) {
                best_index = i;
            }
        }

        const float generation_best = host_fitness(best_index);

        if (generation_best < best_fitness) {

            best_fitness = generation_best;

            Kokkos::deep_copy(host_population, population);

            for (size_t gene = 0; gene < num_genes; gene++) {
                best_genome[gene] = host_population(best_index, gene);
            }
        }

        GenerationRecord record;
        record.generation = generation;
        record.best = generation_best;
        record.best_ever = best_fitness;
        record.mean = (float) (total / (double) settings.population);
        record.seconds = (float) elapsed;

        history.push_back(record);

        if (verbose) {
            std::cout << "  generation " << generation << ": best "
                      << generation_best << " ppm, best ever " << best_fitness
                      << " ppm" << std::endl;
        }

        if (sink != nullptr && !sink->onGeneration(record)) {
            stopped = true;
            break;
        }

        // The last pass through is a report on the final population, not a
        // generation of its own.
        if (generation == settings.generations) {
            break;
        }

        reproduce(population, offspring, fitness);

        /*
          The best few of the outgoing generation are put back untouched.
          Without this a crossover can throw away the best genome found so
          far, and the population is free to drift back uphill; the Python
          script keeps its best in a variable on the side and lets exactly
          that happen.
        */
        if (settings.elitism > 0) {

            std::iota(order.begin(), order.end(), 0u);

            const size_t keep =
                std::min((size_t) settings.elitism, order.size());

            std::partial_sort(
                order.begin(), order.begin() + keep, order.end(),
                [&](const uint32_t& a, const uint32_t& b) {
                    return host_fitness(a) < host_fitness(b);
                });

            for (size_t i = 0; i < keep; i++) {
                host_elite_rows(i) = order[i];
            }

            Kokkos::deep_copy(elite_rows, host_elite_rows);

            const uint32_t the_num_genes = (uint32_t) num_genes;
            const PopulationView from = population;
            const PopulationView into = offspring;
            const Kokkos::View<uint32_t*, Layout, MemSpace> rows = elite_rows;

            Kokkos::parallel_for(
                "carry_elite",
                Kokkos::RangePolicy<ExecSpace>(0, (uint32_t) keep),
                KOKKOS_LAMBDA(const uint32_t i) {
                    for (uint32_t gene = 0; gene < the_num_genes; gene++) {
                        into(i, gene) = from(rows(i), gene);
                    }
                });

            Kokkos::fence();
        }

        // The offspring become the population. Two buffers swapped rather
        // than one copied, so a generation costs no traffic beyond making it.
        PopulationView swap = population;
        population = offspring;
        offspring = swap;

        objective.evaluate(population, fitness);
    }

    return best_genome;
}


float GeneticOptimizer::getBestFitness() const {
    return best_fitness;
}

const std::vector<GenerationRecord>& GeneticOptimizer::getHistory() const {
    return history;
}

bool GeneticOptimizer::wasStopped() const {
    return stopped;
}

}  // namespace optimization
}  // namespace greeter
