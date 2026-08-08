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

  so a shape only decides n: three for a cuboid, one for a sphere, twelve for
  a tetrahedron, none at all for a dipole.

  The whole block of every magnet is laid down once, end to end, in a single
  array, and a kernel is handed a pointer to the start of its own block. That
  is the only reason this header still exists, and it buys two things.

  Nothing is copied per field evaluation. The packing this used to do ran once
  per magnet per observation point, into a stack array.

  And n is no longer bounded. It used to be, because the stack array had to be
  declared with a fixed size, which capped a shape at a couple of dozen
  numbers; a triangular mesh of a thousand facets takes nine thousand. A shape
  may now take as many as it likes, and how many is a property of the magnet
  rather than of its type: two meshes of the same type need not be the same
  size.
*/
KOKKOS_INLINE_FUNCTION
const float* magnetParameters(const FloatVectorView& magnet_parameters,
                              const u_int32_t& parameter_offset) {
    return &magnet_parameters(parameter_offset);
}

}  // namespace greeter

#endif  // MAGNET_PARAMETERS_H
