#include <greeter/DipoleMagnet.h>
#include <greeter/Quaternion.h>

#include <cmath>
#include <iostream>
#include <limits>


greeter::DipoleMagnet::DipoleMagnet():
  position({0.0f, 0.0f, 0.0f}),
  orientation({1.0f, 0.0f, 0.0f, 0.0f}),
  moment({0.0f, 0.0f, 1.0f}) {}

greeter::DipoleMagnet::DipoleMagnet(std::vector<float> _position,
                                    std::vector<float> _orientation,
                                    std::vector<float> _moment):
  position(std::move(_position)),
  orientation(std::move(_orientation)),
  moment(std::move(_moment)) {}

greeter::DipoleMagnet::DipoleMagnet(const DipoleMagnet& other):
  position(other.position),
  orientation(other.orientation),
  moment(other.moment) {}

greeter::DipoleMagnet::~DipoleMagnet() {}


std::string greeter::DipoleMagnet::getStaticTypeName() { return "dipole"; }

uint16_t greeter::DipoleMagnet::getStaticTypeID() { return 4; }

size_t greeter::DipoleMagnet::numberOfParameters() {
  // position (3), orientation (4), no geometry at all, moment (3)
  return 10;
}

uint16_t greeter::DipoleMagnet::getTypeID() const { return getStaticTypeID(); }

size_t greeter::DipoleMagnet::getNumOfParameters() const {
  return numberOfParameters();
}

std::vector<float> greeter::DipoleMagnet::getPosition() const { return position; }

std::vector<float> greeter::DipoleMagnet::getDimensions() const {
  return std::vector<float>();
}

std::vector<float> greeter::DipoleMagnet::getOrientation() const { return orientation; }

std::vector<float> greeter::DipoleMagnet::getMagnetization() const { return moment; }

std::unique_ptr<greeter::Magnet> greeter::DipoleMagnet::clone() const {
  return std::make_unique<greeter::DipoleMagnet>(*this);
}

void greeter::DipoleMagnet::setPosition(const float& x, const float& y, const float& z) {
  position = {x, y, z};
}

void greeter::DipoleMagnet::translate(const float& x, const float& y, const float& z) {
  position[0] += x;
  position[1] += y;
  position[2] += z;
}

void greeter::DipoleMagnet::display() const {
  std::cout << "------------------------------------------------------" << std::endl;
  std::cout << "DipoleMagnet:" << std::endl;
  std::cout << "  position: " << position[0] << " " << position[1] << " "
            << position[2] << std::endl;
  std::cout << "  orientation: " << orientation[0] << " " << orientation[1] << " "
            << orientation[2] << " " << orientation[3] << std::endl;
  std::cout << "  moment [A m^2]: " << moment[0] << " " << moment[1] << " "
            << moment[2] << std::endl;
}


void greeter::DipoleMagnet::calculateMagneticFieldForAxisAlignedDipole(
    const float* moment,
    const float* observation_point,
    float& result_x, float& result_y, float& result_z) {

  const double x = observation_point[0];
  const double y = observation_point[1];
  const double z = observation_point[2];

  const double r2 = x * x + y * y + z * z;

  if (r2 <= 0.0) {

    // The field of a point dipole really is infinite at the point itself.
    // magpylib says so too, and saying zero instead would hide a field of
    // view that had been asked for right on top of a source.
    const float infinity = std::numeric_limits<float>::infinity();

    result_x = moment[0] == 0.0f ? 0.0f : std::copysign(infinity, moment[0]);
    result_y = moment[1] == 0.0f ? 0.0f : std::copysign(infinity, moment[1]);
    result_z = moment[2] == 0.0f ? 0.0f : std::copysign(infinity, moment[2]);

    return;
  }

  const double r = std::sqrt(r2);
  const double r3 = r2 * r;
  const double r5 = r3 * r2;

  const double projected =
    x * (double) moment[0] + y * (double) moment[1] + z * (double) moment[2];

  // B = mu0 / (4 pi) * (3 (m . r) r / r^5 - m / r^3). The prefactor is written
  // out rather than taken from MU0 / (4 pi) so that the whole of it stays in
  // double until the end.
  const double prefactor = (double) greeter::MU0 / (4.0 * M_PI);

  result_x = (float) (prefactor * (3.0 * projected * x / r5 - (double) moment[0] / r3));
  result_y = (float) (prefactor * (3.0 * projected * y / r5 - (double) moment[1] / r3));
  result_z = (float) (prefactor * (3.0 * projected * z / r5 - (double) moment[2] / r3));
}


void greeter::DipoleMagnet::computeMagneticFieldForDipole(
    const float* parameters,
    const float* observation_point,
    float& result_x, float& result_y, float& result_z) {

  const float* position = &parameters[0];
  const float* orientation = &parameters[3];
  const float* moment = &parameters[7];

  const float translated[3] = {
    observation_point[0] - position[0],
    observation_point[1] - position[1],
    observation_point[2] - position[2]
  };

  // The moment is kept in the frame of the dipole and turned with it, which
  // is the convention every other magnet here follows and the one the
  // arrangements rely on.

  if (translated[0] == 0.0f && translated[1] == 0.0f && translated[2] == 0.0f) {

    // Right on top of the dipole. Turning the moment first and putting the
    // infinity on afterwards, rather than turning a field that already holds
    // one: rotating a vector multiplies its components by the entries of a
    // matrix, and an infinity times a zero is a not-a-number. That would turn
    // an answer of "infinite, along the moment" into "infinite along one
    // axis and unknown along the other two".
    float world_moment[3];

    greeter::Quaternion::applyRotationFromQuaternion(
      orientation, moment, world_moment);

    const float infinity = std::numeric_limits<float>::infinity();

    result_x = world_moment[0] == 0.0f ? 0.0f : std::copysign(infinity, world_moment[0]);
    result_y = world_moment[1] == 0.0f ? 0.0f : std::copysign(infinity, world_moment[1]);
    result_z = world_moment[2] == 0.0f ? 0.0f : std::copysign(infinity, world_moment[2]);

    return;
  }

  float local_point[3];

  greeter::Quaternion::applyInverseRotationFromQuaternion(
    orientation, translated, local_point);

  float local_field[3];

  calculateMagneticFieldForAxisAlignedDipole(
    moment, local_point, local_field[0], local_field[1], local_field[2]);

  float field[3];

  greeter::Quaternion::applyRotationFromQuaternion(orientation, local_field, field);

  result_x = field[0];
  result_y = field[1];
  result_z = field[2];
}


std::vector<float> greeter::DipoleMagnet::computeMagneticField(
    double x, double y, double z) const {

  float parameters[10] = {
    position[0], position[1], position[2],
    orientation[0], orientation[1], orientation[2], orientation[3],
    moment[0], moment[1], moment[2]
  };

  const float observation_point[3] = {(float) x, (float) y, (float) z};

  float b_x = 0.0f;
  float b_y = 0.0f;
  float b_z = 0.0f;

  computeMagneticFieldForDipole(parameters, observation_point, b_x, b_y, b_z);

  return {b_x, b_y, b_z};
}


void greeter::DipoleMagnet::computeMagneticField(
    const float* parameters, const float* observation_point,
    float& b_x, float& b_y, float& b_z) const {
  computeMagneticFieldForDipole(parameters, observation_point, b_x, b_y, b_z);
}


greeter::TargetMeshData greeter::DipoleMagnet::generateTargetMesh(
    const float* parameters, const greeter::MeshingSpec& meshing) {

  // A dipole is a cell already, so there is nothing to split and the meshing
  // input is ignored, as it is for a sphere.
  (void) meshing;

  greeter::MeshCell cell;

  cell.point[0] = 0.0f;
  cell.point[1] = 0.0f;
  cell.point[2] = 0.0f;

  // The moment of a cell is what a cell carries, and here it is given
  // directly rather than worked out from a volume and a polarization.
  cell.moment[0] = parameters[7];
  cell.moment[1] = parameters[8];
  cell.moment[2] = parameters[9];

  return greeter::TargetMeshData{cell};
}


greeter::view::ShapeDescriptor greeter::DipoleMagnet::describeShape(
    const float* parameters) {

  (void) parameters;

  greeter::view::ShapeDescriptor shape;

  shape.kind = greeter::view::ShapeKind::Point;

  return shape;
}


void greeter::DipoleMagnet::computePolarizationForDipole(
    const float* parameters, const float* observation_point,
    float& j_x, float& j_y, float& j_z) {

  (void) parameters;
  (void) observation_point;

  // A point has no volume, so there is nowhere for a polarization to be. A
  // dipole is a moment and not a piece of material.
  j_x = 0.0f;
  j_y = 0.0f;
  j_z = 0.0f;
}
