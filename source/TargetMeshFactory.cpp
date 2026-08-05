#include <greeter/TargetMeshFactory.h>
#include <greeter/CubicMagnet.h>
#include <greeter/SphericalMagnet.h>
#include <greeter/TetrahedronMagnet.h>


// See MagneticFieldMethodFactory for why the built in magnets are registered here.
greeter::TargetMeshFactory::TargetMeshFactory() {

    registerGenerateTargetMesh(
        greeter::CuboidMagnet::getStaticTypeID(),
        greeter::CuboidMagnet::generateTargetMesh);

    registerGenerateTargetMesh(
        greeter::SphereMagnet::getStaticTypeID(),
        greeter::SphereMagnet::generateTargetMesh);

    registerGenerateTargetMesh(
        greeter::TetrahedronMagnet::getStaticTypeID(),
        greeter::TetrahedronMagnet::generateTargetMesh);
}


void greeter::TargetMeshFactory::displayRegistered() const {
    std::cout << "Registered target meshers:" << std::endl;
    for (const auto& entry : registry) {
        std::cout << "  Key: " << entry.first << std::endl;
    }
}

bool greeter::TargetMeshFactory::registerGenerateTargetMesh(
    const u_int16_t& key, MeshFunction _method) {
    registry[key] = _method;
    return true;
}

greeter::TargetMeshData greeter::TargetMeshFactory::generateTargetMesh(
    const u_int16_t& key, const float* parameters, const MeshingSpec& meshing) const {

    auto it = registry.find(key);
    if (it != registry.end()) {
        return it->second(parameters, meshing);
    }

    std::cout << "Unknown child type '" << key << "'" << std::endl;
    return greeter::TargetMeshData();
}
