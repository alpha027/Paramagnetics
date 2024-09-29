#include <greeter/MagnetCollection.h>
#include <nlohmann/json.hpp>

greeter::MagnetCollection::MagnetCollection() {}

greeter::MagnetCollection::MagnetCollection(const MagnetCollection& other) {
    magnets.reserve(other.magnets.size());
    for (const auto& base : other.magnets) {
        if (base) {
            magnets.push_back(base->clone());
        }
    }
}

// greeter::MagnetCollection::MagnetCollection(const MagnetCollection& other) {
//   this->magnets.clear();
//   this->magnets = other.magnets;
// }

greeter::MagnetCollection::MagnetCollection(std::vector<std::unique_ptr<greeter::Magnet>> theMagnets):
  magnets(std::move(theMagnets)) {}

greeter::MagnetCollection::MagnetCollection(std::fstream& json_file) {
    this->magnets.clear();
    /*
       To finish after the JSON parser is implemented 
    */
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

u_int32_t greeter::MagnetCollection::get_num_magnets() const {
  return magnets.size();
}

std::unique_ptr<greeter::MagneticFieldSimulator> greeter::MagnetCollection::createSimulator() const {

  u_int32_t num_magnets = get_num_magnets();

  Float3VectorView _positions("positions", num_magnets);
  Float3VectorView _orientations("orientations", num_magnets);
  Float3VectorView _magnetizations("magnetizations", num_magnets);
  Float3VectorView _radii("radii", num_magnets);
  Float3VectorView _observation_points("observation_points", num_magnets);
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
  }


  std::unique_ptr<greeter::MagneticFieldSimulator> simulator = std::make_unique<greeter::MagneticFieldSimulator>(
    _positions, _orientations, _magnetizations, _radii, _magnet_types, _observation_points
  );

// simulator->fillMagnetPositions(positions);
//   simulator->fillMagnetOrientations(orientations);
//   simulator->fillMagnetMagnetizations(magnetizations);
//   simulator->fillMagnetRadii(radii);
//   simulator->fillObservationPoints(observation_points);

  return simulator;
}