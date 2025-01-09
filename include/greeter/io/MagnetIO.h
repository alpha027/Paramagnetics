#ifndef MAGNET_IO_H
#define MAGNET_IO_H

#include <greeter/Magnet.h>
#include <nlohmann/json.hpp>
#include <vector>
#include <memory>


namespace greeter {

class MagnetIO {

    public:

        MagnetIO();
        ~MagnetIO();

        static bool validateJSON(const nlohmann::json& data);

        // void writePosition(nlohmann::json& magnet, const std::vector<float>& position);
        // void writeDimensions(nlohmann::json& magnet, const std::vector<float>& dimensions);
        // void writeOrientation(nlohmann::json& magnet, const std::vector<float>& orientation);
        // void writeMagnetization(nlohmann::json& magnet, const std::vector<float>& magnetization);

        static std::unique_ptr<Magnet> createMagnet(const nlohmann::json& magnet);
};


}  // namespace greeter


#endif // MAGNET_IO_H