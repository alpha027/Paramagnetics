#include <greeter/MagnetCollection.h>


inline
greeter::MagnetCollectionSimulator::MagnetCollectionSimulator(
    Float3VectorView _positions, Float3VectorView _orientations,
    Float3VectorView _magnetizations, Float3VectorView _radii,
    Float3VectorView _observation_points) :
    positions(_positions), orientations(_orientations),
    magnetizations(_magnetizations), radii(_radii),
    observation_points(_observation_points) {}

inline
greeter::MagnetCollectionSimulator::~MagnetCollectionSimulator() {}

inline
void greeter::MagnetCollectionSimulator::simulate() {
    // Kokkos::Timer timer;

    // Kokkos::Random_XorShift64_Pool<> rand_pool(1234);
    // Kokkos::Random_XorShift64<> rand_gen(rand_pool);

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
void greeter::MagnetCollectionSimulator::printValue( u_int64_t observation_point_index ) const {
    std::cout << "value for index " << observation_point_index << ": (" 
              << observation_points( observation_point_index, 0) << ", " 
              << observation_points(observation_point_index, 1) << ", " 
              << observation_points(observation_point_index,2) << ")" << std::endl;
}

KOKKOS_INLINE_FUNCTION
void greeter::MagnetCollectionSimulator::operator()( u_int64_t observation_point_index ) const {

    //std::cout << "set value for index " << observation_point_index  << std::endl;
    observation_points( observation_point_index, 0 ) = 2.1f;
    observation_points( observation_point_index, 1 ) = 2.2f;
    observation_points( observation_point_index, 2 ) = 2.3f;

}