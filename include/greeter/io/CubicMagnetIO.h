#ifndef CUBIC_MAGNET_IO_H
#define CUBIC_MAGNET_IO_H


#include <greeter/Magnet.h>
#include <greeter/MagneticFieldMethodFactory.h>
#include <nlohmann/json.hpp>
#include <vector>
#include <memory>

namespace greeter {

class CubicMagnetIO {

    std::string magnet_type = "cuboid";
    std::vector<std::string> keys = {
        "position", "dimensions",
        "orientation", "magnetization"
    };

    public:

        CubicMagnetIO();
        ~CubicMagnetIO();

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

#endif // CUBIC_MAGNET_IO_H