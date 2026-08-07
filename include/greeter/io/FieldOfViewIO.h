#ifndef FIELD_OF_VIEW_IO_H
#define FIELD_OF_VIEW_IO_H


#include <nlohmann/json.hpp>
#include <greeter/FieldOfView.h>
#include <cstdint>
#include <vector>
#include <memory>

namespace greeter {

  /*
    Reads the "field_of_view" section of an input file:

        "field_of_view": {
          "x": {"min": -0.06, "max": 0.06, "n": 13},
          "y": {"min": -0.06, "max": 0.06, "n": 13},
          "z": {"min":  0.02, "max": 0.10, "n":  9}
        }

    An axis sampled once ("n": 1) is a plane through its "min", which is the
    usual way to ask for a slice.
  */
  class FieldOfViewIO {

    public:

        FieldOfViewIO();
        ~FieldOfViewIO();

        /* x_min, x_max, y_min, y_max, z_min, z_max */
        static std::vector<float> readRanges(const nlohmann::json& fov);

        /* Samples along x, y and z */
        static std::vector<uint32_t> readSubdivisionCounts(const nlohmann::json& fov);

        static greeter::FieldOfView read(const nlohmann::json& fov);

        static std::unique_ptr<FieldOfView> createFOV(const nlohmann::json& fov);
    };

}  // namespace greeter

#endif // FIELD_OF_VIEW_IO_H
