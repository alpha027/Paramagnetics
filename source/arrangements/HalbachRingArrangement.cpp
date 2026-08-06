#include <greeter/arrangements/HalbachRingArrangement.h>
#include <greeter/io/ArrangementIO.h>
#include <greeter/Quaternion.h>

#include <cmath>
#include <stdexcept>


greeter::HalbachRingArrangement::HalbachRingArrangement() {}

greeter::HalbachRingArrangement::~HalbachRingArrangement() {}

std::string greeter::HalbachRingArrangement::getTypeName() {
    return "halbach_ring";
}

std::vector<std::unique_ptr<greeter::Magnet>> greeter::HalbachRingArrangement::expandRing(
    const nlohmann::json& arrangement, const int64_t& turns) {

    const nlohmann::json& parameters = greeter::ArrangementIO::readParameters(arrangement);

    const float radius = greeter::ArrangementIO::readLength(parameters, "radius");
    const uint32_t count = greeter::ArrangementIO::readCount(parameters, "count");

    const nlohmann::json& element = greeter::ArrangementIO::readElement(parameters);

    float arrangement_position[3];
    float arrangement_orientation[4];
    greeter::ArrangementIO::readTransform(
        parameters, arrangement_position, arrangement_orientation);

    const float angle_step = 2.0f * (float) M_PI / (float) count;

    std::vector<std::unique_ptr<greeter::Magnet>> members;
    members.reserve(count);

    for (uint32_t i = 0; i < count; i++) {

        const float angle = (float) i * angle_step;

        const float local_position[3] = {
            radius * std::cos(angle),
            radius * std::sin(angle),
            0.0f
        };

        // The member is turned about the axis of the ring, which is the local
        // z axis, by a multiple of the angle at which it sits. That multiple is
        // what tells a Halbach ring of one order from another.
        float local_orientation[4];
        greeter::Quaternion::set_rotation_from_axis_angle(
            "z", (float) turns * angle, local_orientation);

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

std::vector<std::unique_ptr<greeter::Magnet>> greeter::HalbachRingArrangement::expand(
    const nlohmann::json& arrangement) {

    const nlohmann::json& parameters = greeter::ArrangementIO::readParameters(arrangement);

    // The polarization has to come back to itself after a full turn round the
    // ring, so the order is whole. A "face" belongs to a circular array, which
    // is the arrangement that does not turn its members by the ring.
    if (parameters.contains("face")) {
        throw std::invalid_argument(
            "A Halbach ring turns its members by its \"order\", a \"face\" "
            "belongs to a circular array");
    }

    const int64_t order = greeter::ArrangementIO::readWholeNumber(parameters, "order", 1);

    return expandRing(arrangement, order + 1);
}
