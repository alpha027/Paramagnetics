#include <greeter/TriangularMeshMagnet.h>
#include <greeter/io/CylinderSegmentMagnetIO.h>
#include <greeter/io/TriangularMeshMagnetIO.h>

#include <cmath>
#include <stdexcept>


greeter::CylinderSegmentMagnetIO::CylinderSegmentMagnetIO() {}
greeter::CylinderSegmentMagnetIO::~CylinderSegmentMagnetIO() {}

std::string greeter::CylinderSegmentMagnetIO::getTypeName() {
  return "cylinder_segment";
}


std::vector<float> greeter::CylinderSegmentMagnetIO::readDimensions(
    const nlohmann::json& magnet) {

  const nlohmann::json& parameters = magnet["parameters"];

  if (!parameters.contains("dimensions") || !parameters["dimensions"].is_array()) {
    throw std::invalid_argument(
      "A cylinder segment is given by five numbers, an inner and an outer "
      "radius, a height, and the two angles it spans in degrees");
  }

  std::vector<float> dimensions =
    parameters["dimensions"].get<std::vector<float>>();

  if (dimensions.size() != 5) {
    throw std::invalid_argument(
      "A cylinder segment is given by five numbers, an inner and an outer "
      "radius, a height, and the two angles it spans in degrees");
  }

  if (!(dimensions[0] > 0.0f)) {
    throw std::invalid_argument(
      "The inner radius of a cylinder segment must be greater than zero. A "
      "sector reaching the axis needs a different shape, and a whole solid "
      "cylinder is the \"cylinder\" type");
  }

  if (!(dimensions[1] > dimensions[0])) {
    throw std::invalid_argument(
      "The outer radius of a cylinder segment must be greater than its inner "
      "radius");
  }

  if (!(dimensions[2] > 0.0f)) {
    throw std::invalid_argument(
      "The height of a cylinder segment must be greater than zero");
  }

  if (!(dimensions[4] > dimensions[3])) {
    throw std::invalid_argument(
      "The second angle of a cylinder segment must be greater than the first");
  }

  if (dimensions[4] - dimensions[3] > 360.0f) {
    throw std::invalid_argument(
      "A cylinder segment cannot span more than a whole turn");
  }

  return dimensions;
}


uint32_t greeter::CylinderSegmentMagnetIO::readSegments(const nlohmann::json& magnet) {

  const nlohmann::json& parameters = magnet["parameters"];

  if (!parameters.contains("segments")) {
    return 32;
  }

  if (!parameters["segments"].is_number_integer() ||
      parameters["segments"].get<int64_t>() < 2) {
    throw std::invalid_argument(
      "The \"segments\" of a cylinder segment is how many flat facets its "
      "curved walls are cut into, a whole number of at least two");
  }

  return parameters["segments"].get<uint32_t>();
}


std::vector<float> greeter::CylinderSegmentMagnetIO::buildTriangles(
    const float& inner_radius, const float& outer_radius, const float& height,
    const float& first_angle, const float& second_angle,
    const uint32_t& segments) {

  const double from = (double) first_angle * M_PI / 180.0;
  const double to = (double) second_angle * M_PI / 180.0;

  // A sector spanning a whole turn closes on itself: its two end caps would
  // sit on top of each other, so there are none, and the last facet joins the
  // first.
  const bool whole_turn = std::fabs((to - from) - 2.0 * M_PI) < 1e-9;

  const uint32_t around = whole_turn ? segments : segments + 1;

  std::vector<float> corner;
  corner.reserve(3 * 4 * (size_t) around);

  for (int level = 0; level < 2; level++) {
    for (int ring = 0; ring < 2; ring++) {
      for (uint32_t i = 0; i < around; i++) {

        const double angle = from + (to - from) * (double) i / (double) segments;

        const double radius = ring == 0 ? (double) inner_radius
                                        : (double) outer_radius;

        corner.push_back((float) (radius * std::cos(angle)));
        corner.push_back((float) (radius * std::sin(angle)));
        corner.push_back(level == 0 ? -0.5f * height : 0.5f * height);
      }
    }
  }

  auto at = [&](const uint32_t& level, const uint32_t& ring, const uint32_t& i) {
    return (size_t) level * 2 * around + (size_t) ring * around + (size_t) i;
  };

  std::vector<float> triangles;

  auto addFace = [&](const size_t& a, const size_t& b, const size_t& c) {
    for (const auto& which : {a, b, c}) {
      for (size_t axis = 0; axis < 3; axis++) {
        triangles.push_back(corner[3 * which + axis]);
      }
    }
  };

  for (uint32_t i = 0; i < segments; i++) {

    const uint32_t j = (i + 1) % around;

    // The inner and outer walls, and the bottom and the top, each cut into
    // two triangles per facet. All wound the same way round; whether that is
    // outwards is settled by TriangularMeshMagnet, which turns a body that
    // came out inside in.
    addFace(at(0, 0, i), at(0, 0, j), at(1, 0, j));
    addFace(at(0, 0, i), at(1, 0, j), at(1, 0, i));

    addFace(at(0, 1, i), at(1, 1, i), at(1, 1, j));
    addFace(at(0, 1, i), at(1, 1, j), at(0, 1, j));

    addFace(at(0, 0, i), at(0, 1, i), at(0, 1, j));
    addFace(at(0, 0, i), at(0, 1, j), at(0, 0, j));

    addFace(at(1, 0, i), at(1, 0, j), at(1, 1, j));
    addFace(at(1, 0, i), at(1, 1, j), at(1, 1, i));
  }

  if (!whole_turn) {

    const uint32_t last = segments;

    addFace(at(0, 0, 0), at(1, 0, 0), at(1, 1, 0));
    addFace(at(0, 0, 0), at(1, 1, 0), at(0, 1, 0));

    addFace(at(0, 0, last), at(0, 1, last), at(1, 1, last));
    addFace(at(0, 0, last), at(1, 1, last), at(1, 0, last));
  }

  return triangles;
}


std::unique_ptr<greeter::Magnet> greeter::CylinderSegmentMagnetIO::createMagnet(
    const nlohmann::json& magnet) {

  if (!magnet.contains("parameters") || !magnet["parameters"].is_object()) {
    throw std::invalid_argument("A cylinder segment needs a \"parameters\" object");
  }

  const std::vector<float> dimensions = readDimensions(magnet);

  // Built as the faceted body it is, and from there it is an ordinary
  // triangular mesh: same kernel, same meshing, same everything.
  return std::make_unique<greeter::TriangularMeshMagnet>(
    greeter::TriangularMeshMagnetIO::readPosition(magnet),
    buildTriangles(dimensions[0], dimensions[1], dimensions[2],
                   dimensions[3], dimensions[4], readSegments(magnet)),
    greeter::TriangularMeshMagnetIO::readOrientation(magnet),
    greeter::TriangularMeshMagnetIO::readMagnetization(magnet));
}
