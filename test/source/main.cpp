#define DOCTEST_CONFIG_IMPLEMENT

#include <doctest/doctest.h>
#include <greeter/KokkosDefines.h>

// Kokkos is initialized once for the whole test binary: it cannot be
// re-initialized after a Kokkos::finalize(), so individual test cases must not
// do it themselves.
int main(int argc, char** argv) {
  Kokkos::initialize(argc, argv);

  int result = doctest::Context(argc, argv).run();

  Kokkos::finalize();

  return result;
}
