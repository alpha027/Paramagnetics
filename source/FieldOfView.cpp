#include <greeter/FieldOfView.h>
#include <iostream>

namespace greeter {

  FieldOfView::FieldOfView() {}

  FieldOfView::FieldOfView(std::vector<float> fov): fov(fov) {}

  FieldOfView::FieldOfView(
      std::vector<float> xxyyzz,
      std::vector<u_int32_t> num_points
  ) {

    // for (uint8_t dim=0; dim<3; dim++) {
      
    //   //fov.push_back(xxyyzz[d]);
    // }
    // fov.reserve(6 + 3);
    // fov.insert(fov.end(), xxyyzz.begin(), xxyyzz.end());
    // fov.insert(fov.end(), num_points.begin(), num_points.end());
  }

  FieldOfView::FieldOfView(const FieldOfView& other) {
    fov = other.fov;
  }

  FieldOfView::~FieldOfView() {}

  std::vector<float> FieldOfView::getFOV() const {
    return fov;
  }

  void FieldOfView::setFOV(std::vector<std::vector<float>> fovs) {
    fov.clear();
    fov.insert(fov.end(), fovs[0].begin(), fovs[0].end());
    fov.insert(fov.end(), fovs[1].begin(), fovs[1].end());
  }

  void FieldOfView::display() const {
    std::cout << "Field of view: ";
    for (auto& fov_val : fov) {
      std::cout << fov_val << " ";
    }
    std::cout << std::endl;
  }

  std::vector<float> FieldOfView::getDimensions() const {
    return std::vector<float>(fov.begin(), fov.begin() + 3);
  }

  std::vector<float> FieldOfView::getPosition() const {
    return std::vector<float>(fov.begin() + 3, fov.begin() + 6);
  }

}  // namespace greeter