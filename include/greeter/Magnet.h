#ifndef MAGNET_H
#define MAGNET_H

namespace greeter {

class Magnet {

  public:
    virtual double computeMagneticField(double x, double y, double z) const = 0;

    virtual void computeMagneticField(const float* parameters, const float* observation_point,
                                      float& b_x, float& b_y, float& b_z ) const = 0;

    virtual ~Magnet() = 0;
};

}  // namespace greeter

#endif