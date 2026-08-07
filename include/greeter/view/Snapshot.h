#ifndef VIEW_SNAPSHOT_H
#define VIEW_SNAPSHOT_H

#include <greeter/view/FieldGrid.h>
#include <greeter/view/ForceReport.h>
#include <greeter/view/SceneSnapshot.h>


namespace greeter {
namespace view {

/*
  Everything a viewer needs about one run: the magnets, the field if one was
  simulated, and the forces if they were.

  Either result may be missing. A scene on its own is worth opening, it is
  what the magnets look like before anything is computed.
*/
struct Snapshot {

  SceneSnapshot scene;
  FieldGrid field;
  ForceReport forces;

  bool hasField() const { return !field.empty(); }
  bool hasForces() const { return !forces.empty(); }
};

}  // namespace view
}  // namespace greeter

#endif  // VIEW_SNAPSHOT_H
