#if !defined(QUATERNION_H)
#define QUATERNION_H

#include <vector>

namespace greeter {

class Quaternion {

  std::vector<double> data; // w, x, y, z
  //double w;
  //double x;
  //double y;
  //double z;

  public:

    Quaternion();
    Quaternion(double w, double x, double y, double z);
    Quaternion(const Quaternion& other);
    Quaternion(std::vector<double> data);

    double getW() const;
    double getX() const;
    double getY() const;
    double getZ() const;

    bool operator==(const Quaternion& other) const;
    bool operator!=(const Quaternion& other) const;
    Quaternion operator*(const Quaternion& other) const;
    Quaternion operator+(const Quaternion& other) const;
    Quaternion operator-(const Quaternion& other) const;

    Quaternion getConjugate() const;

    static std::vector<double> getConjugateQuaternion(
      const std::vector<double> quaternion 
    );

    static std::vector<std::vector<double>> getRotationMatrixFromQuaternion(
      const std::vector<double> quaternion1
    );

    static std::vector<double> get_rotation_axis(
      const std::vector<double> quaternion
    );

    static double get_rotation_angle(
      const std::vector<double> quaternion
    );

    static std::vector<double> applyRotation(
      const std::vector<double> quaternion,
      const std::vector<double> vector
    );

    std::vector<double> get_rotation(const std::vector<double> point) const;

    std::vector<double> get_inverse_rotation(const std::vector<double> point) const;

};

}

#endif // QUATERNION_H