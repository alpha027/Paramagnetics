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

        return true;
    };

    greeter::MagnetCollection MagnetIO::read(const nlohmann::json& data) {

        bool isValid = MagnetIO::validateJSON(data);

        if (!isValid) {
            throw std::invalid_argument("Invalid JSON file");
        }

        greeter::MagnetCollection magnet_collection;

        for (auto& magnet : data["magnets"]) {
            std::string magnetType = (std::string) magnet["type"].get<std::string>();
            std::cout << "magnet type in read: " << magnetType << std::endl;
            magnet_collection.addMagnet(
                MethodFactoryIO::getInstance().createMagnet(magnetType, magnet)
            );
        }

        return magnet_collection;
    }

    greeter::FieldOfView MagnetIO::readFieldOfView(const nlohmann::json& fov) {

        std::vector<std::string> keys = {"x", "y", "z"};

        for (auto& key : keys) {
            if (!fov.contains(key)) {
                throw std::invalid_argument("Invalid field of view");
            }
        }

        std::vector<float> xxyyzz = {
            fov["x"]["min"],
            fov["x"]["max"],
            fov["y"]["min"],
            fov["y"]["max"],
            fov["z"]["min"],
            fov["z"]["max"]
        };

        std::vector<u_int32_t> nnn = {
            fov["x"]["n"],
            fov["y"]["n"],
            fov["z"]["n"]
        };

        return greeter::FieldOfView(xxyyzz, nnn);
    }
}  // namespace greeter
