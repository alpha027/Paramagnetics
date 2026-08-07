#ifndef VIEW_FIELD_GRID_H
#define VIEW_FIELD_GRID_H

#include <cstdint>
#include <vector>


namespace greeter {
namespace view {

/*
  A simulated field, together with where it was sampled.

  The simulator hands back three numbers per observation point and nothing
  else, so on its own a result does not say where any of it was measured. That
  is enough to write a CSV and not enough to draw anything: a slice, an
  interpolation and a streamline all need the layout back.

  Samples run with x slowest and z fastest, matching FieldOfView. The points of
  a grid are not stored, because a large grid would spend more memory on
  repeating its own coordinates than on the field; getPoint computes them.
*/
struct FieldGrid {

  /* Whether the samples lie on a regular grid, and so whether the box means
     anything. A field sampled at a list of points is not a grid. */
  bool grid = false;

  float bounds[6] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};  // xx, yy, zz
  uint32_t counts[3] = {0, 0, 0};                          // along x, y, z

  /* Three numbers per sample, the polarization B [T]. */
  std::vector<float> field;

  /* Three numbers per sample. Filled only when the samples are not a grid. */
  std::vector<float> points;

  size_t size() const;

  bool empty() const;

  /* Where sample `index` was taken. */
  void getPoint(const size_t& index, float* xyz) const;

  /* B at sample `index`. */
  void getField(const size_t& index, float* bxyz) const;

  float getMagnitude(const size_t& index) const;

  /* Index of the grid point (i, j, k). Throws when this is not a grid. */
  size_t getIndex(const uint32_t& i, const uint32_t& j, const uint32_t& k) const;

  /* Spacing between neighbouring samples, zero along an axis sampled once. */
  void getSpacing(float* spacing) const;

  /*
    B anywhere inside the box, interpolated between the eight samples around
    the point. Returns false, and leaves `bxyz` alone, for a point outside the
    box or for a field that is not a grid. This is what a streamline follows.
  */
  bool sample(const float* xyz, float* bxyz) const;

  /*
    Smallest and largest sample magnitude, which is the range a colour scale
    is built on. Returns false for an empty field.

    A field spans orders of magnitude between the surface of a magnet and the
    far side of a box, so a scale built on this range straight is usually
    worth reading logarithmically.
  */
  bool getMagnitudeRange(float& minimum, float& maximum) const;

  /*
    The magnitude below which a given fraction of the samples fall, used to
    keep a handful of samples pressed against a magnet from taking the whole
    colour scale. `fraction` is clamped to [0, 1].
  */
  float getMagnitudeQuantile(const float& fraction) const;
};

}  // namespace view
}  // namespace greeter

#endif  // VIEW_FIELD_GRID_H
