#include <greeter/service/SensitivityAnalysis.h>

#include <algorithm>
#include <fstream>
#include <numeric>
#include <stdexcept>

namespace greeter {
  namespace service {
    void SensitivitySettings::validate() const {
      tolerances.validate();
      if (tolerances.trials < 2)
        throw std::invalid_argument("Sensitivity analysis needs at least two trials per point");
      if (sweep_points < 2 || sweep_points > 9)
        throw std::invalid_argument("Use 2 to 9 tolerance sweep points, including zero");
      if (grouping != SensitivityGrouping::SourcesOnly && grouping != SensitivityGrouping::Magnets
          && grouping != SensitivityGrouping::Arrangements)
        throw std::invalid_argument("Unknown sensitivity grouping");
      if (tolerances.strength_percent == 0 && tolerances.position_mm == 0
          && tolerances.direction_degrees == 0 && tolerances.body_tilt_degrees == 0
          && tolerances.size_percent == 0)
        throw std::invalid_argument(
            "Enable at least one nonzero tolerance for sensitivity analysis");
    }

    std::vector<size_t> sensitivityRanks(const SensitivityReport& report,
                                         SensitivityMetric metric) {
      std::vector<size_t> order(report.cases.size()), ranks(report.cases.size(), 0);
      std::iota(order.begin(), order.end(), 0);
      auto score = [&](size_t i) {
        const auto& point = report.cases[i].curve.back();
        return metric == SensitivityMetric::Homogeneity ? point.homogeneity_rms_ppm
                                                        : point.field_shift_rms_ppm;
      };
      order.erase(std::remove_if(order.begin(), order.end(),
                                 [&](size_t i) {
                                   return report.cases[i].curve.empty()
                                          || report.cases[i].curve.back().study.valid_trials < 2;
                                 }),
                  order.end());
      std::stable_sort(order.begin(), order.end(),
                       [&](size_t a, size_t b) { return score(a) > score(b); });
      std::vector<std::string> kinds;
      std::vector<size_t> counts;
      for (size_t index : order) {
        const auto& kind = report.cases[index].kind;
        auto found = std::find(kinds.begin(), kinds.end(), kind);
        if (found == kinds.end()) {
          kinds.push_back(kind);
          counts.push_back(0);
        }
        ranks[index] = ++counts[std::find(kinds.begin(), kinds.end(), kind) - kinds.begin()];
      }
      return ranks;
    }

    namespace {
      void requireComplete(const SensitivityReport& r) {
        if (!r.complete || r.cancelled || r.cases.empty())
          throw std::invalid_argument("Only a completed sensitivity study can be exported");
        r.settings.validate();
        for (const auto& c : r.cases) {
          if (c.curve.size() != r.settings.sweep_points - 1)
            throw std::invalid_argument("Incomplete sensitivity response curve");
          for (const auto& p : c.curve)
            if (p.study.cancelled || p.study.trials.size() != r.settings.tolerances.trials)
              throw std::invalid_argument("Incomplete sensitivity trial results");
        }
      }
      std::ofstream output(const std::string& path) {
        std::ofstream file;
        file.exceptions(std::ios::failbit | std::ios::badbit);
        file.open(path);
        file.precision(17);
        return file;
      }
      std::string quoted(const std::string& value) {
        std::string result = "\"";
        for (char c : value) {
          if (c == '"') result += '"';
          result += c;
        }
        return result + '"';
      }
    }  // namespace

    nlohmann::json sensitivityReportJSON(const SensitivityReport& r) {
      requireComplete(r);
      auto homogeneity_ranks = sensitivityRanks(r, SensitivityMetric::Homogeneity);
      auto field_ranks = sensitivityRanks(r, SensitivityMetric::FieldShift);
      nlohmann::json cases = nlohmann::json::array();
      for (size_t i = 0; i < r.cases.size(); ++i) {
        const auto& c = r.cases[i];
        nlohmann::json curve = nlohmann::json::array();
        for (const auto& p : c.curve) {
          auto study = toleranceReportJSON(p.study);
          // Scene and common provenance are stored once at the root. The case's
          // membership, rather than this sub-study's generic model, defines scope.
          for (const char* field : {"input", "source", "backend", "sampling", "model"})
            study.erase(field);
          curve.push_back(
              {{"scale", p.scale},
               {"homogeneity_rms_change_ppm", p.study.valid_trials
                                                  ? nlohmann::json(p.homogeneity_rms_ppm)
                                                  : nlohmann::json(nullptr)},
               {"field_shift_rms_ppm", p.study.valid_trials ? nlohmann::json(p.field_shift_rms_ppm)
                                                            : nlohmann::json(nullptr)},
               {"study", std::move(study)}});
        }
        cases.push_back(
            {{"kind", c.kind},
             {"key", c.key},
             {"label", c.label},
             {"magnet_indices", c.magnet_indices},
             {"magnet_ids", c.magnet_ids},
             {"rank_homogeneity", homogeneity_ranks[i] ? nlohmann::json(homogeneity_ranks[i])
                                                       : nlohmann::json(nullptr)},
             {"rank_field_shift",
              field_ranks[i] ? nlohmann::json(field_ranks[i]) : nlohmann::json(nullptr)},
             {"curve", std::move(curve)}});
      }
      return {
          {"format", "paramagnetics-sensitivity"},
          {"version", 1},
          {"source", r.source},
          {"backend", r.backend},
          {"input", r.input},
          {"sampling", r.sampling},
          {"magnet_count", r.magnets},
          {"sample_count", r.points},
          {"settings",
           {{"tolerances", toleranceSettingsJSON(r.settings.tolerances)},
            {"sweep_points", r.settings.sweep_points},
            {"grouping", r.settings.grouping == SensitivityGrouping::SourcesOnly ? "sources_only"
                         : r.settings.grouping == SensitivityGrouping::Magnets   ? "magnets"
                                                                               : "arrangements"}}},
          {"nominal",
           {{"mean_T", r.nominal.mean_T},
            {"min_T", r.nominal.min_T},
            {"max_T", r.nominal.max_T},
            {"homogeneity_ppm", r.nominal.homogeneity_ppm},
            {"field_shift_ppm", r.nominal.field_shift_ppm}}},
          {"cases", std::move(cases)},
          {"method",
           "One-at-a-time Monte Carlo screening. Source rows vary one enabled error type on all "
           "magnets; magnet/arrangement rows vary all enabled error types on only their members, "
           "independently. Positive sweep scales multiply the configured tolerances; scale zero is "
           "the nominal design. Seeded draws are matched across positive scales and group "
           "comparisons. Scores are RMS differences from nominal over valid trials, including "
           "bias. Rank within each kind by full-tolerance score; fewer than two valid trials are "
           "unranked. Scores are not additive variance shares and do not measure interactions. "
           "Invalid trials count as acceptance failures; no collision, thermal or "
           "material-response checks."}};
    }

    void writeSensitivityJSON(const SensitivityReport& report, const std::string& path) {
      const auto data = sensitivityReportJSON(report);
      auto file = output(path);
      file << data.dump(2) << '\n';
      file.close();
    }

    void writeSensitivityCSV(const SensitivityReport& r, const std::string& path) {
      requireComplete(r);
      const auto h = sensitivityRanks(r, SensitivityMetric::Homogeneity);
      const auto f = sensitivityRanks(r, SensitivityMetric::FieldShift);
      auto file = output(path);
      file << "kind,key,label,magnet_ids,scale,strength_percent,position_mm,direction_degrees_per_"
              "axis,body_tilt_degrees_per_axis,size_percent,seed,distribution,rank_homogeneity_at_"
              "full_tolerance,rank_field_shift_at_full_tolerance,valid_trials,invalid_trials,"
              "homogeneity_rms_change_ppm,field_shift_rms_ppm,mean_field_T,mean_homogeneity_ppm,"
              "p95_homogeneity_ppm,pass_fraction\n";
      for (size_t i = 0; i < r.cases.size(); ++i) {
        const auto& c = r.cases[i];
        for (const auto& p : c.curve) {
          file << quoted(c.kind) << ',' << quoted(c.key) << ',' << quoted(c.label) << ','
               << quoted(nlohmann::json(c.magnet_ids).dump()) << ',' << p.scale << ',';
          const auto& s = p.study.settings;
          file << s.strength_percent << ',' << s.position_mm << ',' << s.direction_degrees << ','
               << s.body_tilt_degrees << ',' << s.size_percent << ',' << s.seed << ','
               << (s.distribution == ToleranceDistribution::Uniform ? "uniform" : "normal") << ',';
          if (h[i]) file << h[i];
          file << ',';
          if (f[i]) file << f[i];
          file << ',' << p.study.valid_trials << ',' << p.study.trials.size() - p.study.valid_trials
               << ',';
          if (p.study.valid_trials)
            file << p.homogeneity_rms_ppm << ',' << p.field_shift_rms_ppm << ','
                 << p.study.mean_field.mean << ',' << p.study.homogeneity.mean << ','
                 << p.study.homogeneity.p95;
          else
            file << ",,,,";
          file << ',';
          if (p.study.settings.hasLimits())
            file << (double)p.study.passed_trials / p.study.trials.size();
          file << '\n';
        }
      }
      file.close();
    }
  }  // namespace service
}  // namespace greeter
