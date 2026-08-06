#include <greeter/arrangements/CircularArrayArrangement.h>
#include <greeter/arrangements/HalbachRingArrangement.h>
#include <greeter/io/ArrangementIO.h>

#include <stdexcept>


greeter::CircularArrayArrangement::CircularArrayArrangement() {}

greeter::CircularArrayArrangement::~CircularArrayArrangement() {}

std::string greeter::CircularArrayArrangement::getTypeName() {
    return "circular_array";
}

std::vector<std::unique_ptr<greeter::Magnet>> greeter::CircularArrayArrangement::expand(
    const nlohmann::json& arrangement) {

    const nlohmann::json& parameters = greeter::ArrangementIO::readParameters(arrangement);

    // An "order" turns the members by the ring, which is what makes a ring a
    // Halbach one and is the arrangement to ask for instead.
    if (parameters.contains("order")) {
        throw std::invalid_argument(
            "A circular array does not turn its members by an \"order\", "
            "a Halbach ring is the arrangement that does");
    }

    // How often the frame of a member goes round while it goes once round the
    // ring: not at all, or exactly once with it.
    int64_t turns = 0;

    if (parameters.contains("face")) {

        if (!parameters["face"].is_string()) {
            throw std::invalid_argument(
                "The \"face\" of a circular array must be \"axis\" or \"center\"");
        }

        const std::string face = parameters["face"].get<std::string>();

        if (face == "center") {
            turns = 1;
        } else if (face != "axis") {
            throw std::invalid_argument(
                "The \"face\" of a circular array must be \"axis\" or \"center\"");
        }
    }

    return greeter::HalbachRingArrangement::expandRing(arrangement, turns);
}
