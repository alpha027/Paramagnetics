#include <greeter/io/MagnetIO.h>
#include <greeter/io/MethodFactoryIO.h>
#include <set>


namespace greeter {

    MagnetIO::MagnetIO() {}

    MagnetIO::~MagnetIO() {}

    bool MagnetIO::validateJSON(const nlohmann::json& data) {

        std::set<std::string> keys = {"magnets"};
        std::set<std::string> magnet_types = {"cuboid", "sphere", "tetrahedron"};

        std::set<std::string> fov_keys = {"x", "y", "z"};

        // A field of view is only needed for a magnetic field simulation, an
        // input file that only asks for forces may leave it out.
        if (!data.contains("force")) {
            keys.insert("field_of_view");
        }

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

        // Verify the field of view
        if (data.contains("field_of_view")) {
            for (auto& fov_key : fov_keys) {
                if (!data["field_of_view"].contains(fov_key)) {
                    return false;
                }
            };
        };

        // The parameters of a magnet are checked by the reader of its own type,
        // which is the only place that knows what its shape takes.
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
