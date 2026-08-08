#include <greeter/TriangleMagnet.h>
#include <greeter/TetrahedronMagnet.h>
#include <greeter/Quaternion.h>

#include <iostream>


greeter::TriangleMagnet::TriangleMagnet():
  position({0.0f, 0.0f, 0.0f}),
  vertices({0.0f, 0.0f, 0.0f,
            1.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f}),
  orientation({1.0f, 0.0f, 0.0f, 0.0f}),
  magnetization({0.0f, 0.0f, 1.0f}) {}

greeter::TriangleMagnet::TriangleMagnet(std::vector<float> _position,
                                        std::vector<float> _vertices,
                                        std::vector<float> _orientation,
                                        std::vector<float> _magnetization):
  position(std::move(_position)),
  vertices(std::move(_vertices)),
  orientation(std::move(_orientation)),
  magnetization(std::move(_magnetization)) {}

greeter::TriangleMagnet::TriangleMagnet(const TriangleMagnet& other):
  position(other.position),
  vertices(other.vertices),
  orientation(other.orientation),
  magnetization(other.magnetization) {}

greeter::TriangleMagnet::~TriangleMagnet() {}


std::string greeter::TriangleMagnet::getStaticTypeName() { return "triangle"; }

uint16_t greeter::TriangleMagnet::getStaticTypeID() { return 5; }

size_t greeter::TriangleMagnet::numberOfParameters() {
  // position (3), orientation (4), three vertices (9), polarization (3)
  return 19;
}

uint16_t greeter::TriangleMagnet::getTypeID() const { return getStaticTypeID(); }

size_t greeter::TriangleMagnet::getNumOfParameters() const {
  return numberOfParameters();
}

std::vector<float> greeter::TriangleMagnet::getPosition() const { return position; }

std::vector<float> greeter::TriangleMagnet::getDimensions() const { return vertices; }

std::vector<float> greeter::TriangleMagnet::getOrientation() const { return orientation; }

std::vector<float> greeter::TriangleMagnet::getMagnetization() const {
  return magnetization;
}

std::vector<float> greeter::TriangleMagnet::getCentroid() const {

  float local[3] = {0.0f, 0.0f, 0.0f};

  for (size_t corner = 0; corner < 3; corner++) {
    for (size_t axis = 0; axis < 3; axis++) {
      local[axis] += vertices[3 * corner + axis] / 3.0f;
    }
  }

  float turned[3];

  greeter::Quaternion::applyRotationFromQuaternion(
    orientation.data(), local, turned);

  return {position[0] + turned[0], position[1] + turned[1], position[2] + turned[2]};
}

std::unique_ptr<greeter::Magnet> greeter::TriangleMagnet::clone() const {
  return std::make_unique<greeter::TriangleMagnet>(*this);
}

void greeter::TriangleMagnet::setPosition(const float& x, const float& y, const float& z) {
  position = {x, y, z};
}

void greeter::TriangleMagnet::translate(const float& x, const float& y, const float& z) {
  position[0] += x;
  position[1] += y;
  position[2] += z;
}

void greeter::TriangleMagnet::display() const {
  std::cout << "------------------------------------------------------" << std::endl;
  std::cout << "TriangleMagnet:" << std::endl;
  std::cout << "  position: " << position[0] << " " << position[1] << " "
            << position[2] << std::endl;
  for (size_t corner = 0; corner < 3; corner++) {
    std::cout << "  vertex " << corner << ": " << vertices[3 * corner] << " "
              << vertices[3 * corner + 1] << " " << vertices[3 * corner + 2]
              << std::endl;
  }
  std::cout << "  polarization [T]: " << magnetization[0] << " "
            << magnetization[1] << " " << magnetization[2] << std::endl;
}


void greeter::TriangleMagnet::computeMagneticFieldForTriangle(
    const float* parameters,
    const float* observation_point,
    float& result_x, float& result_y, float& result_z) {

  const float* position = &parameters[0];
  const float* orientation = &parameters[3];
  const float* vertices = &parameters[7];
  const float* magnetization = &parameters[16];

  const float translated[3] = {
    observation_point[0] - position[0],
    observation_point[1] - position[1],
    observation_point[2] - position[2]
  };

  float local_point[3];

  greeter::Quaternion::applyInverseRotationFromQuaternion(
    orientation, translated, local_point);

  // The one charged facet, which is the piece every polyhedron here is built
  // from. It was already written for the tetrahedron.
  double b_x = 0.0;
  double b_y = 0.0;
  double b_z = 0.0;

  greeter::TetrahedronMagnet::calculateMagneticFieldForTriangle(
    &vertices[0], &vertices[3], &vertices[6],
    magnetization, local_point,
    b_x, b_y, b_z);

  const float local_field[3] = {(float) b_x, (float) b_y, (float) b_z};

  float field[3];

  greeter::Quaternion::applyRotationFromQuaternion(orientation, local_field, field);

  result_x = field[0];
  result_y = field[1];
  result_z = field[2];
}


std::vector<float> greeter::TriangleMagnet::computeMagneticField(
    double x, double y, double z) const {

  std::vector<float> parameters;
  parameters.reserve(numberOfParameters());

  for (const auto& value : position) parameters.push_back(value);
  for (const auto& value : orientation) parameters.push_back(value);
  for (const auto& value : vertices) parameters.push_back(value);
  for (const auto& value : magnetization) parameters.push_back(value);

  const float observation_point[3] = {(float) x, (float) y, (float) z};

  float b_x = 0.0f;
  float b_y = 0.0f;
  float b_z = 0.0f;

  computeMagneticFieldForTriangle(parameters.data(), observation_point, b_x, b_y, b_z);

  return {b_x, b_y, b_z};
}


void greeter::TriangleMagnet::computeMagneticField(
    const float* parameters, const float* observation_point,
    float& b_x, float& b_y, float& b_z) const {
  computeMagneticFieldForTriangle(parameters, observation_point, b_x, b_y, b_z);
}


greeter::TargetMeshData greeter::TriangleMagnet::generateTargetMesh(
    const float* parameters, const greeter::MeshingSpec& meshing) {

  (void) parameters;
  (void) meshing;

  // A surface encloses no volume, so there is no moment to give a cell. The
  // caller reports this as a target that cannot be meshed.
  return greeter::TargetMeshData();
}


greeter::view::ShapeDescriptor greeter::TriangleMagnet::describeShape(
    const float* parameters) {

  greeter::view::ShapeDescriptor shape;

  // A mesh of exactly one triangle, which the viewer already knows how to
  // draw without being told what a triangle is.
  shape.kind = greeter::view::ShapeKind::Mesh;
  shape.parameters.assign(&parameters[7], &parameters[7] + 9);

  return shape;
}


void greeter::TriangleMagnet::computePolarizationForTriangle(
    const float* parameters, const float* observation_point,
    float& j_x, float& j_y, float& j_z) {

  (void) parameters;
  (void) observation_point;

  // A surface encloses no volume, so there is no inside for it to be the
  // polarization of.
  j_x = 0.0f;
  j_y = 0.0f;
  j_z = 0.0f;
}
