#include <greeter/io/ArrangementFactoryIO.h>
#include <greeter/arrangements/CircularArrayArrangement.h>
#include <greeter/arrangements/HalbachRingArrangement.h>
#include <greeter/arrangements/LinearArrayArrangement.h>
#include <greeter/arrangements/LinearHalbachArrangement.h>

#include <stdexcept>


// See MagneticFieldMethodFactory for why the built in arrangements are registered here.
greeter::ArrangementFactoryIO::ArrangementFactoryIO() {

    registerExpand(
        greeter::LinearArrayArrangement::getTypeName(),
        greeter::LinearArrayArrangement::expand);

    registerExpand(
        greeter::CircularArrayArrangement::getTypeName(),
        greeter::CircularArrayArrangement::expand);

    registerExpand(
        greeter::HalbachRingArrangement::getTypeName(),
        greeter::HalbachRingArrangement::expand);

    registerExpand(
        greeter::LinearHalbachArrangement::getTypeName(),
        greeter::LinearHalbachArrangement::expand);
}


void greeter::ArrangementFactoryIO::displayRegistered() const {
    std::cout << "Registered arrangements:" << std::endl;
    for (const auto& entry : registry) {
        std::cout << "  Key: " << entry.first << std::endl;
    }
}

bool greeter::ArrangementFactoryIO::registerExpand(
    const std::string& key, MethodFunction _method) {
    registry[key] = _method;
    return true;
}

bool greeter::ArrangementFactoryIO::isRegistered(const std::string& key) const {
    return registry.find(key) != registry.end();
}

std::vector<std::unique_ptr<greeter::Magnet>> greeter::ArrangementFactoryIO::expand(
    const std::string& key, const nlohmann::json& arrangement) const {

    auto it = registry.find(key);

    if (it == registry.end()) {
        std::string known;
        for (const auto& entry : registry) {
            known += known.empty() ? "" : ", ";
            known += entry.first;
        }
        throw std::invalid_argument(
            "Unknown arrangement type '" + key + "', the known types are " + known);
    }

    return it->second(arrangement);
}
