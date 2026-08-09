#!/usr/bin/env bash

# Builds ParaMagneticS: the standalone simulator, the Halbach optimizer, the
# test suite and, when Qt 6 is present, the viewer. Everything lands in build/.
#
#   ./compile.sh              simulator, optimizer + tests, and the viewer if Qt 6 is there
#   ./compile.sh --clean      wipe build/ and start over
#   ./compile.sh --debug      Debug build instead of Release
#   ./compile.sh --no-tests   skip the test suite
#   ./compile.sh --no-optimizer  skip the Halbach optimizer
#   ./compile.sh --gui        insist on the viewer, fail without Qt 6
#   ./compile.sh --no-gui     skip the viewer even with Qt 6 installed
#   ./compile.sh -j N         build with N jobs, all cores by default

set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

build_type=Release
jobs="$(nproc 2>/dev/null || echo 4)"
with_tests=1
with_optimizer=1
with_gui=auto
clean=0

usage() { sed -n '3,14p' "$0" | sed 's/^# \{0,1\}//'; }

while [[ $# -gt 0 ]]; do
  case "$1" in
    --clean)    clean=1 ;;
    --debug)    build_type=Debug ;;
    --no-tests) with_tests=0 ;;
    --no-optimizer) with_optimizer=0 ;;
    --gui)      with_gui=1 ;;
    --no-gui)   with_gui=0 ;;
    -j|--jobs)  jobs="$2"; shift ;;
    -h|--help)  usage; exit 0 ;;
    *) echo "Unknown option: $1" >&2; usage >&2; exit 1 ;;
  esac
  shift
done

if [[ "$clean" == 1 ]]; then
  rm -rf "$root/build"
fi

# The viewer is optional because the rest of the project builds on a machine
# without Qt. "auto" means: build it when Qt 6 can be found.
if [[ "$with_gui" == auto ]]; then
  if command -v qmake6 > /dev/null 2>&1 \
     || ls /usr/lib/*/cmake/Qt6/Qt6Config.cmake > /dev/null 2>&1; then
    with_gui=1
  else
    with_gui=0
    echo "-- Qt 6 not found, skipping the viewer (apt install qt6-base-dev libqt6opengl6-dev)"
  fi
fi

flags=(-DCMAKE_BUILD_TYPE="$build_type" -DKokkos_ENABLE_OPENMP=On -DCMAKE_CXX_COMPILER=g++)

echo "== Simulator =="
cmake -S "$root/standalone" -B "$root/build/standalone" "${flags[@]}"
cmake --build "$root/build/standalone" -j "$jobs"

if [[ "$with_optimizer" == 1 ]]; then
  echo "== Halbach optimizer =="
  cmake -S "$root/optimizer" -B "$root/build/optimizer" "${flags[@]}"
  cmake --build "$root/build/optimizer" -j "$jobs"
fi

if [[ "$with_tests" == 1 ]]; then
  echo "== Tests =="
  # doctest 2.4.9 asks for a CMake older than CMake 4 will agree to, hence the
  # policy floor.
  cmake -S "$root/test" -B "$root/build/test" "${flags[@]}" -DCMAKE_POLICY_VERSION_MINIMUM=3.5
  cmake --build "$root/build/test" -j "$jobs"
fi

if [[ "$with_gui" == 1 ]]; then
  echo "== Viewer =="
  cmake -S "$root/gui" -B "$root/build/gui" "${flags[@]}"
  cmake --build "$root/build/gui" -j "$jobs"
fi

echo
echo "Built:"
echo "  $root/build/standalone/Greeter"
[[ "$with_optimizer" == 1 ]] && echo "  $root/build/optimizer/halbach-optimizer"
[[ "$with_tests" == 1 ]] && echo "  $root/build/test/GreeterTests"
[[ "$with_gui" == 1 ]] && echo "  $root/build/gui/paramagnetics-viewer"
echo
echo "Run an example with:  ./run_example.sh"
