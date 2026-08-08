#ifndef TRIANGULAR_MESH_MAGNET_IO_H
#define TRIANGULAR_MESH_MAGNET_IO_H


#include <greeter/Magnet.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <memory>

namespace greeter {

/*
  Reads a body of any shape, as a closed surface of triangles:

      {
        "id": 1,
        "type": "triangular_mesh",
        "parameters": {
          "vertices": [[0,0,0], [1,0,0], [0,1,0], [0,0,1]],
          "faces": [[0,2,1], [0,1,3], [1,2,3], [0,3,2]],
          "magnetization": [0, 0, 1],
          "position": [0, 0, 0],
          "orientation": [1, 0, 0, 0]
        }
      }

  Vertices and faces, the way a mesh comes out of anything that makes one.
  The faces may also be given directly as triangles, three points each, under
  "triangles", which saves writing an index list out by hand for a small
  shape.

  The surface has to be closed, and its faces consistently wound. One wound
  inwards is turned the right way round rather than refused, see
  TriangularMeshMagnet.
*/
class TriangularMeshMagnetIO {

    public:

        TriangularMeshMagnetIO();
        ~TriangularMeshMagnetIO();

        static std::string getTypeName();

        static std::vector<float> readPosition(const nlohmann::json& magnet);
        static std::vector<float> readOrientation(const nlohmann::json& magnet);

        /* Nine floats per face, however the faces were given. */
        static std::vector<float> readTriangles(const nlohmann::json& magnet);

        static std::vector<float> readMagnetization(const nlohmann::json& magnet);

        static std::unique_ptr<Magnet> createMagnet(const nlohmann::json& magnet);
    };

}

#endif // TRIANGULAR_MESH_MAGNET_IO_H
