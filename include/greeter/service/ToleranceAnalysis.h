#ifndef GREETER_TOLERANCE_ANALYSIS_H
#define GREETER_TOLERANCE_ANALYSIS_H

#include <cstddef>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace greeter {
  namespace service {

    enum class ToleranceDistribution { Uniform, Normal };
    enum class ToleranceRegion { Box, InscribedSphere };

    /* Independent errors per magnet. Uniform values are +/- bounds; normal values
       are one standard deviation. Rotation-vector components are sampled per axis.
       Size is one isotropic scale about the magnet's local origin; dipoles have no
       size. Position errors are along global axes, direction errors in local axes. */
    struct ToleranceSettings {
      uint32_t trials = 100;
      uint64_t seed = 42;
      ToleranceDistribution distribution = ToleranceDistribution::Uniform;
      ToleranceRegion region = ToleranceRegion::Box;
      uint32_t points_per_axis = 0;  // 0 retains the input field grid
      double strength_percent = 1.0;
      double position_mm = 0.1;
      double direction_degrees = 1.0;
      double body_tilt_degrees = 0.0;
      double size_percent = 0.0;
      double homogeneity_limit_ppm = 0.0;  // zero disables this acceptance limit
      double minimum_mean_T = 0.0;         // zero disables this acceptance limit

      void validate() const;
      bool hasLimits() const;
    };

    struct ToleranceMetrics {
      double mean_T = 0.0;
      double min_T = 0.0;
      double max_T = 0.0;
      double homogeneity_ppm = 0.0;  // peak-to-peak |B| / mean |B|
      double field_shift_ppm = 0.0;  // signed change of mean |B| from nominal
    };

    struct ToleranceTrial {
      uint32_t trial = 0;  // random trials start at 1; 0 is nominal in CSV export
      bool valid = false;
      bool passed = false;
      ToleranceMetrics metrics;
      std::string error;
    };

    struct ToleranceStatistics {
      double mean = 0.0;
      double stddev = 0.0;  // sample standard deviation, zero with a single trial
      double min = 0.0;
      double p05 = 0.0;
      double median = 0.0;
      double p95 = 0.0;
      double max = 0.0;
    };

    struct ToleranceReport {
      ToleranceSettings settings;
      std::string source;
      std::string backend;
      nlohmann::json input;  // original scene, for reproducibility
      nlohmann::json sampling;
      size_t magnets = 0;
      size_t points = 0;
      ToleranceMetrics nominal;
      std::vector<ToleranceTrial> trials;
      size_t valid_trials = 0;
      size_t passed_trials = 0;  // denominator is ALL trials, including invalid draws
      ToleranceStatistics mean_field;
      ToleranceStatistics homogeneity;
      ToleranceStatistics field_shift;
      bool cancelled = false;
    };

    ToleranceStatistics toleranceStatistics(std::vector<double> values);
    nlohmann::json toleranceReportJSON(const ToleranceReport& report);
    void writeToleranceJSON(const ToleranceReport& report, const std::string& path);
    void writeToleranceCSV(const ToleranceReport& report, const std::string& path);

  }  // namespace service
}  // namespace greeter
#endif
