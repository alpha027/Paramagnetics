#ifndef MAGNETIC_FIELD_SIMULATOR_H
#define MAGNETIC_FIELD_SIMULATOR_H

#include <greeter/KokkosDefines.h>


namespace greeter {

class MagneticFieldSimulator {
  //std::vector<std::unique_ptr<greeter::Magnet>> magnets;

  // The parameters of every magnet, end to end, and where each one starts.
  // See MagnetParameters.h for the layout and for why it is stored this way.
  FloatVectorView magnet_parameters;
  UInt32VectorView parameter_offsets;
  UInt32VectorView magnet_types;
  Float3VectorView observation_points;
  Float3VectorView magnetic_fields;

  // The polarization at each observation point, filled only when asked for.
  Float3VectorView polarizations;
  bool with_polarization = false;

  size_t num_magnets;

  // Kernel and geometry layout of each magnet, resolved once from `magnet_types`.
  MagnetKernelView magnet_kernels;

  void resolveMagnetTypes();

public:

    MagneticFieldSimulator(FloatVectorView magnet_parameters,
      UInt32VectorView parameter_offsets, UInt32VectorView magnet_types,
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

    /*
      Also work out the polarization at every point, which is what turns B
      into H, J and M. Off by default, because it costs a second pass over the
      magnets and most runs only want B. Set before simulate().
    */
    void setComputePolarization(const bool& wanted);

    /* Three floats per observation point, the polarization J [T]. */
    std::vector<float> getPolarizationsFlat() const;

    /* The observation points, likewise flat, in the order they were filled. */
    std::vector<float> getObservationPointsFlat() const;

    size_t getNumObservationPoints() const;
};

}  // namespace greeter

#endif  // MAGNET_COLLECTION_H