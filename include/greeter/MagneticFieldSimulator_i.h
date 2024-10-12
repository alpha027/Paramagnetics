#ifndef MAGNETICFIELDSIMULATOR_I_H
#define MAGNETICFIELDSIMULATOR_I_H

#include <greeter/MagneticFieldSimulator.h>
#include <greeter/MagneticFieldMethodFactory.h>
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

        u_int64_t N = observation_points.extent(0);
        Float3VectorView magnetic_fields("magnetic_fields", N);

        Kokkos::parallel_for( "magnetizations", N, KOKKOS_LAMBDA ( int i ) {
            magnetic_fields(i, 0)  = 0.0;
            magnetic_fields(i, 1)  = 1.0;
            magnetic_fields(i, 2)  = 0.0;
        });

        num_magnets = magnet_types.extent(0);
    }

inline
greeter::MagneticFieldSimulator::~MagneticFieldSimulator() {}

inline
void greeter::MagneticFieldSimulator::simulate() {

    std::cout << "Start simulation !" << std::endl;
    u_int64_t num_samples = getNumObservationPoints();

    //Kokkos::parallel_for( num_samples, *this );
    Kokkos::parallel_for( range_policy( 0, num_samples ),
                          *this );
    Kokkos::fence();

    std::cout << "End simulation !" << std::endl;

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
float* greeter::MagneticFieldSimulator::getTheParameters(const size_t& magnet_index) const {



    float* result = new float[13];

    result[0] = positions(magnet_index, 0);
    result[1] = positions(magnet_index, 1);
    result[2] = positions(magnet_index, 2);

    result[3] = orientations(magnet_index, 0);
    result[4] = orientations(magnet_index, 1);
    result[5] = orientations(magnet_index, 2);
    result[6] = orientations(magnet_index, 3);

    result[7] = dimensions(0);
    result[8] = dimensions(1);
    result[9] = dimensions(2);

    result[10] = magnetizations(magnet_index, 0);
    result[11] = magnetizations(magnet_index, 1);
    result[12] = magnetizations(magnet_index, 2);

    return result;
}


KOKKOS_INLINE_FUNCTION
void greeter::MagneticFieldSimulator::operator()( u_int64_t observation_point_index ) const {

    for (size_t i = 0; i < num_magnets; i++) {

        u_int16_t magnet_type = magnet_types(i);

        float the_observation_point[3] = {
            observation_points(observation_point_index, 0),
            observation_points(observation_point_index, 1),
            observation_points(observation_point_index, 2)
        };

        float bx = 0.0f;
        float by = 0.0f;
        float bz = 0.0f;

        size_t num_parameters = greeter::MagneticFieldMethodFactory::getInstance().getNumberOfParameters(magnet_type);

        size_t start_index = dimensionParameterCumulativeCount(i) - num_parameters;

        if (magnet_type == 0) { // Case of CuboidMagnet
            float magnet_parameters[13] = {
                positions(i, 0), 
                positions(i, 1),
                positions(i, 2),
                orientations(i, 0),
                orientations(i, 1),
                orientations(i, 2),
                orientations(i, 3),
                dimensions(start_index),
                dimensions(start_index + 1),
                dimensions(start_index + 2), 
                magnetizations(i, 0),
                magnetizations(i, 1),
                magnetizations(i, 2) 
            };

            greeter::MagneticFieldMethodFactory::getInstance().computeMagneticField(
            magnet_type,
            magnet_parameters,
            the_observation_point,
            bx, by, bz
            );
        } else if (magnet_type == 1) // Case of SphereMagnet
        {
            float magnet_parameters[11] = {
                positions(i, 0), 
                positions(i, 1),
                positions(i, 2),
                orientations(i, 0),
                orientations(i, 1),
                orientations(i, 2),
                orientations(i, 3),
                dimensions(start_index),
                magnetizations(i, 0),
                magnetizations(i, 1),
                magnetizations(i, 2)
            };

            greeter::MagneticFieldMethodFactory::getInstance().computeMagneticField(
            magnet_type,
            magnet_parameters,
            the_observation_point,
            bx, by, bz
            );
        }

        magnetic_fields(observation_point_index, 0) += bx;
        magnetic_fields(observation_point_index, 1) += by;
        magnetic_fields(observation_point_index, 2) += bz;
    }

}

inline
void greeter::MagneticFieldSimulator::fillDimensionParameterCumulativeCount(){

    size_t N = (size_t) num_magnets;

    dimensionParameterCumulativeCount = UInt32VectorView("dimensionParameterCumulativeCount", N);

    for (size_t i = 0; i < N; i++) {
        if ( i == 0 ) {
            dimensionParameterCumulativeCount(i) = greeter::MagneticFieldMethodFactory::getInstance()
                .getNumberOfParameters(magnet_types(i));
        } else {
            dimensionParameterCumulativeCount(i) = greeter::MagneticFieldMethodFactory::getInstance()
            .getNumberOfParameters(magnet_types(i)) + dimensionParameterCumulativeCount(i-1);
        }
        
    }
}

KOKKOS_INLINE_FUNCTION
void greeter::MagneticFieldSimulator::fillMagnetPositions(const std::vector<std::vector<float>>& _positions) {

    const size_t M = _positions.size();

    auto all_positions = _positions.data();

    Kokkos::parallel_for( "positions", M, KOKKOS_LAMBDA ( int i ) {
       positions(i, 0) = all_positions[i][0]; //_positions[i][0];
       positions(i, 1) = all_positions[i][1]; //_positions[i][1];
       positions(i, 2) = all_positions[i][2]; //_positions[i][2];
    });

    size_t magnet_count = (size_t) M / 3;
    num_magnets = magnet_count;
}

inline
size_t greeter::MagneticFieldSimulator::getNumObservationPoints() const {
    return observation_points.extent(0);
}

KOKKOS_INLINE_FUNCTION
void greeter::MagneticFieldSimulator::fillMagnetOrientations(const std::vector<std::vector<float>>& _orientations) {

    const size_t M = _orientations.size();

    orientations = Float4VectorView("orientations", M);

    Kokkos::parallel_for( "orientations", M, KOKKOS_LAMBDA ( int i ) {
       orientations(i, 0) = _orientations[i][0];
       orientations(i, 1) = _orientations[i][1];
       orientations(i, 2) = _orientations[i][2];
       orientations(i, 3) = _orientations[i][3];
    });

    size_t magnet_count = (size_t) M / 4;
    num_magnets = magnet_count;
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