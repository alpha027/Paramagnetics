#include <greeter/view/SnapshotIO.h>
#include <algorithm>
#include <fstream>
#include <istream>
#include <ostream>
#include <stdexcept>


greeter::view::SnapshotIO::SnapshotIO() {}
greeter::view::SnapshotIO::~SnapshotIO() {}


namespace {

const char SNAPSHOT_MAGIC[8] = {'P', 'M', 'S', 'N', 'A', 'P', '\0', '\0'};
const uint32_t SNAPSHOT_VERSION = 1;

nlohmann::json arrayOf(const float* values, const size_t& count) {
  nlohmann::json array = nlohmann::json::array();
  for (size_t i = 0; i < count; i++) {
    array.push_back(values[i]);
  }
  return array;
}

void readArrayInto(const nlohmann::json& data, const std::string& key,
                   float* values, const size_t& count, const std::string& what) {

  if (!data.contains(key) || !data[key].is_array() ||
      data[key].size() != count) {
    throw std::invalid_argument(
      "A " + what + " needs \"" + key + "\" as " + std::to_string(count) +
      " numbers");
  }

  for (size_t i = 0; i < count; i++) {
    values[i] = data[key][i].get<float>();
  }
}

nlohmann::json shapeToJSON(const greeter::view::ShapeDescriptor& shape) {
  return {
    {"kind", greeter::view::getName(shape.kind)},
    {"type", shape.type_name},
    {"parameters", shape.parameters}
  };
}

greeter::view::ShapeKind kindFromString(const std::string& name) {

  const greeter::view::ShapeKind kinds[] = {
    greeter::view::ShapeKind::Unknown,
    greeter::view::ShapeKind::Box,
    greeter::view::ShapeKind::Cylinder,
    greeter::view::ShapeKind::Sphere,
    greeter::view::ShapeKind::Tetrahedron,
    greeter::view::ShapeKind::Mesh
  };

  for (const auto& kind : kinds) {
    if (greeter::view::getName(kind) == name) {
      return kind;
    }
  }

  // A viewer that meets a shape from a newer writer draws a marker rather
  // than refusing to open the file.
  return greeter::view::ShapeKind::Unknown;
}

greeter::view::ShapeDescriptor shapeFromJSON(const nlohmann::json& data) {

  greeter::view::ShapeDescriptor shape;

  if (!data.is_object()) {
    return shape;
  }

  if (data.contains("kind") && data["kind"].is_string()) {
    shape.kind = kindFromString(data["kind"].get<std::string>());
  }

  if (data.contains("type") && data["type"].is_string()) {
    shape.type_name = data["type"].get<std::string>();
  }

  if (data.contains("parameters") && data["parameters"].is_array()) {
    shape.parameters = data["parameters"].get<std::vector<float>>();
  }

  return shape;
}

nlohmann::json sceneToJSON(const greeter::view::SceneSnapshot& scene) {

  nlohmann::json magnets = nlohmann::json::array();

  for (const auto& magnet : scene.magnets) {
    magnets.push_back({
      {"id", magnet.id},
      {"index", magnet.index},
      {"arrangement", magnet.arrangement_id},
      {"shape", shapeToJSON(magnet.shape)},
      {"position", arrayOf(magnet.position, 3)},
      {"orientation", arrayOf(magnet.orientation, 4)},
      {"magnetization", arrayOf(magnet.magnetization, 3)}
    });
  }

  nlohmann::json arrangements = nlohmann::json::array();

  for (const auto& arrangement : scene.arrangements) {
    arrangements.push_back({
      {"id", arrangement.id},
      {"type", arrangement.type},
      {"members", arrangement.members}
    });
  }

  return {
    {"source", scene.source},
    {"magnets", magnets},
    {"arrangements", arrangements}
  };
}

greeter::view::SceneSnapshot sceneFromJSON(const nlohmann::json& data) {

  greeter::view::SceneSnapshot scene;

  if (!data.is_object()) {
    return scene;
  }

  if (data.contains("source") && data["source"].is_string()) {
    scene.source = data["source"].get<std::string>();
  }

  if (data.contains("magnets") && data["magnets"].is_array()) {
    for (const auto& entry : data["magnets"]) {

      greeter::view::MagnetView magnet;

      magnet.id = entry.value("id", (int64_t) 0);
      magnet.index = entry.value("index", (uint32_t) 0);
      magnet.arrangement_id = entry.value("arrangement", (int64_t) 0);

      if (entry.contains("shape")) {
        magnet.shape = shapeFromJSON(entry["shape"]);
      }

      readArrayInto(entry, "position", magnet.position, 3, "magnet");
      readArrayInto(entry, "orientation", magnet.orientation, 4, "magnet");
      readArrayInto(entry, "magnetization", magnet.magnetization, 3, "magnet");

      scene.magnets.push_back(magnet);
    }
  }

  if (data.contains("arrangements") && data["arrangements"].is_array()) {
    for (const auto& entry : data["arrangements"]) {

      greeter::view::ArrangementView arrangement;

      arrangement.id = entry.value("id", (int64_t) 0);
      arrangement.type = entry.value("type", std::string());

      if (entry.contains("members") && entry["members"].is_array()) {
        arrangement.members = entry["members"].get<std::vector<uint32_t>>();
      }

      scene.arrangements.push_back(arrangement);
    }
  }

  return scene;
}

nlohmann::json forcesToJSON(const greeter::view::ForceReport& forces) {

  nlohmann::json entries = nlohmann::json::array();

  for (const auto& entry : forces.entries) {
    entries.push_back({
      {"id", entry.id},
      {"index", entry.index},
      {"force", arrayOf(entry.force, 3)},
      {"torque", arrayOf(entry.torque, 3)},
      {"pivot", arrayOf(entry.pivot, 3)},
      {"cells", entry.cells}
    });
  }

  return entries;
}

greeter::view::ForceReport forcesFromJSON(const nlohmann::json& data) {

  greeter::view::ForceReport forces;

  if (!data.is_array()) {
    return forces;
  }

  for (const auto& item : data) {

    greeter::view::ForceEntry entry;

    entry.id = item.value("id", (int64_t) 0);
    entry.index = item.value("index", (uint32_t) 0);
    entry.cells = item.value("cells", (uint32_t) 0);

    readArrayInto(item, "force", entry.force, 3, "force result");
    readArrayInto(item, "torque", entry.torque, 3, "force result");
    readArrayInto(item, "pivot", entry.pivot, 3, "force result");

    forces.entries.push_back(entry);
  }

  return forces;
}

/* The shape of a field, without the samples themselves. */
nlohmann::json fieldHeaderToJSON(const greeter::view::FieldGrid& field) {
  return {
    {"grid", field.grid},
    {"bounds", arrayOf(field.bounds, 6)},
    {"counts", {field.counts[0], field.counts[1], field.counts[2]}},
    {"samples", field.size()}
  };
}

void fieldHeaderFromJSON(const nlohmann::json& data,
                         greeter::view::FieldGrid& field) {

  if (!data.is_object()) {
    return;
  }

  field.grid = data.value("grid", false);

  if (data.contains("bounds") && data["bounds"].is_array() &&
      data["bounds"].size() == 6) {
    for (size_t i = 0; i < 6; i++) {
      field.bounds[i] = data["bounds"][i].get<float>();
    }
  }

  if (data.contains("counts") && data["counts"].is_array() &&
      data["counts"].size() == 3) {
    for (size_t i = 0; i < 3; i++) {
      field.counts[i] = data["counts"][i].get<uint32_t>();
    }
  }
}

template <typename T>
void writeValue(std::ostream& stream, const T& value) {
  stream.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

template <typename T>
T readValue(std::istream& stream) {
  T value = T();
  stream.read(reinterpret_cast<char*>(&value), sizeof(T));
  if (!stream) {
    throw std::invalid_argument("Snapshot ends in the middle of a value");
  }
  return value;
}

void writeFloats(std::ostream& stream, const std::vector<float>& values) {

  writeValue<uint64_t>(stream, (uint64_t) values.size());

  if (!values.empty()) {
    stream.write(reinterpret_cast<const char*>(values.data()),
                 (std::streamsize) (values.size() * sizeof(float)));
  }
}

std::vector<float> readFloats(std::istream& stream) {

  const uint64_t count = readValue<uint64_t>(stream);

  std::vector<float> values((size_t) count, 0.0f);

  if (count > 0) {
    stream.read(reinterpret_cast<char*>(values.data()),
                (std::streamsize) (values.size() * sizeof(float)));
    if (!stream) {
      throw std::invalid_argument("Snapshot ends in the middle of its samples");
    }
  }

  return values;
}

}  // namespace


bool greeter::view::SnapshotIO::isJSONPath(const std::string& path) {

  if (path.size() < 5) {
    return false;
  }

  std::string tail = path.substr(path.size() - 5);

  std::transform(tail.begin(), tail.end(), tail.begin(),
                 [](unsigned char c) { return (char) std::tolower(c); });

  return tail == ".json";
}


nlohmann::json greeter::view::SnapshotIO::toJSON(const Snapshot& snapshot) {

  nlohmann::json field = fieldHeaderToJSON(snapshot.field);

  field["field"] = snapshot.field.field;
  field["points"] = snapshot.field.points;

  return {
    {"format", "paramagnetics-snapshot"},
    {"version", SNAPSHOT_VERSION},
    {"scene", sceneToJSON(snapshot.scene)},
    {"field", field},
    {"forces", forcesToJSON(snapshot.forces)}
  };
}


greeter::view::Snapshot greeter::view::SnapshotIO::fromJSON(
    const nlohmann::json& data) {

  if (!data.is_object() || !data.contains("scene")) {
    throw std::invalid_argument(
      "This is not a snapshot, it has no \"scene\"");
  }

  Snapshot snapshot;

  snapshot.scene = sceneFromJSON(data["scene"]);

  if (data.contains("field")) {

    fieldHeaderFromJSON(data["field"], snapshot.field);

    if (data["field"].contains("field") && data["field"]["field"].is_array()) {
      snapshot.field.field = data["field"]["field"].get<std::vector<float>>();
    }

    if (data["field"].contains("points") && data["field"]["points"].is_array()) {
      snapshot.field.points = data["field"]["points"].get<std::vector<float>>();
    }
  }

  if (data.contains("forces")) {
    snapshot.forces = forcesFromJSON(data["forces"]);
  }

  return snapshot;
}


void greeter::view::SnapshotIO::writeBinary(
    const Snapshot& snapshot, std::ostream& stream) {

  const nlohmann::json header = {
    {"format", "paramagnetics-snapshot"},
    {"version", SNAPSHOT_VERSION},
    {"scene", sceneToJSON(snapshot.scene)},
    {"field", fieldHeaderToJSON(snapshot.field)},
    {"forces", forcesToJSON(snapshot.forces)}
  };

  const std::string text = header.dump();

  stream.write(SNAPSHOT_MAGIC, sizeof(SNAPSHOT_MAGIC));
  writeValue<uint32_t>(stream, SNAPSHOT_VERSION);
  writeValue<uint64_t>(stream, (uint64_t) text.size());
  stream.write(text.data(), (std::streamsize) text.size());

  writeFloats(stream, snapshot.field.field);
  writeFloats(stream, snapshot.field.points);
}


greeter::view::Snapshot greeter::view::SnapshotIO::readBinary(
    std::istream& stream) {

  char magic[sizeof(SNAPSHOT_MAGIC)];

  stream.read(magic, sizeof(magic));

  if (!stream || std::string(magic, sizeof(magic)) !=
                 std::string(SNAPSHOT_MAGIC, sizeof(SNAPSHOT_MAGIC))) {
    throw std::invalid_argument("This is not a snapshot file");
  }

  const uint32_t version = readValue<uint32_t>(stream);

  if (version > SNAPSHOT_VERSION) {
    throw std::invalid_argument(
      "This snapshot was written by a newer version of ParaMagneticS, "
      "version " + std::to_string(version));
  }

  const uint64_t length = readValue<uint64_t>(stream);

  std::string text((size_t) length, '\0');

  if (length > 0) {
    stream.read(&text[0], (std::streamsize) length);
    if (!stream) {
      throw std::invalid_argument("Snapshot ends in the middle of its header");
    }
  }

  const nlohmann::json header = nlohmann::json::parse(text);

  Snapshot snapshot;

  snapshot.scene = sceneFromJSON(header.value("scene", nlohmann::json::object()));

  if (header.contains("field")) {
    fieldHeaderFromJSON(header["field"], snapshot.field);
  }

  if (header.contains("forces")) {
    snapshot.forces = forcesFromJSON(header["forces"]);
  }

  snapshot.field.field = readFloats(stream);
  snapshot.field.points = readFloats(stream);

  return snapshot;
}


void greeter::view::SnapshotIO::write(
    const Snapshot& snapshot, const std::string& path) {

  if (isJSONPath(path)) {

    std::ofstream file(path);

    if (!file.is_open()) {
      throw std::invalid_argument("Could not write the snapshot to " + path);
    }

    file << toJSON(snapshot).dump(2) << std::endl;

    return;
  }

  std::ofstream file(path, std::ios::binary);

  if (!file.is_open()) {
    throw std::invalid_argument("Could not write the snapshot to " + path);
  }

  writeBinary(snapshot, file);
}


greeter::view::Snapshot greeter::view::SnapshotIO::read(const std::string& path) {

  if (isJSONPath(path)) {

    std::ifstream file(path);

    if (!file.is_open()) {
      throw std::invalid_argument("Could not open the snapshot " + path);
    }

    return fromJSON(nlohmann::json::parse(file));
  }

  std::ifstream file(path, std::ios::binary);

  if (!file.is_open()) {
    throw std::invalid_argument("Could not open the snapshot " + path);
  }

  return readBinary(file);
}
