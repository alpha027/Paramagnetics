#ifndef LINEAR_ARRAY_ARRANGEMENT_H
#define LINEAR_ARRAY_ARRANGEMENT_H

#include <greeter/Magnet.h>
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <vector>


namespace greeter {

/*
  A lattice of identical magnets on the local axes:

    {
      "id": 100,
      "type": "linear_array",
      "parameters": {
        "count": [4, 2, 1],          // members along x, y and z, a number for a row
        "spacing": [0.05, 0.05, 0],  // distance between neighbours [m], a number for all three
        "centered": true,            // optional, the lattice sits on "position"
        "alternating": "x",          // optional, every other member is turned half
                                     // a turn about this local axis
        "position": [0, 0, 0],       // optional placement of the lattice as a whole
        "orientation": [1, 0, 0, 0],
        "element": {                 // any registered magnet type, without a placement
          "type": "cuboid",
          "parameters": { "dimensions": [0.01, 0.01, 0.01], "magnetization": [0, 0, 1] }
        }
      }
    }

  Members come out with x running fastest and z slowest, so the member at
  (ix, iy, iz) is number ix + nx * (iy + ny * iz). The force section relies on
  that order to name a member.

  "alternating" turns every other member of the lattice by half a turn about
  the named local axis, which reverses a magnetization perpendicular to that
  axis. It is a rotation rather than a sign change on the magnetization, so
  that a member stays a rigid body; naming the axis the magnetization already
  lies along therefore leaves the member as it was.
*/
class LinearArrayArrangement {

    public:

        LinearArrayArrangement();
        ~LinearArrayArrangement();

        static std::string getTypeName();

        static std::vector<std::unique_ptr<greeter::Magnet>> expand(
            const nlohmann::json& arrangement);
};

}  // namespace greeter

#endif // LINEAR_ARRAY_ARRANGEMENT_H
