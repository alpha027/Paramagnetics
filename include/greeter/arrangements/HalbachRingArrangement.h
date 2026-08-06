#ifndef HALBACH_RING_ARRANGEMENT_H
#define HALBACH_RING_ARRANGEMENT_H

#include <greeter/Magnet.h>
#include <nlohmann/json.hpp>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>


namespace greeter {

/*
  A Halbach cylinder: magnets evenly spaced round a circle in the local xy
  plane, each turned about the axis of the ring by a multiple of the angle at
  which it sits.

    {
      "id": 100,
      "type": "halbach_ring",
      "parameters": {
        "radius": 0.3,               // of the circle the members sit on [m]
        "count": 12,                 // members round the ring
        "order": 1,                  // optional, 1 by default
        "position": [0, 0, 0],       // optional placement of the ring as a whole
        "orientation": [1, 0, 0, 0],
        "element": {
          "type": "cuboid",
          "parameters": { "dimensions": [0.12, 0.12, 0.12], "magnetization": [0, 5, 0] }
        }
      }
    }

  A member at the angle t round the ring is turned by (order + 1) * t, which
  makes the ring a 2 * order pole: order 1 is the dipole ring whose field is
  uniform inside and cancels outside, and order 2 is the quadrupole. The
  polarization has to come back to itself after a full turn round the ring, so
  the order is a whole number. It may be negative, and order -1 leaves every
  member pointing the same way.

  The axis the field lies along is set by the element, whose magnetization is
  read in its own frame and turned along with it, so a ring of order 1 built
  from an element polarized along its local y makes a field along y. Which way
  along that axis is decided by the ring and not by the element: an element
  polarized along +y gives a field along -y.

  Members come out in the order they sit round the ring, starting on the local
  x axis and turning towards the local y axis.
*/
class HalbachRingArrangement {

    public:

        HalbachRingArrangement();
        ~HalbachRingArrangement();

        static std::string getTypeName();

        static std::vector<std::unique_ptr<greeter::Magnet>> expand(
            const nlohmann::json& arrangement);

        /*
          The ring that this and a plain circular array are both laid out by.
          A member at the angle t is turned about the axis of the ring by
          turns * t, so a Halbach ring of a given order passes order + 1 and a
          circular array passes 0 or 1.
        */
        static std::vector<std::unique_ptr<greeter::Magnet>> expandRing(
            const nlohmann::json& arrangement, const int64_t& turns);
};

}  // namespace greeter

#endif // HALBACH_RING_ARRANGEMENT_H
