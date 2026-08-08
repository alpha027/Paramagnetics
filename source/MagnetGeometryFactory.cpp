#include <greeter/MagnetGeometryFactory.h>
#include <greeter/CubicMagnet.h>
#include <greeter/SphericalMagnet.h>
#include <greeter/TetrahedronMagnet.h>
#include <greeter/CylinderMagnet.h>
#include <greeter/DipoleMagnet.h>
#include <greeter/TriangleMagnet.h>
#include <greeter/TriangularMeshMagnet.h>
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

    // The one source whose last three numbers are a moment rather than a
    // polarization.
    registerDescribeShape(
        greeter::DipoleMagnet::getStaticTypeID(),
        greeter::DipoleMagnet::getStaticTypeName(),
        greeter::DipoleMagnet::describeShape,
        greeter::view::MomentKind::Moment);

    registerDescribeShape(
        greeter::TriangleMagnet::getStaticTypeID(),
        greeter::TriangleMagnet::getStaticTypeName(),
        greeter::TriangleMagnet::describeShape);

    registerDescribeShape(
        greeter::TriangularMeshMagnet::getStaticTypeID(),
        greeter::TriangularMeshMagnet::getStaticTypeName(),
        greeter::TriangularMeshMagnet::describeShape);
}


bool greeter::MagnetGeometryFactory::registerDescribeShape(
    const uint16_t& key, const std::string& type_name, DescribeFunction _method,
    const greeter::view::MomentKind& moment_kind) {

    registry[key] = Entry{type_name, _method, moment_kind};
    return true;
}


greeter::view::MomentKind greeter::MagnetGeometryFactory::getMomentKind(
    const uint16_t& key) const {

    const auto it = registry.find(key);

    // Almost everything carries a polarization, so that is what an unknown
    // type is assumed to carry.
    return it == registry.end() ? greeter::view::MomentKind::Polarization
                                : it->second.moment_kind;
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
