#ifndef MAGNET_H
#define MAGNET_H

#include <memory>
#include <vector>


namespace greeter {

class Magnet {

  public:
    virtual std::vector<float> computeMagneticField(double x, double y, double z) const = 0;

    virtual void computeMagneticField(const float* parameters, const float* observation_point,
                                      float& b_x, float& b_y, float& b_z ) const = 0;

    virtual ~Magnet() = 0;

    virtual std::unique_ptr<Magnet> clone() const = 0;

    virtual std::vector<float> getPosition() const = 0;

    virtual std::vector<float> getDimensions() const = 0;

    virtual std::vector<float> getOrientation() const = 0;

    virtual std::vector<float> getMagnetization() const = 0;

    virtual void setPosition(const float& x, const float& y, const float& z) = 0;

    virtual void translate(const float& x, const float& y, const float& z) = 0;

    virtual void display() const = 0;

    virtual uint16_t getTypeID() const = 0;


};

}  // namespace greeter

#endif