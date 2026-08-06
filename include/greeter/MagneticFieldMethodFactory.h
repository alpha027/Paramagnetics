#ifndef MAGNETICFIELDMETHODFACTORY_H
#define MAGNETICFIELDMETHODFACTORY_H

#include <memory>
#include <string>
#include <unordered_map>
#include <greeter/KokkosDefines.h>
#include <greeter/Magnet.h>
#include <iostream>


namespace greeter {

/*
  Registry of the field kernels of the magnet classes, keyed by type ID.

  The magnet classes of this library are registered by the constructor rather
  than by a static object in each of their translation units. A linker drops a
  translation unit of a static library when nothing else in it is referenced,
  which silently emptied the registry and is why the registration lives here.
  Anything outside this library still registers itself with the methods below.
*/
class MagneticFieldMethodFactory {

public:
    //using CreatorFunction = std::function<std::unique_ptr<Magnet>()>;

    static MagneticFieldMethodFactory& getInstance() {
        static MagneticFieldMethodFactory instance;  // Guaranteed to be destroyed, instantiated on first use
        return instance;
    }

    using MethodFunction = FieldKernel;

    using NumerOfParametersFunction = size_t (*)();

    // Method to register a class with a key
    bool registerComputeMagneticField(const u_int16_t& key, MethodFunction _method);

    bool registerNumberOfParameters(const u_int16_t& key, NumerOfParametersFunction _method);

    void computeMagneticField(const u_int16_t& key, const float* parameters,
                              const float* observation_point, float& a, float& b, float& c) const;

    /*
      The kernel itself, for a caller that is about to run it many times and
      does not want to pay for a lookup each time. Throws on an unknown type,
      where computeMagneticField above only complains and leaves the field at
      whatever the caller passed in.
    */
    MethodFunction getComputeMagneticField(const u_int16_t& key) const;

    size_t getNumberOfParameters(const u_int16_t& key) const;
    // Method to display all registered types
    void displayRegistered() const;

private:

    MagneticFieldMethodFactory();            // Private constructor, registers the built in magnets
    ~MagneticFieldMethodFactory() = default; // Destructor
    MagneticFieldMethodFactory(const MagneticFieldMethodFactory&) = delete; // Prevent copying
    MagneticFieldMethodFactory& operator=(const MagneticFieldMethodFactory&) = delete; // Prevent assignment
    std::unordered_map<u_int16_t, MethodFunction> registry;  // Map of key to creator function
    std::unordered_map<u_int16_t, NumerOfParametersFunction> registry_parameters;  // Map of key to creator function
};

}  // namespace greeter

#endif