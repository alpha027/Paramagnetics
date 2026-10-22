#include <doctest/doctest.h>
#include <greeter/optimization/GeneticOptimizer.h>
#include <greeter/optimization/HalbachBasis.h>
#include <greeter/service/RobustOptimization.h>
#include <greeter/service/SimulationService.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <set>

namespace {
  using namespace greeter::service;
  nlohmann::json config() {
    return nlohmann::json::parse(R"({"ring_count":1,"ring_separation":0.03,
      "candidates":[{"radius":0.10,"count":8},{"radius":0.12,"count":10}],
      "outer_radius_offset":0.03,"outer_count_offset":2,
      "element":{"size":0.01,"remanence":1.3},"dsv":0.02,"resolution":0.01,
      "symmetry":"full","objective":"ppm_bmag","field_model":"cuboid",
      "genetic":{"population":32,"generations":2,"seed":7}})");
  }
  RobustOptimizationRequest request() {
    RobustOptimizationRequest r;
    r.configuration = config();
    r.source = "optimizer.json";
    auto& s = r.settings;
    s.population = 32;
    s.generations = 2;
    s.shortlist = 4;
    s.screening.trials = 8;
    s.screening.points_per_axis = 3;
    s.validation_trials = 12;
    s.validation_points_per_axis = 5;
    s.screening.strength_percent = 1;
    s.screening.position_mm = 0;
    s.screening.direction_degrees = 0;
    return r;
  }
  RobustCandidate candidate(double nominal, double p95, double p05, size_t passed = 8,
                            size_t valid = 10) {
    RobustCandidate c;
    c.nominal_search_ppm = nominal;
    c.genome = {(uint16_t)nominal};
    c.screening.settings.trials = 10;
    c.screening.settings.homogeneity_limit_ppm = 500;
    c.screening.trials.resize(10);
    c.screening.valid_trials = valid;
    c.screening.passed_trials = passed;
    c.screening.homogeneity.p95 = p95;
    c.screening.mean_field.p05 = p05;
    return c;
  }
}  // namespace

TEST_CASE("robust ranking follows the chosen objective and prioritizes requirements") {
  auto s = request().settings;
  std::vector<RobustCandidate> c
      = {candidate(1, 200, .1, 9), candidate(2, 100, .2, 8), candidate(3, 300, .3, 10)};
  CHECK(robustCandidateOrder(c, s) == std::vector<size_t>{1, 0, 2});
  s.objective = RobustObjective::P05MeanField;
  CHECK(robustCandidateOrder(c, s) == std::vector<size_t>{2, 1, 0});
  s.objective = RobustObjective::AcceptanceRate;
  s.screening.homogeneity_limit_ppm = 500;
  CHECK(robustCandidateOrder(c, s) == std::vector<size_t>{2, 0, 1});
  s.objective = RobustObjective::P95Homogeneity;
  s.screening.minimum_mean_T = .25;
  CHECK(robustCandidateOrder(c, s).front() == 2);  // field floor outranks a better ppm score
  c[2].screening.homogeneity.p95 = 1000;
  s.screening.homogeneity_limit_ppm = 500;
  CHECK_FALSE(robustMeetsRequirements(c[2].screening, s));
}

TEST_CASE("robust quantile objectives reject invalid draws and acceptance uses all assemblies") {
  auto s = request().settings;
  auto invalid = candidate(1, 1, .3, 8, 8);
  auto valid = candidate(2, 100, .2, 7, 10);
  CHECK_FALSE(robustEligible(invalid.screening, s.objective));
  CHECK(robustCandidateOrder({invalid, valid}, s).front() == 1);
  s.objective = RobustObjective::AcceptanceRate;
  s.screening.homogeneity_limit_ppm = 500;
  s.required_acceptance = .75;
  CHECK(robustEligible(invalid.screening, s.objective));
  CHECK(robustMeetsRequirements(invalid.screening, s));  // 8/10, not 8/8
  CHECK(robustCandidateOrder({invalid, valid}, s).front() == 0);
  s.required_acceptance = .85;
  CHECK_FALSE(robustMeetsRequirements(invalid.screening, s));
}

TEST_CASE(
    "robust shortlist consists of distinct observed nominal candidates without changing the "
    "search") {
  using namespace greeter::optimization;
  HalbachSpec spec;
  spec.readJSON(config());
  auto basis = HalbachBasis::build(spec, false);
  HomogeneityObjective objective(basis, Objective::BMagnitude);
  GeneticOptimizer plain(spec.genetic, basis.getNumGenes(), basis.getNumCandidates());
  auto winner = plain.run(objective, nullptr, false);
  GeneticOptimizer archived(spec.genetic, basis.getNumGenes(), basis.getNumCandidates());
  GeneticRunOptions options;
  options.archive_size = 8;
  CHECK(archived.run(objective, nullptr, false, options) == winner);
  const auto& candidates = archived.getCandidates();
  REQUIRE(candidates.size() == 2);
  CHECK(candidates[0].genome == winner);
  CHECK(candidates[0].fitness <= candidates[1].fitness);
  CHECK(candidates[0].genome != candidates[1].genome);
  for (const auto& c : candidates)
    CHECK(c.fitness == doctest::Approx(objective.evaluateGenome(c.genome).ppm).epsilon(2e-6));
}

TEST_CASE(
    "robust optimization screens available finalists and validates the winner on fresh streams") {
  auto r = request();
  r.settings.objective = RobustObjective::P05MeanField;
  const auto report = optimizeRobustly(r);
  CHECK(report.complete);
  CHECK_FALSE(report.cancelled);
  REQUIRE(report.candidates.size() == 2);  // four requested, only two unique genomes exist
  CHECK(report.request.configuration == config());
  CHECK(report.effective_spec["genetic"]["seed"] == r.settings.search_seed);
  CHECK(report.effective_spec["objective"] == "ppm_bmag");
  const double selected_field = report.candidates[report.selected].screening.mean_field.p05;
  for (const auto& c : report.candidates) CHECK(selected_field >= c.screening.mean_field.p05);
  CHECK(report.validation.settings.seed == r.settings.validation_seed);
  CHECK(report.validation.settings.seed != report.candidates[0].screening.settings.seed);
  CHECK(report.validation.trials.size() == r.settings.validation_trials);
  CHECK(report.validation.points > report.candidates[0].screening.points);
  SimulationService replay;
  replay.loadJSON(report.selected_design);
  FieldRequest field;
  REQUIRE(replay.getFieldRequest(field));
  CHECK(field.counts[0] == r.settings.validation_points_per_axis);
  auto study = replay.analyzeTolerance(field, report.validation.settings);
  CHECK(toleranceReportJSON(study)["trials"] == toleranceReportJSON(report.validation)["trials"]);
  CHECK(report.selected_design["robust_optimization"]["validation_requirements_met"] == true);
  CHECK(robustOptimizationJSON(report)["validation"]["settings"]["seed"]
        == r.settings.validation_seed);
}

TEST_CASE("failed robust requirements remain failed in the selected design and report") {
  auto r = request();
  r.settings.screening.minimum_mean_T = 100;
  auto report = optimizeRobustly(r);
  CHECK(report.complete);
  for (const auto& c : report.candidates)
    CHECK_FALSE(robustMeetsRequirements(c.screening, r.settings));
  CHECK_FALSE(robustMeetsRequirements(report.validation, r.settings));
  CHECK(report.selected_design["robust_optimization"]["validation_requirements_met"] == false);
  CHECK(robustOptimizationJSON(report)["validation_requirements_met"] == false);
}

TEST_CASE("robust acceptance screening counts actual invalid assemblies as failures") {
  auto r = request();
  r.configuration["candidates"] = {{{"radius", .10}, {"count", 1}}};
  r.configuration["outer_count_offset"] = 0;
  r.settings.objective = RobustObjective::AcceptanceRate;
  r.settings.screening.distribution = ToleranceDistribution::Normal;
  r.settings.screening.strength_percent = 100;
  r.settings.screening.homogeneity_limit_ppm = 1e9;
  r.settings.screening.trials = 128;
  r.settings.validation_trials = 128;
  auto report = optimizeRobustly(r);
  REQUIRE(report.complete);
  const auto& study = report.candidates[report.selected].screening;
  CHECK(study.valid_trials >= 2);
  CHECK(study.valid_trials < study.trials.size());
  CHECK(study.passed_trials == study.valid_trials);
  CHECK(robustEligible(study, RobustObjective::AcceptanceRate));
  CHECK_FALSE(robustEligible(study, RobustObjective::P95Homogeneity));
  const double rate = (double)study.passed_trials / study.trials.size();
  CHECK(toleranceReportJSON(study)["summary"]["pass_fraction"].get<double>() == rate);
}

TEST_CASE(
    "robust cancellation at basis search screening and validation never yields exportable "
    "results") {
  auto r = request();
  struct Sink : ProgressSink {
    size_t stop = 0, previous = 0;
    bool onProgress(const size_t& done, const size_t& total) override {
      CHECK(done >= previous);
      CHECK(total == 10000);
      previous = done;
      return done < stop;
    }
  } sink;
  SUBCASE("before any basis work") { sink.stop = 0; }
  SUBCASE("basis completion") { sink.stop = 1500; }
  SUBCASE("during search") { sink.stop = 2500; }
  SUBCASE("during screening") { sink.stop = 5000; }
  SUBCASE("during independent validation") { sink.stop = 9000; }
  auto report = optimizeRobustly(r, &sink);
  CHECK(report.cancelled);
  CHECK_FALSE(report.complete);
  CHECK_THROWS_AS(robustOptimizationJSON(report), std::invalid_argument);
}

TEST_CASE("chunked nominal basis agrees exactly and can stop partway through sampling") {
  using namespace greeter::optimization;
  auto data = config();
  data["ring_count"] = 3;
  data["resolution"] = .001;
  HalbachSpec spec;
  spec.readJSON(data);
  auto reference = HalbachBasis::build(spec, false);
  size_t positive_callbacks = 0;
  auto chunked = HalbachBasis::build(spec, false, [&](size_t done, size_t) {
    if (done) ++positive_callbacks;
    return true;
  });
  CHECK(positive_callbacks > 1);
  auto a = reference.getFields(), b = chunked.getFields();
  for (size_t c = 0; c < reference.getNumGenes() * reference.getNumCandidates(); ++c)
    for (size_t axis = 0; axis < 3; ++axis)
      for (size_t p = 0; p < reference.getNumPoints(); ++p) CHECK(a(c, axis, p) == b(c, axis, p));
  auto cancelled = HalbachBasis::build(spec, false, [](size_t done, size_t) { return done == 0; });
  CHECK(cancelled.getNumPoints() == 0);
}

TEST_CASE("robust settings require independent streams valid objectives and usable grids") {
  auto s = request().settings;
  SUBCASE("identical seeds") { s.validation_seed = s.screening.seed; }
  SUBCASE("shifted overlapping trial streams") {
    s.validation_seed = s.screening.seed + 0x9e3779b97f4a7c15ULL;
  }
  SUBCASE("even grid") { s.validation_points_per_axis = 4; }
  SUBCASE("box region") { s.screening.region = ToleranceRegion::Box; }
  SUBCASE("one trial") { s.screening.trials = 1; }
  SUBCASE("no finalists") { s.shortlist = 0; }
  SUBCASE("too many finalists") { s.shortlist = 33; }
  SUBCASE("invalid objective") { s.objective = static_cast<RobustObjective>(99); }
  SUBCASE("acceptance without limits") { s.objective = RobustObjective::AcceptanceRate; }
  SUBCASE("invalid yield") { s.required_acceptance = 1.1; }
  CHECK_THROWS_AS(s.validate(), std::invalid_argument);
}

TEST_CASE("robust exports preserve selection validation and reloadable geometry") {
  const auto report = optimizeRobustly(request());
  const auto dir
      = std::filesystem::temp_directory_path()
        / ("paramagnetics-robust-"
           + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(dir);
  struct Cleanup {
    std::filesystem::path dir;
    ~Cleanup() {
      std::error_code e;
      std::filesystem::remove_all(dir, e);
    }
  } cleanup{dir};
  writeRobustOptimizationJSON(report, (dir / "report.json").string());
  std::ifstream json_file(dir / "report.json");
  nlohmann::json data;
  json_file >> data;
  CHECK(data == robustOptimizationJSON(report));
  CHECK(data["candidates"][report.selected]["selected"] == true);
  writeRobustDesign(report, (dir / "design.json").string());
  SimulationService service;
  service.loadFile((dir / "design.json").string());
  CHECK(service.isLoaded());
  CHECK(service.getInput() == report.selected_design);
  writeRobustOptimizationCSV(report, (dir / "report.csv").string());
  std::ifstream csv(dir / "report.csv");
  std::string line;
  size_t rows = 0;
  while (std::getline(csv, line)) ++rows;
  CHECK(rows == report.candidates.size() + 2);
  CHECK_THROWS(writeRobustDesign(report, (dir / "missing" / "design.json").string()));
  auto incomplete = report;
  incomplete.complete = false;
  CHECK_THROWS(writeRobustDesign(incomplete, (dir / "incomplete.json").string()));
  CHECK_FALSE(std::filesystem::exists(dir / "incomplete.json"));
}
