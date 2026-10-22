#include <greeter/io/HalbachOptimizationIO.h>
#include <greeter/optimization/HalbachBasis.h>
#include <greeter/service/RobustOptimization.h>
#include <greeter/service/SimulationService.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace greeter {
  namespace service {
    namespace {
      class MappedProgress : public ProgressSink {
      public:
        ProgressSink* parent;
        size_t first, length;
        MappedProgress(ProgressSink* parent, size_t first, size_t length)
            : parent(parent), first(first), length(length) {}
        bool onProgress(const size_t& done, const size_t& total) override {
          return !parent || parent->onProgress(first + (total ? length * done / total : 0), 10000);
        }
      };
      class SearchProgress : public optimization::GenerationSink {
      public:
        MappedProgress sink;
        uint32_t generations;
        SearchProgress(ProgressSink* parent, uint32_t generations)
            : sink(parent, 2000, 2000), generations(generations) {}
        bool onGeneration(const optimization::GenerationRecord& r) override {
          return sink.onProgress(r.generation + 1, (size_t)generations + 1);
        }
      };
    }  // namespace

    RobustOptimizationReport optimizeRobustly(const RobustOptimizationRequest& request,
                                              ProgressSink* progress) {
      using namespace optimization;
      request.settings.validate();
      const auto& s = request.settings;
      RobustOptimizationReport report;
      report.request = request;
      auto cancelled = [&]() {
        report.cancelled = true;
        return report;
      };
      if (progress && !progress->onProgress(0, 10000)) return cancelled();
      auto config = request.configuration.contains("halbach_optimization")
                        ? request.configuration.at("halbach_optimization")
                        : request.configuration;
      if (!config.is_object() || !config.contains("candidates") || !config.contains("ring_count")
          || !config.contains("dsv") || !config.contains("resolution"))
        throw std::invalid_argument(
            "Choose a Halbach optimizer configuration or an exported optimized design");
      config["genetic"]["population"] = s.population;
      config["genetic"]["generations"] = s.generations;
      config["genetic"]["seed"] = s.search_seed;
      config["objective"] = "ppm_bmag";
      HalbachSpec spec;
      spec.readJSON(config);
      const double steps = (double)spec.dsv / spec.resolution;
      if (!std::isfinite(steps) || steps > 200)
        throw std::invalid_argument(
            "Nominal search grid exceeds 200 intervals per axis; increase resolution spacing");
      const auto points = makeSamplePoints(spec).size();
      const auto configurations = spec.getNumGenes() * spec.getNumCandidates();
      if (configurations > 20000000 / points)
        throw std::invalid_argument(
            "Nominal basis exceeds 20 million samples; use a coarser search grid or fewer "
            "candidates");
      report.effective_spec = spec.toJSON();
      report.backend = ExecSpace::name();
      MappedProgress basis_progress(progress, 0, 2000);
      auto basis = HalbachBasis::build(spec, false, [&](size_t done, size_t total) {
        return basis_progress.onProgress(done, total);
      });
      if (basis.getNumPoints() == 0) return cancelled();
      HomogeneityObjective objective(basis, Objective::BMagnitude);
      GeneticOptimizer optimizer(spec.genetic, basis.getNumGenes(), basis.getNumCandidates());
      GeneticRunOptions genetic;
      genetic.archive_size = s.shortlist;
      SearchProgress search_progress(progress, s.generations);
      optimizer.run(objective, &search_progress, false, genetic);
      if (optimizer.wasStopped() && !optimizer.wasStalled()) return cancelled();
      report.search_stop_reason = optimizer.wasStalled() ? "stagnation" : "completed";
      report.search_history = nlohmann::json::array();
      for (const auto& r : optimizer.getHistory())
        report.search_history.push_back(
            {{"generation", r.generation}, {"best_ppm", r.best}, {"best_ever_ppm", r.best_ever}});
      const auto& finalists = optimizer.getCandidates();
      if (finalists.empty())
        throw std::runtime_error("The nominal search produced no finite finalists");
      std::vector<nlohmann::json> designs;
      for (size_t i = 0; i < finalists.size(); ++i) {
        if (progress && !progress->onProgress(4000 + 4500 * i / finalists.size(), 10000))
          return cancelled();
        HalbachSolution solution;
        solution.spec = spec;
        solution.genome = finalists[i].genome;
        solution.optimized = objective.evaluateGenome(solution.genome);
        solution.optimized_points = basis.getNumPoints();
        solution.basis_configurations = configurations;
        solution.basis_magnets = basis.getNumMagnets();
        solution.num_magnets = solution.buildCollection().get_num_magnets();
        solution.history = optimizer.getHistory();
        solution.stop_reason = report.search_stop_reason;
        auto design = HalbachOptimizationIO::write(solution, HalbachEmit::Arrangements);
        for (const char* axis : {"x", "y", "z"})
          design["field_of_view"][axis]["n"] = s.validation_points_per_axis;
        SimulationService service;
        service.loadJSON(design);
        FieldRequest field;
        service.getFieldRequest(field);
        MappedProgress screening_progress(
            progress, 4000 + 4500 * i / finalists.size(),
            4500 * (i + 1) / finalists.size() - 4500 * i / finalists.size());
        auto study = service.analyzeTolerance(field, s.screening, &screening_progress);
        if (study.cancelled) return cancelled();
        report.candidates.push_back({finalists[i].genome, finalists[i].fitness, std::move(study)});
        designs.push_back(std::move(design));
      }
      const auto order = robustCandidateOrder(report.candidates, s);
      report.selected = order.front();
      if (!robustEligible(report.candidates[report.selected].screening, s.objective))
        throw std::runtime_error(
            "No eligible finalist; P95/P5 objectives require all screening trials valid. Reduce "
            "tolerances or use acceptance-rate ranking.");
      report.selected_design = designs[report.selected];
      SimulationService validation;
      validation.loadJSON(report.selected_design);
      FieldRequest field;
      validation.getFieldRequest(field);
      auto validation_settings = s.screening;
      validation_settings.trials = s.validation_trials;
      validation_settings.seed = s.validation_seed;
      validation_settings.points_per_axis = s.validation_points_per_axis;
      MappedProgress validation_progress(progress, 8500, 1500);
      report.validation
          = validation.analyzeTolerance(field, validation_settings, &validation_progress);
      if (report.validation.cancelled) return cancelled();
      report.complete = true;
      const auto& screen = report.candidates[report.selected].screening;
      report.selected_design["robust_optimization"] = {
          {"format", "paramagnetics-robust-selection"},
          {"version", 1},
          {"selected_nominal_rank", report.selected + 1},
          {"finalists_screened", report.candidates.size()},
          {"objective", s.objective == RobustObjective::P95Homogeneity ? "minimize_p95_homogeneity"
                        : s.objective == RobustObjective::P05MeanField
                            ? "maximize_p05_mean_field"
                            : "maximize_acceptance_rate"},
          {"screening_settings", toleranceSettingsJSON(screen.settings)},
          {"validation_settings", toleranceSettingsJSON(report.validation.settings)},
          {"screening_summary", toleranceReportJSON(screen)["summary"]},
          {"validation_summary", toleranceReportJSON(report.validation)["summary"]},
          {"required_acceptance", s.required_acceptance},
          {"screening_requirements_met", robustMeetsRequirements(screen, s)},
          {"validation_requirements_met", robustMeetsRequirements(report.validation, s)},
          {"note",
           "Nominal optimizer metadata describes the reduced search lattice. Robust summaries "
           "measure |B| on the full DSV sphere. Independent validation does not guarantee "
           "manufacturing yield or spatial convergence; repeat at finer density and more trials."}};
      return report;
    }
  }  // namespace service
}  // namespace greeter
