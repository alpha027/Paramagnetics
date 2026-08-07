#ifndef MAGNET_PARAMETERS_H
#define MAGNET_PARAMETERS_H

#include <greeter/KokkosDefines.h>
#include <greeter/Magnet.h>


namespace greeter {

/*
  Every magnet class takes its parameters in the same order:

      [0 .. 2]        position
      [3 .. 6]        orientation, as a quaternion (w, x, y, z)
      [7 .. 6 + n]    the n geometry parameters of the shape
      [7 + n .. 9 + n] magnetization

  so a shape only decides n, which is getNumOfParameters() - 10: three for a
  cuboid, one for a sphere, twelve for a tetrahedron. Packing the array is
  therefore the same work for every shape, and doing it here is what keeps the
  inner loop of the simulators free of a branch per magnet type.

  `parameters` has to hold MAX_MAGNET_PARAMETERS floats.
*/
KOKKOS_INLINE_FUNCTION
void packMagnetParameters(
    const Float3VectorView& positions, const Float4VectorView& orientations,
    const Float3VectorView& magnetizations, const FloatVectorView& dimensions,
    const size_t& magnet_index, const size_t& geometry_offset,
    const size_t& geometry_count, float* parameters) {

    parameters[0] = positions(magnet_index, 0);
    parameters[1] = positions(magnet_index, 1);
    parameters[2] = positions(magnet_index, 2);

    parameters[3] = orientations(magnet_index, 0);
    parameters[4] = orientations(magnet_index, 1);
    parameters[5] = orientations(magnet_index, 2);
    parameters[6] = orientations(magnet_index, 3);

    for (size_t j = 0; j < geometry_count; j++) {
        parameters[7 + j] = dimensions(geometry_offset + j);
    }

    parameters[7 + geometry_count] = magnetizations(magnet_index, 0);
    parameters[8 + geometry_count] = magnetizations(magnet_index, 1);
    parameters[9 + geometry_count] = magnetizations(magnet_index, 2);
}

}  // namespace greeter

#endif  // MAGNET_PARAMETERS_H
