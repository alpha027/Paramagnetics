#include <greeter/FieldOfView.h>
#include <iostream>
#include <stdexcept>

namespace greeter {

  FieldOfView::FieldOfView(): grid(false) {}

  FieldOfView::FieldOfView(std::vector<std::vector<float>> fov):
    fov(fov), grid(false) {}

  FieldOfView::FieldOfView(
      std::vector<float> xxyyzz,
      std::vector<uint32_t> num_points): grid(true)
  {
    if (xxyyzz.size() != 6) {
      throw std::invalid_argument(
        "A field of view is bounded by six numbers, x_min, x_max, y_min, "
        "y_max, z_min and z_max");
    }

    if (num_points.size() != 3) {
      throw std::invalid_argument(
        "A field of view is sampled along three axes");
    }

    const std::vector<std::string> axes = {"x", "y", "z"};

    for (size_t axis = 0; axis < 3; axis++) {

      if (num_points[axis] < 1) {
        throw std::invalid_argument(
          "A field of view has to be sampled at least once along " +
          axes[axis] + ", a count of zero leaves nothing to simulate");
      }

      if (xxyyzz[2 * axis + 1] < xxyyzz[2 * axis]) {
        throw std::invalid_argument(
          "The maximum of a field of view along " + axes[axis] +
          " lies below its minimum");
      }
    }

    bounds = xxyyzz;
    counts = num_points;

    // An axis sampled once has no spacing to speak of, and its single sample
    // sits at the minimum. Taking the difference over count - 1 without this
    // divides by zero, and a plane is exactly the case that asks for it.
    std::vector<float> step(3, 0.0f);

    for (size_t axis = 0; axis < 3; axis++) {
      if (num_points[axis] > 1) {
        step[axis] = (xxyyzz[2 * axis + 1] - xxyyzz[2 * axis]) /
                     (float) (num_points[axis] - 1);
      }
    }

    const size_t vecSize =
      (size_t) num_points[0] * (size_t) num_points[1] * (size_t) num_points[2];

    fov.resize(vecSize, std::vector<float>(3, 0.0f));

    size_t index = 0; // Linear index to access the 1D `fov` vector
    for (uint32_t i = 0; i < num_points[0]; ++i) { // X dimension
        float x = xxyyzz[0] + (float) i * step[0];
        for (uint32_t j = 0; j < num_points[1]; ++j) { // Y dimension
            float y = xxyyzz[2] + (float) j * step[1];
            for (uint32_t k = 0; k < num_points[2]; ++k) { // Z dimension
                float z = xxyyzz[4] + (float) k * step[2];

                // Store the point in the grid
                fov[index][0] = x;
                fov[index][1] = y;
                fov[index][2] = z;

                ++index; // Move to the next point in the 1D array
            }
        }
    }
  }

  FieldOfView::FieldOfView(const FieldOfView& other):
    fov(other.fov), grid(other.grid),
    bounds(other.bounds), counts(other.counts) {}

  FieldOfView::~FieldOfView() {}

  std::vector<std::vector<float>> FieldOfView::getFOV() const {
    return fov;
  }

  const std::vector<std::vector<float>>& FieldOfView::getPoints() const {
    return fov;
  }

  void FieldOfView::setFOV(const std::vector<std::vector<float>>& fovs) {
    fov = fovs;

    // Points handed over one by one are not known to lie on a grid, and
    // keeping the old box would describe a set of points that no longer
    // exists.
    grid = false;
    bounds.clear();
    counts.clear();
  }

  size_t FieldOfView::getNumPoints() const {
    return fov.size();
  }

  bool FieldOfView::isGrid() const {
    return grid;
  }

  std::vector<float> FieldOfView::getBounds() const {
    if (!grid) {
      throw std::logic_error(
        "This field of view is a list of points and has no box, ask isGrid() "
        "before asking for one");
    }
    return bounds;
  }

  std::vector<uint32_t> FieldOfView::getCounts() const {
    if (!grid) {
      throw std::logic_error(
        "This field of view is a list of points and is not sampled along "
        "axes, ask isGrid() before asking how many");
    }
    return counts;
  }

  std::vector<float> FieldOfView::getSpacing() const {

    const std::vector<uint32_t> num_points = getCounts();

    std::vector<float> spacing(3, 0.0f);

    for (size_t axis = 0; axis < 3; axis++) {
      if (num_points[axis] > 1) {
        spacing[axis] = (bounds[2 * axis + 1] - bounds[2 * axis]) /
                        (float) (num_points[axis] - 1);
      }
    }

    return spacing;
  }

  size_t FieldOfView::getIndex(
      const uint32_t& i, const uint32_t& j, const uint32_t& k) const {

    const std::vector<uint32_t> num_points = getCounts();

    if (i >= num_points[0] || j >= num_points[1] || k >= num_points[2]) {
      throw std::out_of_range(
        "Grid point outside the field of view");
    }

    // x slowest, z fastest, see the note on the class.
    return ((size_t) i * (size_t) num_points[1] + (size_t) j) *
           (size_t) num_points[2] + (size_t) k;
  }

  void FieldOfView::display() const {

    size_t fov_size = fov.size();

    if (fov_size == 0) {
      std::cout << "Empty field of view" << std::endl;
      return;
    }

    size_t fov_dim = fov[0].size();

    for(size_t i = 0; i < fov_size; i++) {
      for(size_t j = 0; j < fov_dim; j++) {
        std::cout << fov[i][j] << " ";
      }
      std::cout << std::endl;
    }
  }

  std::unique_ptr<FieldOfView> FieldOfView::clone() const {
    return std::make_unique<FieldOfView>(*this);
  }

}  // namespace greeter
