#ifndef VIEWER_VIEW_SETTINGS_H
#define VIEWER_VIEW_SETTINGS_H

#include <cstdint>


namespace viewer {

/*
  What the field is coloured by. The field is a vector, and which number of it
  a colour stands for is a question the viewer has to be asked rather than
  assume.
*/
enum class FieldQuantity : uint8_t {
  Magnitude = 0,
  Bx = 1,
  By = 2,
  Bz = 3
};

/*
  How the colour scale is stretched.

  The field near the surface of a magnet is orders of magnitude stronger than
  the field a few centimetres away, so a linear scale over the whole range
  paints everything but a handful of samples the same colour. Logarithmic is
  therefore the default, and the range is clipped at a quantile rather than at
  the largest sample.
*/
enum class ColorScale : uint8_t {
  Logarithmic = 0,
  Linear = 1
};

/* Everything the viewport is told about how to draw, in one place. */
struct ViewSettings {

  bool show_magnets = true;
  bool show_magnetization = true;
  bool show_outlines = true;
  bool show_axes = true;
  bool show_field_box = true;

  bool show_slice = true;
  bool show_glyphs = false;
  bool show_streamlines = false;

  bool show_forces = true;
  bool show_torques = false;

  /* Which axis the slice plane is across, 0 for x, 1 for y, 2 for z. */
  int slice_axis = 2;

  /* Where along that axis, from 0 at the near face to 1 at the far one. */
  float slice_position = 0.5f;

  /* Samples across the slice, each way. */
  int slice_resolution = 96;

  /* Arrows across the field box, each way. */
  int glyph_count = 8;

  float glyph_scale = 1.0f;

  /* Streamlines started across the slice plane, each way. */
  int streamline_seeds = 12;

  int streamline_steps = 400;

  FieldQuantity quantity = FieldQuantity::Magnitude;

  ColorScale scale = ColorScale::Logarithmic;

  /*
    The quantile the colour scale stops at. Anything above it is drawn in the
    end colour rather than being allowed to flatten everything else.
  */
  float clip_quantile = 0.98f;

  float magnet_opacity = 1.0f;

  /* Which magnet is picked out, by id. Zero for none. */
  int64_t selected_id = 0;
};

}  // namespace viewer

#endif  // VIEWER_VIEW_SETTINGS_H
