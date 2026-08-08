#include <greeter/service/SimulationService.h>

#include <greeter/MagnetCollection.h>
#include <greeter/MagnetGeometryFactory.h>
#include <greeter/io/ForceIO.h>
#include <greeter/io/MagnetIO.h>
#include <greeter/io/SceneIO.h>
#include <greeter/TargetMesh.h>

#include <fstream>
#include <stdexcept>


size_t greeter::service::FieldRequest::getSampleCount() const {
  return (size_t) counts[0] * (size_t) counts[1] * (size_t) counts[2];
}


/*
  Everything that touches the simulation lives here, on the far side of the
  pimpl, so that none of it reaches the header a viewer includes.
*/
struct greeter::service::SimulationService::Impl {

  nlohmann::json input;
  std::string source;

  greeter::Scene scene;

  bool loaded = false;

  void load(const nlohmann::json& data, const std::string& from) {

    // Reading the magnets and the ids they answer to in one pass is what
    // keeps a magnet an arrangement generated nameable in the force results.
    greeter::Scene read = greeter::SceneIO::read(data);

    input = data;
    source = from;
    scene = std::move(read);
    loaded = true;
  }

  void requireLoaded() const {
    if (!loaded) {
      throw std::logic_error("No scene has been loaded");
    }
  }
};


greeter::service::SimulationService::SimulationService():
  impl(std::make_unique<Impl>()) {}

greeter::service::SimulationService::~SimulationService() = default;

greeter::service::SimulationService::SimulationService(
  SimulationService&& other) noexcept = default;

greeter::service::SimulationService&
greeter::service::SimulationService::operator=(
  SimulationService&& other) noexcept = default;


void greeter::service::SimulationService::loadFile(const std::string& path) {

  std::ifstream file(path);

  if (!file.is_open()) {
    throw std::invalid_argument("Could not open " + path);
  }

  impl->load(nlohmann::json::parse(file), path);
}


void greeter::service::SimulationService::loadJSON(
    const nlohmann::json& data, const std::string& source) {
  impl->load(data, source);
}


bool greeter::service::SimulationService::isLoaded() const {
  return impl->loaded;
}


std::string greeter::service::SimulationService::getSource() const {
  return impl->source;
}


const nlohmann::json& greeter::service::SimulationService::getInput() const {
  impl->requireLoaded();
  return impl->input;
}


greeter::view::SceneSnapshot greeter::service::SimulationService::getScene() const {

  impl->requireLoaded();

  const greeter::MagnetCollection& collection = impl->scene.collection;

  greeter::view::SceneSnapshot snapshot;

  snapshot.source = impl->source;

  // Which arrangement generated which magnet, so that a magnet can name its
  // own without the viewer searching for it.
  std::vector<int64_t> owner(collection.get_num_magnets(), 0);

  for (const auto& arrangement : impl->scene.arrangements) {
    for (const auto& member : arrangement.members) {
      if (member < owner.size()) {
        owner[member] = arrangement.id;
      }
    }
  }

  const greeter::MagnetGeometryFactory& shapes =
    greeter::MagnetGeometryFactory::getInstance();

  for (uint32_t index = 0; index < collection.get_num_magnets(); index++) {

    const std::vector<float> parameters = collection.getMagnetParameters(index);

    greeter::view::MagnetView magnet;

    magnet.index = index;
    magnet.id = index < impl->scene.magnet_ids.size()
                  ? impl->scene.magnet_ids[index] : (int64_t) index;
    magnet.arrangement_id = owner[index];

    const uint16_t type = collection.getMagnetTypeID(index);

    magnet.shape = shapes.describeShape(type, parameters.data());

    // Whether the last three numbers below are a polarization or a moment,
    // which is not something a viewer should have to work out from a name.
    magnet.moment_kind = shapes.getMomentKind(type);

    for (size_t axis = 0; axis < 3; axis++) {
      magnet.position[axis] = parameters[axis];
    }

    for (size_t i = 0; i < 4; i++) {
      magnet.orientation[i] = parameters[3 + i];
    }

    // The polarization, or the moment, is the last three of the parameters,
    // whatever the shape put in front of it.
    for (size_t axis = 0; axis < 3; axis++) {
      magnet.magnetization[axis] = parameters[parameters.size() - 3 + axis];
    }

    snapshot.magnets.push_back(magnet);
  }

  for (const auto& arrangement : impl->scene.arrangements) {

    greeter::view::ArrangementView view;

    view.id = arrangement.id;
    view.members = arrangement.members;

    // The type is not carried through the scene, so it is read back out of
    // the input the arrangement came from.
    if (impl->input.contains("arrangements")) {
      for (const auto& entry : impl->input["arrangements"]) {
        if (entry.contains("id") && entry["id"].is_number_integer() &&
            entry["id"].get<int64_t>() == arrangement.id &&
            entry.contains("type") && entry["type"].is_string()) {
          view.type = entry["type"].get<std::string>();
          break;
        }
      }
    }

    snapshot.arrangements.push_back(view);
  }

  return snapshot;
}


bool greeter::service::SimulationService::hasFieldSection() const {
  return impl->loaded && impl->input.contains("field_of_view");
}


bool greeter::service::SimulationService::hasForceSection() const {
  return impl->loaded && greeter::ForceIO::hasForceSection(impl->input);
}


bool greeter::service::SimulationService::getFieldRequest(
    FieldRequest& request) const {

  if (!hasFieldSection()) {
    return false;
  }

  const greeter::FieldOfView fov =
    greeter::MagnetIO::readFieldOfView(impl->input["field_of_view"]);

  const std::vector<float> bounds = fov.getBounds();
  const std::vector<uint32_t> counts = fov.getCounts();

  // Which of the four quantities the file asks for, B unless it says.
  if (impl->input["field_of_view"].contains("quantity")) {

    const nlohmann::json& given = impl->input["field_of_view"]["quantity"];

    if (!given.is_string()) {
      throw std::invalid_argument(
        "The \"quantity\" of a field of view is \"B\", \"H\", \"J\" or \"M\"");
    }

    const std::string name = given.get<std::string>();

    bool known = false;

    for (const auto& kind : {greeter::view::FieldKind::B, greeter::view::FieldKind::H,
                             greeter::view::FieldKind::J, greeter::view::FieldKind::M}) {
      if (greeter::view::getName(kind) == name) {
        request.kind = kind;
        known = true;
        break;
      }
    }

    if (!known) {
      throw std::invalid_argument(
        "A field of view can be asked for \"B\", \"H\", \"J\" or \"M\", not \"" +
        name + "\"");
    }
  }

  for (size_t i = 0; i < 6; i++) {
    request.bounds[i] = bounds[i];
  }

  for (size_t i = 0; i < 3; i++) {
    request.counts[i] = counts[i];
  }

  return true;
}


namespace {

/*
  Where sample `index` of a request sits, with x slowest and z fastest, the
  order FieldOfView lays a grid out in.

  The points of a chunk are worked out one at a time rather than the whole
  grid being built first: a grid of a hundred points a side is a million
  points, which is a hundred megabytes of coordinates nobody needs at once.
*/
void pointAt(const greeter::service::FieldRequest& request, const size_t& index,
             float* xyz) {

  const size_t nz = request.counts[2];
  const size_t ny = request.counts[1];

  const size_t k = index % nz;
  const size_t j = (index / nz) % ny;
  const size_t i = index / (nz * ny);

  const size_t at[3] = {i, j, k};

  for (size_t axis = 0; axis < 3; axis++) {

    if (request.counts[axis] < 2) {
      // A single sample stands at the near face, as linspace does.
      xyz[axis] = request.bounds[2 * axis];
      continue;
    }

    const float step = (request.bounds[2 * axis + 1] - request.bounds[2 * axis]) /
                       (float) (request.counts[axis] - 1);

    xyz[axis] = request.bounds[2 * axis] + (float) at[axis] * step;
  }
}

}  // namespace


greeter::view::FieldGrid greeter::service::SimulationService::simulateField(
    const FieldRequest& request, ProgressSink* progress,
    const RunOptions& options) const {

  impl->requireLoaded();

  for (size_t axis = 0; axis < 3; axis++) {
    if (request.counts[axis] < 1) {
      throw std::invalid_argument(
        "A field of view has to be sampled at least once along each axis");
    }
    if (request.bounds[2 * axis + 1] < request.bounds[2 * axis]) {
      throw std::invalid_argument(
        "The maximum of a field of view lies below its minimum");
    }
  }

  const size_t total = request.getSampleCount();

  greeter::view::FieldGrid grid;

  grid.grid = true;
  grid.kind = request.kind;

  for (size_t i = 0; i < 6; i++) {
    grid.bounds[i] = request.bounds[i];
  }

  for (size_t i = 0; i < 3; i++) {
    grid.counts[i] = request.counts[i];
  }

  grid.field.reserve(3 * total);

  const size_t chunk = options.chunk < 1 ? 1 : options.chunk;

  // The magnets are packed into device views once, and only the observation
  // points change from chunk to chunk.
  std::unique_ptr<greeter::MagneticFieldSimulator> simulator =
    impl->scene.collection.createSimulator();

  // B needs nothing else. The other three need to know, at every point, what
  // material is there.
  const bool needs_polarization = request.kind != greeter::view::FieldKind::B;

  simulator->setComputePolarization(needs_polarization);

  std::vector<std::vector<float>> points;

  for (size_t start = 0; start < total; start += chunk) {

    const size_t count = std::min(chunk, total - start);

    points.assign(count, std::vector<float>(3, 0.0f));

    for (size_t i = 0; i < count; i++) {
      pointAt(request, start + i, points[i].data());
    }

    simulator->fillObservationPoints(points);
    simulator->simulate(options.verbose);

    std::vector<float> chunk_field = simulator->getMagneticFieldsFlat();

    if (needs_polarization) {

      const std::vector<float> polarization = simulator->getPolarizationsFlat();

      for (size_t i = 0; i < chunk_field.size(); i++) {

        switch (request.kind) {

          case greeter::view::FieldKind::J:
            chunk_field[i] = polarization[i];
            break;

          // H = (B - J) / mu0, which is B / mu0 outside every magnet and the
          // demagnetizing field inside one.
          case greeter::view::FieldKind::H:
            chunk_field[i] = (chunk_field[i] - polarization[i]) / greeter::MU0;
            break;

          case greeter::view::FieldKind::M:
            chunk_field[i] = polarization[i] / greeter::MU0;
            break;

          case greeter::view::FieldKind::B:
            break;
        }
      }
    }

    grid.field.insert(grid.field.end(), chunk_field.begin(), chunk_field.end());

    if (progress != nullptr && !progress->onProgress(start + count, total)) {
      // Stopped. An empty grid is not a half drawn one.
      return greeter::view::FieldGrid();
    }
  }

  return grid;
}


greeter::view::ForceReport greeter::service::SimulationService::simulateForces(
    ProgressSink* progress, const RunOptions& options) const {

  impl->requireLoaded();

  if (!hasForceSection()) {
    throw std::invalid_argument(
      "This file asks for no forces, it has no \"force\" section");
  }

  if (progress != nullptr && !progress->onProgress(0, 1)) {
    return greeter::view::ForceReport();
  }

  const greeter::ForceConfig config = greeter::ForceIO::read(
    impl->input, impl->scene.magnet_ids, impl->scene.arrangements);

  const std::vector<greeter::ForceResult> results =
    impl->scene.collection.computeForces(config, options.verbose);

  greeter::view::ForceReport report;

  for (const auto& result : results) {

    greeter::view::ForceEntry entry;

    entry.index = result.target_index;
    entry.id = result.target_index < impl->scene.magnet_ids.size()
                 ? impl->scene.magnet_ids[result.target_index]
                 : (int64_t) result.target_index;
    entry.cells = result.cells;

    for (size_t axis = 0; axis < 3; axis++) {
      entry.force[axis] = result.force[axis];
      entry.torque[axis] = result.torque[axis];
      entry.pivot[axis] = result.pivot[axis];
    }

    report.entries.push_back(entry);
  }

  if (progress != nullptr) {
    progress->onProgress(1, 1);
  }

  return report;
}


greeter::view::Snapshot greeter::service::SimulationService::run(
    ProgressSink* progress, const RunOptions& options) const {

  impl->requireLoaded();

  greeter::view::Snapshot snapshot;

  snapshot.scene = getScene();

  FieldRequest request;

  if (getFieldRequest(request)) {
    snapshot.field = simulateField(request, progress, options);
  }

  if (hasForceSection()) {
    snapshot.forces = simulateForces(progress, options);
  }

  return snapshot;
}
