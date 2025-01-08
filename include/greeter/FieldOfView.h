#ifndef FIELD_OF_VIEW_H
#define FIELD_OF_VIEW_H

#include <vector>
#include <string>
#include <memory>


namespace greeter {

  class FieldOfView {

    std::vector<float> fov;

    public:

        FieldOfView();
        FieldOfView(std::vector<float> fov);
        FieldOfView(
            std::vector<float> xxyyzz,
            std::vector<int32_t> num_points
        );
        FieldOfView(const FieldOfView& other);
        FieldOfView();

        ~FieldOfView();

        std::vector<float> getFOV() const;

        void setFOV(std::vector<std::vector<float>> fov);

        void display() const;

        std::vector<float> getDimensions() const;

        std::vector<float> getPosition() const;

        std::unique_ptr<FieldOfView> clone() const;

        void setPosition(const float& x, const float& y, const float& z);

        void setMagnetization(const float& x, const float& y, const float& z);

        void translate(const float& x, const float& y, const float& z);

        size_t getNumOfParameters() const;

        static std::vector<float> calculateMagneticFieldForFOV(
          std::vector<float> position, std::vector<float> orientation,
          std::vector<float> dimensions, std::vector<float> magnetization,
          std::vector<float> observation_point
        );

        static void calculateMagneticFieldForFOV(
          const float* position, const float* orientation, 
          const float* dimensions, const float* magnetization,
          const float* observation_point,
          float& result_x, float& result_y, float& result_z
        );

  };
}  // namespace greeter