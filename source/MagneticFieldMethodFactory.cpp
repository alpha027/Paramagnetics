# include <greeter/MagneticFieldMethodFactory.h>


void greeter::MagneticFieldMethodFactory::displayRegistered() const {
    std::cout << "Registered classes:" << std::endl;
    for (const auto& entry : registry) {
        std::cout << "  Key: " << entry.first << std::endl;
    }
}

bool greeter::MagneticFieldMethodFactory::registerComputeMagneticField(
    const u_int16_t& key, MethodFunction _method) {
    registry[key] = _method;
    return true;
}

bool greeter::MagneticFieldMethodFactory::registerNumberOfParameters(
    const u_int16_t& key, NumerOfParametersFunction _method) {
    registry_parameters[key] = _method;
    return true;
}

void greeter::MagneticFieldMethodFactory::computeMagneticField(
    const u_int16_t& key, const float* parameters, 
    const float* observation_point, float& a, float& b, float& c) const {

    auto it = registry.find(key);
    if (it != registry.end()) {
        it->second(parameters, observation_point, a, b, c);
    } else {
        std::cout << "Unknown child type '" << key << "'" << std::endl;
    }
}

size_t greeter::MagneticFieldMethodFactory::getNumberOfParameters(const u_int16_t& key) const {
    auto it = registry_parameters.find(key);
    if (it != registry_parameters.end()) {
        return it->second();
    } else {
        std::cout << "Unknown child type '" << key << "'" << std::endl;
        return 0;
    }
}