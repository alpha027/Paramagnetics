#ifndef GREETER_SENSITIVITY_ANALYSIS_H
#define GREETER_SENSITIVITY_ANALYSIS_H

#include <greeter/service/ToleranceAnalysis.h>

namespace greeter {
  namespace service {

    enum class SensitivityGrouping { SourcesOnly, Magnets, Arrangements };
    enum class SensitivityMetric { Homogeneity, FieldShift };

    struct SensitivitySettings {
      ToleranceSettings tolerances;
      SensitivityGrouping grouping = SensitivityGrouping::SourcesOnly;
      uint32_t sweep_points = 5;  // includes the nominal (zero tolerance) endpoint
      void validate() const;
    };

    struct SensitivityPoint {
      double scale = 1.0;  // multiplier of the configured tolerances
      ToleranceReport study;
      double homogeneity_rms_ppm = 0.0;  // RMS change from nominal, including bias
      double field_shift_rms_ppm = 0.0;
    };

    struct SensitivityCase {
      std::string kind;  // source, magnet or arrangement; rank each kind separately
      std::string key;
      std::string label;
      std::vector<uint32_t> magnet_indices;
      std::vector<int64_t> magnet_ids;
      std::vector<SensitivityPoint> curve;  // positive scales; zero is report.nominal
    };

    struct SensitivityReport {
      SensitivitySettings settings;
      std::string source;
      std::string backend;
      nlohmann::json input;
      nlohmann::json sampling;
      size_t magnets = 0;
      size_t points = 0;
      ToleranceMetrics nominal;
      std::vector<SensitivityCase> cases;
      bool cancelled = false;
      bool complete = false;
    };

    // Descending RMS impact at full tolerance, within a case's kind. Zero means
    // insufficient valid trials (fewer than two). Ranks are not variance shares.
    std::vector<size_t> sensitivityRanks(const SensitivityReport& report, SensitivityMetric metric);
    nlohmann::json sensitivityReportJSON(const SensitivityReport& report);
    void writeSensitivityJSON(const SensitivityReport& report, const std::string& path);
    void writeSensitivityCSV(const SensitivityReport& report, const std::string& path);

  }  // namespace service
}  // namespace greeter
#endif
