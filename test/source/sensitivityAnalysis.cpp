#include <doctest/doctest.h>
#include <greeter/service/SimulationService.h>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string_view>

namespace {
  using namespace greeter::service;
  nlohmann::json scene() {
    return nlohmann::json::parse(R"({"magnets":[{"id":7,"type":"sphere","parameters":{
      "dimensions":0.01,"magnetization":1,"position":[0,0,0],"orientation":[1,0,0,0]}}],
      "field_of_view":{"x":{"min":0,"max":0,"n":1},"y":{"min":0,"max":0,"n":1},
      "z":{"min":0.1,"max":0.1,"n":1}}})");
  }
  FieldRequest fieldOf(const SimulationService& service) {
    FieldRequest request;
    REQUIRE(service.getFieldRequest(request));
    return request;
  }
  SensitivitySettings settings() {
    SensitivitySettings s;
    s.tolerances.trials = 100;
    s.tolerances.strength_percent = 1;
    s.tolerances.position_mm = 0;
    s.tolerances.direction_degrees = 0;
    s.sweep_points = 3;
    return s;
  }
  const SensitivityCase& find(const SensitivityReport& r, std::string_view key) {
    for (const auto& c : r.cases)
      if (std::string_view(c.key) == key) return c;
    throw std::runtime_error("Missing sensitivity case: " + std::string(key));
  }
}  // namespace

TEST_CASE("sensitivity strength response matches analytic uncertainty and scales with tolerance") {
  SimulationService service;
  const auto input = scene();
  service.loadJSON(input, "sphere.json");
  const auto request = fieldOf(service);
  const auto before = service.simulateField(request).field;
  auto s = settings();
  s.tolerances.trials = 1000;
  s.sweep_points = 5;
  auto r = service.analyzeSensitivity(request, s);
  CHECK(r.complete);
  CHECK_FALSE(r.cancelled);
  CHECK(r.source == "sphere.json");
  CHECK(r.input == input);
  REQUIRE(r.cases.size() == 1);
  REQUIRE(r.cases[0].curve.size() == 4);
  const auto& curve = r.cases[0].curve;
  CHECK(curve.back().field_shift_rms_ppm == doctest::Approx(1e4 / std::sqrt(3.0)).epsilon(0.06));
  CHECK(curve.back().field_shift_rms_ppm
        == doctest::Approx(4 * curve.front().field_shift_rms_ppm).epsilon(1e-4));
  CHECK(curve.back().homogeneity_rms_ppm == 0);
  for (size_t i = 0; i < curve.size(); ++i) {
    CHECK(curve[i].study.valid_trials == s.tolerances.trials);
    for (size_t t = 0; t < s.tolerances.trials; ++t)
      // Kernel arithmetic is float32. Normalizing the quarter-scale difference
      // amplifies its rounding error. Allow 16 float32 epsilons in ppm.
      CHECK(std::abs(curve[i].study.trials[t].metrics.field_shift_ppm / curve[i].scale
                     - curve.back().study.trials[t].metrics.field_shift_ppm)
            < 16 * std::numeric_limits<float>::epsilon() * 1e6);
  }
  CHECK(service.simulateField(request).field == before);
  CHECK(service.getInput() == input);
  CHECK(sensitivityReportJSON(service.analyzeSensitivity(request, s)) == sensitivityReportJSON(r));
}

TEST_CASE("sensitivity ranks distinguish field strength and homogeneity effects") {
  SimulationService service;
  service.loadJSON(scene());
  auto s = settings();
  s.tolerances.size_percent = 1;
  const auto r = service.analyzeSensitivity(fieldOf(service), s);
  const auto ranks = sensitivityRanks(r, SensitivityMetric::FieldShift);
  REQUIRE(r.cases.size() == 2);
  CHECK(r.cases[0].key == "strength");
  CHECK(r.cases[1].key == "size");
  CHECK(ranks[0] == 2);
  CHECK(ranks[1] == 1);
  CHECK(find(r, "size").curve.back().field_shift_rms_ppm
        == doctest::Approx(3 * find(r, "strength").curve.back().field_shift_rms_ppm).epsilon(0.03));

  // An axial, single-point region has no homogeneity change even when mean
  // field shifts. A finite region exposes position-dependent inhomogeneity.
  auto input = scene();
  input["field_of_view"]["z"] = {{"min", 0.1}, {"max", 0.12}, {"n", 5}};
  service.loadJSON(input);
  s.tolerances.position_mm = 1;
  const auto finite = service.analyzeSensitivity(fieldOf(service), s);
  const auto h = sensitivityRanks(finite, SensitivityMetric::Homogeneity);
  CHECK(h[1] == 1);  // position errors dominate homogeneity, strength scales all samples
  CHECK(find(finite, "position").curve.back().homogeneity_rms_ppm > 100);
  CHECK(find(finite, "strength").curve.back().homogeneity_rms_ppm < 1);
}

TEST_CASE("isolated magnet trials superpose and identify the dominant magnet") {
  auto input = scene();
  auto weak = input["magnets"][0];
  weak["id"] = 9;
  weak["parameters"]["magnetization"] = 0.1;
  input["magnets"].push_back(weak);
  SimulationService service;
  service.loadJSON(input);
  auto s = settings();
  s.grouping = SensitivityGrouping::Magnets;
  auto r = service.analyzeSensitivity(fieldOf(service), s);
  REQUIRE(r.cases.size() == 3);
  const auto& all = find(r, "strength").curve.back();
  const auto& a = find(r, "magnet:7").curve.back();
  const auto& b = find(r, "magnet:9").curve.back();
  CHECK(a.field_shift_rms_ppm > 8 * b.field_shift_rms_ppm);
  CHECK(sensitivityRanks(r, SensitivityMetric::FieldShift) == std::vector<size_t>{1, 1, 2});
  CHECK(find(r, "magnet:7").magnet_indices == std::vector<uint32_t>{0});
  CHECK(find(r, "magnet:9").magnet_ids == std::vector<int64_t>{9});
  for (size_t i = 0; i < all.study.trials.size(); ++i)
    CHECK(all.study.trials[i].metrics.field_shift_ppm
          == doctest::Approx(a.study.trials[i].metrics.field_shift_ppm
                             + b.study.trials[i].metrics.field_shift_ppm)
                 .epsilon(0.001)
                 .scale(100));
}

TEST_CASE(
    "arrangement studies include exactly their expanded members and retain ungrouped magnets") {
  auto input = scene();
  input["arrangements"] = nlohmann::json::parse(R"([{"id":100,"type":"linear_array",
    "parameters":{"count":2,"spacing":0.02,"position":[0,0,-0.1],
    "element":{"type":"sphere","parameters":{"dimensions":0.01,"magnetization":[0,0,1]}}}}])");
  SimulationService service;
  service.loadJSON(input);
  auto s = settings();
  s.grouping = SensitivityGrouping::Arrangements;
  const auto grouped = service.analyzeSensitivity(fieldOf(service), s);
  REQUIRE(grouped.cases.size() == 3);
  const auto& ring = find(grouped, "arrangement:100");
  CHECK(ring.magnet_indices == std::vector<uint32_t>{1, 2});
  CHECK(ring.magnet_ids == std::vector<int64_t>{8, 9});
  CHECK(find(grouped, "magnet:7").magnet_indices == std::vector<uint32_t>{0});
  s.grouping = SensitivityGrouping::Magnets;
  const auto individual = service.analyzeSensitivity(fieldOf(service), s);
  const auto& a = find(individual, "magnet:8").curve.back().study;
  const auto& b = find(individual, "magnet:9").curve.back().study;
  for (size_t i = 0; i < a.trials.size(); ++i)
    CHECK(ring.curve.back().study.trials[i].metrics.field_shift_ppm
          == doctest::Approx(a.trials[i].metrics.field_shift_ppm
                             + b.trials[i].metrics.field_shift_ppm)
                 .epsilon(0.001)
                 .scale(100));
}

TEST_CASE("sensitivity cancellation discards the incomplete study and reports monotonic progress") {
  SimulationService service;
  service.loadJSON(scene());
  struct Sink : ProgressSink {
    size_t previous = 0;
    bool stop = true;
    bool onProgress(const size_t& done, const size_t& total) override {
      CHECK(done >= previous);
      CHECK(done <= total);
      previous = done;
      return !stop || done < 1300000;
    }
  } sink;
  auto r = service.analyzeSensitivity(fieldOf(service), settings(), &sink);
  CHECK(r.cancelled);
  CHECK_FALSE(r.complete);
  CHECK_THROWS_AS(sensitivityReportJSON(r), std::invalid_argument);
  sink.previous = 0;
  sink.stop = false;
  r = service.analyzeSensitivity(fieldOf(service), settings(), &sink);
  CHECK(r.complete);
  CHECK(sink.previous == 3000000);
}

TEST_CASE("invalid sensitivity trials are visible and insufficient valid samples are unranked") {
  SimulationService service;
  service.loadJSON(scene());
  auto s = settings();
  s.tolerances.distribution = ToleranceDistribution::Normal;
  s.tolerances.strength_percent = 100;
  s.tolerances.minimum_mean_T = 1;
  auto r = service.analyzeSensitivity(fieldOf(service), s);
  const auto& p = r.cases[0].curve.back();
  CHECK(p.study.valid_trials < s.tolerances.trials);
  CHECK(p.study.valid_trials > 50);
  CHECK(p.study.passed_trials == 0);
  auto j = sensitivityReportJSON(r);
  CHECK(j["cases"][0]["curve"][1]["study"]["summary"]["pass_fraction"] == 0);
  r.cases[0].curve.back().study.valid_trials = 1;
  CHECK(sensitivityRanks(r, SensitivityMetric::FieldShift)[0] == 0);
  CHECK(sensitivityReportJSON(r)["cases"][0]["rank_field_shift"].is_null());
}

TEST_CASE("invalid normal draws on nominal magnets do not reject isolated group trials") {
  auto input = scene();
  auto second = input["magnets"][0];
  second["id"] = 9;
  input["magnets"].push_back(second);
  SimulationService service;
  service.loadJSON(input);
  auto s = settings();
  s.grouping = SensitivityGrouping::Magnets;
  s.tolerances.distribution = ToleranceDistribution::Normal;
  s.tolerances.strength_percent = 100;
  const auto r = service.analyzeSensitivity(fieldOf(service), s);
  const auto all = find(r, "strength").curve.back().study.valid_trials;
  const auto a = find(r, "magnet:7").curve.back().study.valid_trials;
  const auto b = find(r, "magnet:9").curve.back().study.valid_trials;
  CHECK(a > all);
  CHECK(b > all);
}

TEST_CASE("sensitivity exports include rankings curves provenance and reproducible trials") {
  SimulationService service;
  service.loadJSON(scene(), "sphere.json");
  auto r = service.analyzeSensitivity(fieldOf(service), settings());
  const auto data = sensitivityReportJSON(r);
  CHECK(data["input"] == scene());
  CHECK(data["settings"]["tolerances"]["strength_percent"] == 1);
  CHECK(data["settings"]["tolerances"]["seed"] == 42);
  CHECK(data["cases"][0]["curve"][1]["scale"] == 1);
  CHECK(data["cases"][0]["curve"][1]["study"]["trials"].size() == 100);
  const auto dir
      = std::filesystem::temp_directory_path()
        / ("paramagnetics-sensitivity-"
           + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(dir);
  struct Cleanup {
    std::filesystem::path path;
    ~Cleanup() {
      std::error_code e;
      std::filesystem::remove_all(path, e);
    }
  } cleanup{dir};
  writeSensitivityJSON(r, (dir / "study.json").string());
  std::ifstream json_file(dir / "study.json");
  nlohmann::json read;
  json_file >> read;
  CHECK(read == data);
  writeSensitivityCSV(r, (dir / "study.csv").string());
  std::ifstream csv(dir / "study.csv");
  std::string line;
  size_t rows = 0;
  while (std::getline(csv, line)) ++rows;
  CHECK(rows == 3);  // header + two positive tolerance levels
  CHECK_THROWS(writeSensitivityCSV(r, (dir / "missing" / "study.csv").string()));
  r.complete = false;
  CHECK_THROWS(writeSensitivityJSON(r, (dir / "incomplete.json").string()));
  CHECK_FALSE(std::filesystem::exists(dir / "incomplete.json"));
}

TEST_CASE("sensitivity rejects meaningless or unsupported settings") {
  auto s = settings();
  SUBCASE("one trial") { s.tolerances.trials = 1; }
  SUBCASE("no active errors") { s.tolerances.strength_percent = 0; }
  SUBCASE("one sweep point") { s.sweep_points = 1; }
  SUBCASE("too many sweep points") { s.sweep_points = 10; }
  SUBCASE("invalid grouping") { s.grouping = static_cast<SensitivityGrouping>(99); }
  CHECK_THROWS_AS(s.validate(), std::invalid_argument);
}
