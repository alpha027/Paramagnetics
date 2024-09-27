# include <greeter/MagneticFieldMethodFactory.h>


void greeter::MagneticFieldMethodFactory::displayRegistered() const {
    std::cout << "Registered classes:" << std::endl;
    for (const auto& entry : registry) {
        std::cout << "  Key: " << entry.first << std::endl;
    }
}


bool greeter::MagneticFieldMethodFactory::registerComputeMagneticField(
    const std::string& key, MethodFunction _method) {
    registry[key] = _method;
    return true;
}


void greeter::MagneticFieldMethodFactory::computeMagneticField(
    const std::string& key, const float* parameters, 
    const float* observation_point, float& a, float& b, float& c) const {

    auto it = registry.find(key);
    if (it != registry.end()) {
        it->second(parameters, observation_point, a, b, c);
    } else {
        std::cout << "Unknown child type '" << key << "'" << std::endl;
    }
}