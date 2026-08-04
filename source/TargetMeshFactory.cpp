#include <greeter/TargetMeshFactory.h>


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
