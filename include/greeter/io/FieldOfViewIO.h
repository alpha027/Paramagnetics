#ifndef FIELD_OF_VIEW_IO_H
#define FIELD_OF_VIEW_IO_H


#include <nlohmann/json.hpp>
#include <greeter/FieldOfView.h>
#include <vector>
#include <memory>

namespace greeter {

  class FieldOfViewIO {

    std::vector<std::string> keys = {"x", "y", "z"};

    public:

        FieldOfViewIO();
        ~FieldOfViewIO();

        std::vector<float> readRanges(const nlohmann::json& fov);
        std::vector<float> readSubdivisionCounts(const nlohmann::json& fov);

        std::unique_ptr<FieldOfView> createFOV(const nlohmann::json& fov);
    };

}  // namespace greeter

#endif // FIELD_OF_VIEW_IO_H