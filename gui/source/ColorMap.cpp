#include "ColorMap.h"

#include <algorithm>
#include <cmath>


namespace {

float clamp01(const float& value) {
  return std::max(0.0f, std::min(1.0f, value));
}

/*
  Six points off viridis, interpolated between. Enough to keep the useful
  property of it, which is that equal steps in the number look like equal
  steps in the colour.
*/
const float VIRIDIS[6][3] = {
  {0.267f, 0.005f, 0.329f},
  {0.283f, 0.141f, 0.458f},
  {0.254f, 0.265f, 0.530f},
  {0.164f, 0.471f, 0.558f},
  {0.478f, 0.821f, 0.318f},
  {0.993f, 0.906f, 0.144f}
};

}  // namespace


float viewer::ColorRange::normalise(const float& value) const {

  if (diverging) {

    // The middle of a diverging scale is zero, and both ends are as far from
    // it as the furthest value, so that the colour of a sign is the same
    // whichever sign has the larger extreme.
    const float reach = std::max(std::fabs(minimum), std::fabs(maximum));

    if (reach <= 0.0f) {
      return 0.5f;
    }

    if (scale == viewer::ColorScale::Logarithmic) {

      const float floor_value = reach * 1e-4f;
      const float magnitude = std::max(std::fabs(value), floor_value);

      const float t = std::log10(magnitude / floor_value) /
                      std::log10(reach / floor_value);

      return clamp01(0.5f + (value < 0.0f ? -0.5f : 0.5f) * t);
    }

    return clamp01(0.5f + 0.5f * value / reach);
  }

  if (scale == viewer::ColorScale::Logarithmic) {

    // A field spans decades, and a floor keeps the empty corners of a box
    // from taking the whole of the scale.
    const float top = std::max(maximum, 1e-30f);
    const float floor_value = std::max(minimum, top * 1e-5f);

    const float clamped = std::max(value, floor_value);

    const float span = std::log10(top / floor_value);

    if (!(span > 0.0f)) {
      return 0.0f;
    }

    return clamp01(std::log10(clamped / floor_value) / span);
  }

  if (maximum <= minimum) {
    return 0.0f;
  }

  return clamp01((value - minimum) / (maximum - minimum));
}


void viewer::mapColor(const float& t, float* rgb) {

  const float clamped = clamp01(t);

  const float scaled = clamped * 5.0f;

  const int low = std::min(4, (int) scaled);

  const float fraction = scaled - (float) low;

  for (int i = 0; i < 3; i++) {
    rgb[i] = VIRIDIS[low][i] +
             fraction * (VIRIDIS[low + 1][i] - VIRIDIS[low][i]);
  }
}


void viewer::mapDivergingColor(const float& t, float* rgb) {

  const float clamped = clamp01(t);

  if (clamped < 0.5f) {

    const float fraction = clamped * 2.0f;

    rgb[0] = 0.16f + fraction * (0.96f - 0.16f);
    rgb[1] = 0.36f + fraction * (0.96f - 0.36f);
    rgb[2] = 0.72f + fraction * (0.96f - 0.72f);

    return;
  }

  const float fraction = (clamped - 0.5f) * 2.0f;

  rgb[0] = 0.96f + fraction * (0.79f - 0.96f);
  rgb[1] = 0.96f + fraction * (0.16f - 0.96f);
  rgb[2] = 0.96f + fraction * (0.16f - 0.96f);
}


void viewer::mapColor(const viewer::ColorRange& range, const float& value,
                      float* rgb) {

  const float t = range.normalise(value);

  if (range.diverging) {
    mapDivergingColor(t, rgb);
  } else {
    mapColor(t, rgb);
  }
}


float viewer::quantityOf(const float* b, const viewer::FieldQuantity& quantity) {

  switch (quantity) {
    case viewer::FieldQuantity::Bx: return b[0];
    case viewer::FieldQuantity::By: return b[1];
    case viewer::FieldQuantity::Bz: return b[2];
    case viewer::FieldQuantity::Magnitude:
      break;
  }

  return std::sqrt(b[0] * b[0] + b[1] * b[1] + b[2] * b[2]);
}
