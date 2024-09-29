#include <greeter/MagneticFieldSimulator.h>
#include <greeter/MagneticFieldMethodFactory.h>
#include <greeter/Quaternion.h>

inline
greeter::MagneticFieldSimulator::MagneticFieldSimulator(
    Float3VectorView _positions, Float3VectorView _orientations,
    Float3VectorView _magnetizations, Float3VectorView _dimensions,
    UInt32VectorView _magnet_types,
    Float3VectorView _observation_points) :
    positions(_positions), orientations(_orientations),
    magnetizations(_magnetizations), dimensions(_dimensions),
    magnet_types(_magnet_types),
    observation_points(_observation_points) {

        std::cout << "MagneticFieldSimulator initializing" << std::endl;
        u_int64_t N = observation_points.extent(0);
        Float3VectorView magnetic_fields("magnetic_fields", N);
        std::cout << "N: " << N << std::endl;
        Kokkos::parallel_for( "magnetic_fields", N, KOKKOS_LAMBDA ( int i ) {
            magnetic_fields(i, 0)  = 0.0;
            magnetic_fields(i, 1)  = 0.0;
            magnetic_fields(i, 2)  = 0.0;
        });

        Kokkos::parallel_for( "magnetizations", N, KOKKOS_LAMBDA ( int i ) {
            magnetic_fields(i, 0)  = 0.0;
            magnetic_fields(i, 1)  = 1.0;
            magnetic_fields(i, 2)  = 0.0;
        });

        std::cout << "MagneticFieldSimulator initialized" << std::endl;
    }

inline
greeter::MagneticFieldSimulator::~MagneticFieldSimulator() {}

inline
void greeter::MagneticFieldSimulator::simulate() {
    // Kokkos::Timer timer;

    // Kokkos::Random_XorShift64_Pool<> rand_pool(1234);
    // Kokkos::Random_XorShift64<> rand_gen(rand_pool);
    std::cout << "Start simulation !" << std::endl;
    u_int64_t num_samples = getNumObservationPoints();

    //Kokkos::parallel_for( num_samples, *this );
    Kokkos::parallel_for( range_policy( 0, num_samples ),
                          *this );
    Kokkos::fence();

    //const size_t num_samples = 1000000;
    double total_field = 1.0;
    //std::cout << "Total field: " << total_field << " (computed in " << 0 << " seconds)" << std::endl;

    // for (size_t i = 0; i < num_samples; ++i) {
    //     double x = rand_gen.drand();
    //     double y = rand_gen.drand();
    //     double z = rand_gen.drand();

    //     for (size_t j = 0; j < positions.extent(0); ++j) {
    //         double dx = x - positions(j, 0);
    //         double dy = y - positions(j, 1);
    //         double dz = z - positions(j, 2);

    //         double r = sqrt(dx * dx + dy * dy + dz * dz);
    //         double r3 = r * r * r;

    //         double Bx = 3 * dx * dz / r3;
    //         double By = 3 * dy * dz / r3;
    //         double Bz = 3 * dz * dz / r3;

    //         total_field += Bx * magnetizations(j, 0) + By * magnetizations(j, 1) + Bz * magnetizations(j, 2);
    //     }
    // }

    //double elapsed = timer.seconds();
    //std::cout << "Total field: " << total_field << " (computed in " << elapsed << " seconds)" << std::endl;
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


// KOKKOS_INLINE_FUNCTION
// void greeter::MagneticFieldSimulator::operator()( u_int64_t observation_point_index ) const {

//     //std::cout << "set value for index " << observation_point_index  << std::endl;
//     observation_points( observation_point_index, 0 ) = 2.1f;
//     observation_points( observation_point_index, 1 ) = 2.2f;
//     observation_points( observation_point_index, 2 ) = 2.3f;

// }

KOKKOS_INLINE_FUNCTION
void greeter::MagneticFieldSimulator::operator()( u_int64_t observation_point_index ) const {

    //std::cout << "set value for index " << observation_point_index  << std::endl;
    observation_points( observation_point_index, 0 ) = 2.1f;
    observation_points( observation_point_index, 1 ) = 2.2f;
    observation_points( observation_point_index, 2 ) = 2.3f;

    float translated_observation_point[3] = {observation_points( observation_point_index, 0) - positions(observation_point_index, 0),
                                      observation_points( observation_point_index, 1) - positions(observation_point_index, 1), 
                                      observation_points( observation_point_index, 2) - positions(observation_point_index, 2)};


    float rotation_quat[4] = {orientations(observation_point_index, 0), orientations(observation_point_index, 1), 
                              orientations(observation_point_index, 2), orientations(observation_point_index, 3)};


    float rotated_observation_point[3] = {0.0f, 0.0f, 0.0f};

    

    greeter::Quaternion::applyInverseRotationFromQuaternion(rotation_quat, translated_observation_point, rotated_observation_point);
    std::cout << "after rotation" << std::endl; 
    u_int16_t magnet_type = magnet_types(observation_point_index);

    float parameters[15] = {
        positions(observation_point_index, 0), 
        positions(observation_point_index, 1),
        positions(observation_point_index, 2),
        0.0, 0.0, 0.0,
        dimensions(observation_point_index,0),
        dimensions(observation_point_index,1),
        dimensions(observation_point_index,2), 
        magnetizations(observation_point_index, 0),
        magnetizations(observation_point_index, 1),
        magnetizations(observation_point_index, 2),
        rotated_observation_point[0],
        rotated_observation_point[1],
        rotated_observation_point[2]
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

KOKKOS_INLINE_FUNCTION
void greeter::MagneticFieldSimulator::fillMagnetPositions(const std::vector<std::vector<float>>& _positions) {

    const size_t M = _positions.size();

    auto all_positions = _positions.data();

    Kokkos::parallel_for( "positions", M, KOKKOS_LAMBDA ( int i ) {
       positions(i, 0) = all_positions[i][0]; //_positions[i][0];
       positions(i, 1) = all_positions[i][1]; //_positions[i][1];
       positions(i, 2) = all_positions[i][2]; //_positions[i][2];
    });
    // for (size_t i = 0; i < _positions.size(); ++i) {
    //     positions(i, 0) = _positions[i][0];
    //     positions(i, 1) = _positions[i][1];
    //     positions(i, 2) = _positions[i][2];
    // }
}

KOKKOS_INLINE_FUNCTION
void greeter::MagneticFieldSimulator::fillMagnetOrientations(const std::vector<std::vector<float>>& _orientations) {

    const size_t M = _orientations.size();

    Kokkos::parallel_for( "orientations", M, KOKKOS_LAMBDA ( int i ) {
       orientations(i, 0) = _orientations[i][0];
       orientations(i, 1) = _orientations[i][1];
       orientations(i, 2) = _orientations[i][2];
    });
    // for (size_t i = 0; i < _orientations.size(); ++i) {
    //     orientations(i, 0) = _orientations[i][0];
    //     orientations(i, 1) = _orientations[i][1];
    //     orientations(i, 2) = _orientations[i][2];
    // }
}

KOKKOS_INLINE_FUNCTION
void greeter::MagneticFieldSimulator::fillObservationPoints(const std::vector<std::vector<float>>& _observation_points) {

    const size_t M = _observation_points.size();

    Kokkos::parallel_for( "magnetizations", M, KOKKOS_LAMBDA ( int i ) {
       observation_points(i, 0) = _observation_points[i][0];
       observation_points(i, 1) = _observation_points[i][1];
       observation_points(i, 2) = _observation_points[i][2];
    });
    // for (size_t i = 0; i < _magnetizations.size(); ++i) {
    //     magnetizations(i, 0) = _magnetizations[i][0];
    //     magnetizations(i, 1) = _magnetizations[i][1];
    //     magnetizations(i, 2) = _magnetizations[i][2];
    // }
}