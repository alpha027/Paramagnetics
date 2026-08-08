#include <greeter/view/ShapeMesh.h>
#include <algorithm>
#include <cmath>


namespace {

using greeter::view::ShapeMesh;

void addVertex(ShapeMesh& mesh, const float* point, const float* normal) {

  mesh.vertices.push_back(point[0]);
  mesh.vertices.push_back(point[1]);
  mesh.vertices.push_back(point[2]);

  mesh.normals.push_back(normal[0]);
  mesh.normals.push_back(normal[1]);
  mesh.normals.push_back(normal[2]);
}

void addTriangle(ShapeMesh& mesh, const uint32_t& a, const uint32_t& b,
                 const uint32_t& c) {
  mesh.triangles.push_back(a);
  mesh.triangles.push_back(b);
  mesh.triangles.push_back(c);
}

void addEdge(ShapeMesh& mesh, const uint32_t& a, const uint32_t& b) {
  mesh.edges.push_back(a);
  mesh.edges.push_back(b);
}

void normalise(float* vector) {

  const float length = std::sqrt(vector[0] * vector[0] + vector[1] * vector[1] +
                                 vector[2] * vector[2]);

  if (length > 0.0f) {
    vector[0] /= length;
    vector[1] /= length;
    vector[2] /= length;
  }
}

/* The unit normal of the triangle abc, wound anticlockwise. */
void faceNormal(const float* a, const float* b, const float* c, float* normal) {

  const float u[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
  const float v[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};

  normal[0] = u[1] * v[2] - u[2] * v[1];
  normal[1] = u[2] * v[0] - u[0] * v[2];
  normal[2] = u[0] * v[1] - u[1] * v[0];

  normalise(normal);
}

/*
  A flat face wants its own normal at every corner, otherwise the corners of a
  box are shaded as though it were a ball. Every face therefore gets its own
  copies of the vertices it shares.
*/
void addFlatFace(ShapeMesh& mesh, const std::vector<const float*>& corners,
                 const bool& outline) {

  if (corners.size() < 3) {
    return;
  }

  float normal[3];
  faceNormal(corners[0], corners[1], corners[2], normal);

  const uint32_t first = (uint32_t) mesh.getVertexCount();

  for (const auto& corner : corners) {
    addVertex(mesh, corner, normal);
  }

  for (size_t i = 1; i + 1 < corners.size(); i++) {
    addTriangle(mesh, first, first + (uint32_t) i, first + (uint32_t) i + 1);
  }

  if (outline) {
    for (size_t i = 0; i < corners.size(); i++) {
      addEdge(mesh, first + (uint32_t) i,
              first + (uint32_t) ((i + 1) % corners.size()));
    }
  }
}

ShapeMesh buildBox(const std::vector<float>& parameters) {

  ShapeMesh mesh;

  const float x = 0.5f * parameters[0];
  const float y = 0.5f * parameters[1];
  const float z = 0.5f * parameters[2];

  const float corner[8][3] = {
    {-x, -y, -z}, { x, -y, -z}, { x,  y, -z}, {-x,  y, -z},
    {-x, -y,  z}, { x, -y,  z}, { x,  y,  z}, {-x,  y,  z}
  };

  // Anticlockwise seen from outside, so the normals point out.
  const int face[6][4] = {
    {0, 3, 2, 1},  // -z
    {4, 5, 6, 7},  // +z
    {0, 1, 5, 4},  // -y
    {2, 3, 7, 6},  // +y
    {0, 4, 7, 3},  // -x
    {1, 2, 6, 5}   // +x
  };

  for (const auto& indices : face) {
    addFlatFace(mesh, {corner[indices[0]], corner[indices[1]],
                       corner[indices[2]], corner[indices[3]]}, true);
  }

  return mesh;
}

ShapeMesh buildCylinder(const std::vector<float>& parameters,
                        const uint32_t& segments) {

  ShapeMesh mesh;

  const float radius = 0.5f * parameters[0];
  const float half_height = 0.5f * parameters[1];

  const uint32_t sides = std::max(3u, segments);

  // The wall, with normals pointing away from the axis.
  const uint32_t wall_first = (uint32_t) mesh.getVertexCount();

  for (uint32_t i = 0; i <= sides; i++) {

    const float angle = 2.0f * (float) M_PI * (float) (i % sides) / (float) sides;

    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);

    const float normal[3] = {cosine, sine, 0.0f};

    const float low[3] = {radius * cosine, radius * sine, -half_height};
    const float high[3] = {radius * cosine, radius * sine, half_height};

    addVertex(mesh, low, normal);
    addVertex(mesh, high, normal);
  }

  for (uint32_t i = 0; i < sides; i++) {

    const uint32_t a = wall_first + 2 * i;

    addTriangle(mesh, a, a + 2, a + 3);
    addTriangle(mesh, a, a + 3, a + 1);

    // The rims, which are what makes a cylinder read as one in an outline.
    addEdge(mesh, a, a + 2);
    addEdge(mesh, a + 1, a + 3);
  }

  // The two caps, each a fan around its own centre.
  for (int end = 0; end < 2; end++) {

    const float z = end == 0 ? -half_height : half_height;
    const float normal[3] = {0.0f, 0.0f, end == 0 ? -1.0f : 1.0f};

    const uint32_t centre = (uint32_t) mesh.getVertexCount();
    const float middle[3] = {0.0f, 0.0f, z};

    addVertex(mesh, middle, normal);

    for (uint32_t i = 0; i <= sides; i++) {

      const float angle =
        2.0f * (float) M_PI * (float) (i % sides) / (float) sides;

      const float point[3] = {radius * std::cos(angle), radius * std::sin(angle), z};

      addVertex(mesh, point, normal);
    }

    for (uint32_t i = 0; i < sides; i++) {
      if (end == 0) {
        addTriangle(mesh, centre, centre + i + 2, centre + i + 1);
      } else {
        addTriangle(mesh, centre, centre + i + 1, centre + i + 2);
      }
    }
  }

  return mesh;
}

ShapeMesh buildSphere(const std::vector<float>& parameters,
                      const uint32_t& segments) {

  ShapeMesh mesh;

  const float radius = 0.5f * parameters[0];

  const uint32_t rings = std::max(2u, segments / 2);
  const uint32_t sides = std::max(3u, segments);

  for (uint32_t ring = 0; ring <= rings; ring++) {

    const float polar = (float) M_PI * (float) ring / (float) rings;

    const float sin_polar = std::sin(polar);
    const float cos_polar = std::cos(polar);

    for (uint32_t side = 0; side <= sides; side++) {

      const float azimuth =
        2.0f * (float) M_PI * (float) (side % sides) / (float) sides;

      // On a sphere the outward normal is the direction of the point itself.
      const float normal[3] = {
        sin_polar * std::cos(azimuth),
        sin_polar * std::sin(azimuth),
        cos_polar
      };

      const float point[3] = {
        radius * normal[0], radius * normal[1], radius * normal[2]
      };

      addVertex(mesh, point, normal);
    }
  }

  const uint32_t stride = sides + 1;

  for (uint32_t ring = 0; ring < rings; ring++) {
    for (uint32_t side = 0; side < sides; side++) {

      const uint32_t a = ring * stride + side;
      const uint32_t b = a + stride;

      addTriangle(mesh, a, b, b + 1);
      addTriangle(mesh, a, b + 1, a + 1);
    }
  }

  // A ball has no edges to outline; the equator and a meridian give a viewer
  // something to see through a transparent surface.
  const uint32_t equator = (rings / 2) * stride;

  for (uint32_t side = 0; side < sides; side++) {
    addEdge(mesh, equator + side, equator + side + 1);
  }

  for (uint32_t ring = 0; ring < rings; ring++) {
    addEdge(mesh, ring * stride, (ring + 1) * stride);
  }

  return mesh;
}

ShapeMesh buildTetrahedron(const std::vector<float>& parameters) {

  ShapeMesh mesh;

  const float* vertex[4] = {
    &parameters[0], &parameters[3], &parameters[6], &parameters[9]
  };

  // Which way round each face is wound depends on the handedness of the four
  // points, so the normals are turned outwards afterwards instead.
  const int face[4][3] = {
    {0, 1, 2}, {0, 2, 3}, {0, 3, 1}, {1, 3, 2}
  };

  float centroid[3] = {0.0f, 0.0f, 0.0f};

  for (const auto& point : vertex) {
    for (size_t axis = 0; axis < 3; axis++) {
      centroid[axis] += 0.25f * point[axis];
    }
  }

  for (const auto& indices : face) {

    const float* a = vertex[indices[0]];
    const float* b = vertex[indices[1]];
    const float* c = vertex[indices[2]];

    float normal[3];
    faceNormal(a, b, c, normal);

    // Away from the middle of the body, whichever way the points were given.
    const float outward =
      normal[0] * (a[0] - centroid[0]) +
      normal[1] * (a[1] - centroid[1]) +
      normal[2] * (a[2] - centroid[2]);

    const bool flip = outward < 0.0f;

    if (flip) {
      normal[0] = -normal[0];
      normal[1] = -normal[1];
      normal[2] = -normal[2];
    }

    const uint32_t first = (uint32_t) mesh.getVertexCount();

    addVertex(mesh, a, normal);
    addVertex(mesh, flip ? c : b, normal);
    addVertex(mesh, flip ? b : c, normal);

    addTriangle(mesh, first, first + 1, first + 2);

    addEdge(mesh, first, first + 1);
    addEdge(mesh, first + 1, first + 2);
    addEdge(mesh, first + 2, first);
  }

  return mesh;
}

ShapeMesh buildTriangles(const std::vector<float>& parameters) {

  ShapeMesh mesh;

  for (size_t i = 0; i + 8 < parameters.size(); i += 9) {
    addFlatFace(mesh, {&parameters[i], &parameters[i + 3], &parameters[i + 6]},
                false);
  }

  return mesh;
}

}  // namespace


greeter::view::ShapeMesh greeter::view::buildMesh(
    const greeter::view::ShapeDescriptor& shape, const uint32_t& segments) {

  if (!shape.isValid()) {
    return greeter::view::ShapeMesh();
  }

  switch (shape.kind) {

    case greeter::view::ShapeKind::Box:
      return buildBox(shape.parameters);

    case greeter::view::ShapeKind::Cylinder:
      return buildCylinder(shape.parameters, segments);

    case greeter::view::ShapeKind::Sphere:
      return buildSphere(shape.parameters, segments);

    case greeter::view::ShapeKind::Tetrahedron:
      return buildTetrahedron(shape.parameters);

    case greeter::view::ShapeKind::Mesh:
      return buildTriangles(shape.parameters);

    // A point has no surface. The viewer draws a marker where it sits, the
    // same way it does for a shape it has never met.
    case greeter::view::ShapeKind::Point:
    case greeter::view::ShapeKind::Unknown:
      break;
  }

  return greeter::view::ShapeMesh();
}
