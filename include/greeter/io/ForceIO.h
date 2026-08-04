#ifndef FORCE_IO_H
#define FORCE_IO_H

#include <greeter/ForceConfig.h>
#include <nlohmann/json.hpp>
#include <vector>


namespace greeter {

/*
  Reader of the optional "force" section of the JSON input file:

    "force": {
      "targets": [2],          // magnet ids, or "all"
      "sources": [1],          // optional, defaults to every other magnet
      "meshing": 1000,         // int, [n1, n2, n3], or one entry per target
      "eps": 1e-4,             // optional finite difference step [m]
      "pivot": "centroid"      // "centroid" or [x, y, z]
    }

  The magnet "id" fields are resolved into collection indices with the ids read
  from the "magnets" array, in the order in which they appear there.
*/
class ForceIO {

    public:

        ForceIO();
        ~ForceIO();

        static bool hasForceSection(const nlohmann::json& data);

        // Magnet ids in the order of the "magnets" array of the input file.
        static std::vector<int64_t> readMagnetIds(const nlohmann::json& data);

        static greeter::MeshingSpec readMeshing(const nlohmann::json& meshing);

        static greeter::ForceConfig read(const nlohmann::json& data);
};

}  // namespace greeter

#endif // FORCE_IO_H
