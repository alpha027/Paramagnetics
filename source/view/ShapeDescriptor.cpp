#include <greeter/view/ShapeDescriptor.h>
#include <algorithm>
#include <cmath>


namespace {

/* How many numbers a kind takes, or zero when it takes any multiple. */
size_t expectedParameters(const greeter::view::ShapeKind& kind) {
  switch (kind) {
    case greeter::view::ShapeKind::Box:         return 3;
    case greeter::view::ShapeKind::Cylinder:    return 2;
    case greeter::view::ShapeKind::Sphere:      return 1;
    case greeter::view::ShapeKind::Tetrahedron: return 12;
    case greeter::view::ShapeKind::Mesh:        return 0;
    case greeter::view::ShapeKind::Point:       return 0;
    case greeter::view::ShapeKind::Unknown:     return 0;
  }
  return 0;
}

}  // namespace


std::string greeter::view::getName(const greeter::view::ShapeKind& kind) {
  switch (kind) {
    case greeter::view::ShapeKind::Box:         return "box";
    case greeter::view::ShapeKind::Cylinder:    return "cylinder";
    case greeter::view::ShapeKind::Sphere:      return "sphere";
    case greeter::view::ShapeKind::Tetrahedron: return "tetrahedron";
    case greeter::view::ShapeKind::Mesh:        return "mesh";
    case greeter::view::ShapeKind::Point:       return "point";
    case greeter::view::ShapeKind::Unknown:     return "unknown";
  }
  return "unknown";
}


std::string greeter::view::getName(const greeter::view::MomentKind& kind) {
  switch (kind) {
    case greeter::view::MomentKind::Moment:       return "moment";
    case greeter::view::MomentKind::Polarization: return "polarization";
  }
  return "polarization";
}


std::string greeter::view::getUnit(const greeter::view::MomentKind& kind) {
  switch (kind) {
    case greeter::view::MomentKind::Moment:       return "A m^2";
    case greeter::view::MomentKind::Polarization: return "T";
  }
  return "T";
}


bool greeter::view::ShapeDescriptor::isValid() const {

  if (kind == greeter::view::ShapeKind::Unknown) {
    return false;
  }

  if (kind == greeter::view::ShapeKind::Mesh) {
    // Whole triangles, and at least one of them.
    return !parameters.empty() && parameters.size() % 9 == 0;
  }

  return parameters.size() == expectedParameters(kind);
}


std::vector<float> greeter::view::ShapeDescriptor::getLocalHalfExtent() const {

  std::vector<float> half(3, 0.0f);

  if (!isValid()) {
    return half;
  }

  switch (kind) {

    case greeter::view::ShapeKind::Box:
      half[0] = 0.5f * parameters[0];
      half[1] = 0.5f * parameters[1];
      half[2] = 0.5f * parameters[2];
      break;

    case greeter::view::ShapeKind::Cylinder:
      half[0] = 0.5f * parameters[0];
      half[1] = 0.5f * parameters[0];
      half[2] = 0.5f * parameters[1];
      break;

    case greeter::view::ShapeKind::Sphere:
      half[0] = 0.5f * parameters[0];
      half[1] = half[0];
      half[2] = half[0];
      break;

    case greeter::view::ShapeKind::Tetrahedron:
    case greeter::view::ShapeKind::Mesh: {

      // Both are lists of points, so the extent is however far the furthest
      // of them reaches from the local origin.
      for (size_t i = 0; i + 2 < parameters.size(); i += 3) {
        for (size_t axis = 0; axis < 3; axis++) {
          half[axis] = std::max(half[axis], std::fabs(parameters[i + axis]));
        }
      }
      break;
    }

    case greeter::view::ShapeKind::Point:
    case greeter::view::ShapeKind::Unknown:
      break;
  }

  return half;
}
