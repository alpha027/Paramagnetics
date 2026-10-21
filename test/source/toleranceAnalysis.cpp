#include <doctest/doctest.h>
#include <greeter/service/SimulationService.h>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>

namespace {
  using namespace greeter::service;

  nlohmann::json sphereScene() {
    return {{"magnets",
             {{{"id", 7},
               {"type", "sphere"},
               {"parameters",
                {{"dimensions", 0.01},
                 {"magnetization", 1.0},
                 {"position", {0, 0, 0}},
                 {"orientation", {1, 0, 0, 0}}}}}}},
            {"field_of_view",
             {{"x", {{"min", 0}, {"max", 0}, {"n", 1}}},
              {"y", {{"min", 0}, {"max", 0}, {"n", 1}}},
              {"z", {{"min", 0.1}, {"max", 0.1}, {"n", 1}}}}}};
  }
  SimulationService sphereService() {
    SimulationService service;
    service.loadJSON(sphereScene(), "sphere.json");
    return service;
  }
  ToleranceSettings exact() {
    ToleranceSettings settings;
    settings.trials = 8;
    settings.strength_percent = 0;
    settings.position_mm = 0;
    settings.direction_degrees = 0;
    return settings;
  }
  FieldRequest fieldOf(const SimulationService& service) {
    FieldRequest request;
    REQUIRE(service.getFieldRequest(request));
    return request;
  }
}  // namespace

TEST_CASE("zero tolerance reproduces the nominal field without mutating the scene") {
  auto service = sphereService();
  const auto before = service.getInput();
  const auto request = fieldOf(service);
  const auto field = service.simulateField(request);
  auto settings = exact();
  settings.homogeneity_limit_ppm = 1;
  const auto report = service.analyzeTolerance(request, settings);
  CHECK_FALSE(report.cancelled);
  REQUIRE(report.trials.size() == 8);
  CHECK(report.nominal.mean_T == doctest::Approx(field.getMagnitude(0)).epsilon(1e-6));
  CHECK(report.nominal.mean_T
        == doctest::Approx(2.0 / 3.0 * std::pow(0.01 / 0.1, 3)).epsilon(1e-5));
  CHECK(report.nominal.homogeneity_ppm == 0);
  CHECK(report.mean_field.stddev == 0);
  CHECK(report.field_shift.stddev == 0);
  CHECK(report.passed_trials == 8);
  CHECK(report.valid_trials == 8);
  for (const auto& trial : report.trials) {
    CHECK(trial.valid);
    CHECK(trial.passed);
    CHECK(trial.metrics.mean_T == report.nominal.mean_T);
  }
  CHECK(service.getInput() == before);
  CHECK(service.simulateField(request).field == field.field);
}

TEST_CASE("seeded studies are repeatable and preserve the trial prefix") {
  auto service = sphereService();
  auto request = fieldOf(service);
  auto settings = exact();
  settings.strength_percent = 5;
  auto first = service.analyzeTolerance(request, settings);
  auto again = service.analyzeTolerance(request, settings);
  CHECK(toleranceReportJSON(first) == toleranceReportJSON(again));
  settings.trials += 3;
  auto longer = service.analyzeTolerance(request, settings);
  for (size_t i = 0; i < first.trials.size(); ++i) {
    CHECK(first.trials[i].metrics.mean_T == longer.trials[i].metrics.mean_T);
    CHECK(std::fabs(first.trials[i].metrics.field_shift_ppm) <= 50001);
  }
  settings.seed++;
  auto different = service.analyzeTolerance(request, settings);
  CHECK(different.trials[0].metrics.mean_T != first.trials[0].metrics.mean_T);
}

TEST_CASE("normal strength errors agree with the analytical sphere uncertainty") {
  auto service = sphereService();
  auto settings = exact();
  settings.trials = 600;
  settings.distribution = ToleranceDistribution::Normal;
  settings.strength_percent = 1;
  auto report = service.analyzeTolerance(fieldOf(service), settings);
  CHECK(report.mean_field.mean == doctest::Approx(report.nominal.mean_T).epsilon(0.002));
  CHECK(report.mean_field.stddev / report.nominal.mean_T == doctest::Approx(0.01).epsilon(0.15));
  // Independent errors of two equal sources reduce relative uncertainty by sqrt(2).
  auto input = sphereScene();
  auto second = input["magnets"][0];
  second["id"] = 8;
  input["magnets"].push_back(second);
  service.loadJSON(input);
  auto pair = service.analyzeTolerance(fieldOf(service), settings);
  CHECK(pair.mean_field.stddev / pair.nominal.mean_T
        == doctest::Approx(0.01 / std::sqrt(2.0)).epsilon(0.15));
}

TEST_CASE("dimension and position uncertainties follow the sphere far-field sensitivities") {
  auto service = sphereService();
  auto settings = exact();
  settings.trials = 500;
  settings.distribution = ToleranceDistribution::Normal;
  settings.size_percent = 1;
  const auto dimensions = service.analyzeTolerance(fieldOf(service), settings);
  // Outside a sphere, B is proportional to radius cubed.
  CHECK(dimensions.mean_field.stddev / dimensions.nominal.mean_T
        == doctest::Approx(0.03).epsilon(0.15));
  settings.size_percent = 0;
  settings.position_mm = 1;
  const auto position = service.analyzeTolerance(fieldOf(service), settings);
  // On-axis, dB/B = -3 dz/z to first order, with z = 100 mm.
  CHECK(position.mean_field.stddev / position.nominal.mean_T
        == doctest::Approx(0.03).epsilon(0.15));
}

TEST_CASE("direction and rigid-body tilt perturb the actual vector field") {
  auto service = sphereService();
  auto settings = exact();
  settings.trials = 40;
  settings.direction_degrees = 5;
  const auto direction = service.analyzeTolerance(fieldOf(service), settings);
  CHECK(direction.mean_field.stddev > 0);
  settings.direction_degrees = 0;
  settings.body_tilt_degrees = 5;
  const auto body = service.analyzeTolerance(fieldOf(service), settings);
  CHECK(body.mean_field.stddev > 0);
}

TEST_CASE("sphere masks and independent analysis grids are explicit") {
  auto service = sphereService();
  auto request = fieldOf(service);
  for (int axis = 0; axis < 3; ++axis) {
    request.bounds[2 * axis] = -0.005f;
    request.bounds[2 * axis + 1] = 0.005f;
  }
  auto settings = exact();
  settings.points_per_axis = 3;
  CHECK(service.analyzeTolerance(request, settings).points == 27);
  settings.region = ToleranceRegion::InscribedSphere;
  auto report = service.analyzeTolerance(request, settings);
  CHECK(report.points == 7);
  CHECK(report.nominal.homogeneity_ppm == doctest::Approx(0).epsilon(1e-4));
  settings.points_per_axis = 2;
  CHECK_THROWS_AS(service.analyzeTolerance(request, settings), std::invalid_argument);
}

TEST_CASE("invalid draws are counted as failed assemblies and excluded from statistics") {
  auto service = sphereService();
  auto settings = exact();
  settings.trials = 100;
  settings.distribution = ToleranceDistribution::Normal;
  settings.size_percent = 100;
  settings.minimum_mean_T = 1e-20;
  auto report = service.analyzeTolerance(fieldOf(service), settings);
  CHECK(report.valid_trials > 0);
  CHECK(report.valid_trials < settings.trials);
  CHECK(report.passed_trials == report.valid_trials);
  const auto json = toleranceReportJSON(report);
  CHECK(json["summary"]["pass_fraction"].get<double>()
        == doctest::Approx((double)report.valid_trials / settings.trials));
  for (size_t i = 0; i < report.trials.size(); ++i) {
    if (!report.trials[i].valid) {
      CHECK_FALSE(report.trials[i].error.empty());
      CHECK(json["trials"][i]["metrics"].is_null());
    }
  }
}

TEST_CASE("tolerance analysis cancels during a field chunk without publishing a complete study") {
  auto service = sphereService();
  auto request = fieldOf(service);
  request.bounds[5] = 0.12f;
  request.counts[2] = 3;
  struct Stop : ProgressSink {
    size_t last = 0;
    bool onProgress(const size_t& done, const size_t&) override {
      last = done;
      return done < 4;
    }
  } stop;
  RunOptions options;
  options.chunk = 1;
  const auto report = service.analyzeTolerance(request, exact(), &stop, options);
  CHECK(report.cancelled);
  CHECK(stop.last == 4);
  CHECK(report.trials.empty());
  CHECK_THROWS_AS(toleranceReportJSON(report), std::invalid_argument);
}

TEST_CASE("tolerance report exports include reproducibility inputs and preserve invalid rows") {
  auto service = sphereService();
  auto settings = exact();
  settings.minimum_mean_T = 1;  // Deliberately unreachable: all trials fail.
  auto report = service.analyzeTolerance(fieldOf(service), settings);
  CHECK(report.passed_trials == 0);
  auto data = toleranceReportJSON(report);
  CHECK(data["input"] == sphereScene());
  CHECK(data["settings"]["seed"] == 42);
  CHECK(data["summary"]["pass_fraction"] == 0);
  const auto path
      = std::filesystem::temp_directory_path()
        / ("paramagnetics-tolerance-"
           + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(path);
  struct Cleanup {
    std::filesystem::path path;
    ~Cleanup() {
      std::error_code e;
      std::filesystem::remove_all(path, e);
    }
  } cleanup{path};
  writeToleranceJSON(report, (path / "study.json").string());
  {
    std::ifstream file(path / "study.json");
    nlohmann::json read;
    file >> read;
    CHECK(read == data);
  }
  writeToleranceCSV(report, (path / "study.csv").string());
  std::ifstream file(path / "study.csv");
  std::string line;
  size_t rows = 0;
  while (std::getline(file, line)) ++rows;
  CHECK(rows == settings.trials + 2);  // header, nominal, random trials
  CHECK_THROWS(writeToleranceCSV(report, (path / "missing" / "study.csv").string()));
}

TEST_CASE("statistics use sample standard deviation and interpolated quantiles") {
  const auto stats = toleranceStatistics({1, 2, 3, 4, 5});
  CHECK(stats.mean == 3);
  CHECK(stats.stddev == doctest::Approx(std::sqrt(2.5)));
  CHECK(stats.p05 == doctest::Approx(1.2));
  CHECK(stats.median == 3);
  CHECK(stats.p95 == doctest::Approx(4.8));
  CHECK(toleranceStatistics({7}).stddev == 0);
}

TEST_CASE("invalid settings, zero fields and invalid domains are refused") {
  auto service = sphereService();
  auto request = fieldOf(service);
  auto settings = exact();
  SUBCASE("zero trials") {
    settings.trials = 0;
    CHECK_THROWS_AS(service.analyzeTolerance(request, settings), std::invalid_argument);
  }
  SUBCASE("negative error") {
    settings.position_mm = -1;
    CHECK_THROWS_AS(settings.validate(), std::invalid_argument);
  }
  SUBCASE("non-finite error") {
    settings.direction_degrees = std::numeric_limits<double>::infinity();
    CHECK_THROWS_AS(settings.validate(), std::invalid_argument);
  }
  SUBCASE("grid too large") {
    request.counts[0] = 2000001;
    CHECK_THROWS_AS(service.analyzeTolerance(request, settings), std::invalid_argument);
  }
  SUBCASE("invalid bounds") {
    request.bounds[0] = std::numeric_limits<float>::quiet_NaN();
    CHECK_THROWS_AS(service.analyzeTolerance(request, settings), std::invalid_argument);
  }
  SUBCASE("zero field") {
    auto input = sphereScene();
    input["magnets"][0]["parameters"]["magnetization"] = 0;
    service.loadJSON(input);
    CHECK_THROWS(service.analyzeTolerance(request, settings));
  }
}

TEST_CASE(
    "all built-in geometries support independent strength placement direction and size errors") {
  const nlohmann::json vertices = {{0, 0, 0}, {0.01, 0, 0}, {0, 0.01, 0}, {0, 0, 0.01}};
  const nlohmann::json faces = {{0, 2, 1}, {0, 1, 3}, {0, 3, 2}, {1, 2, 3}};
  std::vector<nlohmann::json> magnets = {
      {{"type", "cuboid"}, {"parameters", {{"dimensions", {0.01, 0.02, 0.03}}}}},
      {{"type", "sphere"}, {"parameters", {{"dimensions", 0.01}}}},
      {{"type", "cylinder"}, {"parameters", {{"dimensions", {0.02, 0.03}}}}},
      {{"type", "tetrahedron"}, {"parameters", {{"vertices", vertices}}}},
      {{"type", "triangular_mesh"}, {"parameters", {{"vertices", vertices}, {"faces", faces}}}},
      {{"type", "triangle"},
       {"parameters", {{"vertices", {{0, 0, 0}, {0.01, 0, 0}, {0, 0.01, 0}}}}}},
      {{"type", "cylinder_segment"}, {"parameters", {{"dimensions", {0.01, 0.02, 0.005, 0, 90}}}}},
      {{"type", "dipole"}, {"parameters", {{"moment", {0, 0, 1}}}}}};
  auto settings = exact();
  settings.trials = 4;
  settings.strength_percent = 2;
  settings.position_mm = 0.1;
  settings.direction_degrees = 2;
  settings.body_tilt_degrees = 1;
  settings.size_percent = 2;
  std::vector<ToleranceReport> reports;
  for (auto magnet : magnets) {
    CAPTURE(magnet["type"].get<std::string>());
    magnet["id"] = 7;
    magnet["parameters"]["position"] = {0, 0, 0};
    magnet["parameters"]["orientation"] = {1, 0, 0, 0};
    if (magnet["type"] != "dipole") magnet["parameters"]["magnetization"] = {0, 0, 1};
    auto input = sphereScene();
    input["magnets"] = {magnet};
    SimulationService service;
    service.loadJSON(input);
    auto report = service.analyzeTolerance(fieldOf(service), settings);
    CHECK(report.valid_trials == settings.trials);
    CHECK(report.mean_field.stddev > 0);
    reports.push_back(std::move(report));
  }
  // Equivalent tetrahedron and closed mesh must keep agreeing after every
  // perturbation, including scaling. The mesh face-count metadata stays intact.
  for (size_t i = 0; i < settings.trials; ++i) {
    CHECK(reports[3].trials[i].metrics.mean_T
          == doctest::Approx(reports[4].trials[i].metrics.mean_T).epsilon(1e-4));
  }
}
