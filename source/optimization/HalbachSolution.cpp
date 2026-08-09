#include <greeter/optimization/HalbachSolution.h>

#include <greeter/arrangements/HalbachRingArrangement.h>

#include <stdexcept>


namespace greeter {
namespace optimization {

std::vector<nlohmann::json> HalbachSolution::buildArrangements() const {

    if (genome.size() != spec.getNumGenes()) {
        throw std::invalid_argument(
            "The genome has " + std::to_string(genome.size()) +
            " genes, the specification asks for " +
            std::to_string(spec.getNumGenes()));
    }

    std::vector<nlohmann::json> arrangements;

    int64_t id = 1;

    for (size_t gene = 0; gene < genome.size(); gene++) {

        for (auto& ring : spec.makeRingsForGene(gene, genome[gene])) {
            ring["id"] = id++;
            arrangements.push_back(std::move(ring));
        }
    }

    return arrangements;
}


greeter::MagnetCollection HalbachSolution::buildCollection() const {

    greeter::MagnetCollection collection;

    // Through the same arrangement an input file goes through, so that the
    // magnet checked here is the magnet the written file describes.
    for (const auto& ring : buildArrangements()) {
        for (auto& member : greeter::HalbachRingArrangement::expand(ring)) {
            collection.addMagnet(std::move(member));
        }
    }

    return collection;
}


std::vector<RingCandidate> HalbachSolution::getChosenCandidates() const {

    std::vector<RingCandidate> chosen;
    chosen.reserve(genome.size());

    for (const auto& gene : genome) {

        if (gene >= spec.candidates.size()) {
            throw std::out_of_range("Gene value out of range");
        }

        chosen.push_back(spec.candidates[gene]);
    }

    return chosen;
}

}  // namespace optimization
}  // namespace greeter
