/*
  The boundary guard.

  A viewer is meant to be separable from the simulation, and saying so in a
  comment is worth nothing: the headers below are the ones a viewer includes,
  and this file is compiled with only the library's own include directory and
  nlohmann/json available. Kokkos is not on the include path.

  So the day somebody adds an #include of MagnetCollection.h, or of anything
  that reaches KokkosDefines.h, to one of these headers, the build stops here
  rather than quietly tying every viewer to Kokkos. That is the whole job of
  this file; there is nothing to run.

  See test/CMakeLists.txt for the target that compiles it.
*/

#include <greeter/view/FieldGrid.h>
#include <greeter/view/ForceReport.h>
#include <greeter/view/SceneSnapshot.h>
#include <greeter/view/ShapeDescriptor.h>
#include <greeter/view/ShapeMesh.h>
#include <greeter/view/Snapshot.h>
#include <greeter/view/SnapshotIO.h>

#include <greeter/service/SimulationService.h>

// The renderer turns an orientation into a matrix, so this one has to stay
// reachable from the far side of the boundary too.
#include <greeter/Quaternion.h>


namespace {

/*
  Named so that a linker error, should this ever be linked into something,
  says what it was for. Nothing calls it.
*/
[[maybe_unused]]
greeter::view::Snapshot theViewLayerCompilesWithoutKokkos() {

  greeter::view::Snapshot snapshot;

  greeter::view::MagnetView magnet;
  magnet.shape.kind = greeter::view::ShapeKind::Box;
  magnet.shape.parameters = {1.0f, 1.0f, 1.0f};

  snapshot.scene.magnets.push_back(magnet);

  const greeter::view::ShapeMesh mesh =
    greeter::view::buildMesh(magnet.shape);

  (void) mesh.getTriangleCount();

  greeter::service::FieldRequest request;
  (void) request.getSampleCount();

  return snapshot;
}

}  // namespace
