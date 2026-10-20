#include <greeter/optimization/HalbachSpec.h>
#include <greeter/arrangements/HalbachRingArrangement.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace greeter {
namespace optimization {
namespace {

struct Square {
    double x, y;
    std::array<double, 2> axes[2];
};

double dot(const std::array<double, 2>& a, const std::array<double, 2>& b) {
    return a[0] * b[0] + a[1] * b[1];
}

// Separating axis test on both squares' face normals. Touching is allowed.
bool overlaps(const Square& a, const Square& b, double side) {
    const std::array<double, 2> delta = {b.x - a.x, b.y - a.y};
    if (dot(delta, delta) >= 2.0 * side * side) return false;
    for (const auto* square : {&a, &b}) {
        for (const auto& axis : square->axes) {
            const double extent = 0.5 * side * (
                std::fabs(dot(a.axes[0], axis)) + std::fabs(dot(a.axes[1], axis)) +
                std::fabs(dot(b.axes[0], axis)) + std::fabs(dot(b.axes[1], axis)));
            if (std::fabs(dot(delta, axis)) >= extent - 1e-6 * side) return false;
        }
    }
    return true;
}

}  // namespace

void HalbachSpec::validateGeometry() const {
    if (!std::isfinite(clearance) || clearance < 0.0f) {
        throw std::invalid_argument("The assembly clearance must be finite and nonnegative");
    }
    // Rings have fixed axial spacing, and candidates may repeat at any position.
    // Keep every combination feasible by requiring an axial assembly gap.
    if (ring_count > 1 && ring_separation + 1e-6f * element.size < element.size + clearance) {
        throw std::invalid_argument("Ring separation must be at least element.size + clearance");
    }
    for (size_t candidate = 0; candidate < candidates.size(); ++candidate) {
        std::vector<Square> squares;
        const auto& chosen = candidates[candidate];
        for (const auto& ring : {makeRingJSON(chosen.radius, chosen.count, 0.0f),
                                makeRingJSON(chosen.radius + outer_radius_offset,
                                             chosen.count + outer_count_offset, 0.0f)}) {
            for (const auto& magnet : HalbachRingArrangement::expand(ring)) {
                const auto p = magnet->getPosition();
                const auto q = magnet->getOrientation();
                const double c = 1.0 - 2.0 * q[3] * q[3];
                const double s = 2.0 * q[0] * q[3];
                Square square{p[0], p[1], {{c, s}, {-s, c}}};
                // Distance from the bore axis to the actual rotated footprint.
                const std::array<double, 2> center = {square.x, square.y};
                const double dx = std::max(0.0, std::fabs(dot(center, square.axes[0]))
                                                - 0.5 * element.size);
                const double dy = std::max(0.0, std::fabs(dot(center, square.axes[1]))
                                                - 0.5 * element.size);
                if (std::hypot(dx, dy) <= 0.5 * dsv + clearance) {
                    throw std::invalid_argument("Candidate " + std::to_string(candidate) +
                        " reaches the sample sphere or its assembly clearance");
                }
                squares.push_back(square);
            }
        }
        for (size_t a = 0; a < squares.size(); ++a) {
            for (size_t b = a + 1; b < squares.size(); ++b) {
                if (overlaps(squares[a], squares[b], element.size + clearance)) {
                    throw std::invalid_argument("Candidate " + std::to_string(candidate) +
                        " has overlapping magnets or insufficient clearance (members " +
                        std::to_string(a) + " and " + std::to_string(b) + ")");
                }
            }
        }
    }
}

}  // namespace optimization
}  // namespace greeter
