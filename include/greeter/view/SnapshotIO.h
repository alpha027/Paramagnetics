#ifndef VIEW_SNAPSHOT_IO_H
#define VIEW_SNAPSHOT_IO_H

#include <greeter/view/Snapshot.h>
#include <nlohmann/json.hpp>
#include <iosfwd>
#include <string>


namespace greeter {
namespace view {

/*
  Reads and writes a snapshot.

  This is what lets the viewer and the simulator be separate programs. A run
  on a machine with no screen writes a snapshot, and a viewer somewhere else
  opens it without the simulator, without Kokkos and without ever parsing the
  input file.

  Two forms:

    .json      readable, and hopeless for a large field. A grid of a hundred
               points a side is three million numbers, which is some hundreds
               of megabytes of text.

    .pmsnap    a JSON header describing the scene, the forces and the shape of
               the field, followed by the samples as raw 32 bit floats. The
               floats are written in the byte order of the machine that wrote
               them, which every machine this runs on shares.

  write() and read() pick by the extension of the path, .json for the first
  and the second otherwise.
*/
class SnapshotIO {

  public:

    SnapshotIO();
    ~SnapshotIO();

    /* The whole snapshot as JSON, field samples included. */
    static nlohmann::json toJSON(const Snapshot& snapshot);

    static Snapshot fromJSON(const nlohmann::json& data);

    static void write(const Snapshot& snapshot, const std::string& path);

    static Snapshot read(const std::string& path);

    static void writeBinary(const Snapshot& snapshot, std::ostream& stream);

    static Snapshot readBinary(std::istream& stream);

    /* What write() would produce for this path, ".pmsnap" or ".json". */
    static bool isJSONPath(const std::string& path);
};

}  // namespace view
}  // namespace greeter

#endif  // VIEW_SNAPSHOT_IO_H
