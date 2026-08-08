#ifndef TRIANGLE_MAGNET_IO_H
#define TRIANGLE_MAGNET_IO_H


#include <greeter/Magnet.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <memory>

namespace greeter {

/*
  Reads one magnetically charged triangle:

      {
        "id": 1,
        "type": "triangle",
        "parameters": {
          "vertices": [[0, 0, 0], [0.01, 0, 0], [0, 0.01, 0]],
          "magnetization": [0, 0, 1],
          "position": [0, 0, 0],
          "orientation": [1, 0, 0, 0]
        }
      }

  A charged surface on its own is not a body; see TriangleMagnet.
*/
class TriangleMagnetIO {

    public:

        TriangleMagnetIO();
        ~TriangleMagnetIO();

        static std::string getTypeName();

        static std::vector<float> readPosition(const nlohmann::json& magnet);
        static std::vector<float> readOrientation(const nlohmann::json& magnet);
        static std::vector<float> readVertices(const nlohmann::json& magnet);
        static std::vector<float> readMagnetization(const nlohmann::json& magnet);

        static std::unique_ptr<Magnet> createMagnet(const nlohmann::json& magnet);
    };

}

#endif // TRIANGLE_MAGNET_IO_H
