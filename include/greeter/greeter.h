#pragma once

#include <string>
#include <vector>

namespace greeter {

  /**  Language codes to be used with the Greeter class */
  enum class LanguageCode { EN, DE, ES, FR };

  /**
   * @brief A class for saying hello in multiple languages
   */
  class Greeter {
    std::string name;

  public:
    /**
     * @brief Creates a new greeter
     * @param name the name to greet
     */
    Greeter(std::string name);

    /**
     * @brief Creates a localized string containing the greeting
     * @param lang the language to greet in
     * @return a string containing the greeting
     */
    std::string greet(LanguageCode lang = LanguageCode::EN) const;
  };

  class Magnet {

  public:
    virtual double computeMagneticField(double x, double y, double z) const = 0;

    virtual ~Magnet() = 0;

  };

  class SphereMagnet: public Magnet {

    std::vector<float> position;
    double radius;
    double magnetization;

    public:
      SphereMagnet(double radius, double magnetization);
      SphereMagnet();
      virtual ~SphereMagnet();
      double computeMagneticField(double x, double y, double z) const override;

      static std::vector<float> calculateMagneticFieldForCube(
        std::vector<float> position, double radius, double magnetization,
        std::vector<float> observation_point
      );
  };

  class CuboidMagnet: public Magnet {

    std::vector<float> position;
    std::vector<float> dimensions;
    std::vector<float> orientation;
    std::vector<float> magnetization;

    public:
      CuboidMagnet();
      CuboidMagnet(std::vector<float> position, std::vector<float> dimensions,
                   std::vector<float> orientation,
                   std::vector<float> magnetization);
      virtual ~CuboidMagnet();
      double computeMagneticField(double x, double y, double z) const override;

      static std::vector<float> calculateMagneticFieldForCube(
        std::vector<float> position, std::vector<float> orientation, 
        std::vector<float> magnetization, std::vector<float> observation_point
      );
    
  };

}  // namespace greeter
