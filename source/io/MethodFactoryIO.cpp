#include <greeter/io/MethodFactoryIO.h>
#include <greeter/io/CubicMagnetIO.h>
#include <greeter/io/SphericalMagnetIO.h>
#include <greeter/io/TetrahedronMagnetIO.h>

#include <stdexcept>


// See MagneticFieldMethodFactory for why the built in readers are registered here.
greeter::MethodFactoryIO::MethodFactoryIO() {

    registerGetMagnet(
        greeter::CubicMagnetIO::getTypeName(),
        greeter::CubicMagnetIO::createMagnet);

    registerGetMagnet(
        greeter::SphericalMagnetIO::getTypeName(),
        greeter::SphericalMagnetIO::createMagnet);

    registerGetMagnet(
        greeter::TetrahedronMagnetIO::getTypeName(),
        greeter::TetrahedronMagnetIO::createMagnet);
}


void greeter::MethodFactoryIO::displayRegistered() const {
    std::cout << "Registered classes:" << std::endl;
    for (const auto& entry : registry) {
        std::cout << "  Key: " << entry.first << std::endl;
    }
}

bool greeter::MethodFactoryIO::registerGetMagnet(
    const std::string& key, MethodFunction _method) {
    registry[key] = _method;
    return true;
}

// bool greeter::MethodFactoryIO::registerNumberOfParameters(
//     const u_int16_t& key, NumerOfParametersFunction _method) {
//     registry_parameters[key] = _method;
//     return true;
// }

std::unique_ptr<greeter::Magnet> greeter::MethodFactoryIO::createMagnet(
    const std::string& key, const nlohmann::json& magnet) const {
    auto it = registry.find(key);

    if (it == registry.end()) {
        // Returning a null magnet used to leave the collection with an entry
        // that crashed on the first use.
        std::string known;
        for (const auto& entry : registry) {
            known += known.empty() ? "" : ", ";
            known += entry.first;
        }
        throw std::invalid_argument(
            "Unknown magnet type '" + key + "', the known types are " + known);
    }

    return it->second(magnet);
}