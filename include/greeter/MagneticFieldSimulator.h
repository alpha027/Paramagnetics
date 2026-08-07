#ifndef MAGNETIC_FIELD_SIMULATOR_H
#define MAGNETIC_FIELD_SIMULATOR_H

#include <greeter/KokkosDefines.h>


namespace greeter {

class MagneticFieldSimulator {
  //std::vector<std::unique_ptr<greeter::Magnet>> magnets;

  Float3VectorView positions;
  Float4VectorView orientations;
  Float3VectorView magnetizations;
  FloatVectorView dimensions;
  UInt32VectorView magnet_types;
  Float3VectorView observation_points;
  Float3VectorView magnetic_fields;
  size_t num_magnets;

  // Kernel and geometry layout of each magnet, resolved once from `magnet_types`.
  MagnetKernelView magnet_kernels;

  void resolveMagnetTypes();

public:

    MagneticFieldSimulator(Float3VectorView positions,
      Float4VectorView orientations, Float3VectorView magnetizations,
      FloatVectorView dimensions, UInt32VectorView magnet_types,
      Float3VectorView observation_points
    );

    //MagneticFieldSimulator();
    //:    positions(positions), orientations(orientations), magnetizations(magnetizations), dimensions(dimensions), observation_points(observation_points) {}

    ~MagneticFieldSimulator();

    void operator()( u_int64_t observation_point_index ) const;

    void simulate();

    /*
      The same, without saying so. A caller that runs the observation points
      in chunks, as a viewer does to stay answerable, would otherwise print
      two lines per chunk.
    */
    void simulate(const bool& verbose);

    void printValue( u_int64_t observation_point_index ) const;
    void printPosition( u_int64_t observation_point_index ) const;
    void printMagneticField( u_int64_t observation_point_index ) const;

    void fillObservationPoints(const std::vector<std::vector<float>>& observation_points);

    void computeMagneticField(const u_int16_t& key, const float* parameters,
                              const float* observation_point, float& a, float& b, float& c);

    void applyRotationFromQuaternion(const float* quaternion, const float* vector, float* result);
    void applyInverseRotationFromQuaternion(const float* quaternion, const float* vector, float* result);

    std::vector<std::vector<float>> getMagneticFields() const;

    /*
      The same values as three floats per observation point, one after the
      other. A vector of vectors costs one allocation per sample, which is
      nothing next to the simulation itself but is a poor thing to hand to a
      viewer that redraws the result.
    */
    std::vector<float> getMagneticFieldsFlat() const;

    /* The observation points, likewise flat, in the order they were filled. */
    std::vector<float> getObservationPointsFlat() const;

    size_t getNumObservationPoints() const;
};

}  // namespace greeter

#endif  // MAGNET_COLLECTION_H