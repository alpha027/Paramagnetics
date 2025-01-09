#include <greeter/FieldOfView.h>
#include <iostream>

namespace greeter {

  FieldOfView::FieldOfView() { fov = std::vector<std::vector<float>>(); }

  FieldOfView::FieldOfView(std::vector<std::vector<float>> fov): fov(fov) {}

  FieldOfView::FieldOfView(
      std::vector<float> xxyyzz,
      std::vector<u_int32_t> num_points) 
  {
    size_t vecSize = (size_t) (num_points[0] * num_points[1] * num_points[2]);

    std::vector<float> step = {
      (xxyyzz[1] - xxyyzz[0]) / (num_points[0] - 1),
      (xxyyzz[3] - xxyyzz[2]) / (num_points[1] - 1),
      (xxyyzz[5] - xxyyzz[4]) / (num_points[2] - 1)
    };

    fov.resize(vecSize, std::vector<float>(3, 0.0f));

    size_t index = 0; // Linear index to access the 1D `fov` vector
    for (u_int32_t i = 0; i < num_points[0]; ++i) { // X dimension
        float x = xxyyzz[0] + i * step[0];
        for (u_int32_t j = 0; j < num_points[1]; ++j) { // Y dimension
            float y = xxyyzz[2] + j * step[1];
            for (u_int32_t k = 0; k < num_points[2]; ++k) { // Z dimension
                float z = xxyyzz[4] + k * step[2];

                // Store the point in the grid
                fov[index][0] = x;
                fov[index][1] = y;
                fov[index][2] = z;

                ++index; // Move to the next point in the 1D array
            }
        }
    }
  }

  FieldOfView::FieldOfView(const FieldOfView& other) {
    fov = other.fov;
  }

  FieldOfView::~FieldOfView() {}

  std::vector<std::vector<float>> FieldOfView::getFOV() const {
    return fov;
  }

  void FieldOfView::setFOV(const std::vector<std::vector<float>>& fovs) {
    // fov.clear();
    // fov.insert(fov.end(), fovs[0].begin(), fovs[0].end());
    // fov.insert(fov.end(), fovs[1].begin(), fovs[1].end());
  }

  void FieldOfView::display() const {

    size_t fov_size = fov.size();
    size_t fov_dim = fov[0].size();

    for(size_t i = 0; i < fov_size; i++) {
      for(size_t j = 0; j < fov_dim; j++) {
        std::cout << fov[i][j] << " ";
      }
      std::cout << std::endl;
    }
    // std::cout << "Field of view: ";
    // for (auto& fov_val : fov) {
    //   std::cout << fov_val << " ";
    // }
    // std::cout << std::endl;
  }

}  // namespace greeter