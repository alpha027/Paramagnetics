#ifndef VIEW_SHAPE_DESCRIPTOR_H
#define VIEW_SHAPE_DESCRIPTOR_H

#include <cstdint>
#include <string>
#include <vector>


namespace greeter {
namespace view {

/*
  What a magnet looks like, told in a way a viewer can draw without knowing
  which class it came from.

  Magnet::getDimensions() means something different for every shape, and the
  only thing that says which is getTypeID(). A viewer that switched on that
  would have to be edited every time a magnet type is added, which is the
  coupling the arrangements were built to avoid. A descriptor carries its own
  meaning instead, and magnet classes hand one out through
  MagnetGeometryFactory.

  Sizes here are always the full extent across the shape, so a sphere gives a
  diameter even though it is built from a radius and its input file asks for
  one. The renderer then has a single rule to remember.
*/
enum class ShapeKind : uint8_t {

  /* Nothing is known about the shape, which is drawn as a marker. */
  Unknown = 0,

  /* parameters: the edge lengths along the local x, y and z axes [m] */
  Box = 1,

  /* parameters: diameter, height [m]. The axis is the local z axis. */
  Cylinder = 2,

  /* parameters: diameter [m] */
  Sphere = 3,

  /* parameters: four local vertices, twelve numbers [m] */
  Tetrahedron = 4,

  /*
    parameters: triangles, nine numbers each, in the local frame [m].

    A shape that is none of the above describes itself this way and needs no
    change anywhere in the viewer.
  */
  Mesh = 5
};

/*
  Not called toString: doctest and other libraries pick up a toString found by
  argument dependent lookup and expect their own string type back from it.
*/
std::string getName(const ShapeKind& kind);

struct ShapeDescriptor {

  ShapeKind kind = ShapeKind::Unknown;

  /* The name the type answers to in an input file, "cuboid" and so on. */
  std::string type_name;

  /* Meaning fixed by `kind`, see above. Lengths are metres. */
  std::vector<float> parameters;

  /*
    Whether the parameters are as many numbers as the kind asks for. A
    descriptor that fails this describes nothing drawable.
  */
  bool isValid() const;

  /*
    Half the extent of the shape along each local axis, which is what a camera
    needs to frame a scene and a picker needs for a first guess. Zero for a
    descriptor that is not valid.
  */
  std::vector<float> getLocalHalfExtent() const;
};

}  // namespace view
}  // namespace greeter

#endif  // VIEW_SHAPE_DESCRIPTOR_H
