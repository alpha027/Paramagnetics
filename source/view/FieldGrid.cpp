#include <greeter/view/FieldGrid.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>


size_t greeter::view::FieldGrid::size() const {
  return field.size() / 3;
}


bool greeter::view::FieldGrid::empty() const {
  return field.empty();
}


void greeter::view::FieldGrid::getSpacing(float* spacing) const {

  for (size_t axis = 0; axis < 3; axis++) {
    if (grid && counts[axis] > 1) {
      spacing[axis] = (bounds[2 * axis + 1] - bounds[2 * axis]) /
                      (float) (counts[axis] - 1);
    } else {
      spacing[axis] = 0.0f;
    }
  }
}


size_t greeter::view::FieldGrid::getIndex(
    const uint32_t& i, const uint32_t& j, const uint32_t& k) const {

  if (!grid) {
    throw std::logic_error(
      "These samples are a list of points and not a grid, so they have no "
      "(i, j, k)");
  }

  if (i >= counts[0] || j >= counts[1] || k >= counts[2]) {
    throw std::out_of_range("Grid point outside the field");
  }

  // x slowest, z fastest, the order FieldOfView lays points out in.
  return ((size_t) i * (size_t) counts[1] + (size_t) j) * (size_t) counts[2] +
         (size_t) k;
}


void greeter::view::FieldGrid::getPoint(const size_t& index, float* xyz) const {

  if (index >= size()) {
    throw std::out_of_range("Sample outside the field");
  }

  if (!grid) {
    xyz[0] = points[3 * index + 0];
    xyz[1] = points[3 * index + 1];
    xyz[2] = points[3 * index + 2];
    return;
  }

  // The coordinates of a grid point are cheaper to work out than to keep.
  const size_t nz = counts[2];
  const size_t ny = counts[1];

  const size_t k = index % nz;
  const size_t j = (index / nz) % ny;
  const size_t i = index / (nz * ny);

  float spacing[3];
  getSpacing(spacing);

  xyz[0] = bounds[0] + (float) i * spacing[0];
  xyz[1] = bounds[2] + (float) j * spacing[1];
  xyz[2] = bounds[4] + (float) k * spacing[2];
}


void greeter::view::FieldGrid::getField(const size_t& index, float* bxyz) const {

  if (index >= size()) {
    throw std::out_of_range("Sample outside the field");
  }

  bxyz[0] = field[3 * index + 0];
  bxyz[1] = field[3 * index + 1];
  bxyz[2] = field[3 * index + 2];
}


float greeter::view::FieldGrid::getMagnitude(const size_t& index) const {

  float b[3];
  getField(index, b);

  return std::sqrt(b[0] * b[0] + b[1] * b[1] + b[2] * b[2]);
}


bool greeter::view::FieldGrid::sample(const float* xyz, float* bxyz) const {

  if (!grid || empty()) {
    return false;
  }

  float spacing[3];
  getSpacing(spacing);

  // Which cell the point falls in, and how far across it, along each axis.
  size_t low[3];
  float fraction[3];

  for (size_t axis = 0; axis < 3; axis++) {

    const float minimum = bounds[2 * axis];
    const float maximum = bounds[2 * axis + 1];

    if (counts[axis] == 1) {
      // A single sample stands for the whole axis, which is what makes a
      // plane interpolate at all.
      low[axis] = 0;
      fraction[axis] = 0.0f;
      continue;
    }

    if (xyz[axis] < minimum || xyz[axis] > maximum) {
      return false;
    }

    const float scaled = (xyz[axis] - minimum) / spacing[axis];

    size_t index = (size_t) scaled;

    // The far face of the box belongs to the last cell.
    if (index >= (size_t) counts[axis] - 1) {
      index = (size_t) counts[axis] - 2;
    }

    low[axis] = index;
    fraction[axis] = scaled - (float) index;
  }

  bxyz[0] = 0.0f;
  bxyz[1] = 0.0f;
  bxyz[2] = 0.0f;

  // The eight corners of the cell, weighted by how near the point is to each.
  for (int corner = 0; corner < 8; corner++) {

    const size_t offset[3] = {
      (size_t) ((corner >> 0) & 1),
      (size_t) ((corner >> 1) & 1),
      (size_t) ((corner >> 2) & 1)
    };

    float weight = 1.0f;
    uint32_t at[3];

    for (size_t axis = 0; axis < 3; axis++) {

      if (counts[axis] == 1) {
        if (offset[axis] == 1) {
          weight = 0.0f;  // no second sample to lean on
        }
        at[axis] = 0;
        continue;
      }

      weight *= offset[axis] == 1 ? fraction[axis] : 1.0f - fraction[axis];
      at[axis] = (uint32_t) (low[axis] + offset[axis]);
    }

    if (weight == 0.0f) {
      continue;
    }

    const size_t index = getIndex(at[0], at[1], at[2]);

    bxyz[0] += weight * field[3 * index + 0];
    bxyz[1] += weight * field[3 * index + 1];
    bxyz[2] += weight * field[3 * index + 2];
  }

  return true;
}


bool greeter::view::FieldGrid::getMagnitudeRange(
    float& minimum, float& maximum) const {

  const size_t count = size();

  if (count == 0) {
    return false;
  }

  minimum = getMagnitude(0);
  maximum = minimum;

  for (size_t i = 1; i < count; i++) {

    const float magnitude = getMagnitude(i);

    minimum = std::min(minimum, magnitude);
    maximum = std::max(maximum, magnitude);
  }

  return true;
}


float greeter::view::FieldGrid::getMagnitudeQuantile(const float& fraction) const {

  const size_t count = size();

  if (count == 0) {
    return 0.0f;
  }

  std::vector<float> magnitudes;
  magnitudes.reserve(count);

  for (size_t i = 0; i < count; i++) {
    magnitudes.push_back(getMagnitude(i));
  }

  const float clamped = std::max(0.0f, std::min(1.0f, fraction));

  size_t at = (size_t) (clamped * (float) (count - 1));

  std::nth_element(magnitudes.begin(), magnitudes.begin() + (long) at,
                   magnitudes.end());

  return magnitudes[at];
}
