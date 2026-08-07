cmake -S standalone -B build/standalone -DCMAKE_BUILD_TYPE=Release -DKokkos_ENABLE_OPENMP=On -DCMAKE_CXX_COMPILER=g++
cmake -S test -B build/test -DCMAKE_BUILD_TYPE=Release -DKokkos_ENABLE_OPENMP=On -DCMAKE_CXX_COMPILER=g++ -DCMAKE_POLICY_VERSION_MINIMUM=3.5

# The viewer, which needs Qt 6. Left out of the line above because the rest of
# the project builds without it.
#   cmake -S gui -B build/gui -DCMAKE_BUILD_TYPE=Release -DKokkos_ENABLE_OPENMP=On -DCMAKE_CXX_COMPILER=g++
