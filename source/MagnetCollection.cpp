#include <greeter/MagnetCollection.h>
#include <greeter/CubicMagnet.h>
#include <nlohmann/json.hpp>
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

  Float3VectorView _positions("positions", num_magnets);
  Float4VectorView _orientations("orientations", num_magnets);
  Float3VectorView _magnetizations("magnetizations", num_magnets);
  Float3VectorView _radii("radii", num_magnets);
  Float3VectorView _observation_points("observation_points", 1);
  UInt32VectorView _magnet_types("magnet_types", num_magnets);

  std::vector<float> positions;
  std::vector<float> orientations;
  std::vector<float> magnetizations;
  std::vector<float> radii;
  std::vector<float> observation_points;

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

    _radii(i, 0) = radii[0];
    _radii(i, 1) = radii[1];
    _radii(i, 2) = radii[2];

    _magnetizations(i, 0) = magnetizations[0];
    _magnetizations(i, 1) = magnetizations[1];
    _magnetizations(i, 2) = magnetizations[2];

    _magnet_types(i) = this->magnets[i]->getTypeID();
  }

  std::unique_ptr<greeter::MagneticFieldSimulator> simulator = std::make_unique<greeter::MagneticFieldSimulator>(
    _positions, _orientations, _magnetizations, _radii, _magnet_types, _observation_points
  );

  return simulator;
}


std::vector<std::vector<float>> greeter::MagnetCollection::simulate(const std::vector<std::vector<float>>& fov) const {

  std::unique_ptr<greeter::MagneticFieldSimulator> simulator = createSimulator();

  simulator->fillObservationPoints(fov);

  simulator->simulate();

  std::vector<std::vector<float>> magnetic_fields = simulator->getMagneticFields();

  return magnetic_fields;
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