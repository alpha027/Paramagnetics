# include <greeter/MagneticFieldMethodFactory.h>
# include <greeter/CubicMagnet.h>
# include <greeter/SphericalMagnet.h>
# include <greeter/TetrahedronMagnet.h>
# include <greeter/CylinderMagnet.h>
# include <greeter/DipoleMagnet.h>
# include <greeter/TriangleMagnet.h>
# include <greeter/TriangularMeshMagnet.h>
# include <stdexcept>


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
    registerComputePolarization(
        greeter::CuboidMagnet::getStaticTypeID(),
        greeter::CuboidMagnet::computePolarizationForCube);
    registerNumberOfParameters(
        greeter::CuboidMagnet::getStaticTypeID(),
        greeter::CuboidMagnet::numberOfParameters);

    registerComputeMagneticField(
        greeter::SphereMagnet::getStaticTypeID(),
        greeter::SphereMagnet::computeMagneticFieldForSphere);
    registerComputePolarization(
        greeter::SphereMagnet::getStaticTypeID(),
        greeter::SphereMagnet::computePolarizationForSphere);
    registerNumberOfParameters(
        greeter::SphereMagnet::getStaticTypeID(),
        greeter::SphereMagnet::numberOfParameters);

    registerComputeMagneticField(
        greeter::TetrahedronMagnet::getStaticTypeID(),
        greeter::TetrahedronMagnet::computeMagneticFieldForTetrahedron);
    registerComputePolarization(
        greeter::TetrahedronMagnet::getStaticTypeID(),
        greeter::TetrahedronMagnet::computePolarizationForTetrahedron);
    registerNumberOfParameters(
        greeter::TetrahedronMagnet::getStaticTypeID(),
        greeter::TetrahedronMagnet::numberOfParameters);

    registerComputeMagneticField(
        greeter::CylinderMagnet::getStaticTypeID(),
        greeter::CylinderMagnet::computeMagneticFieldForCylinder);
    registerComputePolarization(
        greeter::CylinderMagnet::getStaticTypeID(),
        greeter::CylinderMagnet::computePolarizationForCylinder);
    registerNumberOfParameters(
        greeter::CylinderMagnet::getStaticTypeID(),
        greeter::CylinderMagnet::numberOfParameters);

    registerComputeMagneticField(
        greeter::DipoleMagnet::getStaticTypeID(),
        greeter::DipoleMagnet::computeMagneticFieldForDipole);
    registerComputePolarization(
        greeter::DipoleMagnet::getStaticTypeID(),
        greeter::DipoleMagnet::computePolarizationForDipole);
    registerNumberOfParameters(
        greeter::DipoleMagnet::getStaticTypeID(),
        greeter::DipoleMagnet::numberOfParameters);

    registerComputeMagneticField(
        greeter::TriangleMagnet::getStaticTypeID(),
        greeter::TriangleMagnet::computeMagneticFieldForTriangle);
    registerComputePolarization(
        greeter::TriangleMagnet::getStaticTypeID(),
        greeter::TriangleMagnet::computePolarizationForTriangle);
    registerNumberOfParameters(
        greeter::TriangleMagnet::getStaticTypeID(),
        greeter::TriangleMagnet::numberOfParameters);

    // No parameter count: how many a triangular mesh takes depends on how
    // many faces it has, so the number belongs to the magnet and not to the
    // type. See MagnetParameters.h.
    registerComputeMagneticField(
        greeter::TriangularMeshMagnet::getStaticTypeID(),
        greeter::TriangularMeshMagnet::computeMagneticFieldForTriangularMesh);
    registerComputePolarization(
        greeter::TriangularMeshMagnet::getStaticTypeID(),
        greeter::TriangularMeshMagnet::computePolarizationForTriangularMesh);
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

bool greeter::MagneticFieldMethodFactory::registerComputePolarization(
    const u_int16_t& key, MethodFunction _method) {
    registry_polarization[key] = _method;
    return true;
}

greeter::MagneticFieldMethodFactory::MethodFunction
greeter::MagneticFieldMethodFactory::getComputePolarization(const u_int16_t& key) const {

    auto it = registry_polarization.find(key);

    if (it == registry_polarization.end()) {
        throw std::invalid_argument(
            "Magnet type '" + std::to_string(key) + "' does not say where it "
            "is, so H, J and M cannot be worked out for it");
    }

    return it->second;
}

bool greeter::MagneticFieldMethodFactory::hasComputePolarization(
    const u_int16_t& key) const {
    return registry_polarization.find(key) != registry_polarization.end();
}

/*
  How many parameters a shape of fixed size takes.

  Nothing in the simulators needs this any more: they lay every magnet's block
  down end to end and hand a kernel a pointer to its own, so the size of a
  block is a property of the magnet rather than of its type. It stays because
  it says something true and useful about the shapes that do have a fixed
  size, and it is what a test asserts against.

  A shape whose size varies between one magnet and the next, a triangular mesh
  for instance, has no number to register here and does not register one.
*/
bool greeter::MagneticFieldMethodFactory::registerNumberOfParameters(
    const u_int16_t& key, NumerOfParametersFunction _method) {

    registry_parameters[key] = _method;
    return true;
}

bool greeter::MagneticFieldMethodFactory::hasNumberOfParameters(
    const u_int16_t& key) const {
    return registry_parameters.find(key) != registry_parameters.end();
}

std::vector<u_int16_t> greeter::MagneticFieldMethodFactory::getRegisteredTypes() const {

    std::vector<u_int16_t> keys;
    keys.reserve(registry.size());

    for (const auto& entry : registry) {
        keys.push_back(entry.first);
    }

    return keys;
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

greeter::MagneticFieldMethodFactory::MethodFunction
greeter::MagneticFieldMethodFactory::getComputeMagneticField(const u_int16_t& key) const {

    auto it = registry.find(key);
    if (it == registry.end()) {
        std::string known;
        for (const auto& entry : registry) {
            known += known.empty() ? "" : ", ";
            known += std::to_string(entry.first);
        }
        throw std::invalid_argument(
            "Unknown magnet type '" + std::to_string(key) +
            "', the known types are " + known);
    }

    return it->second;
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