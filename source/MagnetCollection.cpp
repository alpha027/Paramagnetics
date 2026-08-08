#include <greeter/MagnetCollection.h>
#include <greeter/CubicMagnet.h>
#include <greeter/arrangements/HalbachRingArrangement.h>
#include <greeter/io/MagnetIO.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <string>

greeter::MagnetCollection::MagnetCollection() {}

greeter::MagnetCollection::MagnetCollection(const MagnetCollection& other) {
    magnets.reserve(other.magnets.size());
    for (const auto& base : other.magnets) {
        if (base) {
            magnets.push_back(base->clone());
        }
    }
}

greeter::MagnetCollection::MagnetCollection(std::vector<std::unique_ptr<greeter::Magnet>> theMagnets):
  magnets(std::move(theMagnets)) {}


bool greeter::MagnetCollection::validJsonFile(std::ifstream& json_file) const {

    // The magnet types this library knows are listed by the readers themselves,
    // in MethodFactoryIO, and the schema is checked in a single place.
    return greeter::MagnetIO::validateJSON(nlohmann::json::parse(json_file));
}


size_t greeter::MagnetCollection::getTotalNumOfParameters() const {
    size_t total = 0;
    for (const auto& magnet : magnets) {
        total += magnet->getNumOfParameters();
    }
    return total;
}

size_t greeter::MagnetCollection::getTotalNumOfGeoParameters() const {
    size_t total = 0;
    for (const auto& magnet : magnets) {
        total += magnet->getNumOfParameters() - 10;
    }
    return total;
}

greeter::MagnetCollection::MagnetCollection(std::ifstream& json_file) {

    // Every magnet type is built by its own reader, which MagnetIO dispatches
    // on, so that a new type does not have to be listed here as well.
    greeter::MagnetCollection parsed =
      greeter::MagnetIO::read(nlohmann::json::parse(json_file));

    this->magnets = std::move(parsed.magnets);
}

greeter::MagnetCollection::~MagnetCollection() {}

void greeter::MagnetCollection::addMagnet(std::unique_ptr<greeter::Magnet> magnet) {
  this->magnets.push_back(std::move(magnet));
}

void greeter::MagnetCollection::removeMagnet(const size_t& index) {
  if (index >= this->magnets.size()) {
    throw std::out_of_range("Index out of range");
  }
  this->magnets.erase(this->magnets.begin() + index);
}

void greeter::MagnetCollection::clearCollection() {
  this->magnets.clear();
}

void greeter::MagnetCollection::translate(const float& x, const float& y, const float& z) {
  for (auto& magnet : this->magnets) {
    magnet->translate(x, y, z);
  }
}

u_int32_t greeter::MagnetCollection::get_num_magnets() const {
  return magnets.size();
}

std::unique_ptr<greeter::MagneticFieldSimulator> greeter::MagnetCollection::createSimulator() const {

  UInt32VectorView _magnet_types("magnet_types", get_num_magnets());
  UInt32VectorView _parameter_offsets("parameter_offsets", get_num_magnets());
  FloatVectorView _magnet_parameters("magnet_parameters", getTotalNumOfParameters());

  fillMagnetParameters(_magnet_parameters, _parameter_offsets, _magnet_types);

  Float3VectorView _observation_points("observation_points", 1);

  return std::make_unique<greeter::MagneticFieldSimulator>(
    _magnet_parameters, _parameter_offsets, _magnet_types, _observation_points
  );
}


/*
  Lay the parameters of every magnet down end to end, each block in the order
  the field kernels read it. Doing it once here is what lets a shape take as
  many geometry numbers as it needs, see MagnetParameters.h.
*/
void greeter::MagnetCollection::fillMagnetParameters(
    FloatVectorView magnet_parameters, UInt32VectorView parameter_offsets,
    UInt32VectorView magnet_types) const {

  size_t at = 0;

  for (u_int32_t i = 0; i < get_num_magnets(); i++) {

    const std::vector<float> parameters = getMagnetParameters(i);

    parameter_offsets(i) = (u_int32_t) at;
    magnet_types(i) = this->magnets[i]->getTypeID();

    for (size_t j = 0; j < parameters.size(); j++) {
      magnet_parameters(at + j) = parameters[j];
    }

    at += parameters.size();
  }
}


std::vector<std::vector<float>> greeter::MagnetCollection::simulate(const std::vector<std::vector<float>>& fov) const {

  std::unique_ptr<greeter::MagneticFieldSimulator> simulator = createSimulator();

  simulator->fillObservationPoints(fov);

  simulator->simulate();

  std::vector<std::vector<float>> magnetic_fields = simulator->getMagneticFields();

  return magnetic_fields;
}

std::vector<std::vector<float>> greeter::MagnetCollection::simulate(const greeter::FieldOfView& fov) const {

  std::unique_ptr<greeter::MagneticFieldSimulator> simulator = createSimulator();

  simulator->fillObservationPoints(fov.getPoints());

  simulator->simulate();

  std::vector<std::vector<float>> magnetic_fields = simulator->getMagneticFields();

  return magnetic_fields;
}

std::vector<float> greeter::MagnetCollection::getMagnetParameters(const size_t& index) const {

  if (index >= this->magnets.size()) {
    throw std::out_of_range("Index out of range");
  }

  const greeter::Magnet& magnet = *this->magnets[index];

  // Layout expected by the field kernels and by the target meshers:
  // position (3), orientation (4), geometric dimensions (n), magnetization (3).
  std::vector<float> parameters;
  parameters.reserve(magnet.getNumOfParameters());

  for (const auto& value : magnet.getPosition()) {
    parameters.push_back(value);
  }
  for (const auto& value : magnet.getOrientation()) {
    parameters.push_back(value);
  }
  for (const auto& value : magnet.getDimensions()) {
    parameters.push_back(value);
  }
  for (const auto& value : magnet.getMagnetization()) {
    parameters.push_back(value);
  }

  return parameters;
}

uint16_t greeter::MagnetCollection::getMagnetTypeID(const size_t& index) const {

  if (index >= this->magnets.size()) {
    throw std::out_of_range("Index out of range");
  }

  return this->magnets[index]->getTypeID();
}

std::unique_ptr<greeter::ForceSimulator> greeter::MagnetCollection::createForceSimulator() const {

  UInt32VectorView _magnet_types("magnet_types", get_num_magnets());
  UInt32VectorView _parameter_offsets("parameter_offsets", get_num_magnets());
  FloatVectorView _magnet_parameters("magnet_parameters", getTotalNumOfParameters());

  fillMagnetParameters(_magnet_parameters, _parameter_offsets, _magnet_types);

  return std::make_unique<greeter::ForceSimulator>(
    _magnet_parameters, _parameter_offsets, _magnet_types
  );
}

std::vector<greeter::ForceResult> greeter::MagnetCollection::computeForces(
    const greeter::ForceConfig& config) const {

  return computeForces(config, true);
}

std::vector<greeter::ForceResult> greeter::MagnetCollection::computeForces(
    const greeter::ForceConfig& config, const bool& verbose) const {

  const u_int32_t num_magnets = get_num_magnets();

  std::unique_ptr<greeter::ForceSimulator> simulator = createForceSimulator();

  std::vector<greeter::TargetSpec> targets;
  targets.reserve(config.targets.size());

  float characteristic_length = 0.0f;

  for (size_t t = 0; t < config.targets.size(); t++) {

    const uint32_t magnet_index = config.targets[t];

    if (magnet_index >= num_magnets) {
      throw std::out_of_range("Force target index out of range");
    }

    const greeter::Magnet& magnet = *this->magnets[magnet_index];

    const std::vector<float> parameters = getMagnetParameters(magnet_index);
    const std::vector<float> position = magnet.getPosition();
    const std::vector<float> orientation = magnet.getOrientation();

    greeter::MeshingSpec meshing;
    if (t < config.meshing.size()) {
      meshing = config.meshing[t];
    }

    greeter::TargetSpec target;
    target.magnet_index = magnet_index;

    target.position[0] = position[0];
    target.position[1] = position[1];
    target.position[2] = position[2];

    target.orientation[0] = orientation[0];
    target.orientation[1] = orientation[1];
    target.orientation[2] = orientation[2];
    target.orientation[3] = orientation[3];

    // A not-a-number pivot entry means "use the centroid of this target". That
    // is the position for a shape that is symmetric around it, but not for a
    // tetrahedron, whose barycenter is wherever its vertices put it.
    if (config.centroid_pivot || t >= config.pivots.size()
        || std::isnan(config.pivots[t][0])) {
      const std::vector<float> centroid = magnet.getCentroid();
      target.pivot[0] = centroid[0];
      target.pivot[1] = centroid[1];
      target.pivot[2] = centroid[2];
    } else {
      target.pivot[0] = config.pivots[t][0];
      target.pivot[1] = config.pivots[t][1];
      target.pivot[2] = config.pivots[t][2];
    }

    if (t < config.sources.size()) {
      target.sources = config.sources[t];
    }

    target.mesh = greeter::TargetMeshFactory::getInstance().generateTargetMesh(
      magnet.getTypeID(), parameters.data(), meshing
    );

    if (target.mesh.empty()) {

      // Either nobody registered a mesher for the type, or the type has no
      // volume to mesh. A charged surface is the second: it carries no moment
      // for a force to act on.
      throw std::invalid_argument(
        "The magnet of type " + std::to_string(magnet.getTypeID()) +
        " cannot be a force target: it was meshed into no cells, which means "
        "either that no mesher is registered for it or that it encloses no "
        "volume, as an open surface does");
    }

    // The vertices of a tetrahedron may well be negative, so the size of a
    // magnet is read off the magnitude of its geometric parameters.
    for (const auto& dimension : magnet.getDimensions()) {
      characteristic_length = std::max(characteristic_length, std::fabs(dimension));
    }

    targets.push_back(std::move(target));
  }

  simulator->fillTargets(targets);

  // The field kernels are evaluated in single precision, so the finite
  // difference step cannot be as small as the 1e-5 * size of magpylib without
  // drowning the gradient in round-off. 1e-3 * size keeps both the round-off
  // and the truncation error of the central difference small.
  float eps = config.eps;
  if (eps <= 0.0f) {
    eps = 1e-3f * characteristic_length;
    if (eps <= 0.0f) {
      eps = 1e-6f;
    }
  }
  simulator->setFiniteDifferenceStep(eps);

  if (config.mesh_report) {
    simulator->printMeshReport();
  }

  simulator->simulate(verbose);

  return simulator->getResults();
}

std::vector<std::vector<float>> greeter::MagnetCollection::simulateForces(
    const greeter::ForceConfig& config) const {

  std::vector<std::vector<float>> matrix;

  for (const auto& result : computeForces(config)) {
    matrix.push_back({
      (float) result.target_index,
      result.force[0], result.force[1], result.force[2],
      result.torque[0], result.torque[1], result.torque[2]
    });
  }

  return matrix;
}

void greeter::MagnetCollection::display(size_t index) const {
  if (index >= this->magnets.size()) {
    throw std::out_of_range("Index out of range");
  }
  this->magnets[index]->display();
}

void greeter::MagnetCollection::display() const {
  for (const auto& magnet : this->magnets) {
    magnet->display();
  }
}

greeter::MagnetCollection greeter::MagnetCollection::operator+(const MagnetCollection& other) const {
  greeter::MagnetCollection new_collection(*this);
  for (const auto& base : other.magnets) {
    if (base) {
      new_collection.magnets.push_back(base->clone());
    }
  }
  return new_collection;
}

greeter::MagnetCollection greeter::MagnetCollection::generateCircularHalbachArray(
      const float& radius, const std::vector<float>& magnet_dimensions,
      const size_t& num_magnets, const std::vector<float>& magnetization ){

  // A ring of cuboids turned by twice the angle at which they sit is a Halbach
  // ring of order one, which the input file can now ask for by name. This is
  // that arrangement, so that there is one place the ring is laid out in.
  const nlohmann::json arrangement = {
    {"type", greeter::HalbachRingArrangement::getTypeName()},
    {"parameters", {
      {"radius", radius},
      {"count", num_magnets},
      {"order", 1},
      {"element", {
        {"type", "cuboid"},
        {"parameters", {
          {"dimensions", magnet_dimensions},
          {"magnetization", magnetization}
        }}
      }}
    }}
  };

  return greeter::MagnetCollection(
    greeter::HalbachRingArrangement::expand(arrangement));
}