#ifndef VIEW_SCENE_SNAPSHOT_H
#define VIEW_SCENE_SNAPSHOT_H

#include <greeter/view/ShapeDescriptor.h>
#include <cstdint>
#include <string>
#include <vector>


namespace greeter {
namespace view {

/* One magnet, as much of it as a viewer has any business knowing. */
struct MagnetView {

  /* The id the magnet answers to in the input file and in the force results. */
  int64_t id = 0;

  /* Where it sits in the collection, which is what a force result indexes. */
  uint32_t index = 0;

  /* The arrangement that generated it, or 0 when it was listed on its own. */
  int64_t arrangement_id = 0;

  ShapeDescriptor shape;

  float position[3] = {0.0f, 0.0f, 0.0f};
  float orientation[4] = {1.0f, 0.0f, 0.0f, 0.0f};  // w, x, y, z

  /* The magnetic polarization J [T], in the frame of the magnet. */
  float magnetization[3] = {0.0f, 0.0f, 0.0f};
};

/* An arrangement and the magnets it generated, so a viewer can group them. */
struct ArrangementView {
  int64_t id = 0;
  std::string type;
  std::vector<uint32_t> members;  // indices into SceneSnapshot::magnets
};

/*
  The magnets of a scene as plain data.

  This is the contract between the simulation and anything that draws it.
  Nothing here knows about Magnet, MagnetCollection or Kokkos, which is what
  lets a viewer link against it without linking against any of them, and what
  lets a snapshot be written to a file and opened somewhere else entirely.
*/
struct SceneSnapshot {

  std::vector<MagnetView> magnets;
  std::vector<ArrangementView> arrangements;

  /* Where the scene came from, for a window title. Not needed to draw it. */
  std::string source;

  bool empty() const;

  /*
    The box the magnets occupy, which is what a camera frames on first
    opening a scene. The extent of a magnet is taken in its own frame and so
    is an over-estimate for a turned one, which is the safe way round for a
    camera. Returns false, and leaves the arguments alone, for an empty scene.
  */
  bool getBounds(float* minimum, float* maximum) const;

  /* The magnet answering to an id, or nullptr. */
  const MagnetView* findById(const int64_t& id) const;

  const ArrangementView* findArrangementById(const int64_t& id) const;
};

}  // namespace view
}  // namespace greeter

#endif  // VIEW_SCENE_SNAPSHOT_H
