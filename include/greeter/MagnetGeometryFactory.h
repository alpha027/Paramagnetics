#ifndef MAGNET_GEOMETRY_FACTORY_H
#define MAGNET_GEOMETRY_FACTORY_H

#include <greeter/view/ShapeDescriptor.h>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>


namespace greeter {

/*
  Registry of what the magnet classes look like, keyed by type ID.

  Mirrors TargetMeshFactory: a viewer draws a magnet without knowing its
  concrete type, exactly as the force simulator meshes one without knowing it.
  Magnet::getDimensions() means something different for each shape and nothing
  on its own, so each class says here how its numbers are to be read.

  A magnet class registers once, next to where it already registers its field
  kernel and its target mesher, and everything downstream of the descriptor is
  then finished with it for good. The classes of this library are registered
  by the constructor, see MagneticFieldMethodFactory for why they cannot
  register themselves.
*/
class MagnetGeometryFactory {

public:

    static MagnetGeometryFactory& getInstance() {
        static MagnetGeometryFactory instance;
        return instance;
    }

    /*
      Takes the parameters of a magnet, laid out as the field kernels and the
      target meshers take them: position (3), orientation (4), the geometry of
      the shape (n), magnetization (3).
    */
    using DescribeFunction =
        std::function<greeter::view::ShapeDescriptor(const float* parameters)>;

    /*
      `moment_kind` says what the last three parameters of the type are: a
      polarization in Tesla for almost everything, a moment in ampere metre
      squared for a dipole. It is registered here rather than worked out from
      the name so that a viewer never has to know any names.
    */
    bool registerDescribeShape(
        const uint16_t& key, const std::string& type_name,
        DescribeFunction _method,
        const greeter::view::MomentKind& moment_kind =
            greeter::view::MomentKind::Polarization);

    greeter::view::MomentKind getMomentKind(const uint16_t& key) const;

    /*
      What the magnet looks like. An unregistered type gives back a descriptor
      of kind Unknown rather than throwing, so that a scene holding one magnet
      nobody has taught the viewer about still opens.
    */
    greeter::view::ShapeDescriptor describeShape(
        const uint16_t& key, const float* parameters) const;

    /* The name the type answers to in an input file, empty when unknown. */
    std::string getTypeName(const uint16_t& key) const;

    bool isRegistered(const uint16_t& key) const;

    std::vector<uint16_t> getRegisteredTypes() const;

    void displayRegistered() const;

private:

    MagnetGeometryFactory();            // Private constructor, registers the built in magnets
    ~MagnetGeometryFactory() = default; // Destructor
    MagnetGeometryFactory(const MagnetGeometryFactory&) = delete; // Prevent copying
    MagnetGeometryFactory& operator=(const MagnetGeometryFactory&) = delete; // Prevent assignment

    struct Entry {
        std::string type_name;
        DescribeFunction describe;
        greeter::view::MomentKind moment_kind;
    };

    std::unordered_map<uint16_t, Entry> registry;
};

}  // namespace greeter

#endif  // MAGNET_GEOMETRY_FACTORY_H
