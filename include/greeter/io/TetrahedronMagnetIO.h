#ifndef TETRAHEDRON_MAGNET_IO_H
#define TETRAHEDRON_MAGNET_IO_H


#include <greeter/Magnet.h>
#include <nlohmann/json.hpp>
#include <vector>
#include <memory>

namespace greeter {

class TetrahedronMagnetIO {

    std::string magnet_type = "tetrahedron";
    std::vector<std::string> keys = {
        "position", "vertices",
        "orientation", "magnetization"
    };

    public:

        TetrahedronMagnetIO();
        ~TetrahedronMagnetIO();

        static std::string getTypeName();

        static std::vector<float> readPosition(const nlohmann::json& magnet);
        static std::vector<float> readVertices(const nlohmann::json& magnet);
        static std::vector<float> readOrientation(const nlohmann::json& magnet);
        static std::vector<float> readMagnetization(const nlohmann::json& magnet);

        static std::unique_ptr<Magnet> createMagnet(const nlohmann::json& magnet);
    };

}

#endif // TETRAHEDRON_MAGNET_IO_H
