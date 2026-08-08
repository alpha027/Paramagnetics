#ifndef DIPOLE_MAGNET_IO_H
#define DIPOLE_MAGNET_IO_H


#include <greeter/Magnet.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <memory>

namespace greeter {

/*
  Reads a point dipole:

      {
        "id": 1,
        "type": "dipole",
        "parameters": {
          "moment": [0, 0, 0.1],       // [A m^2], or a number for [0, 0, m]
          "position": [0, 0, 0],
          "orientation": [1, 0, 0, 0]
        }
      }

  The moment is asked for under its own name rather than under
  "magnetization", because it is not one: a polarization is measured in Tesla
  and a moment in ampere metre squared. Writing it as "magnetization" is
  refused rather than quietly read as a moment.
*/
class DipoleMagnetIO {

    public:

        DipoleMagnetIO();
        ~DipoleMagnetIO();

        static std::string getTypeName();

        static std::vector<float> readPosition(const nlohmann::json& magnet);
        static std::vector<float> readOrientation(const nlohmann::json& magnet);
        static std::vector<float> readMoment(const nlohmann::json& magnet);

        static std::unique_ptr<Magnet> createMagnet(const nlohmann::json& magnet);
    };

}

#endif // DIPOLE_MAGNET_IO_H
