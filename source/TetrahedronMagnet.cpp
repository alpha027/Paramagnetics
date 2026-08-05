#include <greeter/TetrahedronMagnet.h>
#include <greeter/Quaternion.h>
#include <greeter/TargetMeshFactory.h>
#include <algorithm>
#include <cmath>

namespace {

  // A tetrahedron of unit volume, kept small enough to stay a sensible default.
  const std::vector<float> DEFAULT_VERTICES = {
    0.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 1.0f
  };

  inline void cross(const double* a, const double* b, double* result) {
    result[0] = a[1]*b[2] - a[2]*b[1];
    result[1] = a[2]*b[0] - a[0]*b[2];
    result[2] = a[0]*b[1] - a[1]*b[0];
  }

  inline double dot(const double* a, const double* b) {
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
  }

}  // namespace

greeter::TetrahedronMagnet::TetrahedronMagnet() :
  position({0.0f, 0.0f, 0.0f}), vertices(DEFAULT_VERTICES),
  orientation({1.0f, 0.0f, 0.0f, 0.0f}), magnetization({0.0f, 0.0f, 1.0f}) {}

greeter::TetrahedronMagnet::TetrahedronMagnet(
    std::vector<float> _position, std::vector<float> _vertices,
    std::vector<float> _orientation, std::vector<float> _magnetization) :
  position(std::move(_position)), vertices(std::move(_vertices)),
  orientation(std::move(_orientation)), magnetization(std::move(_magnetization)) {}

greeter::TetrahedronMagnet::TetrahedronMagnet(const TetrahedronMagnet& other) :
  position(other.position), vertices(other.vertices),
  orientation(other.orientation), magnetization(other.magnetization) {}

greeter::TetrahedronMagnet::~TetrahedronMagnet() {}

std::unique_ptr<greeter::Magnet> greeter::TetrahedronMagnet::clone() const {
  return std::make_unique<TetrahedronMagnet>(*this);
}

std::vector<float> greeter::TetrahedronMagnet::getPosition() const {
  return {position[0], position[1], position[2]};
}

std::vector<float> greeter::TetrahedronMagnet::getCentroid() const {

  // The vertices are given in the local frame, so the barycenter has to be
  // rotated and translated along with them.
  float barycenter[3] = {0.0f, 0.0f, 0.0f};

  for (size_t k = 0; k < 4; k++) {
    for (size_t i = 0; i < 3; i++) {
      barycenter[i] += vertices[3*k + i] / 4.0f;
    }
  }

  float rotated[3];

  greeter::Quaternion::applyRotationFromQuaternion(
    orientation.data(), barycenter, rotated);

  return {
    position[0] + rotated[0],
    position[1] + rotated[1],
    position[2] + rotated[2]
  };
}

std::vector<float> greeter::TetrahedronMagnet::getDimensions() const {
  return vertices;
}

std::vector<float> greeter::TetrahedronMagnet::getOrientation() const {
  return {orientation[0], orientation[1], orientation[2], orientation[3]};
}

std::vector<float> greeter::TetrahedronMagnet::getMagnetization() const {
  return {magnetization[0], magnetization[1], magnetization[2]};
}

size_t greeter::TetrahedronMagnet::getNumOfParameters() const {
  return greeter::TetrahedronMagnet::numberOfParameters();
}

size_t greeter::TetrahedronMagnet::numberOfParameters() {
  // position (3), orientation (4), four vertices (12), magnetization (3)
  return 22;
}

void greeter::TetrahedronMagnet::setPosition(const float& x, const float& y, const float& z) {
  position[0] = x;
  position[1] = y;
  position[2] = z;
}

void greeter::TetrahedronMagnet::setMagnetization(const float& x, const float& y, const float& z) {
  magnetization[0] = x;
  magnetization[1] = y;
  magnetization[2] = z;
}

void greeter::TetrahedronMagnet::translate(const float& x, const float& y, const float& z) {
  position[0] += x;
  position[1] += y;
  position[2] += z;
}

void greeter::TetrahedronMagnet::display() const {
  std::cout << "------------------------------------------------------" << std::endl;
  std::cout << "TetrahedronMagnet:" << std::endl;
  std::cout << "  position : (" << position[0] << ", " << position[1] << ", " << position[2] << ")" << std::endl;
  for (size_t k = 0; k < 4; k++) {
    std::cout << "  vertex " << k << " : (" << vertices[3*k] << ", "
              << vertices[3*k + 1] << ", " << vertices[3*k + 2] << ")" << std::endl;
  }
  std::cout << "  orientation : (" << orientation[0] << ", " << orientation[1] << ", "
            << orientation[2] << ", " << orientation[3] << ")" << std::endl;
  std::cout << "  magnetization : (" << magnetization[0] << ", " << magnetization[1]
            << ", " << magnetization[2] << ")" << std::endl;
  std::cout << "------------------------------------------------------" << std::endl;
}

float greeter::TetrahedronMagnet::volumeOfTetrahedron(const float* vertices) {

  double edge[3][3];

  for (size_t k = 0; k < 3; k++) {
    for (size_t i = 0; i < 3; i++) {
      edge[k][i] = (double) vertices[3*(k + 1) + i] - (double) vertices[i];
    }
  }

  double normal[3];
  cross(edge[1], edge[2], normal);

  return (float) (std::fabs(dot(edge[0], normal)) / 6.0);
}


void greeter::TetrahedronMagnet::calculateMagneticFieldForTriangle(
        const float* vertex_a, const float* vertex_b, const float* vertex_c,
        const float* magnetization,
        const float* observation_point,
        double& result_x, double& result_y, double& result_z
      )
{
  result_x = 0.0;
  result_y = 0.0;
  result_z = 0.0;

  const double vertex[3][3] = {
    {vertex_a[0], vertex_a[1], vertex_a[2]},
    {vertex_b[0], vertex_b[1], vertex_b[2]},
    {vertex_c[0], vertex_c[1], vertex_c[2]}
  };

  // Surface normal, its direction follows the right hand rule of the vertex order
  double first_edge[3];
  double second_edge[3];

  for (size_t i = 0; i < 3; i++) {
    first_edge[i] = vertex[1][i] - vertex[0][i];
    second_edge[i] = vertex[2][i] - vertex[0][i];
  }

  double normal[3];
  cross(first_edge, second_edge, normal);

  const double normal_length = std::sqrt(dot(normal, normal));

  if (!(normal_length > 0.0)) {  // a degenerate triangle carries no charge
    return;
  }

  for (size_t i = 0; i < 3; i++) {
    normal[i] /= normal_length;
  }

  // Magnetic surface charge, the projection of the polarization on the face
  const double sigma = normal[0] * (double) magnetization[0]
                     + normal[1] * (double) magnetization[1]
                     + normal[2] * (double) magnetization[2];

  // Vertex to observer, and vertex to next vertex
  double to_observer[3][3];
  double distance[3];
  double edge[3][3];
  double edge_length[3];

  for (size_t k = 0; k < 3; k++) {
    for (size_t i = 0; i < 3; i++) {
      to_observer[k][i] = vertex[k][i] - (double) observation_point[i];
      edge[k][i] = vertex[(k + 1) % 3][i] - vertex[k][i];
    }
    distance[k] = std::sqrt(dot(to_observer[k], to_observer[k]));
    edge_length[k] = std::sqrt(dot(edge[k], edge[k]));
  }

  // Line integral along the three edges
  double line_integral[3] = {0.0, 0.0, 0.0};

  for (size_t k = 0; k < 3; k++) {

    if (!(edge_length[k] > 0.0)) {
      continue;
    }

    const double squared_length = edge_length[k] * edge_length[k];
    const double squared_distance = distance[k] * distance[k];
    const double projection = dot(to_observer[k], edge[k]);
    const double reduced_projection = projection / edge_length[k];

    // The measure of how close the observer is to the edge, or to its
    // extension beyond a corner, where the first expression breaks down.
    const double closeness = std::fabs(distance[k] + reduced_projection);

    double integral;

    if (closeness > 1.0e-12) {
      integral = std::log(
        (std::sqrt(squared_length + 2.0 * projection + squared_distance)
         + edge_length[k] + reduced_projection) / closeness) / edge_length[k];
    } else {
      integral = -std::log(
        std::fabs(edge_length[k] - distance[k]) / distance[k]) / edge_length[k];
    }

    for (size_t i = 0; i < 3; i++) {
      line_integral[i] += integral * edge[k][i];
    }
  }

  // Solid angle the triangle subtends at the observer
  double swept[3];
  cross(to_observer[1], to_observer[0], swept);

  const double numerator = dot(to_observer[2], swept);

  const double denominator =
      distance[0] * distance[1] * distance[2]
    + dot(to_observer[2], to_observer[1]) * distance[0]
    + dot(to_observer[2], to_observer[0]) * distance[1]
    + dot(to_observer[1], to_observer[0]) * distance[2];

  double solid_angle = 2.0 * std::atan2(numerator, denominator);

  // Keep the jumps on the edges out of the result
  if (std::fabs(solid_angle) > 6.2831853) {
    solid_angle = 0.0;
  }

  double normal_cross_integral[3];
  cross(normal, line_integral, normal_cross_integral);

  const double field[3] = {
    sigma * (normal[0] * solid_angle - normal_cross_integral[0]) / (4.0 * M_PI),
    sigma * (normal[1] * solid_angle - normal_cross_integral[1]) / (4.0 * M_PI),
    sigma * (normal[2] * solid_angle - normal_cross_integral[2]) / (4.0 * M_PI)
  };

  // The formula degenerates on the corners of the triangle
  if (!std::isfinite(field[0]) || !std::isfinite(field[1]) || !std::isfinite(field[2])) {
    return;
  }

  result_x = field[0];
  result_y = field[1];
  result_z = field[2];
}


void greeter::TetrahedronMagnet::calculateMagneticFieldForAxisAlignedTetrahedron(
        const float* vertices,
        const float* magnetization,
        const float* observation_point,
        float& result_x, float& result_y, float& result_z
      )
{
  float vertex[4][3];

  for (size_t k = 0; k < 4; k++) {
    for (size_t i = 0; i < 3; i++) {
      vertex[k][i] = vertices[3*k + i];
    }
  }

  // Barycentric coordinates of the observer, used below to tell whether it sits
  // inside the body. They are read off the vertices as they were given.
  double edge[3][3];
  double to_observer[3];

  for (size_t k = 0; k < 3; k++) {
    for (size_t i = 0; i < 3; i++) {
      edge[k][i] = (double) vertex[k + 1][i] - (double) vertex[0][i];
    }
  }
  for (size_t i = 0; i < 3; i++) {
    to_observer[i] = (double) observation_point[i] - (double) vertex[0][i];
  }

  double second_cross_third[3];
  double observer_cross_third[3];
  double second_cross_observer[3];

  cross(edge[1], edge[2], second_cross_third);
  cross(to_observer, edge[2], observer_cross_third);
  cross(edge[1], to_observer, second_cross_observer);

  const double determinant = dot(edge[0], second_cross_third);

  bool inside = false;

  if (determinant != 0.0) {
    const double first = dot(to_observer, second_cross_third) / determinant;
    const double second = dot(edge[0], observer_cross_third) / determinant;
    const double third = dot(edge[0], second_cross_observer) / determinant;
    inside = first >= 0.0 && second >= 0.0 && third >= 0.0
             && first + second + third <= 1.0;
  }

  // Order the vertices so that they form a right handed system, which makes the
  // four face normals below point out of the body.
  if (determinant < 0.0) {
    for (size_t i = 0; i < 3; i++) {
      std::swap(vertex[2][i], vertex[3][i]);
    }
  }

  // The four faces, wound so that their normals point outwards
  const size_t faces[4][3] = {
    {0, 2, 1}, {0, 1, 3}, {1, 2, 3}, {0, 3, 2}
  };

  double b_x = 0.0;
  double b_y = 0.0;
  double b_z = 0.0;

  for (size_t f = 0; f < 4; f++) {

    double face_x, face_y, face_z;

    greeter::TetrahedronMagnet::calculateMagneticFieldForTriangle(
      vertex[faces[f][0]], vertex[faces[f][1]], vertex[faces[f][2]],
      magnetization, observation_point,
      face_x, face_y, face_z
    );

    b_x += face_x;
    b_y += face_y;
    b_z += face_z;
  }

  // Inside the body the polarization adds to the field of the surface charges
  if (inside) {
    b_x += (double) magnetization[0];
    b_y += (double) magnetization[1];
    b_z += (double) magnetization[2];
  }

  result_x = (float) b_x;
  result_y = (float) b_y;
  result_z = (float) b_z;
}


void greeter::TetrahedronMagnet::calculateMagneticFieldForTetrahedron(
        const float* position, const float* orientation,
        const float* vertices, const float* magnetization,
        const float* observation_point,
        float& result_x, float& result_y, float& result_z
      )
{
  float translated_observation_point[3] = {
    observation_point[0] - position[0],
    observation_point[1] - position[1],
    observation_point[2] - position[2]
  };

  float rotated_observation_point[3];

  greeter::Quaternion::applyInverseRotationFromQuaternion(
    orientation,
    translated_observation_point,
    rotated_observation_point
  );

  float rotated_B_x, rotated_B_y, rotated_B_z;

  greeter::TetrahedronMagnet::calculateMagneticFieldForAxisAlignedTetrahedron(
    vertices, magnetization,
    rotated_observation_point,
    rotated_B_x, rotated_B_y, rotated_B_z
  );

  float rotated_field[3] = {rotated_B_x, rotated_B_y, rotated_B_z};

  float final_magnetic_field[3];

  greeter::Quaternion::applyRotationFromQuaternion(
    orientation,
    rotated_field,
    final_magnetic_field
  );

  result_x = final_magnetic_field[0];
  result_y = final_magnetic_field[1];
  result_z = final_magnetic_field[2];
}


std::vector<float> greeter::TetrahedronMagnet::calculateMagneticFieldForTetrahedron(
        const float* position, const float* orientation,
        const float* vertices, const float* magnetization,
        const float* observation_point ) {

  float result_x = 0.0f;
  float result_y = 0.0f;
  float result_z = 0.0f;

  greeter::TetrahedronMagnet::calculateMagneticFieldForTetrahedron(
    position, orientation, vertices, magnetization, observation_point,
    result_x, result_y, result_z
  );

  return {result_x, result_y, result_z};
}


void greeter::TetrahedronMagnet::computeMagneticFieldForTetrahedron(
    const float* parameters, const float* observation_point,
    float& result_x, float& result_y, float& result_z ) {

  greeter::TetrahedronMagnet::calculateMagneticFieldForTetrahedron(
    &parameters[0],   // position
    &parameters[3],   // orientation
    &parameters[7],   // four vertices
    &parameters[19],  // magnetization
    observation_point,
    result_x, result_y, result_z
  );
}


void greeter::TetrahedronMagnet::computeMagneticField(
    const float* parameters, const float* observation_point,
    float& b_x, float& b_y, float& b_z) const {

  greeter::TetrahedronMagnet::computeMagneticFieldForTetrahedron(
    parameters, observation_point, b_x, b_y, b_z);
}


std::vector<float> greeter::TetrahedronMagnet::computeMagneticField(
    double x, double y, double z) const {

  float parameters[22];

  for (size_t i = 0; i < 3; i++) {
    parameters[i] = position[i];
  }
  for (size_t i = 0; i < 4; i++) {
    parameters[3 + i] = orientation[i];
  }
  for (size_t i = 0; i < 12; i++) {
    parameters[7 + i] = vertices[i];
  }
  for (size_t i = 0; i < 3; i++) {
    parameters[19 + i] = magnetization[i];
  }

  float observation_point[3] = {(float) x, (float) y, (float) z};

  float b_x, b_y, b_z;

  greeter::TetrahedronMagnet::computeMagneticFieldForTetrahedron(
    parameters, observation_point, b_x, b_y, b_z);

  return {b_x, b_y, b_z};
}


std::string greeter::TetrahedronMagnet::getStaticTypeName() {
  return "tetrahedron";
}

uint16_t greeter::TetrahedronMagnet::getTypeID() const {
  return greeter::TetrahedronMagnet::getStaticTypeID();
}

uint16_t greeter::TetrahedronMagnet::getStaticTypeID() {
  return 2;
}


greeter::TargetMeshData greeter::TetrahedronMagnet::generateTargetMesh(
    const float* parameters, const greeter::MeshingSpec& meshing) {

  const float* vertices = &parameters[7];
  const float* polarization = &parameters[19];

  const float volume = greeter::TetrahedronMagnet::volumeOfTetrahedron(vertices);

  uint32_t requested = meshing.total;

  if (meshing.explicit_split) {
    // A tetrahedron has no three natural axes to split along, so an explicit
    // split is read as the number of cells it asks for.
    requested = std::max(1u, meshing.n[0]) * std::max(1u, meshing.n[1])
              * std::max(1u, meshing.n[2]);
  }

  greeter::TargetMeshData mesh;

  if (requested <= 1) {

    greeter::MeshCell cell;

    for (size_t i = 0; i < 3; i++) {
      cell.point[i] = 0.0f;
      for (size_t k = 0; k < 4; k++) {
        cell.point[i] += vertices[3*k + i] / 4.0f;
      }
      cell.moment[i] = volume * polarization[i] / greeter::MU0;
    }

    mesh.push_back(cell);
    return mesh;
  }

  // A uniform grid in barycentric coordinates with n subdivisions holds
  // (n+1)(n+2)(n+3)/6 points. Pick the n that comes closest to the request.
  auto points_of = [](uint32_t n) -> uint32_t {
    return (n + 1) * (n + 2) * (n + 3) / 6;
  };

  int64_t estimate = (int64_t) std::lround(
    std::cbrt(6.0 * (double) requested) - 1.5);
  estimate = std::max((int64_t) 1, estimate);

  uint32_t subdivisions = (uint32_t) estimate;
  int64_t best_difference =
    std::llabs((int64_t) points_of(subdivisions) - (int64_t) requested);

  for (int64_t candidate = std::max((int64_t) 1, estimate - 2);
       candidate <= estimate + 3; candidate++) {

    const int64_t difference =
      std::llabs((int64_t) points_of((uint32_t) candidate) - (int64_t) requested);

    if (difference < best_difference) {
      best_difference = difference;
      subdivisions = (uint32_t) candidate;
    }
  }

  const uint32_t num_cells = points_of(subdivisions);
  const float cell_volume = volume / (float) num_cells;
  const float moment_scale = cell_volume / greeter::MU0;

  mesh.reserve(num_cells);

  for (uint32_t i = 0; i <= subdivisions; i++) {
    for (uint32_t j = 0; j <= subdivisions - i; j++) {
      for (uint32_t k = 0; k <= subdivisions - i - j; k++) {

        const uint32_t l = subdivisions - i - j - k;

        const float weight[4] = {
          (float) i / (float) subdivisions,
          (float) j / (float) subdivisions,
          (float) k / (float) subdivisions,
          (float) l / (float) subdivisions
        };

        greeter::MeshCell cell;

        for (size_t c = 0; c < 3; c++) {
          cell.point[c] = 0.0f;
          for (size_t v = 0; v < 4; v++) {
            cell.point[c] += weight[v] * vertices[3*v + c];
          }
          cell.moment[c] = moment_scale * polarization[c];
        }

        mesh.push_back(cell);
      }
    }
  }

  return mesh;
}
