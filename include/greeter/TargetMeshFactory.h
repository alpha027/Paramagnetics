#ifndef TARGETMESHFACTORY_H
#define TARGETMESHFACTORY_H

#include <functional>
#include <unordered_map>
#include <greeter/TargetMesh.h>
#include <iostream>


namespace greeter {

/*
  Registry of the meshing methods of the magnet classes, keyed by type ID.

  Mirrors MagneticFieldMethodFactory: every magnet class registers its own
  meshing method from its translation unit, so that the force simulator can
  mesh a target without knowing its concrete type.
*/
class TargetMeshFactory {

public:

    static TargetMeshFactory& getInstance() {
        static TargetMeshFactory instance;  // Guaranteed to be destroyed, instantiated on first use
        return instance;
    }

    using MeshFunction = std::function<TargetMeshData(const float* parameters,
                                                      const MeshingSpec& meshing)>;

    // Method to register a class with a key
    bool registerGenerateTargetMesh(const u_int16_t& key, MeshFunction _method);

    TargetMeshData generateTargetMesh(const u_int16_t& key, const float* parameters,
                                      const MeshingSpec& meshing) const;

    // Method to display all registered types
    void displayRegistered() const;

private:

    TargetMeshFactory() = default;  // Private constructor
    ~TargetMeshFactory() = default; // Destructor
    TargetMeshFactory(const TargetMeshFactory&) = delete; // Prevent copying
    TargetMeshFactory& operator=(const TargetMeshFactory&) = delete; // Prevent assignment
    std::unordered_map<u_int16_t, MeshFunction> registry;  // Map of key to meshing function
};

}  // namespace greeter

#endif
