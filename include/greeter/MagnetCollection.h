#include <greeter/KokkosDefines.h>


namespace greeter {

class MagnetCollectionSimulator {
  //std::vector<std::unique_ptr<greeter::Magnet>> magnets;

  Float3VectorView positions;
  Float3VectorView orientations;
  Float3VectorView magnetizations;
  Float3VectorView radii;
  Float3VectorView observation_points;

public:

    MagnetCollectionSimulator(Float3VectorView positions, Float3VectorView orientations,
      Float3VectorView magnetizations, Float3VectorView radii,
      Float3VectorView observation_points);

    //MagnetCollectionSimulator();
     //:    positions(positions), orientations(orientations), magnetizations(magnetizations), radii(radii), observation_points(observation_points) {}

    ~MagnetCollectionSimulator();

    void operator()( u_int64_t observation_point_index ) const;

    void simulate();

    void printValue( u_int64_t observation_point_index ) const;
    void printPosition( u_int64_t observation_point_index ) const;

    void fillMagnetPositions(const std::vector<std::vector<float>>& positions);
    void fillMagnetOrientations(const std::vector<std::vector<float>>& orientations);
    void fillMagnetMagnetizations(const std::vector<std::vector<float>>& magnetizations);
    void fillMagnetRadii(const std::vector<float>& radii);
    void fillObservationPoints(const std::vector<std::vector<float>>& observation_points);

    u_int64_t getNumObservationPoints() const {
        //return observation_points.extent(0);
        return 100000000;
    }
};

}  // namespace greeter