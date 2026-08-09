#include <doctest/doctest.h>

#include <greeter/MagnetCollection.h>
#include <greeter/arrangements/HalbachRingArrangement.h>
#include <greeter/io/HalbachOptimizationIO.h>
#include <greeter/io/MagnetIO.h>
#include <greeter/io/SceneIO.h>
#include <greeter/optimization/GeneticOptimizer.h>
#include <greeter/optimization/HalbachBasis.h>
#include <greeter/optimization/HalbachOptimizer.h>
#include <greeter/optimization/HomogeneityObjective.h>

#include <nlohmann/json.hpp>
#include <cmath>
#include <set>
#include <string>
#include <vector>

/*
  The optimizer of a Halbach cylinder.

  Everything here rests on one claim: that the field of a choice of rings is
  the sum of the fields of those rings, worked out once each. So the claim
  worth checking hardest is that claim itself, against the simulator the rest
  of this library uses. The genetic algorithm on top of it is checked for the
  properties an algorithm of that kind has to have rather than for a number,
  because the number it lands on is not repeatable across machines and is not
  supposed to be.
*/

namespace {

using namespace greeter::optimization;

/*
  A specification small enough to run inside a test: three ring positions,
  two candidates, and a volume of a few dozen points. The physics is the same
  as the full one, only less of it.
*/
HalbachSpec smallSpec() {

  HalbachSpec spec;

  spec.ring_count = 3;
  spec.ring_separation = 0.03f;

  spec.candidates.clear();
  spec.candidates.push_back({0.10f, 8});
  spec.candidates.push_back({0.12f, 10});

  spec.outer_radius_offset = 0.02f;
  spec.outer_count_offset = 2;

  spec.element.size = 0.01f;
  spec.element.remanence = 1.3f;

  spec.dsv = 0.04f;
  spec.resolution = 0.01f;

  spec.genetic.population = 64;
  spec.genetic.generations = 6;
  spec.genetic.seed = 7;

  return spec;
}

}  // namespace


TEST_CASE("the specification lays the cylinder out the way the Python script does") {

  const HalbachSpec spec;  // the defaults, which are that script's

  SUBCASE("the rings are centred and evenly spaced") {

    const std::vector<float> positions = spec.getRingPositions();

    REQUIRE(positions.size() == 23);

    CHECK(positions.front() == doctest::Approx(-0.242f));
    CHECK(positions.back() == doctest::Approx(0.242f));

    for (size_t i = 1; i < positions.size(); i++) {
      CHECK(positions[i] - positions[i - 1] == doctest::Approx(0.022f));
    }
  }

  SUBCASE("a genome covers the non negative half, mirrored") {

    const std::vector<float> half = spec.getSymmetricRingPositions();

    CHECK(half.size() == 12);
    CHECK(spec.getNumGenes() == 12);
    CHECK(half.front() == doctest::Approx(0.0f));
    CHECK(half.back() == doctest::Approx(0.242f));
  }

  SUBCASE("the gene at the centre stands for one pair of rings, the others two") {

    CHECK(spec.makeRingsForGene(0, 0).size() == 2);
    CHECK(spec.makeRingsForGene(1, 0).size() == 4);
  }

  SUBCASE("the two layers of a gene differ by the offsets") {

    const std::vector<nlohmann::json> rings = spec.makeRingsForGene(3, 5);

    const float inner = rings[0]["parameters"]["radius"].get<float>();
    const float outer = rings[1]["parameters"]["radius"].get<float>();

    CHECK(outer - inner == doctest::Approx(spec.outer_radius_offset));

    CHECK(rings[1]["parameters"]["count"].get<uint32_t>()
          - rings[0]["parameters"]["count"].get<uint32_t>()
          == spec.outer_count_offset);
  }

  SUBCASE("the mirrored rings sit opposite each other") {

    const std::vector<nlohmann::json> rings = spec.makeRingsForGene(4, 0);

    REQUIRE(rings.size() == 4);

    CHECK(rings[0]["parameters"]["position"][2].get<float>()
          == doctest::Approx(-rings[2]["parameters"]["position"][2].get<float>()));
  }

  SUBCASE("the dipole model carries the moment of the cube it replaces") {

    HalbachSpec dipole_spec;
    dipole_spec.field_model = FieldModel::Dipole;

    // m = V * Br / mu0, which is the dip_mom of halbachFields.py.
    const double mu_0 = 4.0e-7 * M_PI;
    const double expected = std::pow((double) dipole_spec.element.size, 3)
                          * (double) dipole_spec.element.remanence / mu_0;

    CHECK(dipole_spec.getDipoleMoment() == doctest::Approx((float) expected));

    CHECK(dipole_spec.getElementJSON()["type"].get<std::string>() == "dipole");
  }

  SUBCASE("a specification that reaches into the magnets is refused") {

    HalbachSpec too_wide;
    too_wide.dsv = 0.6f;  // wider than the smallest candidate radius

    CHECK_THROWS_AS(too_wide.validate(), std::invalid_argument);
  }
}


TEST_CASE("the centre ring is the one that is not mirrored, at any ring count") {

  /*
    The position of the middle ring is arrived at by arithmetic that need not
    land on exactly zero, and a residue read as a position of its own would
    put two rings a nanometre apart where there should be one. So this is
    checked over a range of counts and spacings rather than the one pair the
    defaults happen to use.
  */
  for (const uint32_t count : {1u, 2u, 3u, 4u, 7u, 22u, 23u, 24u, 41u}) {
    for (const float separation : {0.022f, 0.007f, 0.13f, 0.001f}) {

      HalbachSpec spec = smallSpec();
      spec.ring_count = count;
      spec.ring_separation = separation;

      const std::vector<float> half = spec.getSymmetricRingPositions();

      // An odd count has a ring at the centre, an even one does not, and
      // either way the halves account for every ring exactly once.
      const bool odd = count % 2 == 1;

      CHECK(half.size() == (odd ? (count + 1) / 2 : count / 2));

      size_t rings = 0;

      for (size_t gene = 0; gene < half.size(); gene++) {

        const size_t made = spec.makeRingsForGene(gene, 0).size();

        // Two layers at one position, or two layers at each of two.
        CHECK((made == 2 || made == 4));
        CHECK((made == 2) == (half[gene] == 0.0f));

        rings += made;
      }

      CHECK(rings == 2 * count);

      if (odd) {
        CHECK(half.front() == 0.0f);
      } else {
        CHECK(half.front() == doctest::Approx(0.5f * separation));
      }
    }
  }
}


TEST_CASE("the sampled volume is the part of the sphere the symmetry asks for") {

  HalbachSpec spec = smallSpec();

  spec.symmetry = Symmetry::Full;
  const std::vector<std::vector<float>> full = makeSamplePoints(spec);

  spec.symmetry = Symmetry::Hemisphere;
  const std::vector<std::vector<float>> half = makeSamplePoints(spec);

  spec.symmetry = Symmetry::Octant;
  const std::vector<std::vector<float>> octant = makeSamplePoints(spec);

  CHECK(octant.size() < half.size());
  CHECK(half.size() < full.size());

  const float radius = 0.5f * spec.dsv;

  for (const auto& point : full) {
    CHECK(point[0] * point[0] + point[1] * point[1] + point[2] * point[2]
          <= doctest::Approx(radius * radius).epsilon(1e-3));
  }

  for (const auto& point : octant) {
    CHECK(point[0] >= -1e-6f);
    CHECK(point[1] >= -1e-6f);
    CHECK(point[2] >= -1e-6f);
  }
}


TEST_CASE("a basis field is the field of the rings it stands for") {

  const HalbachSpec spec = smallSpec();

  const HalbachBasis basis = HalbachBasis::build(spec, false);

  REQUIRE(basis.getNumGenes() == spec.getNumGenes());
  REQUIRE(basis.getNumCandidates() == spec.getNumCandidates());
  REQUIRE(basis.getNumPoints() > 0);

  /*
    The claim the whole optimizer rests on, checked against the simulator
    every other part of this library uses: what the packed kernel wrote into
    the basis for one configuration is what a collection of those same rings
    simulates to.
  */
  for (size_t gene = 0; gene < basis.getNumGenes(); gene++) {
    for (size_t candidate = 0; candidate < basis.getNumCandidates(); candidate++) {

      greeter::MagnetCollection collection;

      for (const auto& ring : spec.makeRingsForGene(gene, candidate)) {
        for (auto& member : greeter::HalbachRingArrangement::expand(ring)) {
          collection.addMagnet(std::move(member));
        }
      }

      const std::vector<std::vector<float>> expected =
          collection.simulate(basis.getPoints());

      const size_t configuration = gene * basis.getNumCandidates() + candidate;

      for (size_t point = 0; point < basis.getNumPoints(); point++) {
        for (size_t component = 0; component < 3; component++) {
          CHECK(basis.getFields()(configuration, component, point)
                == doctest::Approx(expected[point][component]).epsilon(1e-5));
        }
      }
    }
  }
}


TEST_CASE("summing the basis is simulating the magnet it describes") {

  const HalbachSpec spec = smallSpec();

  const HalbachBasis basis = HalbachBasis::build(spec, false);

  // One candidate per gene, chosen so that not every gene picks the same one.
  std::vector<uint16_t> genome;
  for (size_t gene = 0; gene < basis.getNumGenes(); gene++) {
    genome.push_back((uint16_t) (gene % basis.getNumCandidates()));
  }

  HalbachSolution solution;
  solution.spec = spec;
  solution.genome = genome;

  const greeter::MagnetCollection collection = solution.buildCollection();

  const std::vector<std::vector<float>> expected =
      collection.simulate(basis.getPoints());

  for (size_t point = 0; point < basis.getNumPoints(); point++) {

    float b[3];
    basis.evaluateAt(genome, point, b);

    for (size_t component = 0; component < 3; component++) {
      CHECK(b[component] == doctest::Approx(expected[point][component]).epsilon(1e-4));
    }
  }
}


TEST_CASE("the objective on the device agrees with the objective on the host") {

  HalbachSpec spec = smallSpec();

  for (const auto& kind : {Objective::Bx, Objective::BMagnitude}) {

    spec.objective = kind;

    const HalbachBasis basis = HalbachBasis::build(spec, false);
    const HomogeneityObjective objective(basis, kind);

    const size_t genes = basis.getNumGenes();
    const size_t candidates = basis.getNumCandidates();

    const size_t individuals = 8;

    PopulationView population("population", individuals, genes);
    FitnessView fitness("fitness", individuals);

    std::vector<std::vector<uint16_t>> genomes;

    for (size_t i = 0; i < individuals; i++) {

      std::vector<uint16_t> genome;

      for (size_t gene = 0; gene < genes; gene++) {
        const uint16_t value = (uint16_t) ((i + gene) % candidates);
        genome.push_back(value);
        population(i, gene) = value;
      }

      genomes.push_back(genome);
    }

    objective.evaluate(population, fitness);

    for (size_t i = 0; i < individuals; i++) {
      CHECK(fitness(i)
            == doctest::Approx(objective.evaluateGenome(genomes[i]).ppm)
                   .epsilon(1e-3));
    }
  }
}


TEST_CASE("the evolution improves, keeps its best, and stays inside the alphabet") {

  const HalbachSpec spec = smallSpec();

  const HalbachBasis basis = HalbachBasis::build(spec, false);
  const HomogeneityObjective objective(basis, spec.objective);

  GeneticOptimizer optimizer(
      spec.genetic, basis.getNumGenes(), basis.getNumCandidates());

  const std::vector<uint16_t> best = optimizer.run(objective, nullptr, false);

  REQUIRE(best.size() == basis.getNumGenes());

  SUBCASE("every gene names a candidate that exists") {
    for (const auto& gene : best) {
      CHECK(gene < basis.getNumCandidates());
    }
  }

  SUBCASE("the best ever never gets worse") {

    const std::vector<GenerationRecord>& history = optimizer.getHistory();

    REQUIRE(history.size() == spec.genetic.generations + 1);

    for (size_t i = 1; i < history.size(); i++) {
      CHECK(history[i].best_ever <= history[i - 1].best_ever);
    }
  }

  SUBCASE("the reported best is the score of the reported genome") {
    CHECK(optimizer.getBestFitness()
          == doctest::Approx(objective.evaluateGenome(best).ppm).epsilon(1e-3));
  }

  SUBCASE("the answer is at least as good as the first generation was") {
    CHECK(optimizer.getBestFitness() <= optimizer.getHistory().front().best);
  }
}


TEST_CASE("the same seed gives the same answer") {

  const HalbachSpec spec = smallSpec();

  const HalbachBasis basis = HalbachBasis::build(spec, false);
  const HomogeneityObjective objective(basis, spec.objective);

  GeneticOptimizer first(
      spec.genetic, basis.getNumGenes(), basis.getNumCandidates());
  GeneticOptimizer again(
      spec.genetic, basis.getNumGenes(), basis.getNumCandidates());

  CHECK(first.run(objective, nullptr, false)
        == again.run(objective, nullptr, false));

  GeneticSettings other = spec.genetic;
  other.seed = spec.genetic.seed + 1;

  GeneticOptimizer different(
      other, basis.getNumGenes(), basis.getNumCandidates());

  different.run(objective, nullptr, false);

  // Not an equality check the other way round: two seeds may land on the same
  // answer, and on a problem this small they often do. What has to hold is
  // that the second run is a run of the same problem.
  CHECK(different.getBestFitness() > 0.0f);
}


TEST_CASE("a run asked to stop stops") {

  const HalbachSpec spec = smallSpec();

  const HalbachBasis basis = HalbachBasis::build(spec, false);
  const HomogeneityObjective objective(basis, spec.objective);

  struct StopAtTwo: public GenerationSink {
    bool onGeneration(const GenerationRecord& record) override {
      return record.generation < 2;
    }
  };

  StopAtTwo sink;

  GeneticOptimizer optimizer(
      spec.genetic, basis.getNumGenes(), basis.getNumCandidates());

  optimizer.run(objective, &sink, false);

  CHECK(optimizer.wasStopped());
  CHECK(optimizer.getHistory().size() == 3);
}


TEST_CASE("what the optimizer writes is what the simulator reads") {

  HalbachSpec spec = smallSpec();
  spec.genetic.generations = 3;

  greeter::optimization::RunOptions options;
  options.verbose = false;
  options.verify = true;

  const HalbachSolution solution =
      HalbachOptimizer::run(spec, nullptr, options);

  REQUIRE(solution.was_verified);
  REQUIRE(solution.num_magnets > 0);

  for (const auto& emit : {greeter::HalbachEmit::Arrangements,
                           greeter::HalbachEmit::Magnets}) {

    const nlohmann::json written =
        greeter::HalbachOptimizationIO::write(solution, emit);

    // The file this library reads, not a report beside one.
    CHECK(greeter::MagnetIO::validateJSON(written));

    const greeter::Scene scene = greeter::SceneIO::read(written);

    CHECK(scene.collection.get_num_magnets() == solution.num_magnets);

    /*
      And the same magnet, not merely the same number of them: read back and
      simulated, it gives the homogeneity the run reported.
    */
    size_t points = 0;

    const FieldMetrics measured = HalbachOptimizer::measure(
        scene.collection, solution.spec, solution.spec.objective, points);

    CHECK(points == solution.verified_points);
    CHECK(measured.mean == doctest::Approx(solution.verified.mean).epsilon(1e-4));
    CHECK(measured.ppm == doctest::Approx(solution.verified.ppm).epsilon(1e-3));
  }
}


TEST_CASE("the written record reads back as the specification that made it") {

  HalbachSpec spec = smallSpec();
  spec.field_model = FieldModel::Dipole;
  spec.objective = Objective::BMagnitude;
  spec.symmetry = Symmetry::Hemisphere;
  spec.genetic.generations = 2;

  greeter::optimization::RunOptions options;
  options.verbose = false;
  options.verify = false;

  const HalbachSolution solution =
      HalbachOptimizer::run(spec, nullptr, options);

  const nlohmann::json written = greeter::HalbachOptimizationIO::write(
      solution, greeter::HalbachEmit::Arrangements);

  const HalbachSpec read = greeter::HalbachOptimizationIO::readSpec(written);

  CHECK(read.ring_count == spec.ring_count);
  CHECK(read.getNumCandidates() == spec.getNumCandidates());
  // Through their names: doctest cannot print a scoped enumeration, and the
  // name is what a failure would want to show anyway.
  CHECK(std::string(toString(read.field_model)) == toString(spec.field_model));
  CHECK(std::string(toString(read.objective)) == toString(spec.objective));
  CHECK(std::string(toString(read.symmetry)) == toString(spec.symmetry));
  CHECK(read.genetic.seed == spec.genetic.seed);
  CHECK(read.dsv == doctest::Approx(spec.dsv));
  CHECK(read.element.size == doctest::Approx(spec.element.size));

  SUBCASE("a dipole element is written as a dipole") {
    CHECK(written["arrangements"][0]["parameters"]["element"]["type"]
              .get<std::string>() == "dipole");
  }
}


TEST_CASE("a configuration is read, and a wrong one is refused by name") {

  SUBCASE("what a file overrides is overridden, and the rest is left alone") {

    const nlohmann::json data = nlohmann::json::parse(R"({
      "halbach_optimization": {
        "ring_count": 5,
        "dsv": 0.1,
        "field_model": "dipole",
        "genetic": { "population": 32, "generations": 4 }
      }
    })");

    const HalbachSpec spec = greeter::HalbachOptimizationIO::readSpec(data);

    CHECK(spec.ring_count == 5);
    CHECK(spec.dsv == doctest::Approx(0.1f));
    CHECK(std::string(toString(spec.field_model)) == "dipole");
    CHECK(spec.genetic.population == 32);
    CHECK(spec.genetic.generations == 4);

    // Untouched by the file, so still the default of the Python script.
    CHECK(spec.ring_separation == doctest::Approx(0.022f));
    CHECK(spec.getNumCandidates() == 19);
    CHECK(spec.genetic.tournament == 3);
  }

  SUBCASE("an unknown choice is refused rather than quietly ignored") {

    HalbachSpec spec;

    CHECK_THROWS_AS(
        spec.readJSON(nlohmann::json::parse(R"({"field_model": "monopole"})")),
        std::invalid_argument);

    CHECK_THROWS_AS(
        spec.readJSON(nlohmann::json::parse(R"({"symmetry": "quadrant"})")),
        std::invalid_argument);

    CHECK_THROWS_AS(
        spec.readJSON(nlohmann::json::parse(R"({"dsv": -1})")),
        std::invalid_argument);

    CHECK_THROWS_AS(
        spec.readJSON(nlohmann::json::parse(R"({"genetic": {"cx_prob": 2}})")),
        std::invalid_argument);
  }
}
