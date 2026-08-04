#include <greeter/MagnetCollection.h>
#include <greeter/CubicMagnet.h>
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

    nlohmann::json data = nlohmann::json::parse(json_file);
    std::vector<std::string> keys = {"magnets", "field_of_view"};
    std::vector<std::string> magnet_types = {"cuboid", "sphere"};
    std::vector<std::string> cuboid_keys = {"position", "dimensions", "orientation", "magnetization"};
    std::vector<std::string> sphere_keys = {"radius", "magnetization"};
    std::vector<std::string> fov_keys = {"x", "y", "z"};

    for (auto& key : keys) {
        if (!data.contains(key)) {
            return false;
        }
    }

    // Verify magnet types
    for (auto it = data["magnets"].begin(); it != data["magnets"].end(); ++it) {
      bool type_does_not_exist = true;
      for (auto& magnet_type : magnet_types) {
        if (it->contains(magnet_type)) {
          type_does_not_exist = false;
        }
      }
      if (type_does_not_exist) {
        return false;
      }
    }

    // Verify the field of view
    for (auto& fov_key : fov_keys) {
      if (!data["field_of_view"].contains(fov_key)) {
        return false;
      }
    }

        // if (magnet_type == "cuboid") {
        //   for (auto& cuboid_key : cuboid_keys) {
        //     if (!it->at("cuboid").contains(cuboid_key)) {
        //       return false;
        //     }
        //   }
        // } else if (magnet_type == "sphere") {
        //   for (auto& sphere_key : sphere_keys) {
        //     if (!it->at("sphere").contains(sphere_key)) {
        //       return false;
        //     }
        //   }
        // }
    
    std::cout << "VALID JSON FILE" << std::endl;
    return true;

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
    this->magnets.clear();
    /*
       To finish after the JSON parser is implemented 
    */
    nlohmann::json data = nlohmann::json::parse(json_file);

    if (data.contains("magnets")) {
        for (auto& magnet : data["magnets"]) {
            std::string type = magnet["type"];
            if (type == "cuboid") {
                std::vector<float> position = magnet["cuboid"]["parameters"]["position"];
                std::vector<float> dimensions = magnet["cuboid"]["parameters"]["dimensions"];
                std::vector<float> orientation = magnet["cuboid"]["parameters"]["orientation"];
                std::vector<float> magnetization = magnet["cuboid"]["parameters"]["magnetization"];
                // std::unique_ptr<greeter::Magnet> cuboid_magnet = std::make_unique<greeter::CuboidMagnet>(position, dimensions, orientation, magnetization);
                // this->magnets.push_back(std::move(cuboid_magnet));
            } else if (type == "sphere") {
                float radius = magnet["sphere"]["parameters"]["radius"];
                float magnetization = magnet["sphere"]["parameters"]["magnetization"];
                // std::unique_ptr<greeter::Magnet> sphere_magnet = std::make_unique<greeter::SphereMagnet>(radius, magnetization);
                // this->magnets.push_back(std::move(sphere_magnet));
            } else {
                throw std::invalid_argument("Invalid magnet type");
            }
        }
    } else {
        throw std::invalid_argument("Invalid JSON file");
    }
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

  u_int32_t num_magnets = get_num_magnets();

  size_t tot_num_parameters = getTotalNumOfGeoParameters();

  Float3VectorView _positions("positions", num_magnets);
  Float4VectorView _orientations("orientations", num_magnets);
  Float3VectorView _magnetizations("magnetizations", num_magnets);
  FloatVectorView _radii("radii", tot_num_parameters);
  Float3VectorView _observation_points("observation_points", 1);
  UInt32VectorView _magnet_types("magnet_types", num_magnets);

  std::vector<float> positions;
  std::vector<float> orientations;
  std::vector<float> magnetizations;
  std::vector<float> radii;
  std::vector<float> observation_points;

  std::vector<float> geom_dimensions;
  size_t cum_index = 0;

  for (u_int32_t i = 0; i < num_magnets; i++) {

    positions = this->magnets[i]->getPosition();
    orientations = this->magnets[i]->getOrientation();
    radii = this->magnets[i]->getDimensions();
    magnetizations = this->magnets[i]->getMagnetization();

    _positions(i, 0) = positions[0];
    _positions(i, 1) = positions[1];
    _positions(i, 2) = positions[2];

    _orientations(i, 0) = orientations[0];
    _orientations(i, 1) = orientations[1];
    _orientations(i, 2) = orientations[2];
    _orientations(i, 3) = orientations[3];

    _magnetizations(i, 0) = magnetizations[0];
    _magnetizations(i, 1) = magnetizations[1];
    _magnetizations(i, 2) = magnetizations[2];

    _magnet_types(i) = this->magnets[i]->getTypeID();

    size_t num_parameters = this->magnets[i]->getNumOfParameters() - 10;
    geom_dimensions = this->magnets[i]->getDimensions();

    for (size_t j = 0; j < num_parameters; j++) {
      _radii(cum_index  + j) = geom_dimensions[j];
    }
    cum_index += num_parameters;
  }

  std::unique_ptr<greeter::MagneticFieldSimulator> simulator = std::make_unique<greeter::MagneticFieldSimulator>(
    _positions, _orientations, _magnetizations, _radii, _magnet_types, _observation_points
  );

  return simulator;
}


std::vector<std::vector<float>> greeter::MagnetCollection::simulate(const std::vector<std::vector<float>>& fov) const {

  std::unique_ptr<greeter::MagneticFieldSimulator> simulator = createSimulator();

  simulator->fillObservationPoints(fov);

  simulator->fillDimensionParameterCumulativeCount();

  simulator->simulate();

  std::vector<std::vector<float>> magnetic_fields = simulator->getMagneticFields();

  return magnetic_fields;
}

std::vector<std::vector<float>> greeter::MagnetCollection::simulate(const greeter::FieldOfView& fov) const {

  std::unique_ptr<greeter::MagneticFieldSimulator> simulator = createSimulator();

  simulator->fillObservationPoints(fov.getFOV());

  simulator->fillDimensionParameterCumulativeCount();

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

std::unique_ptr<greeter::ForceSimulator> greeter::MagnetCollection::createForceSimulator() const {

  const u_int32_t num_magnets = get_num_magnets();
  const size_t tot_num_geo_parameters = getTotalNumOfGeoParameters();

  Float3VectorView _positions("positions", num_magnets);
  Float4VectorView _orientations("orientations", num_magnets);
  Float3VectorView _magnetizations("magnetizations", num_magnets);
  FloatVectorView _dimensions("dimensions", tot_num_geo_parameters);
  UInt32VectorView _magnet_types("magnet_types", num_magnets);
  UInt32VectorView _geometry_offsets("geometry_offsets", num_magnets);

  size_t cum_index = 0;

  for (u_int32_t i = 0; i < num_magnets; i++) {

    const std::vector<float> position = this->magnets[i]->getPosition();
    const std::vector<float> orientation = this->magnets[i]->getOrientation();
    const std::vector<float> magnetization = this->magnets[i]->getMagnetization();
    const std::vector<float> geom_dimensions = this->magnets[i]->getDimensions();

    _positions(i, 0) = position[0];
    _positions(i, 1) = position[1];
    _positions(i, 2) = position[2];

    _orientations(i, 0) = orientation[0];
    _orientations(i, 1) = orientation[1];
    _orientations(i, 2) = orientation[2];
    _orientations(i, 3) = orientation[3];

    _magnetizations(i, 0) = magnetization[0];
    _magnetizations(i, 1) = magnetization[1];
    _magnetizations(i, 2) = magnetization[2];

    _magnet_types(i) = this->magnets[i]->getTypeID();

    _geometry_offsets(i) = (u_int32_t) cum_index;

    for (size_t j = 0; j < geom_dimensions.size(); j++) {
      _dimensions(cum_index + j) = geom_dimensions[j];
    }
    cum_index += geom_dimensions.size();
  }

  return std::make_unique<greeter::ForceSimulator>(
    _positions, _orientations, _magnetizations, _dimensions,
    _magnet_types, _geometry_offsets
  );
}

std::vector<greeter::ForceResult> greeter::MagnetCollection::computeForces(
    const greeter::ForceConfig& config) const {

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

    // The centroid of the elementary magnets of this library is their position.
    // A not-a-number pivot entry means "use the centroid of this target".
    if (config.centroid_pivot || t >= config.pivots.size()
        || std::isnan(config.pivots[t][0])) {
      target.pivot[0] = position[0];
      target.pivot[1] = position[1];
      target.pivot[2] = position[2];
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
      throw std::invalid_argument("No target mesher registered for this magnet type");
    }

    for (const auto& dimension : magnet.getDimensions()) {
      characteristic_length = std::max(characteristic_length, dimension);
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

  simulator->simulate();

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

  greeter::MagnetCollection halbach_magnet_collection;

  float angle_step = 2.0f * M_PI / num_magnets;

  for (size_t i = 0; i < num_magnets; i++) {

    float angle_rad = i * angle_step;

    float x = radius * cos(angle_rad);
    float y = radius * sin(angle_rad);
    float z = 0.0f;

    std::vector<float> thePosition = {x, y, z};

    float quaternion_rotation[4];

    greeter::Quaternion::set_rotation_from_axis_angle(
      "z", 2.0f*angle_rad, quaternion_rotation
    );

    std::vector<float> magnet_orientation = {
      quaternion_rotation[0], quaternion_rotation[1],
      quaternion_rotation[2], quaternion_rotation[3] 
    };

    std::unique_ptr<greeter::Magnet> cuboid_magnet = std::make_unique<greeter::CuboidMagnet>(
      thePosition, magnet_dimensions, magnet_orientation, magnetization
    );

    halbach_magnet_collection.addMagnet(std::move(cuboid_magnet));
  }

  return halbach_magnet_collection;
}