#ifndef VIEWER_GEOMETRY_BUILDER_H
#define VIEWER_GEOMETRY_BUILDER_H

#include "ColorMap.h"
#include "ViewSettings.h"

#include <greeter/view/Snapshot.h>

#include <cstdint>
#include <vector>


namespace viewer {

/* Lit triangles: position, normal and colour, nine floats a vertex. */
struct SurfaceBuffer {
  std::vector<float> data;
  size_t getVertexCount() const { return data.size() / 9; }
  bool empty() const { return data.empty(); }
  void clear() { data.clear(); }
};

/* Plain lines: position and colour, six floats a vertex. */
struct LineBuffer {
  std::vector<float> data;
  size_t getVertexCount() const { return data.size() / 6; }
  bool empty() const { return data.empty(); }
  void clear() { data.clear(); }
};

/* A magnet as something a click can land on. */
struct PickSphere {
  int64_t id = 0;
  float center[3] = {0.0f, 0.0f, 0.0f};
  float radius = 0.0f;
};

/*
  Turns a snapshot into things to draw.

  Nothing here touches OpenGL or Qt, which is what makes it the part of the
  viewer that can be reasoned about, and tested, without a window.
*/

/*
  The range the colours cover. Read off a quantile rather than the largest
  sample, because a few samples pressed against the surface of a magnet would
  otherwise take the whole scale and leave the rest of the box one colour.
*/
ColorRange computeRange(const greeter::view::FieldGrid& field,
                        const ViewSettings& settings);

void buildMagnets(const greeter::view::SceneSnapshot& scene,
                  const ViewSettings& settings,
                  SurfaceBuffer& surfaces,
                  LineBuffer& outlines,
                  LineBuffer& magnetization,
                  std::vector<PickSphere>& picks);

/*
  A plane through the field box, coloured by the field on it. The plane is
  sampled on its own grid rather than on the one the field was simulated on,
  so that it can be moved smoothly and drawn at whatever resolution is asked
  for.
*/
void buildSlice(const greeter::view::FieldGrid& field,
                const ViewSettings& settings,
                const ColorRange& range,
                SurfaceBuffer& surfaces);

void buildGlyphs(const greeter::view::FieldGrid& field,
                 const ViewSettings& settings,
                 const ColorRange& range,
                 LineBuffer& lines);

/*
  Field lines through the slice plane, followed both ways from a grid of
  seeds on it. Drawn in one colour rather than by the colour scale, see the
  note in the implementation.
*/
void buildStreamlines(const greeter::view::FieldGrid& field,
                      const ViewSettings& settings,
                      LineBuffer& lines);

void buildForces(const greeter::view::SceneSnapshot& scene,
                 const greeter::view::ForceReport& forces,
                 const ViewSettings& settings,
                 LineBuffer& lines);

void buildFieldBox(const greeter::view::FieldGrid& field, LineBuffer& lines);

void buildAxes(const float& length, LineBuffer& lines);

/*
  Which magnet a ray meets first, or 0. The ray is tested against the sphere
  around each magnet rather than its triangles: a click lands on the magnet a
  user means well enough, and this needs no second render pass.
*/
int64_t pick(const std::vector<PickSphere>& spheres,
             const float* origin, const float* direction);

}  // namespace viewer

#endif  // VIEWER_GEOMETRY_BUILDER_H
