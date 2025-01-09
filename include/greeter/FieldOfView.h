#ifndef FIELD_OF_VIEW_H
#define FIELD_OF_VIEW_H

#include <vector>
#include <string>
#include <memory>


namespace greeter {

  class FieldOfView {

    std::vector<std::vector<float>> fov;

    public:

        FieldOfView();
        FieldOfView(std::vector<std::vector<float>> fov);
        FieldOfView(
            std::vector<float> xxyyzz,
            std::vector<u_int32_t> num_points
        );
        FieldOfView(const FieldOfView& other);

        ~FieldOfView();

        std::vector<std::vector<float>> getFOV() const;

        void setFOV(const std::vector<std::vector<float>>& fov);

        void display() const;

        std::unique_ptr<FieldOfView> clone() const;

        void setPosition(const float& x, const float& y, const float& z);

        void setMagnetization(const float& x, const float& y, const float& z);

        void translate(const float& x, const float& y, const float& z);

        size_t getNumOfParameters() const;

  };
}  // namespace greeter

#endif // FIELD_OF_VIEW_H