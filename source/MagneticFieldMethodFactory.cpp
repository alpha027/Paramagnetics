# include <greeter/MagneticFieldMethodFactory.h>
# include <greeter/CubicMagnet.h>
# include <greeter/SphericalMagnet.h>
# include <greeter/TetrahedronMagnet.h>


/*
  The magnets that ship with this library. Naming their kernels here is what
  pulls their translation units out of the static library, so a new magnet
  class of this library has to be added to this list to be reachable through
  the registry. A magnet defined outside of it registers itself instead.
*/
greeter::MagneticFieldMethodFactory::MagneticFieldMethodFactory() {

    registerComputeMagneticField(
        greeter::CuboidMagnet::getStaticTypeID(),
        greeter::CuboidMagnet::computeMagneticFieldForCube);
    registerNumberOfParameters(
        greeter::CuboidMagnet::getStaticTypeID(),
        greeter::CuboidMagnet::numberOfParameters);

    registerComputeMagneticField(
        greeter::SphereMagnet::getStaticTypeID(),
        greeter::SphereMagnet::computeMagneticFieldForSphere);
    registerNumberOfParameters(
        greeter::SphereMagnet::getStaticTypeID(),
        greeter::SphereMagnet::numberOfParameters);

    registerComputeMagneticField(
        greeter::TetrahedronMagnet::getStaticTypeID(),
        greeter::TetrahedronMagnet::computeMagneticFieldForTetrahedron);
    registerNumberOfParameters(
        greeter::TetrahedronMagnet::getStaticTypeID(),
        greeter::TetrahedronMagnet::numberOfParameters);
}


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