#include <greeter/analysis/SensitivityAnalyzer.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>

namespace greeter {
  namespace analysis {
    namespace {
      using namespace service;
      using Amount = double ToleranceSettings::*;
      struct Source {
        const char* key;
        const char* label;
        Amount amount;
      };
      const Source sources[]
          = {{"strength", "Strength / remanence", &ToleranceSettings::strength_percent},
             {"position", "Position", &ToleranceSettings::position_mm},
             {"direction", "Magnetization direction", &ToleranceSettings::direction_degrees},
             {"body_tilt", "Body tilt", &ToleranceSettings::body_tilt_degrees},
             {"size", "Isotropic size", &ToleranceSettings::size_percent}};

      // A fixed unit count per study keeps progress monotonic when the baseline
      // has fewer trials. The inner engine still checks cancellation per chunk.
      class StudyProgress : public ProgressSink {
      public:
        ProgressSink* sink;
        size_t index = 0;
        size_t count;
        StudyProgress(ProgressSink* sink, size_t count) : sink(sink), count(count) {}
        bool onProgress(const size_t& done, const size_t& total) override {
          return !sink
                 || sink->onProgress(index * 1000000 + (total ? 1000000 * done / total : 0),
                                     count * 1000000);
        }
      };
    }  // namespace

    service::SensitivityReport analyzeSensitivity(const MagnetCollection& collection,
                                                  const view::SceneSnapshot& scene,
                                                  const service::FieldRequest& request,
                                                  const service::SensitivitySettings& settings,
                                                  service::ProgressSink* progress,
                                                  const service::RunOptions& options) {
      using namespace service;
      settings.validate();
      SensitivityReport report;
      report.settings = settings;
      report.magnets = collection.get_num_magnets();
      if (report.magnets == 0 || scene.magnets.size() != report.magnets)
        throw std::invalid_argument("Sensitivity analysis needs a matching, nonempty scene");

      std::vector<Amount> source_amounts;
      auto add = [&](const std::string& kind, const std::string& key, const std::string& label,
                     std::vector<uint32_t> members, Amount amount) {
        SensitivityCase c;
        c.kind = kind;
        c.key = key;
        c.label = label;
        c.magnet_indices = std::move(members);
        for (auto index : c.magnet_indices) c.magnet_ids.push_back(scene.magnets.at(index).id);
        report.cases.push_back(std::move(c));
        source_amounts.push_back(amount);
      };
      std::vector<uint32_t> all(report.magnets);
      std::iota(all.begin(), all.end(), 0);
      for (const auto& source : sources)
        if (settings.tolerances.*(source.amount) > 0.0)
          add("source", source.key, source.label, all, source.amount);

      std::vector<bool> grouped(report.magnets, false);
      if (settings.grouping == SensitivityGrouping::Arrangements) {
        for (const auto& arrangement : scene.arrangements) {
          if (arrangement.members.empty()) continue;
          for (auto member : arrangement.members) {
            if (member >= report.magnets || grouped[member])
              throw std::invalid_argument("Arrangement members must be unique and in range");
            grouped[member] = true;
          }
          add("arrangement", "arrangement:" + std::to_string(arrangement.id),
              "Arrangement " + std::to_string(arrangement.id) + " (" + arrangement.type + ")",
              arrangement.members, nullptr);
        }
      }
      if (settings.grouping != SensitivityGrouping::SourcesOnly) {
        for (uint32_t index = 0; index < report.magnets; ++index) {
          if (grouped[index]) continue;
          add("magnet", "magnet:" + std::to_string(scene.magnets[index].id),
              "Magnet " + std::to_string(scene.magnets[index].id), {index}, nullptr);
        }
      }
      const size_t count = 1 + report.cases.size() * (settings.sweep_points - 1);
      if (count > std::numeric_limits<size_t>::max() / 1000000)
        throw std::invalid_argument("Sensitivity study exceeds this platform's work capacity");
      StudyProgress combined(progress, count);

      auto baseline_settings = settings.tolerances;
      baseline_settings.trials = 1;
      for (const auto& source : sources) baseline_settings.*(source.amount) = 0.0;
      auto baseline = analyzeTolerance(collection, request, baseline_settings, &combined, options);
      if (baseline.cancelled) {
        report.cancelled = true;
        return report;
      }
      report.nominal = baseline.nominal;
      report.backend = baseline.backend;
      report.sampling = baseline.sampling;
      report.points = baseline.points;
      ++combined.index;

      for (size_t i = 0; i < report.cases.size(); ++i) {
        auto& c = report.cases[i];
        for (uint32_t step = 1; step < settings.sweep_points; ++step) {
          const double scale = (double)step / (settings.sweep_points - 1);
          auto s = settings.tolerances;
          for (const auto& source : sources) {
            // Source rows vary one error type on all magnets. Group rows vary
            // all enabled error types on only that group's members, independently.
            s.*(source.amount)
                *= (!source_amounts[i] || source_amounts[i] == source.amount) ? scale : 0.0;
          }
          SensitivityPoint point;
          point.scale = scale;
          point.study = analyzeTolerance(collection, request, s, &combined, options,
                                         c.kind == "source" ? nullptr : &c.magnet_indices);
          if (point.study.cancelled) {
            report.cancelled = true;
            return report;
          }
          double homogeneity2 = 0.0, shift2 = 0.0;
          for (const auto& trial : point.study.trials) {
            if (!trial.valid) continue;
            const double delta = trial.metrics.homogeneity_ppm - report.nominal.homogeneity_ppm;
            homogeneity2 += delta * delta;
            shift2 += trial.metrics.field_shift_ppm * trial.metrics.field_shift_ppm;
          }
          if (point.study.valid_trials) {
            point.homogeneity_rms_ppm = std::sqrt(homogeneity2 / point.study.valid_trials);
            point.field_shift_rms_ppm = std::sqrt(shift2 / point.study.valid_trials);
          }
          c.curve.push_back(std::move(point));
          ++combined.index;
        }
      }
      report.complete = true;
      return report;
    }
  }  // namespace analysis
}  // namespace greeter
