#include<greeter/Quaternion.h>
#include <stdexcept>
#include <cmath>

greeter::Quaternion::Quaternion() : data({0, 0, 0, 0}) {}

greeter::Quaternion::Quaternion(float w, float x, float y, float z) : data({w, x, y, z}) {}

greeter::Quaternion::Quaternion(const greeter::Quaternion& other) : data(other.data) {}

greeter::Quaternion::Quaternion(std::vector<float> _data) {
    if (_data.size() != 4) {
        throw std::invalid_argument("Quaternion must be initialized with 4 values");
    }
    data = _data;
}

float greeter::Quaternion::getW() const {
    return data[0];
}

float greeter::Quaternion::getX() const {
    return data[1];
}

float greeter::Quaternion::getY() const {
    return data[2];
}

float greeter::Quaternion::getZ() const {
    return data[3];
}

bool greeter::Quaternion::operator==(const greeter::Quaternion& other) const {
    return data == other.data;
}

bool greeter::Quaternion::operator!=(const greeter::Quaternion& other) const {
    return data != other.data;
}


greeter::Quaternion greeter::Quaternion::operator*(const greeter::Quaternion& other) const {
    float w = data[0] * other.data[0] - data[1] * other.data[1] - data[2] * other.data[2] - data[3] * other.data[3];
    float x = data[0] * other.data[1] + data[1] * other.data[0] + data[2] * other.data[3] - data[3] * other.data[2];
    float y = data[0] * other.data[2] - data[1] * other.data[3] + data[2] * other.data[0] + data[3] * other.data[1];
    float z = data[0] * other.data[3] + data[1] * other.data[2] - data[2] * other.data[1] + data[3] * other.data[0];
    return greeter::Quaternion(w, x, y, z);
}

greeter::Quaternion greeter::Quaternion::operator+(const greeter::Quaternion& other) const {
    return greeter::Quaternion(data[0] + other.data[0], data[1] + other.data[1], data[2] + other.data[2], data[3] + other.data[3]);
}

greeter::Quaternion greeter::Quaternion::operator-(const greeter::Quaternion& other) const {
    return greeter::Quaternion(data[0] - other.data[0], data[1] - other.data[1], data[2] - other.data[2], data[3] - other.data[3]);
}

greeter::Quaternion greeter::Quaternion::getConjugate() const {
    return greeter::Quaternion(data[0], -data[1], -data[2], -data[3]);
}

std::vector<float> greeter::Quaternion::getConjugateQuaternion(const std::vector<float> quaternion) {
    if (quaternion.size() != 4) {
        throw std::invalid_argument("Quaternion must be initialized with 4 values");
    }
    return {quaternion[0], -quaternion[1], -quaternion[2], -quaternion[3]};
}

std::vector<std::vector<float>> greeter::Quaternion::
    getRotationMatrixFromQuaternion(const std::vector<float> quaternion) {

    if (quaternion.size() != 4) {
        throw std::invalid_argument("Quaternion must be initialized with 4 values");
    }

    float w = quaternion[0];
    float x = quaternion[1];
    float y = quaternion[2];
    float z = quaternion[3];

    std::vector<std::vector<float>> rotation_matrix(3, std::vector<float>(3, 0));

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

std::vector<float> greeter::Quaternion::
    get_rotation_axis(const std::vector<float> quaternion) {

    if (quaternion.size() != 4) {
        throw std::invalid_argument("Quaternion must be initialized with 4 values");
    }

    float x = quaternion[1];
    float y = quaternion[2];
    float z = quaternion[3];

    float angle = 2 * acos(quaternion[0]);
    float s = sqrt(1 - quaternion[0] * quaternion[0]);

    if (s < 1e-6) {
        return {1, 0, 0};
    }

    return {x / s, y / s, z / s};
}


float greeter::Quaternion::
    get_rotation_angle(const std::vector<float> quaternion) {

    if (quaternion.size() != 4) {
        throw std::invalid_argument("Quaternion must be initialized with 4 values");
    }

    return 2 * acos(quaternion[0]);
}


std::vector<float> greeter::Quaternion::applyRotationFromQuaternion(
    const std::vector<float> quaternion,
    const std::vector<float> vector ) {

    if (quaternion.size() != 4 || vector.size() != 3) {
        throw std::invalid_argument("Quaternion must be initialized with 4 values");
    }

    float arr[3];

    greeter::Quaternion::applyRotationFromQuaternion(
        quaternion.data(),
        vector.data(),
        arr
    );

    std::vector<float> result(arr, arr + 3);
    delete[] arr;

    return result;
}

void greeter::Quaternion::applyRotationFromQuaternion(
      const float* quaternion,
      const float* vector,
      float* result
    ) {

    float w = quaternion[0];
    float x = quaternion[1];
    float y = quaternion[2];
    float z = quaternion[3];

    float vx = vector[0];
    float vy = vector[1];
    float vz = vector[2];

    float x2 = x * x;
    float y2 = y * y;
    float z2 = z * z;
    float w2 = w * w;

    float wx = w * x;
    float wy = w * y;
    float wz = w * z;
    float xy = x * y;
    float xz = x * z;
    float yz = y * z;

    //result = new float[3];

    result[0] = (w2 + x2 - y2 - z2) * vx + 2 * (xy - wz) * vy + 2 * (wy + xz) * vz;
    result[1] = 2 * (xy + wz) * vx + (w2 - x2 + y2 - z2) * vy + 2 * (yz - wx) * vz;
    result[2] = 2 * (xz - wy) * vx + 2 * (wx + yz) * vy + (w2 - x2 - y2 + z2) * vz;

}


void greeter::Quaternion::applyInverseRotationFromQuaternion(
      const float* quaternion,
      const float* vector,
      float* result
    ) {

    float conjugate_quaternion[4] = {quaternion[0], -quaternion[1], -quaternion[2], -quaternion[3]};

    greeter::Quaternion::applyRotationFromQuaternion(
        conjugate_quaternion,
        vector,
        result
    );
}

void greeter::Quaternion::set_rotation_from_axis_angle(
      const std::string& axis,
      const float& angle_in_rad,
      float* quaternion ) {

    float half_angle = angle_in_rad / 2.0f;
    float s = sin(half_angle);
    float c = cos(half_angle);

    if (axis == "x") {
        quaternion[0] = c;
        quaternion[1] = s;
        quaternion[2] = 0;
        quaternion[3] = 0;
    } else if (axis == "y") {
        quaternion[0] = c;
        quaternion[1] = 0;
        quaternion[2] = s;
        quaternion[3] = 0;
    } else if (axis == "z") {
        quaternion[0] = c;
        quaternion[1] = 0;
        quaternion[2] = 0;
        quaternion[3] = s;
    } else {
        throw std::invalid_argument("Axis must be one of 'x', 'y', 'z'");
    }
}


std::vector<float> greeter::Quaternion::get_rotation(const std::vector<float> point) const {
    return greeter::Quaternion::applyRotationFromQuaternion(data, point);
}


std::vector<float> greeter::Quaternion::get_inverse_rotation(const std::vector<float> point) const {
    std::vector<float> inverse_quaternion = getConjugateQuaternion(data);
    return greeter::Quaternion::applyRotationFromQuaternion(inverse_quaternion, point);
}