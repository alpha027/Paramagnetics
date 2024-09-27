#ifndef MAGNETICFIELDMETHODFACTORY_H
#define MAGNETICFIELDMETHODFACTORY_H

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <greeter/Magnet.h>
#include <iostream>


namespace greeter {

class MagneticFieldMethodFactory {

public:
    //using CreatorFunction = std::function<std::unique_ptr<Magnet>()>;

    static MagneticFieldMethodFactory& getInstance() {
        static MagneticFieldMethodFactory instance;  // Guaranteed to be destroyed, instantiated on first use
        return instance;
    }

    using MethodFunction = std::function<void(const float* parameters, 
                                           const float* observation_point,
                                           float& a, float& b, float& c)>;

    // Method to register a class with a key
    bool registerComputeMagneticField(const u_int16_t& key, MethodFunction _method);

    void computeMagneticField(const u_int16_t& key, const float* parameters, 
                              const float* observation_point, float& a, float& b, float& c) const;
    // Method to display all registered types
    void displayRegistered() const;

private:

    MagneticFieldMethodFactory() = default;  // Private constructor
    ~MagneticFieldMethodFactory() = default; // Destructor
    MagneticFieldMethodFactory(const MagneticFieldMethodFactory&) = delete; // Prevent copying
    MagneticFieldMethodFactory& operator=(const MagneticFieldMethodFactory&) = delete; // Prevent assignment
    std::unordered_map<u_int16_t, MethodFunction> registry;  // Map of key to creator function
};

}  // namespace greeter

#endif