#!/usr/bin/env bash

# Rebuilds every design in this directory from its config.json.
#
# Each design folder ends up holding:
#
#   config.json      what was asked for, and why: the notes in it are the
#                    record of how the parameters were chosen
#   optimized.json   what came out. An input file the simulator runs as it
#                    stands, with the homogeneity and field figures beside it
#                    in its "halbach_optimization" object
#   field.csv        the field the verification measured, one row a sample:
#                    x,y,z,Bx,By,Bz,Bmag over the whole sample sphere
#   convergence.csv  the search, one row a generation
#
#   ./regenerate.sh              every design
#   ./regenerate.sh nmr-5ring-10mm-tube   just that one
#   ./regenerate.sh --check      rebuild into a temporary place and compare,
#                                without touching what is here

set -euo pipefail

# Kokkos complains on every run otherwise, and pinning is worth having anyway.
export OMP_PROC_BIND="${OMP_PROC_BIND:-spread}"
export OMP_PLACES="${OMP_PLACES:-threads}"

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/.." && pwd)"
optimizer="$root/build/optimizer/halbach-optimizer"

if [[ ! -x "$optimizer" ]]; then
  echo "No optimizer at $optimizer. Build it with: $root/compile.sh" >&2
  exit 1
fi

check=0
if [[ "${1:-}" == "--check" ]]; then check=1; shift; fi

designs=("$@")
if [[ ${#designs[@]} -eq 0 ]]; then
  mapfile -t designs < <(cd "$here" && find . -mindepth 2 -maxdepth 2 -name config.json \
                                       -printf '%h\n' | sed 's|^\./||' | sort)
fi

for design in "${designs[@]}"; do

  config="$here/$design/config.json"
  [[ -f "$config" ]] || { echo "No config.json in $design" >&2; exit 1; }

  if [[ "$check" == 1 ]]; then
    out="$(mktemp -d)"
    trap 'rm -rf "$out"' EXIT
  else
    out="$here/$design"
  fi

  echo "== $design"
  "$optimizer" -c "$config" -q \
      -o "$out/optimized.json" \
      --field "$out/field.csv" \
      --history "$out/convergence.csv" > /dev/null

  if [[ "$check" == 1 ]]; then
    # The genome and the figures are what has to match; timings never will.
    if python3 - "$here/$design/optimized.json" "$out/optimized.json" <<'PY'
import json, sys
a=json.load(open(sys.argv[1]))["halbach_optimization"]
b=json.load(open(sys.argv[2]))["halbach_optimization"]
same = a["genome"]==b["genome"] and a["verified"]==b["verified"]
print("   reproduces" if same else "   DIFFERS")
sys.exit(0 if same else 1)
PY
    then :; else exit 1; fi
  else
    python3 - "$out/optimized.json" <<'PY'
import json, sys
d=json.load(open(sys.argv[1]))["halbach_optimization"]
v=d["verified"]
print("   %d magnets, %.1f mT (%.2f MHz), %.1f ppm over %.0f mm"
      % (d["magnet_count"], 1e3*v["mean_T"], 42.577*v["mean_T"],
         v["homogeneity_ppm"], 1e3*d["dsv"]))
PY
  fi
done
