#include <greeter/arrangements/LinearArrayArrangement.h>
#include <greeter/io/ArrangementIO.h>

#include <cmath>
#include <stdexcept>


namespace {

    // Half a turn about a local axis, which is what "alternating" applies.
    void readAlternatingFlip(const nlohmann::json& parameters, bool& alternating, float* flip) {

        alternating = false;

        flip[0] = 1.0f;
        flip[1] = 0.0f;
        flip[2] = 0.0f;
        flip[3] = 0.0f;

        if (!parameters.contains("alternating")) {
            return;
        }

        const nlohmann::json& given = parameters["alternating"];

        if (given.is_boolean() && !given.get<bool>()) {
            return;
        }

        if (!given.is_string()) {
            throw std::invalid_argument(
                "The \"alternating\" of a linear array must be \"x\", \"y\", \"z\" or false");
        }

        const std::string axis = given.get<std::string>();

        if (axis == "x") {
            flip[1] = 1.0f;
        } else if (axis == "y") {
            flip[2] = 1.0f;
        } else if (axis == "z") {
            flip[3] = 1.0f;
        } else {
            throw std::invalid_argument(
                "The \"alternating\" of a linear array must be \"x\", \"y\", \"z\" or false");
        }

        flip[0] = 0.0f;
        alternating = true;
    }

}  // namespace


greeter::LinearArrayArrangement::LinearArrayArrangement() {}

greeter::LinearArrayArrangement::~LinearArrayArrangement() {}

std::string greeter::LinearArrayArrangement::getTypeName() {
    return "linear_array";
}

std::vector<std::unique_ptr<greeter::Magnet>> greeter::LinearArrayArrangement::expand(
    const nlohmann::json& arrangement) {

    const nlohmann::json& parameters = greeter::ArrangementIO::readParameters(arrangement);

    uint32_t counts[3];
    greeter::ArrangementIO::readCounts(parameters, "count", counts);

    float spacing[3];
    greeter::ArrangementIO::readSpacing(parameters, "spacing", spacing);

    // Two members on top of each other are not a lattice, and the field between
    // them does not exist, so this is caught here rather than left to produce
    // infinities much later.
    for (size_t i = 0; i < 3; i++) {
        if (counts[i] > 1 && !(std::fabs(spacing[i]) > 0.0f)) {
            throw std::invalid_argument(
                "A linear array with more than one member along an axis needs a "
                "spacing along that axis");
        }
    }

    const bool centered = greeter::ArrangementIO::readCentered(parameters);

    bool alternating = false;
    float flip[4];
    readAlternatingFlip(parameters, alternating, flip);

    const nlohmann::json& element = greeter::ArrangementIO::readElement(parameters);

    float arrangement_position[3];
    float arrangement_orientation[4];
    greeter::ArrangementIO::readTransform(
        parameters, arrangement_position, arrangement_orientation);

    const float identity[4] = {1.0f, 0.0f, 0.0f, 0.0f};

    // Centred, the lattice sits on the position of the arrangement rather than
    // starting there, so a lattice grows symmetrically as members are added.
    float offset[3] = {0.0f, 0.0f, 0.0f};
    if (centered) {
        for (size_t i = 0; i < 3; i++) {
            offset[i] = 0.5f * (float) (counts[i] - 1) * spacing[i];
        }
    }

    std::vector<std::unique_ptr<greeter::Magnet>> members;
    members.reserve((size_t) counts[0] * counts[1] * counts[2]);

    // x runs fastest, so that a row keeps the order it is written in.
    for (uint32_t iz = 0; iz < counts[2]; iz++) {
        for (uint32_t iy = 0; iy < counts[1]; iy++) {
            for (uint32_t ix = 0; ix < counts[0]; ix++) {

                const float local_position[3] = {
                    (float) ix * spacing[0] - offset[0],
                    (float) iy * spacing[1] - offset[1],
                    (float) iz * spacing[2] - offset[2]
                };

                const bool flipped = alternating && ((ix + iy + iz) % 2 == 1);

                float position[3];
                float orientation[4];
                greeter::ArrangementIO::compose(
                    arrangement_position, arrangement_orientation,
                    local_position, flipped ? flip : identity,
                    position, orientation);

                members.push_back(
                    greeter::ArrangementIO::buildElement(element, position, orientation));
            }
        }
    }

    return members;
}
