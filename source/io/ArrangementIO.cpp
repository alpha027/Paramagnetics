#include <greeter/io/ArrangementIO.h>
#include <greeter/io/MethodFactoryIO.h>
#include <greeter/Quaternion.h>

#include <cmath>
#include <stdexcept>


namespace greeter {

    ArrangementIO::ArrangementIO() {}

    ArrangementIO::~ArrangementIO() {}

    const nlohmann::json& ArrangementIO::readParameters(const nlohmann::json& arrangement) {

        if (!arrangement.contains("parameters") || !arrangement["parameters"].is_object()) {
            throw std::invalid_argument("An arrangement needs a \"parameters\" object");
        }

        return arrangement["parameters"];
    }

    void ArrangementIO::readTransform(const nlohmann::json& parameters,
                                      float* position, float* orientation) {

        position[0] = 0.0f;
        position[1] = 0.0f;
        position[2] = 0.0f;

        orientation[0] = 1.0f;
        orientation[1] = 0.0f;
        orientation[2] = 0.0f;
        orientation[3] = 0.0f;

        if (parameters.contains("position")) {

            const nlohmann::json& given = parameters["position"];

            if (!given.is_array() || given.size() != 3) {
                throw std::invalid_argument(
                    "The position of an arrangement must have three components");
            }

            for (size_t i = 0; i < 3; i++) {
                position[i] = given[i].get<float>();
            }
        }

        if (parameters.contains("orientation")) {

            const nlohmann::json& given = parameters["orientation"];

            if (!given.is_array() || given.size() != 4) {
                throw std::invalid_argument(
                    "The orientation of an arrangement must be a quaternion [w, x, y, z]");
            }

            double norm = 0.0;
            for (size_t i = 0; i < 4; i++) {
                orientation[i] = given[i].get<float>();
                norm += (double) orientation[i] * (double) orientation[i];
            }

            norm = std::sqrt(norm);

            if (!(norm > 0.0)) {
                throw std::invalid_argument(
                    "The orientation of an arrangement must not be the zero quaternion");
            }

            for (size_t i = 0; i < 4; i++) {
                orientation[i] = (float) (orientation[i] / norm);
            }
        }
    }

    void ArrangementIO::compose(const float* arrangement_position,
                                const float* arrangement_orientation,
                                const float* local_position,
                                const float* local_orientation,
                                float* position, float* orientation) {

        // The member is turned in the frame of the arrangement first and by the
        // arrangement second, which is what the Hamilton product spells in this
        // order.
        const greeter::Quaternion arrangement_rotation(
            arrangement_orientation[0], arrangement_orientation[1],
            arrangement_orientation[2], arrangement_orientation[3]);

        const greeter::Quaternion local_rotation(
            local_orientation[0], local_orientation[1],
            local_orientation[2], local_orientation[3]);

        const greeter::Quaternion composed = arrangement_rotation * local_rotation;

        orientation[0] = composed.getW();
        orientation[1] = composed.getX();
        orientation[2] = composed.getY();
        orientation[3] = composed.getZ();

        float turned[3];
        greeter::Quaternion::applyRotationFromQuaternion(
            arrangement_orientation, local_position, turned);

        position[0] = arrangement_position[0] + turned[0];
        position[1] = arrangement_position[1] + turned[1];
        position[2] = arrangement_position[2] + turned[2];
    }

    const nlohmann::json& ArrangementIO::readElement(const nlohmann::json& parameters) {

        if (!parameters.contains("element") || !parameters["element"].is_object()) {
            throw std::invalid_argument(
                "An arrangement needs an \"element\" object, the magnet it repeats");
        }

        const nlohmann::json& element = parameters["element"];

        if (!element.contains("type") || !element["type"].is_string()) {
            throw std::invalid_argument("The element of an arrangement needs a \"type\"");
        }

        if (!element.contains("parameters") || !element["parameters"].is_object()) {
            throw std::invalid_argument(
                "The element of an arrangement needs a \"parameters\" object");
        }

        // An arrangement is what decides where its members go, so an element
        // that carries a placement of its own is a contradiction rather than
        // something to silently overwrite.
        for (const auto& key : {"position", "orientation"}) {
            if (element["parameters"].contains(key)) {
                throw std::invalid_argument(
                    "The element of an arrangement must not set its own \"" +
                    std::string(key) + "\", the arrangement places its members");
            }
        }

        return element;
    }

    std::unique_ptr<greeter::Magnet> ArrangementIO::buildElement(
        const nlohmann::json& element,
        const float* position, const float* orientation) {

        // The element is a magnet in every respect but its placement, so it is
        // built by the reader of its own type rather than here.
        nlohmann::json placed = element;

        placed["parameters"]["position"] = {position[0], position[1], position[2]};
        placed["parameters"]["orientation"] = {
            orientation[0], orientation[1], orientation[2], orientation[3]};

        return greeter::MethodFactoryIO::getInstance().createMagnet(
            element["type"].get<std::string>(), placed);
    }

    namespace {

        uint32_t asCount(const nlohmann::json& value, const std::string& name) {

            if (!value.is_number_integer() && !value.is_number_unsigned()) {
                throw std::invalid_argument(
                    "The \"" + name + "\" of an arrangement must be a whole number");
            }

            const int64_t count = value.get<int64_t>();

            if (count < 1) {
                throw std::invalid_argument(
                    "The \"" + name + "\" of an arrangement must be at least one");
            }

            return (uint32_t) count;
        }

    }  // namespace

    void ArrangementIO::readCounts(const nlohmann::json& parameters,
                                   const std::string& name, uint32_t* counts) {

        if (!parameters.contains(name)) {
            throw std::invalid_argument("An arrangement needs a \"" + name + "\"");
        }

        const nlohmann::json& given = parameters[name];

        if (given.is_number()) {
            // A row is the common case and does not have to be spelled [n, 1, 1].
            counts[0] = asCount(given, name);
            counts[1] = 1;
            counts[2] = 1;
            return;
        }

        if (given.is_array() && given.size() == 3) {
            for (size_t i = 0; i < 3; i++) {
                counts[i] = asCount(given[i], name);
            }
            return;
        }

        throw std::invalid_argument(
            "The \"" + name + "\" of an arrangement must be a whole number or "
            "an array of three whole numbers");
    }

    void ArrangementIO::readSpacing(const nlohmann::json& parameters,
                                    const std::string& name, float* spacing) {

        if (!parameters.contains(name)) {
            throw std::invalid_argument("An arrangement needs a \"" + name + "\"");
        }

        const nlohmann::json& given = parameters[name];

        if (given.is_number()) {
            const float value = given.get<float>();
            spacing[0] = value;
            spacing[1] = value;
            spacing[2] = value;
            return;
        }

        if (given.is_array() && given.size() == 3) {
            for (size_t i = 0; i < 3; i++) {
                if (!given[i].is_number()) {
                    throw std::invalid_argument(
                        "The \"" + name + "\" of an arrangement must be a number");
                }
                spacing[i] = given[i].get<float>();
            }
            return;
        }

        throw std::invalid_argument(
            "The \"" + name + "\" of an arrangement must be a number or "
            "an array of three numbers");
    }

    uint32_t ArrangementIO::readCount(const nlohmann::json& parameters,
                                      const std::string& name) {

        if (!parameters.contains(name)) {
            throw std::invalid_argument("An arrangement needs a \"" + name + "\"");
        }

        return asCount(parameters[name], name);
    }

    float ArrangementIO::readLength(const nlohmann::json& parameters,
                                    const std::string& name) {

        if (!parameters.contains(name) || !parameters[name].is_number()) {
            throw std::invalid_argument(
                "An arrangement needs a \"" + name + "\", as a number");
        }

        const float length = parameters[name].get<float>();

        if (!(length > 0.0f)) {
            throw std::invalid_argument(
                "The \"" + name + "\" of an arrangement must be strictly positive");
        }

        return length;
    }

    float ArrangementIO::readNonZero(const nlohmann::json& parameters,
                                     const std::string& name) {

        if (!parameters.contains(name) || !parameters[name].is_number()) {
            throw std::invalid_argument(
                "An arrangement needs a \"" + name + "\", as a number");
        }

        const float value = parameters[name].get<float>();

        if (!(std::fabs(value) > 0.0f)) {
            throw std::invalid_argument(
                "The \"" + name + "\" of an arrangement must not be zero");
        }

        return value;
    }

    int64_t ArrangementIO::readWholeNumber(const nlohmann::json& parameters,
                                           const std::string& name,
                                           const int64_t& fallback) {

        if (!parameters.contains(name)) {
            return fallback;
        }

        const nlohmann::json& given = parameters[name];

        if (!given.is_number_integer() && !given.is_number_unsigned()) {
            throw std::invalid_argument(
                "The \"" + name + "\" of an arrangement must be a whole number");
        }

        return given.get<int64_t>();
    }

    bool ArrangementIO::readCentered(const nlohmann::json& parameters) {

        if (!parameters.contains("centered")) {
            return true;
        }

        if (!parameters["centered"].is_boolean()) {
            throw std::invalid_argument(
                "The \"centered\" of an arrangement must be true or false");
        }

        return parameters["centered"].get<bool>();
    }

}  // namespace greeter
