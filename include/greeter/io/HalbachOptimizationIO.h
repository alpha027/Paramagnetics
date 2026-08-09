#ifndef HALBACH_OPTIMIZATION_IO_H
#define HALBACH_OPTIMIZATION_IO_H

#include <greeter/optimization/HalbachSolution.h>
#include <greeter/optimization/HalbachSpec.h>
#include <nlohmann/json.hpp>
#include <string>


namespace greeter {

/* Whether the written file lists the rings or the magnets they stand for. */
enum class HalbachEmit {

    /*
      One "halbach_ring" per ring: forty six objects for a magnet of two
      thousand nine hundred, and the form the optimizer actually decides in,
      so a reader can see which candidate went where.
    */
    Arrangements,

    /* Every magnet written out one by one, for a reader that wants them. */
    Magnets
};

/*
  Reads what an optimization is asked to do, and writes what it decided.

  What comes out is an input file of this library, not a report: it has
  "arrangements" and a "field_of_view", it passes MagnetIO::validateJSON, and
  the standalone simulator runs it as it stands. What the optimization found
  travels in a "halbach_optimization" object beside them, which the readers
  ignore, so the one file is both the answer and the record of how it was
  arrived at.
*/
class HalbachOptimizationIO {

  public:

    HalbachOptimizationIO();
    ~HalbachOptimizationIO();

    /*
      Reads a specification.

      Either the whole file is the configuration, or it carries it under
      "halbach_optimization", which is what the written file does. That is
      what lets the output of one run be the input of the next.
    */
    static greeter::optimization::HalbachSpec readSpec(const nlohmann::json& data);

    static greeter::optimization::HalbachSpec readSpecFile(const std::string& path);

    /* The solution as an input file that describes the optimized magnet. */
    static nlohmann::json write(
        const greeter::optimization::HalbachSolution& solution,
        const HalbachEmit& emit);

    static void writeFile(
        const greeter::optimization::HalbachSolution& solution,
        const HalbachEmit& emit, const std::string& path);

    /* The convergence curve, one row a generation. */
    static void writeHistoryCSV(
        const greeter::optimization::HalbachSolution& solution,
        const std::string& path);

    /*
      One magnet in the schema of an input file.

      Only the shapes an optimized Halbach magnet is built out of, which is
      the cuboid and the dipole. Anything else is refused by name rather than
      written out wrongly.
    */
    static nlohmann::json writeMagnet(const greeter::MagnetCollection& collection,
                                      const size_t& index, const int64_t& id);
};

}  // namespace greeter

#endif  // HALBACH_OPTIMIZATION_IO_H
