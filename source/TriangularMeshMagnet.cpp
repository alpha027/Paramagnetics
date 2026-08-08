#include <greeter/TriangularMeshMagnet.h>
#include <greeter/TetrahedronMagnet.h>
#include <greeter/Quaternion.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <map>
#include <stdexcept>


namespace {

/* Where the triangles start inside a parameter block. */
constexpr size_t TRIANGLES_AT = 8;

size_t faceCountOf(const float* parameters) {
  return (size_t) (parameters[7] + 0.5f);
}

void crossOf(const double* a, const double* b, double* result) {
  result[0] = a[1] * b[2] - a[2] * b[1];
  result[1] = a[2] * b[0] - a[0] * b[2];
  result[2] = a[0] * b[1] - a[1] * b[0];
}

double dotOf(const double* a, const double* b) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

double lengthOf(const double* a) {
  return std::sqrt(dotOf(a, a));
}

/*
  An edge of the surface, as a pair of vertex positions rounded to a grid.

  Two faces that share an edge have to agree about where it is, and their
  vertices are floats that came out of the same arithmetic, so comparing them
  exactly is right often enough to be tempting and wrong often enough to
  matter. Rounding to a grid far finer than any geometry and far coarser than
  the last bits of a float makes the comparison say what it means.
*/
using Point = std::array<long long, 3>;

Point pointKey(const float* vertex) {

  Point key{};

  for (size_t axis = 0; axis < 3; axis++) {
    key[axis] = (long long) std::llround((double) vertex[axis] * 1e9);
  }

  return key;
}

}  // namespace


greeter::TriangularMeshMagnet::TriangularMeshMagnet():
  position({0.0f, 0.0f, 0.0f}),
  orientation({1.0f, 0.0f, 0.0f, 0.0f}),
  magnetization({0.0f, 0.0f, 1.0f}) {

  // The smallest closed surface there is, a tetrahedron of unit legs.
  const float a[3] = {0.0f, 0.0f, 0.0f};
  const float b[3] = {1.0f, 0.0f, 0.0f};
  const float c[3] = {0.0f, 1.0f, 0.0f};
  const float d[3] = {0.0f, 0.0f, 1.0f};

  const float* faces[4][3] = {
    {a, c, b}, {a, b, d}, {b, c, d}, {a, d, c}
  };

  for (const auto& face : faces) {
    for (size_t corner = 0; corner < 3; corner++) {
      for (size_t axis = 0; axis < 3; axis++) {
        triangles.push_back(face[corner][axis]);
      }
    }
  }

  checkAndOrient(triangles);
}


greeter::TriangularMeshMagnet::TriangularMeshMagnet(
    std::vector<float> _position, std::vector<float> _triangles,
    std::vector<float> _orientation, std::vector<float> _magnetization):
  position(std::move(_position)),
  triangles(std::move(_triangles)),
  orientation(std::move(_orientation)),
  magnetization(std::move(_magnetization)) {

  checkAndOrient(triangles);
}


greeter::TriangularMeshMagnet::TriangularMeshMagnet(const TriangularMeshMagnet& other):
  position(other.position),
  triangles(other.triangles),
  orientation(other.orientation),
  magnetization(other.magnetization) {}


greeter::TriangularMeshMagnet::~TriangularMeshMagnet() {}


std::string greeter::TriangularMeshMagnet::getStaticTypeName() {
  return "triangular_mesh";
}

uint16_t greeter::TriangularMeshMagnet::getStaticTypeID() { return 6; }

uint16_t greeter::TriangularMeshMagnet::getTypeID() const { return getStaticTypeID(); }

size_t greeter::TriangularMeshMagnet::getNumOfFaces() const {
  return triangles.size() / 9;
}

size_t greeter::TriangularMeshMagnet::getNumOfParameters() const {
  // position (3), orientation (4), the face count (1), the faces, J (3)
  return 11 + triangles.size();
}

std::vector<float> greeter::TriangularMeshMagnet::getPosition() const { return position; }

std::vector<float> greeter::TriangularMeshMagnet::getDimensions() const {

  // The face count leads, so that a kernel given nothing but a pointer can
  // tell where the faces stop.
  std::vector<float> dimensions;
  dimensions.reserve(1 + triangles.size());

  dimensions.push_back((float) getNumOfFaces());

  dimensions.insert(dimensions.end(), triangles.begin(), triangles.end());

  return dimensions;
}

std::vector<float> greeter::TriangularMeshMagnet::getOrientation() const {
  return orientation;
}

std::vector<float> greeter::TriangularMeshMagnet::getMagnetization() const {
  return magnetization;
}

std::unique_ptr<greeter::Magnet> greeter::TriangularMeshMagnet::clone() const {
  return std::make_unique<greeter::TriangularMeshMagnet>(*this);
}

void greeter::TriangularMeshMagnet::setPosition(
    const float& x, const float& y, const float& z) {
  position = {x, y, z};
}

void greeter::TriangularMeshMagnet::translate(
    const float& x, const float& y, const float& z) {
  position[0] += x;
  position[1] += y;
  position[2] += z;
}

void greeter::TriangularMeshMagnet::display() const {
  std::cout << "------------------------------------------------------" << std::endl;
  std::cout << "TriangularMeshMagnet:" << std::endl;
  std::cout << "  faces: " << getNumOfFaces() << std::endl;
  std::cout << "  volume [m^3]: " << getVolume() << std::endl;
  std::cout << "  position: " << position[0] << " " << position[1] << " "
            << position[2] << std::endl;
  std::cout << "  polarization [T]: " << magnetization[0] << " "
            << magnetization[1] << " " << magnetization[2] << std::endl;
}


double greeter::TriangularMeshMagnet::volumeOf(
    const float* triangles, const size_t& face_count) {

  // The divergence theorem: the volume is the sum over the faces of
  // a . (b x c) / 6, which counts the signed volume of the tetrahedra each
  // face makes with the origin. Positive for an outward wound surface.
  double total = 0.0;

  for (size_t face = 0; face < face_count; face++) {

    const float* corner = &triangles[9 * face];

    double a[3], b[3], c[3];

    for (size_t axis = 0; axis < 3; axis++) {
      a[axis] = (double) corner[axis];
      b[axis] = (double) corner[3 + axis];
      c[axis] = (double) corner[6 + axis];
    }

    double b_cross_c[3];
    crossOf(b, c, b_cross_c);

    total += dotOf(a, b_cross_c);
  }

  return total / 6.0;
}


float greeter::TriangularMeshMagnet::getVolume() const {
  return (float) volumeOf(triangles.data(), getNumOfFaces());
}


std::vector<float> greeter::TriangularMeshMagnet::getCentroid() const {

  const size_t face_count = getNumOfFaces();

  // The centre of volume, by the same decomposition the volume uses: each
  // face makes a tetrahedron with the origin, whose centroid is a quarter of
  // the way along the sum of its corners.
  double weighted[3] = {0.0, 0.0, 0.0};
  double total = 0.0;

  for (size_t face = 0; face < face_count; face++) {

    const float* corner = &triangles[9 * face];

    double a[3], b[3], c[3];

    for (size_t axis = 0; axis < 3; axis++) {
      a[axis] = (double) corner[axis];
      b[axis] = (double) corner[3 + axis];
      c[axis] = (double) corner[6 + axis];
    }

    double b_cross_c[3];
    crossOf(b, c, b_cross_c);

    const double signed_volume = dotOf(a, b_cross_c) / 6.0;

    total += signed_volume;

    for (size_t axis = 0; axis < 3; axis++) {
      weighted[axis] += signed_volume * (a[axis] + b[axis] + c[axis]) / 4.0;
    }
  }

  float local[3] = {0.0f, 0.0f, 0.0f};

  if (std::fabs(total) > 0.0) {
    for (size_t axis = 0; axis < 3; axis++) {
      local[axis] = (float) (weighted[axis] / total);
    }
  }

  float turned[3];

  greeter::Quaternion::applyRotationFromQuaternion(
    orientation.data(), local, turned);

  return {position[0] + turned[0], position[1] + turned[1], position[2] + turned[2]};
}


void greeter::TriangularMeshMagnet::checkAndOrient(std::vector<float>& triangles) {

  if (triangles.empty() || triangles.size() % 9 != 0) {
    throw std::invalid_argument(
      "A triangular mesh is whole triangles, nine numbers each, and at least "
      "one of them");
  }

  const size_t face_count = triangles.size() / 9;

  if (face_count < 4) {
    throw std::invalid_argument(
      "A closed surface takes at least four triangles, and this one has " +
      std::to_string(face_count));
  }

  // Every edge of a closed, consistently wound surface is walked exactly
  // twice, once in each direction: the two faces that share it go round it
  // opposite ways. Counting the directed edges therefore catches both a hole
  // in the surface and a face that was wound the wrong way round.
  std::map<std::pair<Point, Point>, int> directed;

  for (size_t face = 0; face < face_count; face++) {

    const float* corner = &triangles[9 * face];

    for (size_t edge = 0; edge < 3; edge++) {

      const Point from = pointKey(&corner[3 * edge]);
      const Point to = pointKey(&corner[3 * ((edge + 1) % 3)]);

      if (from == to) {
        throw std::invalid_argument(
          "A triangular mesh has a face with two corners in the same place, "
          "which encloses nothing");
      }

      directed[{from, to}] += 1;
    }
  }

  for (const auto& entry : directed) {

    if (entry.second != 1) {
      throw std::invalid_argument(
        "A triangular mesh has an edge walked the same way by more than one "
        "face, so its faces are not wound consistently");
    }

    const auto reverse = directed.find({entry.first.second, entry.first.first});

    if (reverse == directed.end()) {
      throw std::invalid_argument(
        "A triangular mesh has an edge belonging to only one face, so the "
        "surface is not closed");
    }
  }

  // Closed and consistent, but possibly inside out, which would put the wrong
  // sign on every face charge. Which way round a mesh comes out of a CAD
  // program is not worth making anyone think about, so it is simply turned.
  if (volumeOf(triangles.data(), face_count) < 0.0) {
    for (size_t face = 0; face < face_count; face++) {
      for (size_t axis = 0; axis < 3; axis++) {
        std::swap(triangles[9 * face + 3 + axis], triangles[9 * face + 6 + axis]);
      }
    }
  }
}


bool greeter::TriangularMeshMagnet::isInside(
    const float* triangles, const size_t& face_count,
    const float* observation_point) {

  // Seen from inside a closed surface, its faces cover the whole sphere and
  // the solid angles they subtend add to 4 pi. From outside they cancel to
  // zero. Nothing has to be cast in any particular direction, so there is no
  // direction that makes this go wrong.
  double total = 0.0;

  for (size_t face = 0; face < face_count; face++) {

    const float* corner = &triangles[9 * face];

    double a[3], b[3], c[3];

    for (size_t axis = 0; axis < 3; axis++) {
      a[axis] = (double) corner[axis] - (double) observation_point[axis];
      b[axis] = (double) corner[3 + axis] - (double) observation_point[axis];
      c[axis] = (double) corner[6 + axis] - (double) observation_point[axis];
    }

    const double length_a = lengthOf(a);
    const double length_b = lengthOf(b);
    const double length_c = lengthOf(c);

    // Sitting exactly on a corner: it is on the surface, which counts as in.
    if (length_a == 0.0 || length_b == 0.0 || length_c == 0.0) {
      return true;
    }

    double b_cross_c[3];
    crossOf(b, c, b_cross_c);

    // Van Oosterom and Strackee, IEEE Trans. Biomed. Eng. BME-30(2), 1983.
    const double numerator = dotOf(a, b_cross_c);

    const double denominator =
      length_a * length_b * length_c +
      dotOf(a, b) * length_c +
      dotOf(a, c) * length_b +
      dotOf(b, c) * length_a;

    total += 2.0 * std::atan2(numerator, denominator);
  }

  /*
    Strictly outside, the faces cancel and the total is zero. Strictly inside,
    they cover the sphere and it is 4 pi. There is nothing in between except
    the surface itself: a point on a face sees 2 pi, one on an edge sees the
    angle of the wedge, one on a corner the angle of the corner. So the test
    is against nothing rather than against half, which counts a point on the
    surface as inside.

    That is a choice, and it is the one the tetrahedron here already makes
    with its barycentric test, and the one magpylib makes. A point exactly on
    the surface of a magnet has no single answer to give, since the field is
    discontinuous across it, so what matters is that the same convention is
    used everywhere rather than which one it is.

    The threshold sits far above what the cancellation leaves behind, some
    1e-15 per face, and far below any wedge worth calling one.
  */
  return std::fabs(total) > 1e-6;
}


void greeter::TriangularMeshMagnet::computeMagneticFieldForTriangularMesh(
    const float* parameters,
    const float* observation_point,
    float& result_x, float& result_y, float& result_z) {

  const float* position = &parameters[0];
  const float* orientation = &parameters[3];

  const size_t face_count = faceCountOf(parameters);

  const float* triangles = &parameters[TRIANGLES_AT];
  const float* magnetization = &parameters[TRIANGLES_AT + 9 * face_count];

  const float translated[3] = {
    observation_point[0] - position[0],
    observation_point[1] - position[1],
    observation_point[2] - position[2]
  };

  float local_point[3];

  greeter::Quaternion::applyInverseRotationFromQuaternion(
    orientation, translated, local_point);

  double b_x = 0.0;
  double b_y = 0.0;
  double b_z = 0.0;

  for (size_t face = 0; face < face_count; face++) {

    const float* corner = &triangles[9 * face];

    double face_x, face_y, face_z;

    greeter::TetrahedronMagnet::calculateMagneticFieldForTriangle(
      &corner[0], &corner[3], &corner[6],
      magnetization, local_point,
      face_x, face_y, face_z);

    b_x += face_x;
    b_y += face_y;
    b_z += face_z;
  }

  // Inside the body the polarization adds to the field of the surface
  // charges, exactly as it does for a tetrahedron.
  if (isInside(triangles, face_count, local_point)) {
    b_x += (double) magnetization[0];
    b_y += (double) magnetization[1];
    b_z += (double) magnetization[2];
  }

  const float local_field[3] = {(float) b_x, (float) b_y, (float) b_z};

  float field[3];

  greeter::Quaternion::applyRotationFromQuaternion(orientation, local_field, field);

  result_x = field[0];
  result_y = field[1];
  result_z = field[2];
}


std::vector<float> greeter::TriangularMeshMagnet::computeMagneticField(
    double x, double y, double z) const {

  std::vector<float> parameters;
  parameters.reserve(getNumOfParameters());

  for (const auto& value : position) parameters.push_back(value);
  for (const auto& value : orientation) parameters.push_back(value);
  for (const auto& value : getDimensions()) parameters.push_back(value);
  for (const auto& value : magnetization) parameters.push_back(value);

  const float observation_point[3] = {(float) x, (float) y, (float) z};

  float b_x = 0.0f;
  float b_y = 0.0f;
  float b_z = 0.0f;

  computeMagneticFieldForTriangularMesh(
    parameters.data(), observation_point, b_x, b_y, b_z);

  return {b_x, b_y, b_z};
}


void greeter::TriangularMeshMagnet::computeMagneticField(
    const float* parameters, const float* observation_point,
    float& b_x, float& b_y, float& b_z) const {
  computeMagneticFieldForTriangularMesh(
    parameters, observation_point, b_x, b_y, b_z);
}


greeter::TargetMeshData greeter::TriangularMeshMagnet::generateTargetMesh(
    const float* parameters, const greeter::MeshingSpec& meshing) {

  const size_t face_count = faceCountOf(parameters);

  const float* triangles = &parameters[TRIANGLES_AT];
  const float* polarization = &parameters[TRIANGLES_AT + 9 * face_count];

  const double volume = volumeOf(triangles, face_count);

  greeter::TargetMeshData mesh;

  if (!(volume > 0.0)) {
    return mesh;
  }

  const double moment_scale = volume / (double) greeter::MU0;

  uint32_t wanted = meshing.total;

  if (meshing.explicit_split) {
    wanted = std::max(1u, meshing.n[0]) * std::max(1u, meshing.n[1]) *
             std::max(1u, meshing.n[2]);
  }

  if (wanted <= 1) {

    // One cell at the centre of volume, which is the far field of the body
    // and all a coarse target needs.
    double weighted[3] = {0.0, 0.0, 0.0};
    double total = 0.0;

    for (size_t face = 0; face < face_count; face++) {

      const float* corner = &triangles[9 * face];

      double a[3], b[3], c[3];

      for (size_t axis = 0; axis < 3; axis++) {
        a[axis] = (double) corner[axis];
        b[axis] = (double) corner[3 + axis];
        c[axis] = (double) corner[6 + axis];
      }

      double b_cross_c[3];
      crossOf(b, c, b_cross_c);

      const double signed_volume = dotOf(a, b_cross_c) / 6.0;

      total += signed_volume;

      for (size_t axis = 0; axis < 3; axis++) {
        weighted[axis] += signed_volume * (a[axis] + b[axis] + c[axis]) / 4.0;
      }
    }

    greeter::MeshCell cell;

    for (size_t axis = 0; axis < 3; axis++) {
      cell.point[axis] = (float) (weighted[axis] / total);
      cell.moment[axis] = (float) (moment_scale * (double) polarization[axis]);
    }

    return greeter::TargetMeshData{cell};
  }

  // A body of any shape has no natural way to be cut up, so the box around it
  // is cut into a grid and the cells whose centres fall inside are kept.
  float low[3];
  float high[3];

  for (size_t axis = 0; axis < 3; axis++) {
    low[axis] = triangles[axis];
    high[axis] = triangles[axis];
  }

  for (size_t corner = 0; corner < 3 * face_count; corner++) {
    for (size_t axis = 0; axis < 3; axis++) {
      const float value = triangles[3 * corner + axis];
      low[axis] = std::min(low[axis], value);
      high[axis] = std::max(high[axis], value);
    }
  }

  const double box_volume =
    (double) (high[0] - low[0]) * (double) (high[1] - low[1]) *
    (double) (high[2] - low[2]);

  // Aiming for `wanted` cells inside the body, so the grid over the box has
  // to be finer by however much of the box the body fills.
  const double fill = box_volume > 0.0 ? volume / box_volume : 1.0;

  const double target_cells = (double) wanted / std::max(fill, 1e-3);

  uint32_t n[3] = {1, 1, 1};

  const double cell_size = std::cbrt(box_volume / target_cells);

  for (size_t axis = 0; axis < 3; axis++) {
    const double span = (double) (high[axis] - low[axis]);
    n[axis] = (uint32_t) std::max(1L, std::lround(span / cell_size));
  }

  std::vector<float> points;

  for (uint32_t i = 0; i < n[0]; i++) {
    for (uint32_t j = 0; j < n[1]; j++) {
      for (uint32_t k = 0; k < n[2]; k++) {

        const uint32_t at[3] = {i, j, k};

        float point[3];

        for (size_t axis = 0; axis < 3; axis++) {
          const float span = high[axis] - low[axis];
          point[axis] = low[axis] +
            span * ((float) at[axis] + 0.5f) / (float) n[axis];
        }

        if (isInside(triangles, face_count, point)) {
          points.insert(points.end(), point, point + 3);
        }
      }
    }
  }

  if (points.empty()) {
    // A body too thin for the grid to catch. One cell at the centre is a
    // worse answer than a fine one, and a better answer than none.
    greeter::MeshingSpec single;
    single.total = 1;
    return generateTargetMesh(parameters, single);
  }

  const size_t kept = points.size() / 3;

  // The cells stand in for the whole body, so the moments are shared out to
  // add up to exactly what the body carries however badly the grid tiles it.
  const double per_cell = moment_scale / (double) kept;

  mesh.reserve(kept);

  for (size_t cell_index = 0; cell_index < kept; cell_index++) {

    greeter::MeshCell cell;

    for (size_t axis = 0; axis < 3; axis++) {
      cell.point[axis] = points[3 * cell_index + axis];
      cell.moment[axis] = (float) (per_cell * (double) polarization[axis]);
    }

    mesh.push_back(cell);
  }

  return mesh;
}


greeter::view::ShapeDescriptor greeter::TriangularMeshMagnet::describeShape(
    const float* parameters) {

  const size_t face_count = faceCountOf(parameters);

  greeter::view::ShapeDescriptor shape;

  shape.kind = greeter::view::ShapeKind::Mesh;
  shape.parameters.assign(&parameters[TRIANGLES_AT],
                          &parameters[TRIANGLES_AT] + 9 * face_count);

  return shape;
}


void greeter::TriangularMeshMagnet::computePolarizationForTriangularMesh(
    const float* parameters, const float* observation_point,
    float& j_x, float& j_y, float& j_z) {

  j_x = 0.0f;
  j_y = 0.0f;
  j_z = 0.0f;

  const float* position = &parameters[0];
  const float* orientation = &parameters[3];

  const size_t face_count = faceCountOf(parameters);

  const float* triangles = &parameters[TRIANGLES_AT];
  const float* magnetization = &parameters[TRIANGLES_AT + 9 * face_count];

  const float translated[3] = {
    observation_point[0] - position[0],
    observation_point[1] - position[1],
    observation_point[2] - position[2]
  };

  float local[3];

  greeter::Quaternion::applyInverseRotationFromQuaternion(
    orientation, translated, local);

  if (!isInside(triangles, face_count, local)) {
    return;
  }

  float turned[3];

  greeter::Quaternion::applyRotationFromQuaternion(
    orientation, magnetization, turned);

  j_x = turned[0];
  j_y = turned[1];
  j_z = turned[2];
}
