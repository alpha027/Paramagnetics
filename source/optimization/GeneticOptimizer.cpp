#include <greeter/optimization/GeneticOptimizer.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <numeric>
#include <map>
#include <stdexcept>
#include <filesystem>
#include <fstream>


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
    if (!std::isfinite(settings.improvement_ppm) || settings.improvement_ppm < 0.0f) {
        throw std::invalid_argument("Minimum improvement must be finite and nonnegative");
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
    const bool& verbose, const GeneticRunOptions& options) {

    if (objective.getNumGenes() != num_genes
        || objective.getNumCandidates() != num_candidates) {
        throw std::invalid_argument(
            "The objective and the optimizer disagree about the shape of a genome");
    }

    history.clear();
    candidates.clear();
    if (options.archive_size > 1024)
        throw std::invalid_argument("Keep at most 1024 nominal finalists");
    stopped = false;
    stalled = false;
    best_fitness = std::numeric_limits<float>::max();
    best_genome.assign(num_genes, 0);
    random_pool = RandPoolType(settings.seed);

    if (options.checkpoint_every == 0) {
        throw std::invalid_argument("Checkpoint interval must be at least one generation");
    }
    const nlohmann::json signature = {
        {"population", settings.population}, {"genes", num_genes},
        {"candidates", num_candidates}, {"seed", settings.seed},
        {"cx_prob", settings.cx_prob}, {"mut_prob", settings.mut_prob},
        {"gene_mut_prob", settings.gene_mut_prob}, {"tournament", settings.tournament},
        {"elitism", settings.elitism}, {"backend", ExecSpace::name()},
        {"concurrency", ExecSpace().concurrency()}, {"problem", options.problem}
    };
    nlohmann::json resumed;
    uint32_t start_generation = 0;
    if (!options.resume_path.empty()) {
        std::ifstream file(options.resume_path);
        if (!file) throw std::invalid_argument("Could not open checkpoint " + options.resume_path);
        file >> resumed;
        if (resumed.at("version") != 1 || resumed.at("signature") != signature) {
            throw std::invalid_argument("Checkpoint requires the same problem, operators, backend and thread count");
        }
        const auto generation = resumed.at("generation").get<int64_t>();
        if (generation < 0 || generation > settings.generations) {
            throw std::invalid_argument("Checkpoint generation exceeds the requested search length");
        }
        start_generation = (uint32_t) generation;
        if (!resumed.at("population").is_array() || resumed.at("population").size() != settings.population) {
            throw std::invalid_argument("Invalid checkpoint population size");
        }
        auto readGenome = [&](const nlohmann::json& row) {
            if (!row.is_array() || row.size() != num_genes) {
                throw std::invalid_argument("Invalid checkpoint genome size");
            }
            std::vector<uint16_t> genome;
            for (const auto& value : row) {
                if (!value.is_number_integer() || value.get<int64_t>() < 0 ||
                    value.get<int64_t>() >= (int64_t) num_candidates) {
                    throw std::invalid_argument("Checkpoint gene is outside the candidate list");
                }
                genome.push_back(value.get<uint16_t>());
            }
            return genome;
        };
        best_genome = readGenome(resumed.at("best_genome"));
        best_fitness = resumed.at("best_fitness").get<float>();
        const float measured = objective.evaluateGenome(best_genome).ppm;
        if (!std::isfinite(best_fitness) || best_fitness < 0.0f ||
            std::fabs(measured - best_fitness) > 1e-3f * std::max(1.0f, measured)) {
            throw std::invalid_argument("Checkpoint fitness does not match its best genome");
        }
        for (const auto& row : resumed.at("population")) readGenome(row);
        for (const auto& item : resumed.at("history")) {
            GenerationRecord record;
            record.generation = item.at("generation").get<uint32_t>();
            record.best = item.at("best").get<float>();
            record.best_ever = item.at("best_ever").get<float>();
            record.mean = item.at("mean").get<float>();
            record.seconds = item.at("seconds").get<float>();
            if (record.generation != history.size() || !std::isfinite(record.best) ||
                !std::isfinite(record.best_ever) || !std::isfinite(record.mean)) {
                throw std::invalid_argument("Invalid checkpoint history");
            }
            history.push_back(record);
        }
        if (history.size() != (size_t) start_generation + 1) {
            throw std::invalid_argument("Checkpoint history does not reach its generation");
        }
        history.pop_back(); // Re-score the saved population and report this generation once.
    }

    PopulationView population("population", settings.population, num_genes);
    PopulationView offspring("offspring", settings.population, num_genes);

    FitnessView fitness("fitness", settings.population);

    // A first generation drawn uniformly from the alphabet, which is what
    // toolbox.attr_bool does in the script this ports.
    if (resumed.is_null()) {
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
    } else {
        auto host = Kokkos::create_mirror_view(population);
        for (size_t row = 0; row < settings.population; ++row) {
            for (size_t gene = 0; gene < num_genes; ++gene) {
                host(row, gene) = resumed["population"][row][gene].get<uint16_t>();
            }
        }
        Kokkos::deep_copy(population, host);
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
    uint32_t last_improvement = resumed.is_null() ? 0 : resumed.at("last_improvement").get<uint32_t>();
    float significant_best = resumed.is_null() ? std::numeric_limits<float>::max()
                                              : resumed.at("significant_best").get<float>();
    if (last_improvement > start_generation || !std::isfinite(significant_best)) {
        throw std::invalid_argument("Invalid checkpoint stagnation state");
    }

    auto checkpoint = [&](uint32_t generation) {
        if (options.checkpoint_path.empty()) return;
        Kokkos::deep_copy(host_population, population);
        nlohmann::json rows = nlohmann::json::array();
        for (size_t row = 0; row < settings.population; ++row) {
            nlohmann::json genome = nlohmann::json::array();
            for (size_t gene = 0; gene < num_genes; ++gene) genome.push_back(host_population(row, gene));
            rows.push_back(std::move(genome));
        }
        nlohmann::json records = nlohmann::json::array();
        for (const auto& r : history) records.push_back({
            {"generation", r.generation}, {"best", r.best}, {"best_ever", r.best_ever},
            {"mean", r.mean}, {"seconds", r.seconds}});
        const nlohmann::json data = {
            {"version", 1}, {"signature", signature}, {"generation", generation},
            {"population", rows}, {"best_genome", best_genome}, {"best_fitness", best_fitness}, {"history", records},
            {"last_improvement", last_improvement}, {"significant_best", significant_best}
        };
        const std::filesystem::path path(options.checkpoint_path);
        if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
        auto temporary = path;
        temporary += ".tmp";
        std::ofstream file;
        file.exceptions(std::ios::failbit | std::ios::badbit);
        file.open(temporary);
        file << data.dump();
        file.close();
        std::filesystem::rename(temporary, path);
    };

    for (uint32_t generation = start_generation; generation <= settings.generations;
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

        if (options.archive_size > 0) {
            Kokkos::deep_copy(host_population, population);
            std::map<std::vector<uint16_t>, float> distinct;
            for (const auto& c : candidates) distinct.emplace(c.genome, c.fitness);
            for (size_t row = 0; row < settings.population; ++row) {
                if (!std::isfinite(host_fitness(row))) continue;
                std::vector<uint16_t> genome(num_genes);
                for (size_t gene = 0; gene < num_genes; ++gene)
                    genome[gene] = host_population(row, gene);
                distinct.emplace(std::move(genome), host_fitness(row));
            }
            if (best_fitness < std::numeric_limits<float>::max())
                distinct.emplace(best_genome, best_fitness);
            candidates.clear();
            for (const auto& c : distinct) candidates.push_back({c.first, c.second});
            std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
                return a.fitness != b.fitness ? a.fitness < b.fitness
                                               : a.genome < b.genome;
            });
            if (candidates.size() > options.archive_size) candidates.resize(options.archive_size);
        }

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

        if (significant_best - generation_best > settings.improvement_ppm) {
            significant_best = generation_best;
            last_improvement = generation;
        }

        if (verbose) {
            std::cout << "  generation " << generation << ": best "
                      << generation_best << " ppm, best ever " << best_fitness
                      << " ppm" << std::endl;
        }

        const bool cancelled = sink != nullptr && !sink->onGeneration(record);
        const bool stagnant = settings.stall_generations > 0 &&
            generation - last_improvement >= settings.stall_generations;
        if (generation % options.checkpoint_every == 0 || cancelled || stagnant ||
            generation == settings.generations) checkpoint(generation);

        if (cancelled) {
            stopped = true;
            break;
        }

        if (stagnant) {
            stopped = true;
            stalled = true;
            break;
        }

        // The last pass through is a report on the final population, not a
        // generation of its own.
        if (generation == settings.generations) {
            break;
        }

        // Each generation owns its random stream, so resuming does not depend
        // on opaque random-pool state from preceding generations.
        random_pool = RandPoolType(settings.seed + 0x9e3779b97f4a7c15ULL * (generation + 1ULL));
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

bool GeneticOptimizer::wasStalled() const {
    return stalled;
}

}  // namespace optimization
}  // namespace greeter
