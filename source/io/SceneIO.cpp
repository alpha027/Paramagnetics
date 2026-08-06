#include <greeter/io/SceneIO.h>
#include <greeter/io/ArrangementFactoryIO.h>
#include <greeter/io/MagnetIO.h>
#include <greeter/io/MethodFactoryIO.h>

#include <algorithm>
#include <stdexcept>
#include <unordered_set>
#include <string>


namespace greeter {

    SceneIO::SceneIO() {}

    SceneIO::~SceneIO() {}

    greeter::Scene SceneIO::read(const nlohmann::json& data) {

        if (!greeter::MagnetIO::validateJSON(data)) {
            throw std::invalid_argument("Invalid JSON file");
        }

        greeter::Scene scene;

        // The magnets the file lists, in the order it lists them.
        if (data.contains("magnets")) {

            int64_t index = 0;

            for (const auto& magnet : data["magnets"]) {

                scene.collection.addMagnet(
                    greeter::MethodFactoryIO::getInstance().createMagnet(
                        magnet["type"].get<std::string>(), magnet));

                scene.magnet_ids.push_back(
                    magnet.contains("id") ? magnet["id"].get<int64_t>() : index);

                index++;
            }
        }

        // A generated member is numbered on from the highest id the file used,
        // so that it can never take the id of a magnet the file named.
        int64_t next_id = 0;
        for (const auto& id : scene.magnet_ids) {
            next_id = std::max(next_id, id + 1);
        }

        if (data.contains("arrangements")) {

            int64_t index = 0;

            std::unordered_set<int64_t> arrangement_ids;

            for (const auto& arrangement : data["arrangements"]) {

                const std::string type = arrangement["type"].get<std::string>();

                greeter::ArrangementMembers record;
                record.id = arrangement.contains("id")
                          ? arrangement["id"].get<int64_t>()
                          : index;

                if (!arrangement_ids.insert(record.id).second) {
                    throw std::invalid_argument(
                        "The arrangement id " + std::to_string(record.id) + " is used twice");
                }

                std::vector<std::unique_ptr<greeter::Magnet>> members =
                    greeter::ArrangementFactoryIO::getInstance().expand(type, arrangement);

                if (members.empty()) {
                    throw std::invalid_argument(
                        "The arrangement '" + type + "' generated no magnet");
                }

                for (auto& member : members) {
                    record.members.push_back((uint32_t) scene.magnet_ids.size());
                    scene.magnet_ids.push_back(next_id++);
                    scene.collection.addMagnet(std::move(member));
                }

                scene.arrangements.push_back(std::move(record));

                index++;
            }
        }

        // An id names one magnet. Two magnets sharing one used to leave the
        // second unreachable, with the force section quietly acting on the first.
        std::unordered_set<int64_t> seen;
        for (const auto& id : scene.magnet_ids) {
            if (!seen.insert(id).second) {
                throw std::invalid_argument(
                    "The magnet id " + std::to_string(id) + " is used twice");
            }
        }

        return scene;
    }

}  // namespace greeter
