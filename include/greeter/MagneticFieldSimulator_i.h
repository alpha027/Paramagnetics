#ifndef MAGNETICFIELDSIMULATOR_I_H
#define MAGNETICFIELDSIMULATOR_I_H

#include <greeter/MagneticFieldSimulator.h>
#include <greeter/MagneticFieldMethodFactory.h>
#include <greeter/MagnetParameters.h>
#include <greeter/Quaternion.h>

inline
greeter::MagneticFieldSimulator::MagneticFieldSimulator(
    Float3VectorView _positions, Float4VectorView _orientations,
    Float3VectorView _magnetizations, FloatVectorView _dimensions,
    UInt32VectorView _magnet_types,
    Float3VectorView _observation_points) :
    positions(_positions), orientations(_orientations),
    magnetizations(_magnetizations),
    dimensions(_dimensions),
    magnet_types(_magnet_types),
    observation_points(_observation_points) {

        num_magnets = magnet_types.extent(0);

        resolveMagnetTypes();
    }

/*
  Look up, once, everything the inner loop needs to know about a magnet type:
  the kernel to call and where the geometry of the magnet sits in `dimensions`.
  Doing this per magnet per observation point, as the loop used to, meant a hash
  lookup and a branch on the type for every field evaluation.
*/
inline
void greeter::MagneticFieldSimulator::resolveMagnetTypes() {

    const greeter::MagneticFieldMethodFactory& factory =
        greeter::MagneticFieldMethodFactory::getInstance();

    magnet_kernels = MagnetKernelView("magnet_kernels", num_magnets);

    u_int32_t offset = 0;

    for (size_t i = 0; i < num_magnets; i++) {

        const u_int16_t magnet_type = (u_int16_t) magnet_types(i);

        // The shape only decides how many geometry parameters follow the
        // position and the orientation, see packMagnetParameters.
        const u_int32_t count =
            (u_int32_t) factory.getNumberOfParameters(magnet_type) - 10;

        magnet_kernels(i).kernel = factory.getComputeMagneticField(magnet_type);
        magnet_kernels(i).geometry_offset = offset;
        magnet_kernels(i).geometry_count = count;

        offset += count;
    }
}

inline
greeter::MagneticFieldSimulator::~MagneticFieldSimulator() {}

inline
void greeter::MagneticFieldSimulator::simulate() {
    simulate(true);
}

inline
void greeter::MagneticFieldSimulator::simulate(const bool& verbose) {

    if (verbose) {
        std::cout << "Start simulation !" << std::endl;
    }

    u_int64_t num_samples = getNumObservationPoints();

    //Kokkos::parallel_for( num_samples, *this );
    Kokkos::parallel_for( range_policy( 0, num_samples ),
                          *this );
    Kokkos::fence();

    if (verbose) {
        std::cout << "End simulation !" << std::endl;
    }
}

inline
void greeter::MagneticFieldSimulator::printValue( u_int64_t observation_point_index ) const {
    std::cout << "value for index " << observation_point_index << ": (" 
              << observation_points( observation_point_index, 0) << ", " 
              << observation_points(observation_point_index, 1) << ", " 
              << observation_points(observation_point_index,2) << ")" << std::endl;
}

inline
void greeter::MagneticFieldSimulator::printPosition( u_int64_t observation_point_index ) const {
    std::cout << "position for index " << observation_point_index << ": (" 
              << positions( observation_point_index, 0) << ", " 
              << positions(observation_point_index, 1) << ", " 
              << positions(observation_point_index,2) << ")" << std::endl;
}

inline
void greeter::MagneticFieldSimulator::printMagneticField( u_int64_t observation_point_index ) const {
    std::cout << "magnetic field for index " << observation_point_index << ": (" 
              << magnetic_fields( observation_point_index, 0) << ", " 
              << magnetic_fields(observation_point_index, 1) << ", " 
              << magnetic_fields(observation_point_index,2) << ")" << std::endl;
}


inline
void greeter::MagneticFieldSimulator::computeMagneticField(const u_int16_t& key, const float* parameters, 
                              const float* observation_point, float& a, float& b, float& c) {
    MagneticFieldMethodFactory::getInstance().computeMagneticField(key, parameters, observation_point, a, b, c);
}


inline
void greeter::MagneticFieldSimulator::applyRotationFromQuaternion(const float* quaternion, const float* vector, float* result) {
    Quaternion::applyRotationFromQuaternion(quaternion, vector, result);
}


inline
void greeter::MagneticFieldSimulator::applyInverseRotationFromQuaternion(const float* quaternion, const float* vector, float* result) {
    Quaternion::applyInverseRotationFromQuaternion(quaternion, vector, result);
}


inline
std::vector<std::vector<float>> greeter::MagneticFieldSimulator::getMagneticFields() const {

    u_int64_t N = getNumObservationPoints();

    std::vector<std::vector<float>> result(N, std::vector<float>(3, 0.0f));

    for(u_int64_t i = 0; i < N; i++) {
        result[i][0] = magnetic_fields(i, 0);
        result[i][1] = magnetic_fields(i, 1);
        result[i][2] = magnetic_fields(i, 2);
    }

    return result;
}

inline
std::vector<float> greeter::MagneticFieldSimulator::getMagneticFieldsFlat() const {

    const u_int64_t N = getNumObservationPoints();

    std::vector<float> result(3 * (size_t) N, 0.0f);

    for(u_int64_t i = 0; i < N; i++) {
        result[3 * (size_t) i + 0] = magnetic_fields(i, 0);
        result[3 * (size_t) i + 1] = magnetic_fields(i, 1);
        result[3 * (size_t) i + 2] = magnetic_fields(i, 2);
    }

    return result;
}

inline
std::vector<float> greeter::MagneticFieldSimulator::getObservationPointsFlat() const {

    const u_int64_t N = getNumObservationPoints();

    std::vector<float> result(3 * (size_t) N, 0.0f);

    for(u_int64_t i = 0; i < N; i++) {
        result[3 * (size_t) i + 0] = observation_points(i, 0);
        result[3 * (size_t) i + 1] = observation_points(i, 1);
        result[3 * (size_t) i + 2] = observation_points(i, 2);
    }

    return result;
}

KOKKOS_INLINE_FUNCTION
void greeter::MagneticFieldSimulator::operator()( u_int64_t observation_point_index ) const {

    const float the_observation_point[3] = {
        observation_points(observation_point_index, 0),
        observation_points(observation_point_index, 1),
        observation_points(observation_point_index, 2)
    };

    for (size_t i = 0; i < num_magnets; i++) {

        const MagnetKernel magnet = magnet_kernels(i);

        float magnet_parameters[greeter::MAX_MAGNET_PARAMETERS];

        greeter::packMagnetParameters(
            positions, orientations, magnetizations, dimensions,
            i, magnet.geometry_offset, magnet.geometry_count, magnet_parameters);

        float bx = 0.0f;
        float by = 0.0f;
        float bz = 0.0f;

        magnet.kernel(magnet_parameters, the_observation_point, bx, by, bz);

        magnetic_fields(observation_point_index, 0) += bx;
        magnetic_fields(observation_point_index, 1) += by;
        magnetic_fields(observation_point_index, 2) += bz;
    }

}

inline
size_t greeter::MagneticFieldSimulator::getNumObservationPoints() const {
    return observation_points.extent(0);
}

KOKKOS_INLINE_FUNCTION
void greeter::MagneticFieldSimulator::fillObservationPoints(const std::vector<std::vector<float>>& _observation_points) {

    const size_t M = _observation_points.size();

    observation_points = Float3VectorView("observation_points", M);

    magnetic_fields = Float3VectorView("magnetic_fields", M);

    Kokkos::parallel_for( "magnetizations", M, KOKKOS_LAMBDA ( int i ) {
       observation_points(i, 0) = _observation_points[i][0];
       observation_points(i, 1) = _observation_points[i][1];
       observation_points(i, 2) = _observation_points[i][2];
    });
}


#endif // MAGNETICFIELDSIMULATOR_I_H