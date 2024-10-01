#ifndef MAGNETIC_FIELD_SIMULATOR_H
#define MAGNETIC_FIELD_SIMULATOR_H

#include <greeter/KokkosDefines.h>


namespace greeter {

class MagneticFieldSimulator {
  //std::vector<std::unique_ptr<greeter::Magnet>> magnets;

  Float3VectorView positions;
  Float4VectorView orientations;
  Float3VectorView magnetizations;
  Float3VectorView dimensions;
  UInt32VectorView magnet_types;
  Float3VectorView observation_points;
  Float3VectorView magnetic_fields;
  size_t num_magnets;

public:

    MagneticFieldSimulator(Float3VectorView positions,
      Float4VectorView orientations, Float3VectorView magnetizations,
      Float3VectorView dimensions, UInt32VectorView magnet_types,
      Float3VectorView observation_points
    );

    //MagneticFieldSimulator();
    //:    positions(positions), orientations(orientations), magnetizations(magnetizations), dimensions(dimensions), observation_points(observation_points) {}

    ~MagneticFieldSimulator();

    void operator()( u_int64_t observation_point_index ) const;

    void simulate();

    void printValue( u_int64_t observation_point_index ) const;
    void printPosition( u_int64_t observation_point_index ) const;
    void printMagneticField( u_int64_t observation_point_index ) const;

    void fillMagnetPositions(const std::vector<std::vector<float>>& positions);
    void fillMagnetOrientations(const std::vector<std::vector<float>>& orientations);
    void fillMagnetMagnetizations(const std::vector<std::vector<float>>& magnetizations);
    void fillMagnetdimensions(const std::vector<float>& dimensions);
    void fillObservationPoints(const std::vector<std::vector<float>>& observation_points);

    void computeMagneticField(const u_int16_t& key, const float* parameters, 
                              const float* observation_point, float& a, float& b, float& c);

    void applyRotationFromQuaternion(const float* quaternion, const float* vector, float* result);
    void applyInverseRotationFromQuaternion(const float* quaternion, const float* vector, float* result);

    std::vector<std::vector<float>> getMagneticFields() const;

    size_t getNumObservationPoints() const;
};

}  // namespace greeter

#endif  // MAGNET_COLLECTION_H