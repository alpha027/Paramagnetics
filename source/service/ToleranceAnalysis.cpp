#include <greeter/service/ToleranceAnalysis.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace greeter {
  namespace service {

    void ToleranceSettings::validate() const {
      if (trials < 1 || trials > 100000) throw std::invalid_argument("Use 1 to 100000 trials");
      if (distribution != ToleranceDistribution::Uniform
          && distribution != ToleranceDistribution::Normal)
        throw std::invalid_argument("Unknown tolerance distribution");
      if (region != ToleranceRegion::Box && region != ToleranceRegion::InscribedSphere)
        throw std::invalid_argument("Unknown tolerance region");
      if (points_per_axis > 101) throw std::invalid_argument("Use at most 101 samples per axis");
      for (double value : {strength_percent, position_mm, direction_degrees, body_tilt_degrees,
                           size_percent, homogeneity_limit_ppm, minimum_mean_T}) {
        if (!std::isfinite(value) || value < 0.0)
          throw std::invalid_argument(
              "Tolerances and acceptance limits must be finite and nonnegative");
      }
      if (strength_percent > 100.0 || size_percent > 100.0)
        throw std::invalid_argument("Strength and size tolerances must not exceed 100 percent");
      if (direction_degrees > 180.0 || body_tilt_degrees > 180.0)
        throw std::invalid_argument("Rotation tolerances must not exceed 180 degrees per axis");
    }

    bool ToleranceSettings::hasLimits() const {
      return homogeneity_limit_ppm > 0.0 || minimum_mean_T > 0.0;
    }

    ToleranceStatistics toleranceStatistics(std::vector<double> values) {
      ToleranceStatistics result;
      if (values.empty()) return result;
      // Welford's algorithm avoids cancellation for a nearly constant field.
      double m2 = 0.0;
      size_t count = 0;
      for (double value : values) {
        if (!std::isfinite(value)) throw std::invalid_argument("Statistics require finite values");
        const double delta = value - result.mean;
        result.mean += delta / ++count;
        m2 += delta * (value - result.mean);
      }
      result.stddev = count > 1 ? std::sqrt(std::max(0.0, m2 / (count - 1))) : 0.0;
      std::sort(values.begin(), values.end());
      auto quantile = [&](double fraction) {
        const double index = fraction * (values.size() - 1);
        const size_t low = (size_t)index;
        const size_t high = std::min(low + 1, values.size() - 1);
        return values[low] + (index - low) * (values[high] - values[low]);
      };
      result.min = values.front();
      result.max = values.back();
      result.p05 = quantile(0.05);
      result.median = quantile(0.5);
      result.p95 = quantile(0.95);
      return result;
    }

    namespace {
      nlohmann::json metrics(const ToleranceMetrics& m) {
        return {{"mean_T", m.mean_T},
                {"min_T", m.min_T},
                {"max_T", m.max_T},
                {"homogeneity_ppm", m.homogeneity_ppm},
                {"field_shift_ppm", m.field_shift_ppm}};
      }
      nlohmann::json statistics(const ToleranceStatistics& s) {
        return {{"mean", s.mean},     {"stddev", s.stddev}, {"min", s.min}, {"p05", s.p05},
                {"median", s.median}, {"p95", s.p95},       {"max", s.max}};
      }
      void requireComplete(const ToleranceReport& report) {
        if (report.cancelled || report.trials.size() != report.settings.trials)
          throw std::invalid_argument("Only a completed tolerance study can be exported");
      }
      std::ofstream outputFile(const std::string& path) {
        std::ofstream file;
        file.exceptions(std::ios::failbit | std::ios::badbit);
        file.open(path);
        file.precision(17);
        return file;
      }
    }  // namespace

    nlohmann::json toleranceSettingsJSON(const ToleranceSettings& s) {
      return {
          {"trials", s.trials},
          {"seed", s.seed},
          {"distribution", s.distribution == ToleranceDistribution::Uniform ? "uniform" : "normal"},
          {"region", s.region == ToleranceRegion::Box ? "box" : "inscribed_sphere"},
          {"points_per_axis", s.points_per_axis},
          {"strength_percent", s.strength_percent},
          {"position_mm", s.position_mm},
          {"direction_degrees_per_axis", s.direction_degrees},
          {"body_tilt_degrees_per_axis", s.body_tilt_degrees},
          {"size_percent", s.size_percent},
          {"homogeneity_limit_ppm", s.homogeneity_limit_ppm},
          {"minimum_mean_T", s.minimum_mean_T}};
    }

    nlohmann::json toleranceReportJSON(const ToleranceReport& r) {
      requireComplete(r);
      const auto& s = r.settings;
      nlohmann::json trials = nlohmann::json::array();
      for (const auto& t : r.trials) {
        trials.push_back({{"trial", t.trial},
                          {"valid", t.valid},
                          {"passed", t.passed},
                          {"metrics", t.valid ? metrics(t.metrics) : nlohmann::json(nullptr)},
                          {"error", t.error}});
      }
      return {
          {"format", "paramagnetics-tolerance"},
          {"version", 1},
          {"source", r.source},
          {"backend", r.backend},
          {"input", r.input},
          {"sampling", r.sampling},
          {"sampler", "mt19937_64, Box-Muller normal draws, per-trial seed stream v1"},
          {"precision", "float32 kernels and samples, float64 accumulation and statistics"},
          {"magnet_count", r.magnets},
          {"sample_count", r.points},
          {"settings", toleranceSettingsJSON(s)},
          {"nominal", metrics(r.nominal)},
          {"trials", trials},
          {"summary",
           {{"valid_trials", r.valid_trials},
            {"invalid_trials", r.trials.size() - r.valid_trials},
            {"passed_trials",
             s.hasLimits() ? nlohmann::json(r.passed_trials) : nlohmann::json(nullptr)},
            {"pass_fraction", s.hasLimits()
                                  ? nlohmann::json((double)r.passed_trials / r.trials.size())
                                  : nlohmann::json(nullptr)},
            {"mean_field_T", r.valid_trials ? statistics(r.mean_field) : nlohmann::json(nullptr)},
            {"homogeneity_ppm",
             r.valid_trials ? statistics(r.homogeneity) : nlohmann::json(nullptr)},
            {"field_shift_ppm",
             r.valid_trials ? statistics(r.field_shift) : nlohmann::json(nullptr)}}},
          {"model",
           "Independent per-magnet errors; isotropic size scaling about the local origin; "
           "fixed-polarization B kernels; no collision or material-response checks. Statistics use "
           "valid trials; acceptance counts invalid trials as failures."}};
    }

    void writeToleranceJSON(const ToleranceReport& report, const std::string& path) {
      const auto data = toleranceReportJSON(report);
      auto file = outputFile(path);
      file << data.dump(2) << '\n';
      file.close();
    }

    void writeToleranceCSV(const ToleranceReport& report, const std::string& path) {
      requireComplete(report);
      auto file = outputFile(path);
      file << "trial,valid,mean_T,min_T,max_T,homogeneity_ppm,field_shift_ppm,passed,error\n";
      auto row = [&](const ToleranceTrial& t) {
        file << t.trial << ',' << (t.valid ? 1 : 0) << ',';
        if (t.valid)
          file << t.metrics.mean_T << ',' << t.metrics.min_T << ',' << t.metrics.max_T << ','
               << t.metrics.homogeneity_ppm << ',' << t.metrics.field_shift_ppm;
        else
          file << ",,,,";
        file << ',';
        if (report.settings.hasLimits()) file << (t.passed ? 1 : 0);
        file << ",\"";
        for (char c : t.error) {
          if (c == '"') file << '"';
          file << c;
        }
        file << "\"\n";
      };
      ToleranceTrial nominal;
      nominal.valid = true;
      nominal.metrics = report.nominal;
      nominal.passed = (report.settings.homogeneity_limit_ppm <= 0.0
                        || report.nominal.homogeneity_ppm <= report.settings.homogeneity_limit_ppm)
                       && report.nominal.mean_T >= report.settings.minimum_mean_T;
      row(nominal);
      for (const auto& t : report.trials) row(t);
      file.close();
    }

  }  // namespace service
}  // namespace greeter
