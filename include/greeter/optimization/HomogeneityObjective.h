#ifndef HOMOGENEITY_OBJECTIVE_H
#define HOMOGENEITY_OBJECTIVE_H

#include <greeter/KokkosDefines.h>
#include <greeter/optimization/HalbachBasis.h>
#include <greeter/optimization/HalbachSpec.h>
#include <cstdint>
#include <vector>


namespace greeter {
namespace optimization {

/* One integer index per gene. Sixteen bits is more candidates than anyone
   would list, and keeps a population of ten thousand under a megabyte. */
typedef Kokkos::View<uint16_t**, Layout, MemSpace> PopulationView;
typedef Kokkos::View<float*, Layout, MemSpace> FitnessView;

/*
  The peak, the trough and the total of a field, gathered together.

  Kokkos combines several reducers in one pass only for a top level policy,
  not for a reduction nested inside a team, and the whole point of the
  evaluation below is that it walks the volume once. So the three quantities
  the homogeneity needs travel as one value with one join.

  The total is kept in double. A hundred thousand single precision samples of
  nearly the same size lose, in their sum, digits that the answer is measured
  in.
*/
struct FieldExtent {
    float min = 0.0f;
    float max = 0.0f;
    double sum = 0.0;
};

template <class Space>
struct FieldExtentReducer {

  public:

    typedef FieldExtentReducer reducer;
    typedef FieldExtent value_type;
    typedef Kokkos::View<value_type, Space, Kokkos::MemoryUnmanaged> result_view_type;

    KOKKOS_INLINE_FUNCTION
    explicit FieldExtentReducer(value_type& _value): value(_value) {}

    KOKKOS_INLINE_FUNCTION
    void join(value_type& into, const value_type& from) const {
        if (from.min < into.min) { into.min = from.min; }
        if (from.max > into.max) { into.max = from.max; }
        into.sum += from.sum;
    }

    KOKKOS_INLINE_FUNCTION
    void init(value_type& into) const {
        into.min = Kokkos::reduction_identity<float>::min();
        into.max = Kokkos::reduction_identity<float>::max();
        into.sum = Kokkos::reduction_identity<double>::sum();
    }

    KOKKOS_INLINE_FUNCTION
    value_type& reference() const { return value; }

    KOKKOS_INLINE_FUNCTION
    result_view_type view() const { return result_view_type(&value); }

    KOKKOS_INLINE_FUNCTION
    bool references_scalar() const { return true; }

  private:

    value_type& value;
};

/* What a field over the sampled volume looks like, in one line. */
struct FieldMetrics {

    float min = 0.0f;
    float max = 0.0f;
    float mean = 0.0f;

    /*
      Peak to peak over the mean, in parts per million, which is how the
      homogeneity of a magnet is quoted.

      The mean is taken in magnitude. The Python script divides by the signed
      mean, which is the same number whenever the field points the way it is
      expected to, and is a trap when it does not: a genome that flipped the
      sign of the mean would be rewarded rather than measured.
    */
    float ppm = 0.0f;
};

/*
  How good a genome is: how flat the field it produces is.

  The genome is not turned back into magnets to answer that. Every gene picks
  one precomputed field out of HalbachBasis, and the field of the genome is
  their sum, so the whole evaluation is a walk over as many arrays as there
  are genes. That is the reason the basis exists and the reason a generation
  of ten thousand individuals is seconds rather than hours.
*/
class HomogeneityObjective {

  public:

    HomogeneityObjective();
    HomogeneityObjective(const HalbachBasis& basis, const Objective& objective);

    /*
      Scores a whole population in one parallel region.

      A team per individual, the team spread over the observation points, and
      the peak, the trough and the total taken in a single pass with three
      reducers rather than three passes. The configuration each gene points at
      is worked out once per team into scratch, since it is otherwise
      recomputed at every one of the thousands of points.
    */
    void evaluate(const PopulationView& population, const FitnessView& fitness) const;

    /* One genome, on the host, the plain way. What a report quotes. */
    FieldMetrics evaluateGenome(const std::vector<uint16_t>& genome) const;

    /*
      The same figure for a field that was sampled some other way, so that a
      run can be checked against the simulator rather than against itself.
      The values are the ones the objective measures: Bx, or |B|.
    */
    static FieldMetrics summarise(const std::vector<float>& values);

    size_t getNumGenes() const;
    size_t getNumCandidates() const;
    size_t getNumPoints() const;

  private:

    Kokkos::View<float***, Layout, MemSpace> fields;

    size_t num_genes = 0;
    size_t num_candidates = 0;
    size_t num_points = 0;

    Objective objective = Objective::Bx;
};

}  // namespace optimization
}  // namespace greeter

#endif  // HOMOGENEITY_OBJECTIVE_H
