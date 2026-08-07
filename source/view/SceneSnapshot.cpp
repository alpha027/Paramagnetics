#include <greeter/view/SceneSnapshot.h>
#include <algorithm>


bool greeter::view::SceneSnapshot::empty() const {
  return magnets.empty();
}


bool greeter::view::SceneSnapshot::getBounds(float* minimum, float* maximum) const {

  if (magnets.empty()) {
    return false;
  }

  bool started = false;

  for (const auto& magnet : magnets) {

    const std::vector<float> half = magnet.shape.getLocalHalfExtent();

    for (size_t axis = 0; axis < 3; axis++) {

      const float low = magnet.position[axis] - half[axis];
      const float high = magnet.position[axis] + half[axis];

      if (!started) {
        minimum[axis] = low;
        maximum[axis] = high;
      } else {
        minimum[axis] = std::min(minimum[axis], low);
        maximum[axis] = std::max(maximum[axis], high);
      }
    }

    started = true;
  }

  return true;
}


const greeter::view::MagnetView* greeter::view::SceneSnapshot::findById(
    const int64_t& id) const {

  for (const auto& magnet : magnets) {
    if (magnet.id == id) {
      return &magnet;
    }
  }

  return nullptr;
}


const greeter::view::ArrangementView*
greeter::view::SceneSnapshot::findArrangementById(const int64_t& id) const {

  for (const auto& arrangement : arrangements) {
    if (arrangement.id == id) {
      return &arrangement;
    }
  }

  return nullptr;
}
