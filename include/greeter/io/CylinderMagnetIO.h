#ifndef CYLINDER_MAGNET_IO_H
#define CYLINDER_MAGNET_IO_H


#include <greeter/Magnet.h>
#include <nlohmann/json.hpp>
#include <vector>
#include <memory>

namespace greeter {

class CylinderMagnetIO {

    std::string magnet_type = "cylinder";
    std::vector<std::string> keys = {
        "position", "dimensions",
        "orientation", "magnetization"
    };

    public:

        CylinderMagnetIO();
        ~CylinderMagnetIO();

        static std::string getTypeName();

        static std::vector<float> readPosition(const nlohmann::json& magnet);
        static std::vector<float> readDimensions(const nlohmann::json& magnet);
        static std::vector<float> readOrientation(const nlohmann::json& magnet);
        static std::vector<float> readMagnetization(const nlohmann::json& magnet);

        static std::unique_ptr<Magnet> createMagnet(const nlohmann::json& magnet);
    };

}

#endif // CYLINDER_MAGNET_IO_H
