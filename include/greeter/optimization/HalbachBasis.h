#ifndef HALBACH_BASIS_H
#define HALBACH_BASIS_H

#include <greeter/KokkosDefines.h>
#include <greeter/optimization/HalbachSpec.h>
#include <vector>


namespace greeter {
namespace optimization {

/*
  The field every choice the optimizer can make would produce, worked out once.

  The whole optimization rests on one fact: the field of a set of permanent
  magnets is the sum of their fields. A genome picks one ring candidate per
  ring position, and the rings of one position do not move when the choice at
  another position changes. So the field of a genome is the sum of as many
  precomputed fields as there are genes, and the genetic algorithm never has
  to evaluate a magnet again after this class has run.

  What is stored is

      fields(configuration, component, point)

  where a configuration is a (gene, candidate) pair flattened as

      configuration = gene * num_candidates + candidate

  and holds every ring that pair stands for: both layers, at +z and -z. The
  point index is last because it is the one that is walked, so LayoutRight
  makes each component of each configuration a contiguous run: a vector loop
  on a CPU, a coalesced read on a GPU.

  All three components are kept rather than the x component the Python script
  keeps. Superposition is linear per component, so summing the three and
  taking the norm afterwards is exact, and that is what makes an objective on
  |B| available at no cost beyond the memory.
*/
class HalbachBasis {

  public:

    HalbachBasis();
    ~HalbachBasis();

    /*
      Samples the field of every candidate configuration.

      One Kokkos kernel over (configuration, observation point), not one
      simulation per configuration: the magnets of every configuration are
      packed end to end first, exactly as MagnetCollection::fillMagnetParameters
      packs one collection, so that the whole precomputation is a single
      parallel region a million iterations wide.
    */
    static HalbachBasis build(const HalbachSpec& spec, const bool& verbose);

    /* The observation points that were sampled, three floats each. */
    const std::vector<std::vector<float>>& getPoints() const;

    size_t getNumPoints() const;
    size_t getNumGenes() const;
    size_t getNumCandidates() const;

    /* How many magnets went into the precomputation, for a report. */
    size_t getNumMagnets() const;

    Kokkos::View<float***, Layout, MemSpace> getFields() const;

    /*
      The field of one genome at one point, on the host, summed the plain way.
      The device objective is checked against this.
    */
    void evaluateAt(const std::vector<uint16_t>& genome, const size_t& point,
                    float* b) const;

  private:

    Kokkos::View<float***, Layout, MemSpace> fields;

    std::vector<std::vector<float>> points;

    size_t num_genes = 0;
    size_t num_candidates = 0;
    size_t num_magnets = 0;
};

/*
  The points of the sampled volume, in the order the basis stores them.

  A regular grid over the box of the DSV, thinned to the points that lie
  inside the sphere and on the side of it the symmetry asks for. The Python
  script builds the same set with a numpy mask.
*/
std::vector<std::vector<float>> makeSamplePoints(const HalbachSpec& spec);

}  // namespace optimization
}  // namespace greeter

#endif  // HALBACH_BASIS_H
