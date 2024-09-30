#if !defined(QUATERNION_H)
#define QUATERNION_H

#include <vector>
#include <string>

namespace greeter {

class Quaternion {

  std::vector<float> data; // w, x, y, z
  //float w;
  //float x;
  //float y;
  //float z;

  public:

    Quaternion();
    Quaternion(float w, float x, float y, float z);
    Quaternion(const Quaternion& other);
    Quaternion(std::vector<float> data);

    float getW() const;
    float getX() const;
    float getY() const;
    float getZ() const;

    bool operator==(const Quaternion& other) const;
    bool operator!=(const Quaternion& other) const;
    Quaternion operator*(const Quaternion& other) const;
    Quaternion operator+(const Quaternion& other) const;
    Quaternion operator-(const Quaternion& other) const;

    Quaternion getConjugate() const;

    static std::vector<float> getConjugateQuaternion(
      const std::vector<float> quaternion 
    );

    static std::vector<std::vector<float>> getRotationMatrixFromQuaternion(
      const std::vector<float> quaternion1
    );

    static std::vector<float> get_rotation_axis(
      const std::vector<float> quaternion
    );

    static float get_rotation_angle(
      const std::vector<float> quaternion
    );

    static std::vector<float> applyRotationFromQuaternion(
      const std::vector<float> quaternion,
      const std::vector<float> vector
    );

    static void applyRotationFromQuaternion(
      const float* quaternion,
      const float* vector,
      float* result
    );

    static void applyInverseRotationFromQuaternion(
      const float* quaternion,
      const float* vector,
      float* result
    );

    // Set rotation from axis and angle (static)
    static void set_rotation_from_axis_angle(
      const std::string& axis,
      const float& angle_in_rad,
      float* quaternion
    );

    std::vector<float> get_rotation(const std::vector<float> point) const;

    std::vector<float> get_inverse_rotation(const std::vector<float> point) const;

};

}

#endif // QUATERNION_H