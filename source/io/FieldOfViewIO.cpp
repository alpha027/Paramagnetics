#include <greeter/Quaternion.h>
#include <greeter/io/FieldOfViewIO.h>

greeter::FieldOfViewIO::FieldOfViewIO() {}
greeter::FieldOfViewIO::~FieldOfViewIO() {}


std::vector<float> greeter::FieldOfViewIO::readRanges(const nlohmann::json& fov) {

    std::vector<float> x_min = fov["x"]["min"];
    std::vector<float> x_max = fov["x"]["max"];
    std::vector<float> y_min = fov["y"]["min"];
    std::vector<float> y_max = fov["y"]["max"];
    std::vector<float> z_min = fov["z"]["min"];
    std::vector<float> z_max = fov["z"]["max"];

    std::vector<float> ranges = {
        x_min[0], x_max[0], y_min[0], y_max[0], z_min[0], z_max[0]
    };

    return ranges;
}

std::vector<float> greeter::FieldOfViewIO::readSubdivisionCounts(const nlohmann::json& fov) {

    std::vector<int32_t> x_count = fov["x"]["n"];
    std::vector<int32_t> y_count = fov["y"]["n"];
    std::vector<int32_t> z_count = fov["z"]["n"];

    std::vector<float> counts = {
        (float) x_count[0], (float) y_count[0], (float) z_count[0]
    };

    return counts;
}