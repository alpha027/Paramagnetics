#include <greeter/io/FieldOfViewIO.h>
#include <stdexcept>
#include <string>

greeter::FieldOfViewIO::FieldOfViewIO() {}
greeter::FieldOfViewIO::~FieldOfViewIO() {}


namespace {

const std::vector<std::string>& axisKeys() {
    static const std::vector<std::string> keys = {"x", "y", "z"};
    return keys;
}

/*
  The description of one axis, checked before anything is read out of it. The
  messages name the axis, because a file with three of them is otherwise a
  guessing game.
*/
const nlohmann::json& readAxis(const nlohmann::json& fov, const std::string& key) {

    if (!fov.is_object() || !fov.contains(key) || !fov[key].is_object()) {
        throw std::invalid_argument(
            "A field of view says how it is sampled along \"" + key +
            "\", as an object with \"min\", \"max\" and \"n\"");
    }

    const nlohmann::json& axis = fov[key];

    for (const auto& bound : {"min", "max"}) {
        if (!axis.contains(bound) || !axis[bound].is_number()) {
            throw std::invalid_argument(
                "The \"" + key + "\" axis of a field of view needs a number "
                "for \"" + std::string(bound) + "\"");
        }
    }

    if (!axis.contains("n") || !axis["n"].is_number_integer() ||
        axis["n"].get<int64_t>() < 1) {
        throw std::invalid_argument(
            "The \"" + key + "\" axis of a field of view is sampled \"n\" "
            "times, which is a whole number of at least one");
    }

    return axis;
}

}  // namespace


std::vector<float> greeter::FieldOfViewIO::readRanges(const nlohmann::json& fov) {

    std::vector<float> ranges;
    ranges.reserve(6);

    for (const auto& key : axisKeys()) {
        const nlohmann::json& axis = readAxis(fov, key);
        ranges.push_back(axis["min"].get<float>());
        ranges.push_back(axis["max"].get<float>());
    }

    return ranges;
}

std::vector<uint32_t> greeter::FieldOfViewIO::readSubdivisionCounts(const nlohmann::json& fov) {

    std::vector<uint32_t> counts;
    counts.reserve(3);

    for (const auto& key : axisKeys()) {
        counts.push_back(readAxis(fov, key)["n"].get<uint32_t>());
    }

    return counts;
}

greeter::FieldOfView greeter::FieldOfViewIO::read(const nlohmann::json& fov) {
    return greeter::FieldOfView(readRanges(fov), readSubdivisionCounts(fov));
}

std::unique_ptr<greeter::FieldOfView> greeter::FieldOfViewIO::createFOV(const nlohmann::json& fov) {
    return std::make_unique<greeter::FieldOfView>(read(fov));
}
