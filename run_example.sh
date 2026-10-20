#!/usr/bin/env bash

# Runs the simulator on an example input and leaves the results in output/.
#
#   ./run_example.sh                     simulate arrangements.json
#   ./run_example.sh magnets.json        simulate another input file
#   ./run_example.sh --view              …then open the result in the viewer
#   ./run_example.sh --snapshot run.pmsnap   also keep a snapshot of the run
#
# The CSVs land in output/, the snapshot wherever --view or --snapshot put it.

set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

input=""
snapshot=""
view=0

usage() { sed -n '3,10p' "$0" | sed 's/^# \{0,1\}//'; }

while [[ $# -gt 0 ]]; do
  case "$1" in
    --view)        view=1 ;;
    -s|--snapshot) snapshot="$2"; shift ;;
    -h|--help)     usage; exit 0 ;;
    -*) echo "Unknown option: $1" >&2; usage >&2; exit 1 ;;
    *)  input="$1" ;;
  esac
  shift
done

input="${input:-$root/arrangements.json}"

simulator="$root/build/standalone/Greeter"
viewer="$root/build/gui/paramagnetics-viewer"

if [[ ! -x "$simulator" ]]; then
  echo "Simulator not built yet, run ./compile.sh first" >&2
  exit 1
fi

if [[ "$view" == 1 && ! -x "$viewer" ]]; then
  echo "Viewer not built, run ./compile.sh (needs Qt 6)" >&2
  exit 1
fi

if [[ ! -f "$input" ]]; then
  echo "No such input file: $input" >&2
  exit 1
fi

# Each example run gets its own results directory.
mkdir -p "$root/output"
results="$(mktemp -d "$root/output/run-XXXXXX")"

# Viewing needs a snapshot to open; make a throwaway one when the caller did
# not ask to keep one.
if [[ "$view" == 1 && -z "$snapshot" ]]; then
  snapshot="$results/run.pmsnap"
fi

# Pin the OpenMP threads, which Kokkos otherwise warns about on every run.
export OMP_PROC_BIND="${OMP_PROC_BIND:-spread}"
export OMP_PLACES="${OMP_PLACES:-threads}"

echo "== Simulating $(basename "$input") =="

if [[ -n "$snapshot" ]]; then
  "$simulator" --input "$input" --output-dir "$results" --snapshot "$snapshot"
else
  "$simulator" --input "$input" --output-dir "$results"
fi

echo
echo "Results in $results/"

if [[ "$view" == 1 ]]; then
  echo "== Opening the viewer =="
  exec "$viewer" "$snapshot"
fi
