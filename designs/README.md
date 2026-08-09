# Magnet designs

One folder per magnet. Each was produced by `halbach-optimizer` from the
`config.json` beside it, and each folder holds both the answer and the evidence
for it.

| file | what it is |
| --- | --- |
| `config.json` | what was asked for. The `_`-prefixed keys are the record of *why* each parameter is what it is — the sweeps behind it, the constraints that bind, and what was traded against what. The optimizer ignores them. |
| `optimized.json` | what came out. **An input file the simulator runs as it stands** — `arrangements` plus a `field_of_view` — with the homogeneity and field figures beside them in a `halbach_optimization` object the readers ignore. |
| `field.csv` | the field the verification measured, one row a sample: `x,y,z,Bx,By,Bz,Bmag` over the whole sample sphere, at nine significant digits. |
| `convergence.csv` | the search, one row a generation: `generation,best_ppm,best_ever_ppm,mean_ppm,seconds`. |

## Where the homogeneity and field figures live

In `optimized.json`, under `halbach_optimization`. Two blocks, and the
difference between them matters:

```json
"optimized": { "homogeneity_ppm": 90.4,  "mean_T": 0.2127, "min_T": ..., "max_T": ...,
               "peak_to_peak_T": ..., "points": 4662 },
"verified":  { "homogeneity_ppm": 91.4,  "mean_T": 0.2127, ..., "points": 33401 }
```

`optimized` is the figure the search was steered by, over whatever symmetry
reduction it used. `verified` is the same figure taken again over the **whole**
sphere, from the magnets themselves, through the simulator the rest of the
project uses. **Quote `verified`.** A number that was optimized cannot also be
the evidence that the optimization worked, and the two do come apart: on
`nmr-3ring-21magnets` the octant search said 531 ppm where the honest figure was
719, because a 3-magnet ring carries a harmonic the octant reflections do not
respect. Everything with ten or more magnets to a ring agrees to about 1%.

Alongside them: `basis` (the size of the precomputation), `magnet_count`,
`genome`, `chosen` (which candidate went to which ring position, with radii and
counts), and `timing_s`.

## The designs

Field figures are `verified`, over the whole sample sphere.

| design | magnets | cube | field | homogeneity | DSV | tube |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| `imaging-200mm-dsv` | 2971 | 12 mm | 49.1 mT (2.09 MHz) | 805 ppm | 200 mm | — imaging |
| `nmr-21ring-10mm-tube` | 789 | 8 mm | 316.0 mT (13.46 MHz) | 215 ppm | 20 mm | 10 mm |
| `nmr-5ring-10mm-tube` | 185 | 8 mm | 212.7 mT (9.05 MHz) | **91 ppm** | 12 mm | 10 mm |
| `nmr-3ring-5mm-tube` | 99 | 8 mm | 178.6 mT (7.60 MHz) | 247 ppm | 8 mm | 5 mm |
| `nmr-2ring-5mm-tube` | 86 | 8 mm | 82.8 mT (3.52 MHz) | 246 ppm | 6 mm | 5 mm |
| `nmr-3ring-21magnets` | 21 | 24 mm | 171.2 mT (7.29 MHz) | 602 ppm | 4 mm | capillary |
| `nmr-2ring-10magnets` | 10 | 24 mm | 83.9 mT (3.57 MHz) | 9396 ppm | 4 mm | — not usable |

`imaging-200mm-dsv` is the port of `HalbachOptimisation/`, the Python project
this optimizer replaces. The rest are NMR magnets for a sample tube along the
bore, with B0 transverse to it.

## Reading the ppm figure

Peak-to-peak over a sphere is a worst-case number and it **overstates the line
you would actually see**, often by a lot. What is left after optimization is
mostly ring-discreteness ripple, which grows as (r/R)^(N-2) — for a 15-magnet
ring that is r¹³, so almost all of it lives in a thin shell at the very edge of
the sphere. On `nmr-5ring-10mm-tube`, half the volume sits within 4.8 ppm of the
mean while the peak-to-peak is 91.

The lineshape is just the histogram of `Bmag` over the detected volume, so
`field.csv` gives it directly:

| design | p-p ppm | line FWHM | as ppm | T2\* |
| --- | ---: | ---: | ---: | ---: |
| `nmr-21ring-10mm-tube` | 215 | 96 Hz | 7.2 | 3.3 ms |
| `nmr-5ring-10mm-tube` | 91 | 48 Hz | 5.3 | 6.6 ms |
| `nmr-3ring-5mm-tube` | 247 | 78 Hz | 10.3 | 4.1 ms |
| `nmr-2ring-5mm-tube` | 246 | 22 Hz | 6.2 | 14.7 ms |
| `nmr-3ring-21magnets` | 602 | 1024 Hz | 140 | 0.31 ms |
| `nmr-2ring-10magnets` | 9396 | 5878 Hz | 1644 | 0.05 ms |

Against a probe dead time of 10–50 µs, every T2\* above 1 ms loses well under a
percent of the FID. None of these has the ~1 ppm linewidth chemical shift needs,
which is what shimming is for; and none of it survives temperature drift, since
NdFeB moves about −1100 ppm/K.

## Regenerating

```bash
./regenerate.sh                       # every design
./regenerate.sh nmr-5ring-10mm-tube   # one of them
./regenerate.sh --check               # rebuild into a temp dir and compare, changing nothing
```

`--check` compares the genome and the `verified` figures, not the timings.
Every design here reproduces exactly for its seed and thread count; across
different thread counts the genetic operators draw from a thread-partitioned
random pool and float sums reassociate, so the answers agree on the design but
need not agree in the last digit.

The `field.csv` files are about 3 MB each, 22 MB in total. They are
reproducible from `config.json` in seconds to a couple of minutes, so add
`designs/*/field.csv` to `.gitignore` if you would rather not carry them.

## Running one

```bash
./build/standalone/Greeter -i designs/nmr-5ring-10mm-tube/optimized.json \
                           -s designs/nmr-5ring-10mm-tube/snapshot.json
```

The snapshot opens in the viewer. `optimized.json` needs no editing — the
metadata block is ignored by the readers, and the `field_of_view` is already the
sample sphere's bounding box.
