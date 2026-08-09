#ifndef HALBACH_SPEC_H
#define HALBACH_SPEC_H

#include <nlohmann/json.hpp>
#include <cstdint>
#include <string>
#include <vector>


namespace greeter {
namespace optimization {

/*
  Which quantity the homogeneity is measured on.

  The basis fields are stored per component and summed per component, so both
  of these are linear in the ring contributions right up to the last step.
  Only the norm of BMagnitude is not, and it is taken once, after the sum.
*/
enum class Objective {
    Bx,          // the x component alone, which is what the Python script uses
    BMagnitude   // |B|, the quantity an MRI homogeneity figure usually means
};

/* Which part of the sampled sphere the objective is evaluated over. */
enum class Symmetry {
    Octant,      // x, y, z all >= 0, the eighth the Python script uses
    Hemisphere,  // z >= 0
    Full
};

/*
  Which field model the basis is built with.

  Cuboid is the analytic kernel of this library, which is what the magnets
  actually are. Dipole is the far field approximation the Python script uses,
  kept because it is the only way to compare against the numbers that script
  printed.
*/
enum class FieldModel {
    Cuboid,
    Dipole
};

/*
  One entry of the list the optimizer chooses from.

  A gene of the genome is an index into this list, and picking entry s for
  ring position p means: an inner ring of `count` magnets at `radius`, and an
  outer ring of `count + outer_count_offset` magnets at
  `radius + outer_radius_offset`. The two layers move together, which is what
  makes one gene enough per position.
*/
struct RingCandidate {
    float radius = 0.0f;    // [m], inner layer
    uint32_t count = 0;     // magnets in the inner ring
};

/*
  The magnet the rings are built out of.

  Only the side length is asked for, because a Halbach ring of cubes is what
  both the Python script and every arrangement in this repository build, and
  because the dipole model has to derive a moment from a volume. Everything
  else about the element follows from the field model.
*/
struct Element {
    float size = 0.012f;      // [m], side of the cube
    float remanence = 1.3f;   // [T], Br of the material
};

/* How the genetic algorithm is run. */
struct GeneticSettings {

    uint32_t population = 10000;
    uint32_t generations = 100;

    float cx_prob = 0.55f;         // chance a pair is crossed
    float mut_prob = 0.4f;         // chance an individual is mutated at all
    float gene_mut_prob = 0.05f;   // chance each gene of that individual moves

    uint32_t tournament = 3;

    /*
      How many of the best are carried into the next generation untouched.

      The Python script keeps none, and tracks the best it ever saw by hand
      while letting the population lose it. One elite costs a single genome
      and makes the reported best a member of the population that produced it.
    */
    uint32_t elitism = 1;

    uint64_t seed = 42;
};

/*
  Everything the optimizer needs, and the whole of what a configuration file
  can say.

  Constructed with the defaults of homogeneityOptimisation.py, so that an
  optimizer run with no configuration file at all is the port of that script.
*/
class HalbachSpec {

  public:

    HalbachSpec();

    /* Reads what a configuration object overrides, leaving the rest alone. */
    void readJSON(const nlohmann::json& data);

    /* Throws with a message naming the field that is wrong. */
    void validate() const;

    /*
      Where the rings sit along the axis, mirrored about the origin.

      A cylinder of `ring_count` rings spaced `ring_separation` apart, centred
      on the origin. An even count has no ring at zero, an odd one does.
    */
    std::vector<float> getRingPositions() const;

    /*
      The non negative half of the above, which is what the genome indexes.

      The magnet is built mirror symmetric in z, so a gene at a position other
      than zero stands for two rings per layer rather than one.
    */
    std::vector<float> getSymmetricRingPositions() const;

    size_t getNumGenes() const;
    size_t getNumCandidates() const;

    /* The moment [A m^2] a cube of this element is worth, m = V * Br / mu0. */
    float getDipoleMoment() const;

    /*
      The element as an arrangement writes it: a cuboid carrying a
      polarization along its local x, or a dipole carrying the equivalent
      moment along the same axis. The Halbach ring turns it from there.
    */
    nlohmann::json getElementJSON() const;

    /*
      One "halbach_ring" arrangement, written exactly as an input file of this
      library writes one. Ring layout is decided here and nowhere else, so the
      magnet the optimizer measures and the magnet it writes out cannot drift
      apart.
    */
    nlohmann::json makeRingJSON(const float& radius, const uint32_t& count,
                                const float& z) const;

    /*
      Every ring one gene stands for: the inner and the outer layer, at +z and,
      unless the gene sits at zero, at -z as well. Two rings or four.
    */
    std::vector<nlohmann::json> makeRingsForGene(const size_t& gene,
                                                 const size_t& candidate) const;

    /* The whole spec back out again, for the record written with a solution. */
    nlohmann::json toJSON() const;

    uint32_t ring_count = 23;
    float ring_separation = 0.022f;   // [m]

    std::vector<RingCandidate> candidates;

    float outer_radius_offset = 0.021f;   // [m]
    uint32_t outer_count_offset = 7;

    /*
      Order of the Halbach rings, as HalbachRingArrangement means it: a member
      is turned by (order + 1) times the angle at which it sits. Order one is
      the dipolar ring, whose field inside is uniform and transverse, and is
      the kValue = 2 of the Python script.
    */
    int64_t order = 1;

    Element element;

    float dsv = 0.2f;              // [m], diameter of the sampled sphere
    float resolution = 0.005f;     // [m], spacing of the sample grid

    Symmetry symmetry = Symmetry::Octant;
    Objective objective = Objective::Bx;
    FieldModel field_model = FieldModel::Cuboid;

    GeneticSettings genetic;
};

/*
  A float as a JSON number a person can read.

  Every length here is a float, and a float widened to the double a JSON
  number is made of prints as seventeen digits of which ten are the noise of
  the conversion: a twelve millimetre cube comes out as 0.012000000104308128.
  Rounded to the seven significant digits a float is good for, it comes out
  as 0.012.

  A float needs nine digits to be certain of surviving the trip, so this
  gives up the last bit or two of a number that was not written as a short
  decimal to begin with. That is a fraction of a nanometre on the radius of a
  ring, weighed against a file nobody can read.
*/
double tidy(const float& value);

const char* toString(const Objective& objective);
const char* toString(const Symmetry& symmetry);
const char* toString(const FieldModel& model);

}  // namespace optimization
}  // namespace greeter

#endif  // HALBACH_SPEC_H
