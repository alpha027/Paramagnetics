cmake -S standalone -B build/standalone -DCMAKE_BUILD_TYPE=Release -DKokkos_ENABLE_OPENMP=On -DCMAKE_CXX_COMPILER=g++
cmake -S test -B build/test -DCMAKE_BUILD_TYPE=Release -DKokkos_ENABLE_OPENMP=On -DCMAKE_CXX_COMPILER=g++
