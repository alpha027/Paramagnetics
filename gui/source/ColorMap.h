#ifndef VIEWER_COLOR_MAP_H
#define VIEWER_COLOR_MAP_H

#include "ViewSettings.h"


namespace viewer {

/*
  Turns a number into a colour.

  The range a scale covers is worked out once for a field and then handed
  around, so that the slice, the arrows and the legend all agree about what a
  colour means. A legend that disagrees with the picture is worse than none.
*/
struct ColorRange {

  float minimum = 0.0f;
  float maximum = 1.0f;

  ColorScale scale = ColorScale::Logarithmic;

  /* Whether the quantity runs either side of zero, as a component does. */
  bool diverging = false;

  /* Where a value falls in the range, from 0 to 1. */
  float normalise(const float& value) const;
};

/*
  A perceptually even scale, dark blue through green to yellow, near enough to
  viridis to read the same way. `t` outside [0, 1] is clamped.
*/
void mapColor(const float& t, float* rgb);

/*
  For a quantity that runs either side of zero: blue for negative, white in
  the middle, red for positive. A single-ended scale would hide the sign,
  which for a field component is the interesting part.
*/
void mapDivergingColor(const float& t, float* rgb);

/* Whichever of the two the range asks for. */
void mapColor(const ColorRange& range, const float& value, float* rgb);

/* The number a colour stands for, given the quantity being shown. */
float quantityOf(const float* b, const FieldQuantity& quantity);

}  // namespace viewer

#endif  // VIEWER_COLOR_MAP_H
