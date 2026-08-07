#include <greeter/view/ForceReport.h>
#include <algorithm>
#include <cmath>


namespace {

float magnitudeOf(const float* vector) {
  return std::sqrt(vector[0] * vector[0] + vector[1] * vector[1] +
                   vector[2] * vector[2]);
}

}  // namespace


float greeter::view::ForceEntry::getForceMagnitude() const {
  return magnitudeOf(force);
}


float greeter::view::ForceEntry::getTorqueMagnitude() const {
  return magnitudeOf(torque);
}


bool greeter::view::ForceReport::empty() const {
  return entries.empty();
}


const greeter::view::ForceEntry* greeter::view::ForceReport::findById(
    const int64_t& id) const {

  for (const auto& entry : entries) {
    if (entry.id == id) {
      return &entry;
    }
  }

  return nullptr;
}


bool greeter::view::ForceReport::getForceMagnitudeRange(
    float& minimum, float& maximum) const {

  if (entries.empty()) {
    return false;
  }

  minimum = entries[0].getForceMagnitude();
  maximum = minimum;

  for (const auto& entry : entries) {
    const float magnitude = entry.getForceMagnitude();
    minimum = std::min(minimum, magnitude);
    maximum = std::max(maximum, magnitude);
  }

  return true;
}


bool greeter::view::ForceReport::getTorqueMagnitudeRange(
    float& minimum, float& maximum) const {

  if (entries.empty()) {
    return false;
  }

  minimum = entries[0].getTorqueMagnitude();
  maximum = minimum;

  for (const auto& entry : entries) {
    const float magnitude = entry.getTorqueMagnitude();
    minimum = std::min(minimum, magnitude);
    maximum = std::max(maximum, magnitude);
  }

  return true;
}
