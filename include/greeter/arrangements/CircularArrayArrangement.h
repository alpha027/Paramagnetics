#ifndef CIRCULAR_ARRAY_ARRANGEMENT_H
#define CIRCULAR_ARRAY_ARRANGEMENT_H

#include <greeter/Magnet.h>
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <vector>


namespace greeter {

/*
  Magnets evenly spaced round a circle in the local xy plane, without the
  turning that makes a ring a Halbach one.

    {
      "id": 100,
      "type": "circular_array",
      "parameters": {
        "radius": 0.3,               // of the circle the members sit on [m]
        "count": 8,                  // members round the ring
        "face": "axis",              // optional, "axis" by default
        "position": [0, 0, 0],       // optional placement of the ring as a whole
        "orientation": [1, 0, 0, 0],
        "element": {
          "type": "cylinder",
          "parameters": { "dimensions": [0.05, 0.1], "magnetization": [0, 0, 1] }
        }
      }
    }

  "face" says how a member is turned:

    "axis"    every member keeps the orientation of the ring, so a ring of
              magnets all polarized the same way
    "center"  the frame of a member follows it round the ring, so its local x
              axis points away from the middle and its local y axis runs along
              the ring

  With "center", what ends up pointing at the middle is decided by the element:
  its magnetization is read in its own frame, so [J, 0, 0] is radial and
  [0, J, 0] is tangential.

  This is the same ring a Halbach one is laid out on, so "axis" is a Halbach
  ring of order -1 and "center" is one of order 0. It is written separately
  because a ring that is not a Halbach ring should not have to be asked for as
  one.
*/
class CircularArrayArrangement {

    public:

        CircularArrayArrangement();
        ~CircularArrayArrangement();

        static std::string getTypeName();

        static std::vector<std::unique_ptr<greeter::Magnet>> expand(
            const nlohmann::json& arrangement);
};

}  // namespace greeter

#endif // CIRCULAR_ARRAY_ARRANGEMENT_H
