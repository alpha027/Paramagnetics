#include <greeter/io/MagnetIO.h>
#include <greeter/io/MethodFactoryIO.h>
#include <set>


namespace greeter {

    MagnetIO::MagnetIO() {}

    MagnetIO::~MagnetIO() {}

    bool MagnetIO::validateJSON(const nlohmann::json& data) {

        std::set<std::string> keys = {"magnets", "field_of_view"};
        std::set<std::string> magnet_types = {"cuboid", "sphere"};

        std::set<std::string> cuboid_keys = {"position", "dimensions", "orientation", "magnetization"};
        std::set<std::string> sphere_keys = {"radius", "magnetization"};
        std::set<std::string> fov_keys = {"x", "y", "z"};

        for (auto& key : keys) {
            if (!data.contains(key)) {
                return false;
            }
        };

        for (auto it = data["magnets"].begin(); it != data["magnets"].end(); ++it) {
            bool magnetTypeExists = magnet_types.count(it->at("type"));
            if (!magnetTypeExists) {
                return false;
            }
        };

        // Verify magnet types
        // for (auto it = data["magnets"].begin(); it != data["magnets"].end(); ++it) {
        // bool type_does_not_exist = true;
        //     for (auto& magnet_type : magnet_types) {
        //         if (it->contains(magnet_type)) {
        //         type_does_not_exist = false;
        //         }
        //     }
        //     if (type_does_not_exist) {
        //         return false;
        //     }
        // };


        // Verify the field of view
        for (auto& fov_key : fov_keys) {
        if (!data["field_of_view"].contains(fov_key)) {
            return false;
        }
        };

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

    };

    greeter::MagnetCollection MagnetIO::read(const nlohmann::json& data) {

        bool isValid = MagnetIO::validateJSON(data);

        if (!isValid) {
            throw std::invalid_argument("Invalid JSON file");
        }

        greeter::MagnetCollection magnet_collection;

        for (auto& magnet : data["magnets"]) {
            std::string magnetType = magnet["type"];
            magnet_collection.addMagnet(
                MethodFactoryIO::getInstance().createMagnet(magnetType, magnet)
            );
            }

        // for (auto& magnet : data["magnets"]) {
        //     std::string type = magnet["type"];
        //     if (type == "cuboid") {
        //         std::vector<float> position = magnet["cuboid"]["parameters"]["position"];
        //         std::vector<float> dimensions = magnet["cuboid"]["parameters"]["dimensions"];
        //         std::vector<float> orientation = magnet["cuboid"]["parameters"]["orientation"];
        //         std::vector<float> magnetization = magnet["cuboid"]["parameters"]["magnetization"];
        //         std::unique_ptr<greeter::Magnet> cuboid_magnet = std::make_unique<greeter::CuboidMagnet>(position, dimensions, orientation, magnetization);
        //         magnet_collection.addMagnet(std::move(cuboid_magnet));
        //     } else if (type == "sphere") {
        //         float radius = magnet["sphere"]["parameters"]["radius"];
        //         float magnetization = magnet["sphere"]["parameters"]["magnetization"];
        //         std::unique_ptr<greeter::Magnet> sphere_magnet = std::make_unique<greeter::SphereMagnet>(radius, magnetization);
        //         magnet_collection.addMagnet(std::move(sphere_magnet));
        //     } else {
        //         throw std::invalid_argument("Invalid magnet type");
        //     }
        // }
        return magnet_collection;
    }
}  // namespace greeter
