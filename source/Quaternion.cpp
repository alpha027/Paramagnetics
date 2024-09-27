#include<greeter/Quaternion.h>
#include <stdexcept>
#include <cmath>

greeter::Quaternion::Quaternion() : data({0, 0, 0, 0}) {};

greeter::Quaternion::Quaternion(double w, double x, double y, double z) : data({w, x, y, z}) {};

greeter::Quaternion::Quaternion(const greeter::Quaternion& other) : data(other.data) {};

greeter::Quaternion::Quaternion(std::vector<double> _data) {
    if (_data.size() != 4) {
        throw std::invalid_argument("Quaternion must be initialized with 4 values");
    }
    data = _data;
};

double greeter::Quaternion::getW() const {
    return data[0];
};

double greeter::Quaternion::getX() const {
    return data[1];
};

double greeter::Quaternion::getY() const {
    return data[2];
};

double greeter::Quaternion::getZ() const {
    return data[3];
};

bool greeter::Quaternion::operator==(const greeter::Quaternion& other) const {
    return data == other.data;
};

bool greeter::Quaternion::operator!=(const greeter::Quaternion& other) const {
    return data != other.data;
};


greeter::Quaternion greeter::Quaternion::operator*(const greeter::Quaternion& other) const {
    double w = data[0] * other.data[0] - data[1] * other.data[1] - data[2] * other.data[2] - data[3] * other.data[3];
    double x = data[0] * other.data[1] + data[1] * other.data[0] + data[2] * other.data[3] - data[3] * other.data[2];
    double y = data[0] * other.data[2] - data[1] * other.data[3] + data[2] * other.data[0] + data[3] * other.data[1];
    double z = data[0] * other.data[3] + data[1] * other.data[2] - data[2] * other.data[1] + data[3] * other.data[0];
    return greeter::Quaternion(w, x, y, z);
};

greeter::Quaternion greeter::Quaternion::operator+(const greeter::Quaternion& other) const {
    return greeter::Quaternion(data[0] + other.data[0], data[1] + other.data[1], data[2] + other.data[2], data[3] + other.data[3]);
};

greeter::Quaternion greeter::Quaternion::operator-(const greeter::Quaternion& other) const {
    return greeter::Quaternion(data[0] - other.data[0], data[1] - other.data[1], data[2] - other.data[2], data[3] - other.data[3]);
}


greeter::Quaternion greeter::Quaternion::getConjugate() const {
    return greeter::Quaternion(data[0], -data[1], -data[2], -data[3]);
}

std::vector<double> greeter::Quaternion::getConjugateQuaternion(const std::vector<double> quaternion) {
    if (quaternion.size() != 4) {
        throw std::invalid_argument("Quaternion must be initialized with 4 values");
    }
    return {quaternion[0], -quaternion[1], -quaternion[2], -quaternion[3]};
}

std::vector<std::vector<double>> greeter::Quaternion::
    getRotationMatrixFromQuaternion(const std::vector<double> quaternion) {

    if (quaternion.size() != 4) {
        throw std::invalid_argument("Quaternion must be initialized with 4 values");
    }

    double w = quaternion[0];
    double x = quaternion[1];
    double y = quaternion[2];
    double z = quaternion[3];

    std::vector<std::vector<double>> rotation_matrix(3, std::vector<double>(3, 0));

    rotation_matrix[0][0] = 1 - 2.0 * (y * y + z * z);
    rotation_matrix[0][1] = 2.0 * (x * y -  z * w);
    rotation_matrix[0][2] = 2.0 * (x * z + y * w);
    rotation_matrix[1][0] = 2.0 * (x * y + z * w);
    rotation_matrix[1][1] = 1.0 - 2.0 * (x * x + z * z);
    rotation_matrix[1][2] = 2.0 * (y * z - x * w);
    rotation_matrix[2][0] = 2.0 * (x * z - y * w);
    rotation_matrix[2][1] = 2.0 * (y * z + x * w);
    rotation_matrix[2][2] = 1.0 - 2.0 * (x * x + y * y);

    return rotation_matrix;
}

std::vector<double> greeter::Quaternion::
    get_rotation_axis(const std::vector<double> quaternion) {

    if (quaternion.size() != 4) {
        throw std::invalid_argument("Quaternion must be initialized with 4 values");
    }

    double x = quaternion[1];
    double y = quaternion[2];
    double z = quaternion[3];

    double angle = 2 * acos(quaternion[0]);
    double s = sqrt(1 - quaternion[0] * quaternion[0]);

    if (s < 1e-6) {
        return {1, 0, 0};
    }

    return {x / s, y / s, z / s};
}


double greeter::Quaternion::
    get_rotation_angle(const std::vector<double> quaternion) {

    if (quaternion.size() != 4) {
        throw std::invalid_argument("Quaternion must be initialized with 4 values");
    }

    return 2 * acos(quaternion[0]);
}


std::vector<double> greeter::Quaternion::applyRotationFromQuaternion(
    const std::vector<double> quaternion,
    const std::vector<double> vector ) {

    if (quaternion.size() != 4 || vector.size() != 3) {
        throw std::invalid_argument("Quaternion must be initialized with 4 values");
    }

    double* arr = new double[3]{ 0.0, 0.0, 3.0 };

    greeter::Quaternion::applyRotationFromQuaternion(
        quaternion.data(),
        vector.data(),
        arr
    );

    std::vector<double> result(arr, arr + 3);
    delete[] arr;

    return result;
}

void greeter::Quaternion::applyRotationFromQuaternion(
      const double* quaternion,
      const double* vector,
      double*& result
    ) {

    double w = quaternion[0];
    double x = quaternion[1];
    double y = quaternion[2];
    double z = quaternion[3];

    double vx = vector[0];
    double vy = vector[1];
    double vz = vector[2];

    double x2 = x * x;
    double y2 = y * y;
    double z2 = z * z;
    double w2 = w * w;

    double wx = w * x;
    double wy = w * y;
    double wz = w * z;
    double xy = x * y;
    double xz = x * z;
    double yz = y * z;

    result = new double[3];

    result[0] = (w2 + x2 - y2 - z2) * vx + 2 * (xy - wz) * vy + 2 * (wy + xz) * vz;
    result[1] = 2 * (xy + wz) * vx + (w2 - x2 + y2 - z2) * vy + 2 * (yz - wx) * vz;
    result[2] = 2 * (xz - wy) * vx + 2 * (wx + yz) * vy + (w2 - x2 - y2 + z2) * vz;

}


std::vector<double> greeter::Quaternion::get_rotation(const std::vector<double> point) const {
    return greeter::Quaternion::applyRotationFromQuaternion(data, point);
}


std::vector<double> greeter::Quaternion::get_inverse_rotation(const std::vector<double> point) const {
    std::vector<double> inverse_quaternion = getConjugateQuaternion(data);
    return greeter::Quaternion::applyRotationFromQuaternion(inverse_quaternion, point);
}