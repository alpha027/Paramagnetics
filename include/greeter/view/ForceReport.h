#ifndef VIEW_FORCE_REPORT_H
#define VIEW_FORCE_REPORT_H

#include <cstdint>
#include <vector>


namespace greeter {
namespace view {

/* The force and torque on one magnet. */
struct ForceEntry {

  /* The id the magnet answers to, not its place in the collection. */
  int64_t id = 0;

  uint32_t index = 0;

  float force[3] = {0.0f, 0.0f, 0.0f};    // [N]
  float torque[3] = {0.0f, 0.0f, 0.0f};   // [N*m]

  /* The point the torque refers to, and where an arrow is drawn from. */
  float pivot[3] = {0.0f, 0.0f, 0.0f};

  /* How finely the magnet was meshed, which is what the numbers rest on. */
  uint32_t cells = 0;

  float getForceMagnitude() const;
  float getTorqueMagnitude() const;
};

/*
  The result of a force simulation, keyed by magnet id.

  Forces span a wide range within one scene, so a viewer scaling arrows by the
  largest of them draws one arrow and thirty stubs. getForceMagnitudeRange is
  there to be scaled logarithmically, or clipped, rather than used straight.
*/
struct ForceReport {

  std::vector<ForceEntry> entries;

  bool empty() const;

  const ForceEntry* findById(const int64_t& id) const;

  bool getForceMagnitudeRange(float& minimum, float& maximum) const;
  bool getTorqueMagnitudeRange(float& minimum, float& maximum) const;
};

}  // namespace view
}  // namespace greeter

#endif  // VIEW_FORCE_REPORT_H
