#include <greeter/optimization/HalbachSpec.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>


namespace greeter {
namespace optimization {

namespace {

constexpr double MU_0 = 4.0e-7 * M_PI;

/*
  A number that has to be there and has to be positive. Reading a length as
  zero and carrying on gives a ring of no radius rather than a message.
*/
float readPositive(const nlohmann::json& data, const std::string& name,
                   const float& fallback) {

    if (!data.contains(name)) {
        return fallback;
    }

    if (!data[name].is_number()) {
        throw std::invalid_argument(
            "\"" + name + "\" of a Halbach optimization must be a number");
    }

    const float value = data[name].get<float>();

    if (!(value > 0.0f)) {
        throw std::invalid_argument(
            "\"" + name + "\" of a Halbach optimization must be positive");
    }

    return value;
}

float readNumber(const nlohmann::json& data, const std::string& name,
                 const float& fallback) {

    if (!data.contains(name)) {
        return fallback;
    }

    if (!data[name].is_number()) {
        throw std::invalid_argument(
            "\"" + name + "\" of a Halbach optimization must be a number");
    }

    return data[name].get<float>();
}

uint32_t readCount(const nlohmann::json& data, const std::string& name,
                   const uint32_t& fallback, const uint32_t& least) {

    if (!data.contains(name)) {
        return fallback;
    }

    if (!data[name].is_number_integer() && !data[name].is_number_unsigned()) {
        throw std::invalid_argument(
            "\"" + name + "\" of a Halbach optimization must be a whole number");
    }

    const int64_t value = data[name].get<int64_t>();

    if (value < (int64_t) least) {
        throw std::invalid_argument(
            "\"" + name + "\" of a Halbach optimization must be at least " +
            std::to_string(least));
    }

    return (uint32_t) value;
}

/* A probability, which is a number between zero and one inclusive. */
float readProbability(const nlohmann::json& data, const std::string& name,
                      const float& fallback) {

    const float value = readNumber(data, name, fallback);

    if (value < 0.0f || value > 1.0f) {
        throw std::invalid_argument(
            "\"" + name + "\" of a Halbach optimization is a probability, "
            "which lies between zero and one");
    }

    return value;
}

/*
  One of a fixed set of names. Anything else is refused with the list, rather
  than quietly falling back to the default and optimizing something the file
  did not ask for.
*/
std::string readChoice(const nlohmann::json& data, const std::string& name,
                       const std::vector<std::string>& allowed,
                       const std::string& fallback) {

    if (!data.contains(name)) {
        return fallback;
    }

    if (!data[name].is_string()) {
        throw std::invalid_argument("\"" + name + "\" must be a string");
    }

    const std::string value = data[name].get<std::string>();

    for (const auto& candidate : allowed) {
        if (value == candidate) {
            return value;
        }
    }

    std::string message = "\"" + name + "\" is one of";
    for (const auto& candidate : allowed) {
        message += " \"" + candidate + "\"";
    }
    message += ", not \"" + value + "\"";

    throw std::invalid_argument(message);
}

}  // namespace


double tidy(const float& value) {

    if (!std::isfinite(value)) {
        return (double) value;
    }

    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.7g", (double) value);

    return std::strtod(buffer, nullptr);
}


const char* toString(const Objective& objective) {
    return objective == Objective::Bx ? "ppm_bx" : "ppm_bmag";
}

const char* toString(const Symmetry& symmetry) {
    switch (symmetry) {
        case Symmetry::Octant:     return "octant";
        case Symmetry::Hemisphere: return "hemisphere";
        default:                   return "full";
    }
}

const char* toString(const FieldModel& model) {
    return model == FieldModel::Cuboid ? "cuboid" : "dipole";
}


HalbachSpec::HalbachSpec() {

    /*
      The candidates of homogeneityOptimisation.py.

      That script lists nineteen radii and twenty magnet counts, and pairs
      them by index, so its last count is never used. The pairing is written
      out here rather than left to two lists of different lengths that happen
      to be zipped.
    */
    const float radii[] = {
        0.148f, 0.151f, 0.154f, 0.156f, 0.159f, 0.162f, 0.165f,
        0.168f, 0.171f, 0.174f, 0.177f, 0.180f, 0.183f, 0.186f,
        0.189f, 0.192f, 0.195f, 0.198f, 0.201f
    };

    const size_t count = sizeof(radii) / sizeof(radii[0]);

    candidates.reserve(count);

    for (size_t i = 0; i < count; i++) {
        RingCandidate candidate;
        candidate.radius = radii[i];
        candidate.count = (uint32_t) (50 + i);
        candidates.push_back(candidate);
    }
}


void HalbachSpec::readJSON(const nlohmann::json& data) {

    if (!data.is_object()) {
        throw std::invalid_argument(
            "A Halbach optimization configuration is an object");
    }

    ring_count = readCount(data, "ring_count", ring_count, 1);
    ring_separation = readPositive(data, "ring_separation", ring_separation);

    outer_radius_offset =
        readNumber(data, "outer_radius_offset", outer_radius_offset);

    outer_count_offset =
        readCount(data, "outer_count_offset", outer_count_offset, 0);

    if (data.contains("order")) {
        if (!data["order"].is_number_integer()) {
            throw std::invalid_argument(
                "The \"order\" of a Halbach ring is a whole number");
        }
        order = data["order"].get<int64_t>();
    }

    if (data.contains("candidates")) {

        const nlohmann::json& given = data["candidates"];

        if (!given.is_array() || given.empty()) {
            throw std::invalid_argument(
                "\"candidates\" is a non empty array of {\"radius\", \"count\"}");
        }

        candidates.clear();
        candidates.reserve(given.size());

        for (const auto& entry : given) {

            if (!entry.is_object()) {
                throw std::invalid_argument(
                    "A ring candidate is an object with \"radius\" and \"count\"");
            }

            RingCandidate candidate;
            candidate.radius = readPositive(entry, "radius", 0.0f);
            candidate.count = readCount(entry, "count", 0, 1);

            if (candidate.radius <= 0.0f) {
                throw std::invalid_argument(
                    "A ring candidate needs a \"radius\"");
            }

            candidates.push_back(candidate);
        }
    }

    if (data.contains("element")) {

        const nlohmann::json& given = data["element"];

        element.size = readPositive(given, "size", element.size);
        element.remanence = readNumber(given, "remanence", element.remanence);
    }

    dsv = readPositive(data, "dsv", dsv);
    resolution = readPositive(data, "resolution", resolution);

    const std::string symmetry_name = readChoice(
        data, "symmetry", {"octant", "hemisphere", "full"}, toString(symmetry));

    symmetry = symmetry_name == "octant"     ? Symmetry::Octant
             : symmetry_name == "hemisphere" ? Symmetry::Hemisphere
                                             : Symmetry::Full;

    const std::string objective_name = readChoice(
        data, "objective", {"ppm_bx", "ppm_bmag"}, toString(objective));

    objective = objective_name == "ppm_bx" ? Objective::Bx : Objective::BMagnitude;

    const std::string model_name = readChoice(
        data, "field_model", {"cuboid", "dipole"}, toString(field_model));

    field_model = model_name == "cuboid" ? FieldModel::Cuboid : FieldModel::Dipole;

    if (data.contains("genetic")) {

        const nlohmann::json& given = data["genetic"];

        genetic.population = readCount(given, "population", genetic.population, 2);
        genetic.generations = readCount(given, "generations", genetic.generations, 1);
        genetic.tournament = readCount(given, "tournament", genetic.tournament, 1);
        genetic.elitism = readCount(given, "elitism", genetic.elitism, 0);

        genetic.cx_prob = readProbability(given, "cx_prob", genetic.cx_prob);
        genetic.mut_prob = readProbability(given, "mut_prob", genetic.mut_prob);
        genetic.gene_mut_prob =
            readProbability(given, "gene_mut_prob", genetic.gene_mut_prob);

        if (given.contains("seed")) {
            if (!given["seed"].is_number_integer() &&
                !given["seed"].is_number_unsigned()) {
                throw std::invalid_argument("The \"seed\" is a whole number");
            }
            genetic.seed = given["seed"].get<uint64_t>();
        }
    }

    validate();
}


void HalbachSpec::validate() const {

    if (candidates.empty()) {
        throw std::invalid_argument(
            "A Halbach optimization needs at least one ring candidate");
    }

    // A gene is stored as a 16 bit index, which is what the population view
    // holds, and there is no sensible run with more candidates than that.
    if (candidates.size() > 65535) {
        throw std::invalid_argument(
            "A Halbach optimization takes at most 65535 ring candidates");
    }

    for (const auto& candidate : candidates) {

        if (!(candidate.radius > 0.0f)) {
            throw std::invalid_argument("A ring candidate needs a positive radius");
        }

        if (candidate.count < 1) {
            throw std::invalid_argument("A ring candidate needs at least one magnet");
        }

        if (candidate.radius + outer_radius_offset <= 0.0f) {
            throw std::invalid_argument(
                "The \"outer_radius_offset\" leaves the outer ring at a radius "
                "of zero or less");
        }
    }

    // The sampled sphere has to fit inside the bore, or the observation points
    // land inside the magnets and the field there is neither uniform nor,
    // for a dipole, finite.
    float smallest = candidates[0].radius;
    for (const auto& candidate : candidates) {
        smallest = std::min(smallest, candidate.radius);
        smallest = std::min(smallest, candidate.radius + outer_radius_offset);
    }

    if (0.5f * dsv >= smallest - 0.5f * element.size) {
        throw std::invalid_argument(
            "The sampled sphere reaches the magnets: half the \"dsv\" is at "
            "least the smallest candidate radius less half the element size");
    }

    if (resolution > dsv) {
        throw std::invalid_argument(
            "The \"resolution\" is coarser than the whole \"dsv\"");
    }

    if (genetic.elitism >= genetic.population) {
        throw std::invalid_argument(
            "The \"elitism\" carries over the whole population, which leaves "
            "no room for a next generation");
    }

    if (element.remanence == 0.0f) {
        throw std::invalid_argument(
            "An element of zero remanence produces no field at all");
    }
}


std::vector<float> HalbachSpec::getRingPositions() const {

    std::vector<float> positions;
    positions.reserve(ring_count);

    const float length = (float) (ring_count - 1) * ring_separation;

    for (uint32_t i = 0; i < ring_count; i++) {
        positions.push_back(-0.5f * length + (float) i * ring_separation);
    }

    return positions;
}


std::vector<float> HalbachSpec::getSymmetricRingPositions() const {

    std::vector<float> positions;

    /*
      A ring the arithmetic puts at the centre may land a rounding error to
      either side of it, and the ring at the centre is the one that is not
      mirrored. A residue read as a position of its own would put two rings a
      nanometre apart where there should be one, so the centre is snapped
      shut. The tolerance is relative to the spacing, since that is the only
      length here that says what "close to zero" means.
    */
    const float tolerance = 1e-4f * ring_separation;

    for (const auto& position : getRingPositions()) {

        if (std::fabs(position) < tolerance) {
            positions.push_back(0.0f);
        } else if (position > 0.0f) {
            positions.push_back(position);
        }
    }

    return positions;
}


size_t HalbachSpec::getNumGenes() const {
    return getSymmetricRingPositions().size();
}


size_t HalbachSpec::getNumCandidates() const {
    return candidates.size();
}


float HalbachSpec::getDipoleMoment() const {

    // m = V * J / mu0, with J the remanence of the material and V the volume
    // of the cube. This is the same number as the dip_mom of halbachFields.py,
    // which writes it as Br * a^3 / (4 pi * 1e-7).
    const double volume = (double) element.size * (double) element.size
                        * (double) element.size;

    return (float) (volume * (double) element.remanence / MU_0);
}


nlohmann::json HalbachSpec::getElementJSON() const {

    // The polarization, or the moment, points along the local x of the
    // element. A Halbach ring turns its members about its own axis, carrying
    // that direction round with them, so a member at angle theta ends up
    // polarized along (cos((order + 1) theta), sin((order + 1) theta), 0).
    // With order one that is the 2 theta of the Python script.
    if (field_model == FieldModel::Dipole) {

        return nlohmann::json{
            {"type", "dipole"},
            {"parameters", {
                {"moment", {tidy(getDipoleMoment()), 0.0, 0.0}}
            }}
        };
    }

    return nlohmann::json{
        {"type", "cuboid"},
        {"parameters", {
            {"dimensions", {tidy(element.size), tidy(element.size),
                            tidy(element.size)}},
            {"magnetization", {tidy(element.remanence), 0.0, 0.0}}
        }}
    };
}


nlohmann::json HalbachSpec::makeRingJSON(
    const float& radius, const uint32_t& count, const float& z) const {

    return nlohmann::json{
        {"type", "halbach_ring"},
        {"parameters", {
            {"radius", tidy(radius)},
            {"count", count},
            {"order", order},
            {"position", {0.0, 0.0, tidy(z)}},
            {"orientation", {1.0, 0.0, 0.0, 0.0}},
            {"element", getElementJSON()}
        }}
    };
}


std::vector<nlohmann::json> HalbachSpec::makeRingsForGene(
    const size_t& gene, const size_t& candidate) const {

    const std::vector<float> positions = getSymmetricRingPositions();

    if (gene >= positions.size()) {
        throw std::out_of_range("Gene index out of range");
    }

    if (candidate >= candidates.size()) {
        throw std::out_of_range("Ring candidate index out of range");
    }

    const RingCandidate& chosen = candidates[candidate];

    const float z = positions[gene];

    std::vector<nlohmann::json> rings;
    rings.reserve(4);

    // The two layers move together, which is what makes one gene enough for a
    // position: an inner ring, and an outer ring further out with more magnets
    // in it.
    rings.push_back(makeRingJSON(chosen.radius, chosen.count, z));
    rings.push_back(makeRingJSON(chosen.radius + outer_radius_offset,
                                 chosen.count + outer_count_offset, z));

    // The magnet is built mirror symmetric about the middle, so every gene but
    // the one at the centre stands for a pair of positions.
    if (z != 0.0f) {
        rings.push_back(makeRingJSON(chosen.radius, chosen.count, -z));
        rings.push_back(makeRingJSON(chosen.radius + outer_radius_offset,
                                     chosen.count + outer_count_offset, -z));
    }

    return rings;
}


nlohmann::json HalbachSpec::toJSON() const {

    nlohmann::json candidate_list = nlohmann::json::array();

    for (const auto& candidate : candidates) {
        candidate_list.push_back({
            {"radius", tidy(candidate.radius)},
            {"count", candidate.count}
        });
    }

    return nlohmann::json{
        {"ring_count", ring_count},
        {"ring_separation", tidy(ring_separation)},
        {"candidates", candidate_list},
        {"outer_radius_offset", tidy(outer_radius_offset)},
        {"outer_count_offset", outer_count_offset},
        {"order", order},
        {"element", {
            {"size", tidy(element.size)},
            {"remanence", tidy(element.remanence)}
        }},
        {"dsv", tidy(dsv)},
        {"resolution", tidy(resolution)},
        {"symmetry", toString(symmetry)},
        {"objective", toString(objective)},
        {"field_model", toString(field_model)},
        {"genetic", {
            {"population", genetic.population},
            {"generations", genetic.generations},
            {"cx_prob", tidy(genetic.cx_prob)},
            {"mut_prob", tidy(genetic.mut_prob)},
            {"gene_mut_prob", tidy(genetic.gene_mut_prob)},
            {"tournament", genetic.tournament},
            {"elitism", genetic.elitism},
            {"seed", genetic.seed}
        }}
    };
}

}  // namespace optimization
}  // namespace greeter
