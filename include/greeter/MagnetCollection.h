#ifndef MAGNET_COLLECTION_H
#define MAGNET_COLLECTION_H

#include <greeter/Magnet.h>
#include <greeter/MagneticFieldMethodFactory.h>
#include <greeter/Quaternion.h>
#include <vector>
#include <memory>
#include <fstream>
#include <greeter/MagneticFieldSimulator_i.h>


namespace greeter {

class MagnetCollection {

  std::vector<std::unique_ptr<greeter::Magnet>> magnets;

  public:

    MagnetCollection();
    MagnetCollection(const MagnetCollection& other);
    MagnetCollection(std::vector<std::unique_ptr<greeter::Magnet>> magnets);
    MagnetCollection(std::ifstream& json_file);
    // MagnetCollection()
    ~MagnetCollection();

    void addMagnet(std::unique_ptr<greeter::Magnet> magnet);
    void removeMagnet(const size_t& index);

    void computeMagneticField(const float* observation_point, float& b_x, float& b_y, float& b_z) const;

    bool validJsonFile(std::ifstream& json_file) const;

    // MagnetCollection generateArray(float radius, size_t num_magnets);

    u_int32_t get_num_magnets() const;

    void display(size_t index) const;

    std::unique_ptr<greeter::MagneticFieldSimulator> createSimulator() const;

    std::vector<std::vector<float>> simulate(const std::vector<std::vector<float>>& fov) const;
};

}  // namespace greeter


#endif // MAGNET_COLLECTION_H