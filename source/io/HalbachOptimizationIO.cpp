#include <greeter/io/HalbachOptimizationIO.h>

#include <greeter/CubicMagnet.h>
#include <greeter/DipoleMagnet.h>

#include <cmath>
#include <fstream>
#include <stdexcept>


namespace greeter {

namespace {

using greeter::optimization::HalbachSolution;
using greeter::optimization::HalbachSpec;
using greeter::optimization::FieldMetrics;
using greeter::optimization::tidy;

/* The three numbers a vector of a JSON file is written as. */
nlohmann::json vector3(const float* values) {
    return nlohmann::json::array({tidy(values[0]), tidy(values[1]), tidy(values[2])});
}

/*
  The box the optimization measured in, as a "field_of_view".

  The written file is meant to be runnable, and a file with no field of view
  and no force section is not one this library reads at all. The grid is the
  one HalbachBasis samples, so a run of the written file lands on the points
  the answer was measured at.
*/
nlohmann::json fieldOfView(const HalbachSpec& spec) {

    const int64_t steps = std::lround(spec.dsv / spec.resolution);

    const float half = 0.5f * spec.dsv;

    const nlohmann::json axis = {
        {"min", tidy(-half)}, {"max", tidy(half)}, {"n", steps + 1}
    };

    return nlohmann::json{{"x", axis}, {"y", axis}, {"z", axis}};
}

nlohmann::json metrics(const FieldMetrics& measured, const size_t& points) {
    return nlohmann::json{
        {"homogeneity_ppm", tidy(measured.ppm)},
        {"mean_T", tidy(measured.mean)},
        {"min_T", tidy(measured.min)},
        {"max_T", tidy(measured.max)},
        {"peak_to_peak_T", tidy(measured.max - measured.min)},
        {"points", points}
    };
}

}  // namespace


HalbachOptimizationIO::HalbachOptimizationIO() {}

HalbachOptimizationIO::~HalbachOptimizationIO() {}


greeter::optimization::HalbachSpec HalbachOptimizationIO::readSpec(
    const nlohmann::json& data) {

    greeter::optimization::HalbachSpec spec;

    if (data.contains("halbach_optimization")) {
        spec.readJSON(data["halbach_optimization"]);
    } else {
        spec.readJSON(data);
    }

    return spec;
}


greeter::optimization::HalbachSpec HalbachOptimizationIO::readSpecFile(
    const std::string& path) {

    std::ifstream file(path);

    if (!file.is_open()) {
        throw std::invalid_argument("Could not open " + path);
    }

    return readSpec(nlohmann::json::parse(file));
}


nlohmann::json HalbachOptimizationIO::writeMagnet(
    const greeter::MagnetCollection& collection, const size_t& index,
    const int64_t& id) {

    const std::vector<float> parameters = collection.getMagnetParameters(index);
    const uint16_t type = collection.getMagnetTypeID(index);

    // position (3), orientation (4), the geometry of the shape, magnetization
    // (3). See MagnetParameters.h, which is the one place that layout is set.
    const float* position = parameters.data();
    const float* orientation = parameters.data() + 3;
    const float* dimensions = parameters.data() + 7;

    if (type == greeter::CuboidMagnet::getStaticTypeID()) {

        return nlohmann::json{
            {"id", id},
            {"type", greeter::CuboidMagnet::getStaticTypeName()},
            {"parameters", {
                {"dimensions", vector3(dimensions)},
                {"magnetization", vector3(dimensions + 3)},
                {"position", vector3(position)},
                {"orientation", nlohmann::json::array({
                    tidy(orientation[0]), tidy(orientation[1]),
                    tidy(orientation[2]), tidy(orientation[3])
                })}
            }}
        };
    }

    if (type == greeter::DipoleMagnet::getStaticTypeID()) {

        // A dipole has no geometry at all, so its moment sits where the
        // dimensions of a shaped magnet would start.
        return nlohmann::json{
            {"id", id},
            {"type", greeter::DipoleMagnet::getStaticTypeName()},
            {"parameters", {
                {"moment", vector3(dimensions)},
                {"position", vector3(position)},
                {"orientation", nlohmann::json::array({
                    tidy(orientation[0]), tidy(orientation[1]),
                    tidy(orientation[2]), tidy(orientation[3])
                })}
            }}
        };
    }

    throw std::invalid_argument(
        "A Halbach optimization writes out cuboids and dipoles, not the magnet "
        "type " + std::to_string(type));
}


nlohmann::json HalbachOptimizationIO::write(
    const HalbachSolution& solution, const HalbachEmit& emit) {

    nlohmann::json record = solution.spec.toJSON();

    nlohmann::json genome = nlohmann::json::array();
    for (const auto& gene : solution.genome) {
        genome.push_back(gene);
    }

    nlohmann::json chosen = nlohmann::json::array();

    const std::vector<float> positions = solution.spec.getSymmetricRingPositions();
    const std::vector<greeter::optimization::RingCandidate> candidates =
        solution.getChosenCandidates();

    for (size_t gene = 0; gene < candidates.size(); gene++) {
        chosen.push_back({
            {"ring_position", tidy(positions[gene])},
            {"candidate", solution.genome[gene]},
            {"inner_radius", tidy(candidates[gene].radius)},
            {"inner_count", candidates[gene].count},
            {"outer_radius",
             tidy(candidates[gene].radius + solution.spec.outer_radius_offset)},
            {"outer_count",
             candidates[gene].count + solution.spec.outer_count_offset}
        });
    }

    record["genome"] = genome;
    record["chosen"] = chosen;
    record["magnet_count"] = solution.num_magnets;

    record["basis"] = {
        {"configurations", solution.basis_configurations},
        {"magnets", solution.basis_magnets},
        {"points", solution.optimized_points}
    };

    record["optimized"] = metrics(solution.optimized, solution.optimized_points);

    if (solution.was_verified) {
        record["verified"] = metrics(solution.verified, solution.verified_points);
    }

    record["timing_s"] = {
        {"basis", solution.basis_seconds},
        {"evolution", solution.evolution_seconds},
        {"verification", solution.verification_seconds}
    };

    record["generations_run"] =
        solution.history.empty() ? 0 : solution.history.back().generation;

    nlohmann::json data;

    // First, so that a person opening the file reads what was found before
    // the two thousand magnets it was found in.
    data["halbach_optimization"] = record;

    if (emit == HalbachEmit::Magnets) {

        const greeter::MagnetCollection collection = solution.buildCollection();

        nlohmann::json magnets = nlohmann::json::array();

        for (size_t i = 0; i < collection.get_num_magnets(); i++) {
            magnets.push_back(writeMagnet(collection, i, (int64_t) i + 1));
        }

        data["magnets"] = magnets;

    } else {

        nlohmann::json arrangements = nlohmann::json::array();

        for (const auto& ring : solution.buildArrangements()) {
            arrangements.push_back(ring);
        }

        data["arrangements"] = arrangements;
    }

    data["field_of_view"] = fieldOfView(solution.spec);

    return data;
}


void HalbachOptimizationIO::writeFile(
    const HalbachSolution& solution, const HalbachEmit& emit,
    const std::string& path) {

    std::ofstream file(path);

    if (!file.is_open()) {
        throw std::invalid_argument("Could not open " + path + " for writing");
    }

    file << write(solution, emit).dump(1, ' ') << std::endl;
}


void HalbachOptimizationIO::writeFieldCSV(
    const HalbachSolution& solution, const std::string& path) {

    if (solution.verified_field.empty()) {
        throw std::invalid_argument(
            "This run kept no field samples, so there is nothing to write. A "
            "run has to be asked for them, and one that skipped the "
            "verification never took any");
    }

    std::ofstream file(path);

    if (!file.is_open()) {
        throw std::invalid_argument("Could not open " + path + " for writing");
    }

    // Nine digits, for the same reason the simulator writes nine: the whole
    // point of these files is a field that varies in the seventh.
    file.precision(9);

    file << "x,y,z,Bx,By,Bz,Bmag" << std::endl;

    for (const auto& sample : solution.verified_field) {

        const double magnitude = std::sqrt(
            (double) sample.b[0] * sample.b[0] +
            (double) sample.b[1] * sample.b[1] +
            (double) sample.b[2] * sample.b[2]);

        file << sample.position[0] << "," << sample.position[1] << ","
             << sample.position[2] << "," << sample.b[0] << "," << sample.b[1]
             << "," << sample.b[2] << "," << magnitude << std::endl;
    }
}


void HalbachOptimizationIO::writeHistoryCSV(
    const HalbachSolution& solution, const std::string& path) {

    std::ofstream file(path);

    if (!file.is_open()) {
        throw std::invalid_argument("Could not open " + path + " for writing");
    }

    file << "generation,best_ppm,best_ever_ppm,mean_ppm,seconds" << std::endl;

    for (const auto& record : solution.history) {
        file << record.generation << "," << record.best << ","
             << record.best_ever << "," << record.mean << ","
             << record.seconds << std::endl;
    }
}

}  // namespace greeter
