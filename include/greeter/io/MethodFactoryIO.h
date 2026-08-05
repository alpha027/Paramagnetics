#ifndef METHODFACTORYIO_H
#define METHODFACTORYIO_H

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <greeter/Magnet.h>
#include <nlohmann/json.hpp>
#include <iostream>


namespace greeter {

/*
  Registry of the JSON readers of the magnet classes, keyed by the "type" of
  the input file. The readers of this library are registered by the
  constructor, see MagneticFieldMethodFactory for why they cannot register
  themselves.
*/
class MethodFactoryIO {

public:
    //using CreatorFunction = std::function<std::unique_ptr<Magnet>()>;

    static MethodFactoryIO& getInstance() {
        static MethodFactoryIO instance;  // Guaranteed to be destroyed, instantiated on first use
        return instance;
    }

    using MethodFunction = std::function<std::unique_ptr<Magnet>(
                                           const nlohmann::json& magnet)>;

    using NumerOfParametersFunction = std::function<size_t()>;

    // Method to register a class with a key
    bool registerGetMagnet(const std::string& key, MethodFunction _method);

    std::unique_ptr<Magnet> createMagnet(const std::string& key, const nlohmann::json& magnet) const;

    // bool registerNumberOfParameters(const std::string& key, NumerOfParametersFunction _method);

    // void computeMagneticField(const std::string& key, const float* parameters,
    //                           const float* observation_point, float& a, float& b, float& c) const;

    // size_t getNumberOfParameters(const std::string& key) const;
    // Method to display all registered types
    void displayRegistered() const;

private:

    MethodFactoryIO();            // Private constructor, registers the built in readers
    ~MethodFactoryIO() = default; // Destructor
    MethodFactoryIO(const MethodFactoryIO&) = delete; // Prevent copying
    MethodFactoryIO& operator=(const MethodFactoryIO&) = delete; // Prevent assignment
    std::unordered_map<std::string, MethodFunction> registry;  // Map of key to creator function
    // std::unordered_map<u_int16_t, NumerOfParametersFunction> registry_parameters;  // Map of key to creator function
};

}  // namespace greeter

#endif