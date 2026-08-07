#ifndef VIEW_SHAPE_MESH_H
#define VIEW_SHAPE_MESH_H

#include <greeter/view/ShapeDescriptor.h>
#include <cstdint>
#include <vector>


namespace greeter {
namespace view {

/*
  Triangles, in the local frame of a magnet.

  Turning a descriptor into triangles happens here rather than in the renderer
  so that it can be tested without a window, and so that a renderer written
  against this never learns what a cylinder is.
*/
struct ShapeMesh {

  std::vector<float> vertices;      // 3 per vertex [m]
  std::vector<float> normals;       // 3 per vertex, unit length
  std::vector<uint32_t> triangles;  // 3 indices each

  /* Pairs of indices, the edges worth drawing as an outline. */
  std::vector<uint32_t> edges;

  size_t getVertexCount() const { return vertices.size() / 3; }
  size_t getTriangleCount() const { return triangles.size() / 3; }

  bool empty() const { return triangles.empty(); }
};

/*
  The triangles of a shape. `segments` is how finely a round shape is cut,
  and is ignored by the flat ones. A descriptor that describes nothing gives
  back an empty mesh, which a renderer draws as a marker instead.
*/
ShapeMesh buildMesh(const ShapeDescriptor& shape, const uint32_t& segments = 24);

}  // namespace view
}  // namespace greeter

#endif  // VIEW_SHAPE_MESH_H
