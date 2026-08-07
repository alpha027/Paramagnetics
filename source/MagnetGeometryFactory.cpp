#include <greeter/MagnetGeometryFactory.h>
#include <greeter/CubicMagnet.h>
#include <greeter/SphericalMagnet.h>
#include <greeter/TetrahedronMagnet.h>
#include <greeter/CylinderMagnet.h>
#include <iostream>


// See MagneticFieldMethodFactory for why the built in magnets are registered here.
greeter::MagnetGeometryFactory::MagnetGeometryFactory() {

    registerDescribeShape(
        greeter::CuboidMagnet::getStaticTypeID(),
        greeter::CuboidMagnet::getStaticTypeName(),
        greeter::CuboidMagnet::describeShape);

    registerDescribeShape(
        greeter::SphereMagnet::getStaticTypeID(),
        greeter::SphereMagnet::getStaticTypeName(),
        greeter::SphereMagnet::describeShape);

    registerDescribeShape(
        greeter::TetrahedronMagnet::getStaticTypeID(),
        greeter::TetrahedronMagnet::getStaticTypeName(),
        greeter::TetrahedronMagnet::describeShape);

    registerDescribeShape(
        greeter::CylinderMagnet::getStaticTypeID(),
        greeter::CylinderMagnet::getStaticTypeName(),
        greeter::CylinderMagnet::describeShape);
}


bool greeter::MagnetGeometryFactory::registerDescribeShape(
    const uint16_t& key, const std::string& type_name, DescribeFunction _method) {

    registry[key] = Entry{type_name, _method};
    return true;
}


greeter::view::ShapeDescriptor greeter::MagnetGeometryFactory::describeShape(
    const uint16_t& key, const float* parameters) const {

    const auto it = registry.find(key);

    if (it == registry.end()) {
        // A shape nobody has described is still a magnet at a place, and a
        // viewer draws it as a marker there.
        greeter::view::ShapeDescriptor unknown;
        unknown.type_name = "type " + std::to_string(key);
        return unknown;
    }

    greeter::view::ShapeDescriptor shape = it->second.describe(parameters);

    shape.type_name = it->second.type_name;

    return shape;
}


std::string greeter::MagnetGeometryFactory::getTypeName(const uint16_t& key) const {

    const auto it = registry.find(key);

    return it == registry.end() ? std::string() : it->second.type_name;
}


bool greeter::MagnetGeometryFactory::isRegistered(const uint16_t& key) const {
    return registry.find(key) != registry.end();
}


std::vector<uint16_t> greeter::MagnetGeometryFactory::getRegisteredTypes() const {

    std::vector<uint16_t> keys;
    keys.reserve(registry.size());

    for (const auto& entry : registry) {
        keys.push_back(entry.first);
    }

    return keys;
}


void greeter::MagnetGeometryFactory::displayRegistered() const {
    std::cout << "Registered magnet shapes:" << std::endl;
    for (const auto& entry : registry) {
        std::cout << "  Key: " << entry.first << " (" << entry.second.type_name
                  << ")" << std::endl;
    }
}
