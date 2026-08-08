#ifndef CYLINDER_SEGMENT_MAGNET_IO_H
#define CYLINDER_SEGMENT_MAGNET_IO_H


#include <greeter/Magnet.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <memory>

namespace greeter {

/*
  A ring sector: the arc shaped block that real Halbach rings and motor rotors
  are actually made of.

      {
        "id": 1,
        "type": "cylinder_segment",
        "parameters": {
          "dimensions": [0.02, 0.03, 0.01, -30, 30],  // r1, r2, h [m], phi1, phi2 [deg]
          "magnetization": [0, 0, 1],
          "segments": 32,               // optional, facets across the arc
          "position": [0, 0, 0],
          "orientation": [1, 0, 0, 0]
        }
      }

  The axis is the local z axis and the sector is centred on the local origin,
  as a cylinder is. The angles are measured from the local x axis towards the
  local y axis, in degrees, as magpylib measures them.

  How this is built, and what that costs
  --------------------------------------

  It is a faceted body, not a closed form: the curved walls are cut into
  `segments` flat strips and the result is a TriangularMeshMagnet, whose field
  is then exact for the faceted shape. It is not exact for the curved one.

  magpylib does have the closed form, and it is some two and a half thousand
  lines resting on a third elliptic integral and about a hundred and fifty
  separate cases for the places the general expression breaks down. Porting
  that faithfully is a piece of work in its own right, and porting it
  unfaithfully would be worse than not having it.

  What the faceting costs, measured against magpylib's closed form for a
  sector of r1 = 20 mm, r2 = 30 mm, h = 10 mm over 60 degrees, at probes
  inside and outside it:

      facets across the arc     worst error
                          4          2.2 %
                          8          0.56 %
                         16          0.14 %
                         32          0.035 %
                         64          0.009 %

  The error falls with the square of the facet size, so the default of 32 is
  already far below any tolerance a magnet is actually made to. Raise it if
  the field very close to the curved wall matters.

  r1 has to be greater than zero. A solid sector reaching the axis needs a
  different triangulation, and a whole solid cylinder is the `cylinder` type.
*/
class CylinderSegmentMagnetIO {

    public:

        CylinderSegmentMagnetIO();
        ~CylinderSegmentMagnetIO();

        static std::string getTypeName();

        /* r1, r2, h, phi1, phi2, with the angles in degrees. */
        static std::vector<float> readDimensions(const nlohmann::json& magnet);

        static uint32_t readSegments(const nlohmann::json& magnet);

        /*
          The closed surface of a ring sector, nine floats per face. A sector
          spanning a whole turn has no end caps and its arc wraps round.
        */
        static std::vector<float> buildTriangles(
            const float& inner_radius, const float& outer_radius,
            const float& height, const float& first_angle,
            const float& second_angle, const uint32_t& segments);

        static std::unique_ptr<Magnet> createMagnet(const nlohmann::json& magnet);
    };

}

#endif // CYLINDER_SEGMENT_MAGNET_IO_H
