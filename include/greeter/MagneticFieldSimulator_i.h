#ifndef MAGNETICFIELDSIMULATOR_I_H
#define MAGNETICFIELDSIMULATOR_I_H

#include <greeter/MagneticFieldSimulator.h>
#include <greeter/MagneticFieldMethodFactory.h>
#include <greeter/MagnetParameters.h>
#include <greeter/Quaternion.h>

inline
greeter::MagneticFieldSimulator::MagneticFieldSimulator(
    FloatVectorView _magnet_parameters, UInt32VectorView _parameter_offsets,
    UInt32VectorView _magnet_types,
    Float3VectorView _observation_points) :
    magnet_parameters(_magnet_parameters),
    parameter_offsets(_parameter_offsets),
    magnet_types(_magnet_types),
    observation_points(_observation_points) {

        num_magnets = magnet_types.extent(0);

        resolveMagnetTypes();
    }

/*
  Look up, once, the kernel of every magnet and where its parameters start.
  Doing the lookup per magnet per observation point, as the loop used to, meant
  a hash lookup and a branch on the type for every field evaluation.
*/
inline
void greeter::MagneticFieldSimulator::resolveMagnetTypes() {

    const greeter::MagneticFieldMethodFactory& factory =
        greeter::MagneticFieldMethodFactory::getInstance();

    magnet_kernels = MagnetKernelView("magnet_kernels", num_magnets);

    for (size_t i = 0; i < num_magnets; i++) {

        const u_int16_t magnet_type = (u_int16_t) magnet_types(i);

        magnet_kernels(i).kernel = factory.getComputeMagneticField(magnet_type);
        magnet_kernels(i).parameter_offset = parameter_offsets(i);

        // A type that does not say where it is can still give a field; only
        // H, J and M are then out of reach, and asking for them says so.
        magnet_kernels(i).polarization =
            factory.hasComputePolarization(magnet_type)
                ? factory.getComputePolarization(magnet_type) : nullptr;
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
void greeter::MagneticFieldSimulator::setComputePolarization(const bool& wanted) {
    with_polarization = wanted;
}

inline
std::vector<float> greeter::MagneticFieldSimulator::getPolarizationsFlat() const {

    const u_int64_t N = getNumObservationPoints();

    std::vector<float> result(3 * (size_t) N, 0.0f);

    if (!with_polarization) {
        return result;
    }

    for(u_int64_t i = 0; i < N; i++) {
        result[3 * (size_t) i + 0] = polarizations(i, 0);
        result[3 * (size_t) i + 1] = polarizations(i, 1);
        result[3 * (size_t) i + 2] = polarizations(i, 2);
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

        // A pointer into the one array the parameters of every magnet live
        // in, rather than a copy made per magnet per point.
        const float* parameters =
            greeter::magnetParameters(magnet_parameters, magnet.parameter_offset);

        float bx = 0.0f;
        float by = 0.0f;
        float bz = 0.0f;

        magnet.kernel(parameters, the_observation_point, bx, by, bz);

        magnetic_fields(observation_point_index, 0) += bx;
        magnetic_fields(observation_point_index, 1) += by;
        magnetic_fields(observation_point_index, 2) += bz;

        if (with_polarization && magnet.polarization != nullptr) {

            float jx = 0.0f;
            float jy = 0.0f;
            float jz = 0.0f;

            magnet.polarization(parameters, the_observation_point, jx, jy, jz);

            polarizations(observation_point_index, 0) += jx;
            polarizations(observation_point_index, 1) += jy;
            polarizations(observation_point_index, 2) += jz;
        }
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

    polarizations = Float3VectorView("polarizations", with_polarization ? M : 0);

    Kokkos::parallel_for( "magnetizations", M, KOKKOS_LAMBDA ( int i ) {
       observation_points(i, 0) = _observation_points[i][0];
       observation_points(i, 1) = _observation_points[i][1];
       observation_points(i, 2) = _observation_points[i][2];
    });
}


#endif // MAGNETICFIELDSIMULATOR_I_H