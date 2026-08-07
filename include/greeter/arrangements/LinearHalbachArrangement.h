#ifndef LINEAR_HALBACH_ARRANGEMENT_H
#define LINEAR_HALBACH_ARRANGEMENT_H

#include <greeter/Magnet.h>
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <vector>


namespace greeter {

/*
  A linear Halbach array: magnets in a row along the local x axis, each turned
  a little further about the local y axis than the one before, so that the
  polarization sweeps round in the local xz plane. The field of such a row is
  strong on one side and nearly cancels on the other.

    {
      "id": 100,
      "type": "halbach_linear",
      "parameters": {
        "count": 8,                  // members in the row
        "spacing": 0.02,             // distance between neighbours [m]
        "steps_per_period": 4,       // members per full turn of the polarization
        "centered": true,            // optional, the row sits on "position"
        "position": [0, 0, 0],       // optional placement of the row as a whole
        "orientation": [1, 0, 0, 0],
        "element": {
          "type": "cuboid",
          "parameters": { "dimensions": [0.02, 0.02, 0.02], "magnetization": [0, 0, 1] }
        }
      }
    }

  The turn of member i is 2 * pi * i / steps_per_period, so member 0 is never
  turned however the row is placed. Instead of the number of members per turn,
  the "wavelength" of the polarization may be given as a length, which is the
  same thing divided by the spacing.

  Four members per period is the usual choice, each turned a quarter turn from
  the last. The count need not be a whole number of periods, and the steps per
  period need not be whole either.

  For a positive number of members to a period, and an element polarized along
  its local z, the strong side is local -z. Negating "steps_per_period", or
  "wavelength", mirrors the row and so swaps the two sides.

  The side the field is strong on also depends on the element, whose
  magnetization is read in its own frame and turned along with it. An element
  polarized along its local z is what the turn about y sweeps through the xz
  plane; one polarized along y would be turned about its own axis and sweep
  nothing.
*/
class LinearHalbachArrangement {

    public:

        LinearHalbachArrangement();
        ~LinearHalbachArrangement();

        static std::string getTypeName();

        static std::vector<std::unique_ptr<greeter::Magnet>> expand(
            const nlohmann::json& arrangement);
};

}  // namespace greeter

#endif // LINEAR_HALBACH_ARRANGEMENT_H
