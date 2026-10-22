#include <greeter/service/RobustOptimization.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <numeric>
#include <stdexcept>
#include <unordered_set>

namespace greeter {
  namespace service {
    RobustOptimizationSettings::RobustOptimizationSettings() {
      screening.trials = 50;
      screening.points_per_axis = 7;
      screening.region = ToleranceRegion::InscribedSphere;
    }

    void RobustOptimizationSettings::validate() const {
      screening.validate();
      if (screening.trials < 2 || validation_trials < 2 || validation_trials > 100000)
        throw std::invalid_argument(
            "Use at least two screening and validation trials (at most 100000)");
      if (shortlist < 1 || shortlist > 32 || population < 2 || population > 100000
          || generations < 1 || generations > 10000)
        throw std::invalid_argument(
            "Use 1–32 finalists, population 2–100000 and 1–10000 generations");
      if (screening.region != ToleranceRegion::InscribedSphere)
        throw std::invalid_argument("Robust optimization measures the full DSV sphere");
      for (auto density : {screening.points_per_axis, validation_points_per_axis})
        if (density < 3 || density > 101 || density % 2 == 0)
          throw std::invalid_argument("Use an odd analysis grid density from 3 to 101");
      if (objective != RobustObjective::P95Homogeneity && objective != RobustObjective::P05MeanField
          && objective != RobustObjective::AcceptanceRate)
        throw std::invalid_argument("Unknown robust objective");
      if (!std::isfinite(required_acceptance) || required_acceptance < 0 || required_acceptance > 1)
        throw std::invalid_argument("Required acceptance must be between zero and one");
      if (objective == RobustObjective::AcceptanceRate && !screening.hasLimits())
        throw std::invalid_argument(
            "Acceptance optimization needs a homogeneity or minimum-field limit");
      // A different base seed alone is insufficient: per-trial streams could
      // overlap after an offset by the odd seed-stride constant.
      constexpr uint64_t stride = 0x9e3779b97f4a7c15ULL;
      std::unordered_set<uint64_t> streams;
      for (uint64_t i = 1; i <= screening.trials; ++i) streams.insert(screening.seed + stride * i);
      for (uint64_t i = 1; i <= validation_trials; ++i)
        if (streams.count(validation_seed + stride * i))
          throw std::invalid_argument(
              "Validation must use fresh random streams; choose another validation seed");
    }

    bool robustEligible(const ToleranceReport& r, RobustObjective objective) {
      return !r.cancelled && r.trials.size() == r.settings.trials && r.valid_trials >= 2
             && (objective == RobustObjective::AcceptanceRate || r.valid_trials == r.trials.size());
    }

    bool robustMeetsRequirements(const ToleranceReport& r, const RobustOptimizationSettings& s) {
      if (!robustEligible(r, s.objective)) return false;
      if (s.objective == RobustObjective::AcceptanceRate)
        return (double)r.passed_trials / r.trials.size() >= s.required_acceptance;
      return (s.screening.homogeneity_limit_ppm <= 0
              || r.homogeneity.p95 <= s.screening.homogeneity_limit_ppm)
             && r.mean_field.p05 >= s.screening.minimum_mean_T;
    }

    std::vector<size_t> robustCandidateOrder(const std::vector<RobustCandidate>& candidates,
                                             const RobustOptimizationSettings& s) {
      std::vector<size_t> order(candidates.size());
      std::iota(order.begin(), order.end(), 0);
      auto score = [&](const ToleranceReport& r) {
        if (s.objective == RobustObjective::P95Homogeneity) return r.homogeneity.p95;
        if (s.objective == RobustObjective::P05MeanField) return -r.mean_field.p05;
        return -(double)r.passed_trials / r.trials.size();
      };
      std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        const auto& x = candidates[a];
        const auto& y = candidates[b];
        const bool ex = robustEligible(x.screening, s.objective),
                   ey = robustEligible(y.screening, s.objective);
        if (ex != ey) return ex;
        const bool fx = robustMeetsRequirements(x.screening, s),
                   fy = robustMeetsRequirements(y.screening, s);
        if (fx != fy) return fx;
        if (ex && score(x.screening) != score(y.screening))
          return score(x.screening) < score(y.screening);
        if (x.nominal_search_ppm != y.nominal_search_ppm)
          return x.nominal_search_ppm < y.nominal_search_ppm;
        return x.genome < y.genome;
      });
      return order;
    }

    namespace {
      const char* objectiveName(RobustObjective o) {
        return o == RobustObjective::P95Homogeneity ? "minimize_p95_homogeneity"
               : o == RobustObjective::P05MeanField ? "maximize_p05_mean_field"
                                                    : "maximize_acceptance_rate";
      }
      void complete(const RobustOptimizationReport& r) {
        if (!r.complete || r.cancelled || r.candidates.empty() || r.selected >= r.candidates.size()
            || !r.selected_design.is_object())
          throw std::invalid_argument("Only a completed robust optimization can be exported");
        r.request.settings.validate();
        for (const auto& c : r.candidates) (void)toleranceReportJSON(c.screening);
        (void)toleranceReportJSON(r.validation);
      }
      std::ofstream output(const std::string& path) {
        std::ofstream file;
        file.exceptions(std::ios::failbit | std::ios::badbit);
        file.open(path);
        file.precision(17);
        return file;
      }
    }  // namespace

    nlohmann::json robustOptimizationJSON(const RobustOptimizationReport& r) {
      complete(r);
      const auto& s = r.request.settings;
      const auto order = robustCandidateOrder(r.candidates, s);
      std::vector<size_t> ranks(order.size());
      for (size_t i = 0; i < order.size(); ++i) ranks[order[i]] = i + 1;
      nlohmann::json candidates = nlohmann::json::array();
      for (size_t i = 0; i < r.candidates.size(); ++i) {
        const auto& c = r.candidates[i];
        candidates.push_back({{"nominal_rank", i + 1},
                              {"robust_rank", ranks[i]},
                              {"genome", c.genome},
                              {"nominal_search_ppm", c.nominal_search_ppm},
                              {"eligible", robustEligible(c.screening, s.objective)},
                              {"requirements_met", robustMeetsRequirements(c.screening, s)},
                              {"selected", i == r.selected},
                              {"screening", toleranceReportJSON(c.screening)}});
      }
      return {
          {"format", "paramagnetics-robust-optimization"},
          {"version", 1},
          {"source", r.request.source},
          {"configuration", r.request.configuration},
          {"effective_spec", r.effective_spec},
          {"backend", r.backend},
          {"settings",
           {{"objective", objectiveName(s.objective)},
            {"shortlist", s.shortlist},
            {"population", s.population},
            {"generations", s.generations},
            {"search_seed", s.search_seed},
            {"screening", toleranceSettingsJSON(s.screening)},
            {"validation_trials", s.validation_trials},
            {"validation_seed", s.validation_seed},
            {"validation_points_per_axis", s.validation_points_per_axis},
            {"required_acceptance", s.required_acceptance}}},
          {"search_history", r.search_history},
          {"search_stop_reason", r.search_stop_reason},
          {"candidates", std::move(candidates)},
          {"selected_nominal_rank", r.selected + 1},
          {"validation", toleranceReportJSON(r.validation)},
          {"validation_requirements_met", robustMeetsRequirements(r.validation, s)},
          {"selected_design", r.selected_design},
          {"method",
           "Re-rank distinct nominal |B| finalists visited by a genetic search. Screen the full "
           "DSV sphere with matched per-trial seed streams; different magnet counts change draw "
           "mappings. Rank eligible, requirement-meeting candidates first, then the requested "
           "robust objective, then nominal search ppm and genome. P95/P5 objectives require every "
           "trial valid; acceptance counts invalid trials as failures. P95 homogeneity and P5 "
           "mean-field limits are separate empirical constraints, not a joint probability "
           "guarantee. Validate only the selected design with disjoint random streams, never "
           "reselect using validation. An unmet requirement is reported, not silently declared "
           "successful. This is shortlist screening, not a global robust optimum or confidence "
           "bound. Fixed-polarization model; no collision, thermal or material-response checks."}};
    }

    void writeRobustOptimizationJSON(const RobustOptimizationReport& r, const std::string& path) {
      const auto data = robustOptimizationJSON(r);
      auto file = output(path);
      file << data.dump(2) << '\n';
      file.close();
    }
    void writeRobustDesign(const RobustOptimizationReport& r, const std::string& path) {
      complete(r);
      auto file = output(path);
      file << r.selected_design.dump(2) << '\n';
      file.close();
    }
    void writeRobustOptimizationCSV(const RobustOptimizationReport& r, const std::string& path) {
      complete(r);
      const auto& s = r.request.settings;
      const auto order = robustCandidateOrder(r.candidates, s);
      std::vector<size_t> ranks(order.size());
      for (size_t i = 0; i < order.size(); ++i) ranks[order[i]] = i + 1;
      auto file = output(path);
      file << "phase,nominal_rank,robust_rank,selected,genome,nominal_search_ppm,nominal_mean_T,"
              "nominal_homogeneity_ppm,p95_homogeneity_ppm,p05_mean_T,pass_fraction,valid_trials,"
              "total_trials,eligible,requirements_met,seed,samples\n";
      auto row = [&](const char* phase, size_t i, const ToleranceReport& t) {
        const auto& c = r.candidates[i];
        file << phase << ',' << i + 1 << ',' << ranks[i] << ',' << (i == r.selected) << ",\""
             << nlohmann::json(c.genome).dump() << "\"," << c.nominal_search_ppm << ','
             << t.nominal.mean_T << ',' << t.nominal.homogeneity_ppm << ',';
        if (t.valid_trials) file << t.homogeneity.p95;
        file << ',';
        if (t.valid_trials) file << t.mean_field.p05;
        file << ',';
        if (t.settings.hasLimits()) file << (double)t.passed_trials / t.trials.size();
        file << ',' << t.valid_trials << ',' << t.trials.size() << ','
             << robustEligible(t, s.objective) << ',' << robustMeetsRequirements(t, s) << ','
             << t.settings.seed << ',' << t.points << '\n';
      };
      for (auto i : order) row("screening", i, r.candidates[i].screening);
      row("validation", r.selected, r.validation);
      file.close();
    }
  }  // namespace service
}  // namespace greeter
