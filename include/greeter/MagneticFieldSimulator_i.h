#ifndef MAGNETICFIELDSIMULATOR_I_H
#define MAGNETICFIELDSIMULATOR_I_H

#include <greeter/MagneticFieldSimulator.h>
#include <greeter/MagneticFieldMethodFactory.h>
#include <greeter/Quaternion.h>

inline
greeter::MagneticFieldSimulator::MagneticFieldSimulator(
    Float3VectorView _positions, Float4VectorView _orientations,
    Float3VectorView _magnetizations, Float3VectorView _dimensions,
    UInt32VectorView _magnet_types,
    Float3VectorView _observation_points) :
    positions(_positions), orientations(_orientations),
    magnetizations(_magnetizations), dimensions(_dimensions),
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

    for(int i = 0; i < N; i++) {
        result[i][0] = magnetic_fields(i, 0);
        result[i][1] = magnetic_fields(i, 1);
        result[i][2] = magnetic_fields(i, 2);
    }

    return result;
}

KOKKOS_INLINE_FUNCTION
void greeter::MagneticFieldSimulator::operator()( u_int64_t observation_point_index ) const {

    for (size_t i = 0; i < num_magnets; i++) {

        float translated_observation_point[3] = {
            observation_points( observation_point_index, 0) - positions(i, 0),
            observation_points( observation_point_index, 1) - positions(i, 1), 
            observation_points( observation_point_index, 2) - positions(i, 2)
        };

        float rotation_quat[4] = {
            orientations(i, 0), orientations(i, 1), 
            orientations(i, 2), orientations(i, 3)
        };

        float rotated_observation_point[3] = {0.0f, 0.0f, 0.0f,};

        greeter::Quaternion::applyInverseRotationFromQuaternion(rotation_quat, translated_observation_point, rotated_observation_point);

        u_int16_t magnet_type = magnet_types(0);

        float parameters[13] = {
            positions(i, 0), 
            positions(i, 1),
            positions(i, 2),
            0.0, 0.0, 0.0, 0.0,
            dimensions(i,0),
            dimensions(i,1),
            dimensions(i,2), 
            magnetizations(i, 0),
            magnetizations(i, 1),
            magnetizations(i, 2)
        };

        float rotated_bx = 0.0;
        float rotated_by = 0.0;
        float rotated_bz = 0.0;

        greeter::MagneticFieldMethodFactory::getInstance().computeMagneticField(
            magnet_type,
            parameters,
            rotated_observation_point,
            rotated_bx, rotated_by, rotated_bz
        );

        float rotated_field[3] = {rotated_bx, rotated_by, rotated_bz};

        float final_field[3] = {0.0, 0.0, 0.0};

        greeter::Quaternion::applyRotationFromQuaternion(rotation_quat, rotated_field, final_field);

        magnetic_fields(observation_point_index, 0) += final_field[0];
        magnetic_fields(observation_point_index, 1) += final_field[1];
        magnetic_fields(observation_point_index, 2) += final_field[2];

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