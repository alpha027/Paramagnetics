#include <greeter/io/MethodFactoryIO.h>


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
    if (it != registry.end()) {
        return it->second(magnet);
    } else {
        std::cout << "Unknown child type '" << key << "'" << std::endl;
        return nullptr;
    }
}