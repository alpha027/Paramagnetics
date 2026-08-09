#include <greeter/KokkosDefines.h>
#include <greeter/io/HalbachOptimizationIO.h>
#include <greeter/optimization/HalbachOptimizer.h>

#include <cxxopts.hpp>

#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>


namespace {

/*
  Says how the evolution is going, on one line a generation that rewrites
  itself. A hundred generations of ten thousand individuals is a wait, and a
  wait with nothing on the screen looks like a hang.
*/
class ConsoleGenerations: public greeter::optimization::GenerationSink {

  public:

    explicit ConsoleGenerations(const uint32_t& _total): total(_total) {}

    bool onGeneration(
        const greeter::optimization::GenerationRecord& record) override {

        std::cout << "\r  generation " << record.generation << " of " << total
                  << ", best " << std::fixed << std::setprecision(1)
                  << record.best_ever << " ppm          " << std::flush;

        if (record.generation == total) {
            std::cout << std::endl;
        }

        return true;
    }

  private:

    uint32_t total;
};


void report(const greeter::optimization::HalbachSolution& solution) {

    const std::vector<float> positions =
        solution.spec.getSymmetricRingPositions();

    std::cout << std::endl << "Chosen rings" << std::endl;

    for (size_t gene = 0; gene < solution.genome.size(); gene++) {

        const greeter::optimization::RingCandidate& candidate =
            solution.spec.candidates[solution.genome[gene]];

        std::cout << "  z = " << std::fixed << std::setprecision(4)
                  << positions[gene] << " m   candidate "
                  << solution.genome[gene] << "   inner "
                  << std::setprecision(3) << candidate.radius << " m of "
                  << candidate.count << "   outer "
                  << candidate.radius + solution.spec.outer_radius_offset
                  << " m of "
                  << candidate.count + solution.spec.outer_count_offset
                  << std::endl;
    }

    std::cout << std::endl
              << "Basis:            " << solution.basis_configurations
              << " configurations, " << solution.basis_magnets
              << " magnets sampled at " << solution.optimized_points
              << " points" << std::endl
              << "Magnets:          " << solution.num_magnets << std::endl;

    std::cout << std::setprecision(2);

    std::cout << "Optimized over " << solution.optimized_points
              << " points: " << std::fixed << std::setprecision(1)
              << solution.optimized.ppm << " ppm, mean "
              << 1e3f * solution.optimized.mean << " mT" << std::endl;

    if (solution.was_verified) {
        std::cout << "Verified over " << solution.verified_points
                  << " points:  " << solution.verified.ppm << " ppm, mean "
                  << 1e3f * solution.verified.mean << " mT, peak to peak "
                  << std::setprecision(4)
                  << 1e3f * (solution.verified.max - solution.verified.min)
                  << " mT" << std::endl;
    }

    std::cout << std::setprecision(2)
              << "Time: basis " << solution.basis_seconds << " s, evolution "
              << solution.evolution_seconds << " s";

    if (solution.was_verified) {
        std::cout << ", verification " << solution.verification_seconds << " s";
    }

    std::cout << std::endl;
}

}  // namespace


auto main(int argc, char** argv) -> int {

    cxxopts::Options options(
        "halbach-optimizer",
        "ParaMagneticS, homogeneity optimization of a Halbach cylinder");

    options.add_options()
        ("c,config", "Configuration file. Without one the defaults are those "
                     "of HalbachOptimisation/homogeneityOptimisation.py",
         cxxopts::value<std::string>())
        ("o,output", "Where to write the optimized magnet, as an input file "
                     "this project's simulator runs",
         cxxopts::value<std::string>()->default_value("output/halbach_optimized.json"))
        ("history", "Also write the convergence curve, one row a generation",
         cxxopts::value<std::string>())
        ("emit", "Write the result as \"arrangements\" or as \"magnets\"",
         cxxopts::value<std::string>()->default_value("arrangements"))
        ("field-model", "\"cuboid\" for the analytic kernel of this library, "
                        "\"dipole\" for the far field the Python script uses",
         cxxopts::value<std::string>())
        ("objective", "\"ppm_bx\" or \"ppm_bmag\"",
         cxxopts::value<std::string>())
        ("symmetry", "Part of the sphere the search is measured over: "
                     "\"octant\", \"hemisphere\" or \"full\"",
         cxxopts::value<std::string>())
        ("g,generations", "Generations to evolve", cxxopts::value<uint32_t>())
        ("p,population", "Individuals in a generation", cxxopts::value<uint32_t>())
        ("s,seed", "Seed of the random number pool", cxxopts::value<uint64_t>())
        ("no-verify", "Skip measuring the answer over the whole sphere")
        ("q,quiet", "Do not announce every step")
        ("h,help", "Print this");

    /*
      Kokkos reads its own "--kokkos-..." arguments out of the command line,
      and this parser refuses anything it does not know about, which is worth
      keeping: a mistyped option is a run that quietly does something else.
      So the two are separated rather than the parser being made permissive,
      and Kokkos still sees the whole of the original line.
    */
    std::vector<char*> arguments;
    arguments.reserve(argc);

    for (int i = 0; i < argc; i++) {
        if (std::string(argv[i]).rfind("--kokkos-", 0) != 0) {
            arguments.push_back(argv[i]);
        }
    }

    int filtered_argc = (int) arguments.size();
    char** filtered_argv = arguments.data();

    cxxopts::ParseResult parsed;

    try {
        parsed = options.parse(filtered_argc, filtered_argv);
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << std::endl;
        return 1;
    }

    if (parsed.count("help")) {
        std::cout << options.help() << std::endl;
        return 0;
    }

    Kokkos::initialize(argc, argv);

    int status = 0;

    // In its own scope: everything holding a Kokkos view has to be gone before
    // Kokkos::finalize, and a solution that outlives it takes the process down.
    {
        try {

            greeter::optimization::HalbachSpec spec;

            if (parsed.count("config")) {
                spec = greeter::HalbachOptimizationIO::readSpecFile(
                    parsed["config"].as<std::string>());
            }

            // The command line has the last word over the file, so that a
            // shorter run of the same configuration does not need a second one.
            nlohmann::json overrides = nlohmann::json::object();

            if (parsed.count("field-model")) {
                overrides["field_model"] = parsed["field-model"].as<std::string>();
            }
            if (parsed.count("objective")) {
                overrides["objective"] = parsed["objective"].as<std::string>();
            }
            if (parsed.count("symmetry")) {
                overrides["symmetry"] = parsed["symmetry"].as<std::string>();
            }
            if (parsed.count("generations")) {
                overrides["genetic"]["generations"] =
                    parsed["generations"].as<uint32_t>();
            }
            if (parsed.count("population")) {
                overrides["genetic"]["population"] =
                    parsed["population"].as<uint32_t>();
            }
            if (parsed.count("seed")) {
                overrides["genetic"]["seed"] = parsed["seed"].as<uint64_t>();
            }

            if (!overrides.empty()) {
                spec.readJSON(overrides);
            }

            const std::string emit_name = parsed["emit"].as<std::string>();

            if (emit_name != "arrangements" && emit_name != "magnets") {
                throw std::invalid_argument(
                    "--emit is \"arrangements\" or \"magnets\", not \""
                    + emit_name + "\"");
            }

            const greeter::HalbachEmit emit =
                emit_name == "magnets" ? greeter::HalbachEmit::Magnets
                                       : greeter::HalbachEmit::Arrangements;

            greeter::optimization::RunOptions run_options;
            run_options.verbose = false;
            run_options.verify = !parsed.count("no-verify");

            const bool quiet = parsed.count("quiet") > 0;

            ConsoleGenerations generations(spec.genetic.generations);

            const greeter::optimization::HalbachSolution solution =
                greeter::optimization::HalbachOptimizer::run(
                    spec, quiet ? nullptr : &generations, run_options);

            if (!quiet) {
                report(solution);
            }

            const std::string output_path = parsed["output"].as<std::string>();

            greeter::HalbachOptimizationIO::writeFile(solution, emit, output_path);

            std::cout << "Written to " << output_path << std::endl;

            if (parsed.count("history")) {

                const std::string history_path =
                    parsed["history"].as<std::string>();

                greeter::HalbachOptimizationIO::writeHistoryCSV(
                    solution, history_path);

                std::cout << "Convergence written to " << history_path
                          << std::endl;
            }

        } catch (const std::exception& error) {
            std::cerr << "Error: " << error.what() << std::endl;
            status = 1;
        }
    }

    Kokkos::finalize();

    return status;
}
