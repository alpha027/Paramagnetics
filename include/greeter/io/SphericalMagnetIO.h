#ifndef SPHERICAL_MAGNET_IO_H
#define SPHERICAL_MAGNET_IO_H


#include <greeter/Magnet.h>
#include <nlohmann/json.hpp>
#include <vector>
#include <memory>

namespace greeter {

class SphericalMagnetIO {

    std::string magnet_type = "sphere";
    std::vector<std::string> keys = {
        "position", "dimensions",
        "orientation", "magnetization"
    };

    public:

        SphericalMagnetIO();
        ~SphericalMagnetIO();

        static std::string getTypeName();

        static std::vector<float> readPosition(const nlohmann::json& magnet);
        static std::vector<float> readDimensions(const nlohmann::json& magnet);
        static std::vector<float> readOrientation(const nlohmann::json& magnet);
        static std::vector<float> readMagnetization(const nlohmann::json& magnet);

        // void writePosition(nlohmann::json& magnet, const std::vector<float>& position);
        // void writeDimensions(nlohmann::json& magnet, const std::vector<float>& dimensions);
        // void writeOrientation(nlohmann::json& magnet, const std::vector<float>& orientation);
        // void writeMagnetization(nlohmann::json& magnet, const std::vector<float>& magnetization);

        static std::unique_ptr<Magnet> createMagnet(const nlohmann::json& magnet);
    };

}

#endif // SPHERICAL_MAGNET_IO_H