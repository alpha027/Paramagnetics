#include <greeter/arrangements/LinearHalbachArrangement.h>
#include <greeter/io/ArrangementIO.h>
#include <greeter/Quaternion.h>

#include <cmath>
#include <stdexcept>


greeter::LinearHalbachArrangement::LinearHalbachArrangement() {}

greeter::LinearHalbachArrangement::~LinearHalbachArrangement() {}

std::string greeter::LinearHalbachArrangement::getTypeName() {
    return "halbach_linear";
}

std::vector<std::unique_ptr<greeter::Magnet>> greeter::LinearHalbachArrangement::expand(
    const nlohmann::json& arrangement) {

    const nlohmann::json& parameters = greeter::ArrangementIO::readParameters(arrangement);

    const uint32_t count = greeter::ArrangementIO::readCount(parameters, "count");
    const float spacing = greeter::ArrangementIO::readLength(parameters, "spacing");

    // How far round the polarization goes from one member to the next, given
    // either as the members it takes to come back round or as the length the
    // row covers in doing so.
    const bool has_steps = parameters.contains("steps_per_period");
    const bool has_wavelength = parameters.contains("wavelength");

    if (has_steps == has_wavelength) {
        throw std::invalid_argument(
            "A linear Halbach array needs either a \"steps_per_period\" or a "
            "\"wavelength\", and not both");
    }

    const float steps_per_period =
        has_steps ? greeter::ArrangementIO::readNonZero(parameters, "steps_per_period")
                  : greeter::ArrangementIO::readNonZero(parameters, "wavelength") / spacing;

    const bool centered = greeter::ArrangementIO::readCentered(parameters);

    const nlohmann::json& element = greeter::ArrangementIO::readElement(parameters);

    float arrangement_position[3];
    float arrangement_orientation[4];
    greeter::ArrangementIO::readTransform(
        parameters, arrangement_position, arrangement_orientation);

    // Centred, the row sits on the position of the arrangement rather than
    // starting there.
    const float offset = centered ? 0.5f * (float) (count - 1) * spacing : 0.0f;

    // The turn is counted off the index rather than the position along the row,
    // so that the first member is never turned however the row is placed.
    const float angle_step = 2.0f * (float) M_PI / steps_per_period;

    std::vector<std::unique_ptr<greeter::Magnet>> members;
    members.reserve(count);

    for (uint32_t i = 0; i < count; i++) {

        const float local_position[3] = {
            (float) i * spacing - offset,
            0.0f,
            0.0f
        };

        // Turning about the local y axis sweeps a polarization along the local
        // z axis through the xz plane, which is what makes the field of the row
        // strong on one side and weak on the other.
        float local_orientation[4];
        greeter::Quaternion::set_rotation_from_axis_angle(
            "y", (float) i * angle_step, local_orientation);

        float position[3];
        float orientation[4];
        greeter::ArrangementIO::compose(
            arrangement_position, arrangement_orientation,
            local_position, local_orientation,
            position, orientation);

        members.push_back(
            greeter::ArrangementIO::buildElement(element, position, orientation));
    }

    return members;
}
