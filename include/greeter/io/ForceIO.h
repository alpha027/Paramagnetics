#ifndef FORCE_IO_H
#define FORCE_IO_H

#include <greeter/ForceConfig.h>
#include <greeter/io/ArrangementIO.h>
#include <nlohmann/json.hpp>
#include <cstdint>
#include <vector>


namespace greeter {

/*
  Reader of the optional "force" section of the JSON input file:

    "force": {
      "targets": [2],          // magnet ids, {"arrangement": id}, or "all"
      "sources": [1],          // optional, defaults to every other magnet
      "meshing": 1000,         // int, [n1, n2, n3], or one entry per target
      "eps": 1e-4,             // optional finite difference step [m]
      "pivot": "centroid"      // "centroid" or [x, y, z]
    }

  The magnet "id" fields are resolved into collection indices with the ids of
  the scene, which cover the magnets the file lists as well as the ones its
  arrangements generated. {"arrangement": id} names every member of one
  arrangement at once, since those are not written out one by one.
*/
class ForceIO {

    public:

        ForceIO();
        ~ForceIO();

        static bool hasForceSection(const nlohmann::json& data);

        // Magnet ids in the order of the "magnets" array of the input file.
        // This sees only the magnets the file lists, so a file that also has
        // arrangements has to be read through SceneIO.
        static std::vector<int64_t> readMagnetIds(const nlohmann::json& data);

        static greeter::MeshingSpec readMeshing(const nlohmann::json& meshing);

        static greeter::ForceConfig read(
            const nlohmann::json& data,
            const std::vector<int64_t>& magnet_ids,
            const std::vector<greeter::ArrangementMembers>& arrangements);

        // For a file that lists every magnet one by one, where the ids of the
        // scene are the ids of the "magnets" array.
        static greeter::ForceConfig read(const nlohmann::json& data);
};

}  // namespace greeter

#endif // FORCE_IO_H
