#ifndef GREETER_ROBUST_OPTIMIZATION_H
#define GREETER_ROBUST_OPTIMIZATION_H

#include <greeter/service/ToleranceAnalysis.h>

namespace greeter {
  namespace service {
    class ProgressSink;

    enum class RobustObjective { P95Homogeneity, P05MeanField, AcceptanceRate };

    struct RobustOptimizationSettings {
      RobustOptimizationSettings();
      ToleranceSettings screening;
      uint32_t shortlist = 8;
      uint32_t population = 256;
      uint32_t generations = 30;
      uint64_t search_seed = 42;
      uint32_t validation_trials = 200;
      uint64_t validation_seed = 2026;
      uint32_t validation_points_per_axis = 11;
      RobustObjective objective = RobustObjective::P95Homogeneity;
      double required_acceptance = 0.95;
      void validate() const;
    };

    struct RobustOptimizationRequest {
      nlohmann::json configuration;
      std::string source;
      RobustOptimizationSettings settings;
    };

    struct RobustCandidate {
      std::vector<uint16_t> genome;
      double nominal_search_ppm = 0.0;
      ToleranceReport screening;
    };

    struct RobustOptimizationReport {
      RobustOptimizationRequest request;
      nlohmann::json effective_spec;
      nlohmann::json search_history;
      std::string search_stop_reason;
      std::string backend;
      std::vector<RobustCandidate> candidates;  // nominal search order, best first
      size_t selected = 0;
      ToleranceReport validation;
      nlohmann::json selected_design;
      bool complete = false;
      bool cancelled = false;
    };

    bool robustEligible(const ToleranceReport& study, RobustObjective objective);
    bool robustMeetsRequirements(const ToleranceReport& study,
                                 const RobustOptimizationSettings& settings);
    std::vector<size_t> robustCandidateOrder(const std::vector<RobustCandidate>& candidates,
                                             const RobustOptimizationSettings& settings);
    RobustOptimizationReport optimizeRobustly(const RobustOptimizationRequest& request,
                                              ProgressSink* progress = nullptr);
    nlohmann::json robustOptimizationJSON(const RobustOptimizationReport& report);
    void writeRobustOptimizationJSON(const RobustOptimizationReport& report,
                                     const std::string& path);
    void writeRobustOptimizationCSV(const RobustOptimizationReport& report,
                                    const std::string& path);
    void writeRobustDesign(const RobustOptimizationReport& report, const std::string& path);
  }  // namespace service
}  // namespace greeter
#endif
