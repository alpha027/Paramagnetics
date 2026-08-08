[![Actions Status](https://github.com/TheLartians/ModernCppStarter/workflows/MacOS/badge.svg)](https://github.com/TheLartians/ModernCppStarter/actions)
[![Actions Status](https://github.com/TheLartians/ModernCppStarter/workflows/Windows/badge.svg)](https://github.com/TheLartians/ModernCppStarter/actions)
[![Actions Status](https://github.com/TheLartians/ModernCppStarter/workflows/Ubuntu/badge.svg)](https://github.com/TheLartians/ModernCppStarter/actions)
[![Actions Status](https://github.com/TheLartians/ModernCppStarter/workflows/Style/badge.svg)](https://github.com/TheLartians/ModernCppStarter/actions)
[![Actions Status](https://github.com/TheLartians/ModernCppStarter/workflows/Install/badge.svg)](https://github.com/TheLartians/ModernCppStarter/actions)
[![codecov](https://codecov.io/gh/TheLartians/ModernCppStarter/branch/master/graph/badge.svg)](https://codecov.io/gh/TheLartians/ModernCppStarter)

<!-- <p align="center">
  <img src="https://repository-images.githubusercontent.com/254842585/4dfa7580-7ffb-11ea-99d0-46b8fe2f4170" height="175" width="auto" />
</p> -->


<div align="center" style="text-align: center;">
  <img src="magnet.webp" alt="Description of image" width="245">
</div>

# ParaMagneticS

Stands for parallel magnetic field simulations. This repository offers a user friendly tool to efficiently simulate the magnetic field stemming from simple magnet geometries: cuboid, sphere, tetrahedron and cylinder. ParaMagneticS is implemented in C++ and allows to explore the magnetic field for a combination of magnets. The simulations are performed in a parallel manner to reduce the design iteration time for different magnet configurations.

## Features

- Elementary magnets: cuboids, spheres, tetrahedra, cylinders, ring sectors, point dipoles, charged triangles, and bodies of any shape at all as a closed triangular mesh
- The magnetic flux density **B**, the field strength **H**, the polarization **J** and the magnetization **M**
- Parametrised arrangements of them, built from the parameter file rather than written out one by one
- Parallel magnetic force and torque simulation between magnets
- A Qt viewer for the result, which is a separate program and needs neither Kokkos nor the input file
- Easy and seamless workflow using a JSON parameter file
- [Modern CMake practices](https://pabloariasal.github.io/2018/02/19/its-time-to-do-cmake-right/)
- Clean separation of library and executable code
- Integrated test suite
- Continuous integration via [GitHub Actions](https://help.github.com/en/actions/)
- Code coverage via [codecov](https://codecov.io)
<!-- - Code formatting enforced by [clang-format](https://clang.llvm.org/docs/ClangFormat.html) and [cmake-format](https://github.com/cheshirekow/cmake_format) via [Format.cmake](https://github.com/TheLartians/Format.cmake)
- Reproducible dependency management via [CPM.cmake](https://github.com/TheLartians/CPM.cmake)
- Installable target with automatic versioning information and header generation via [PackageProject.cmake](https://github.com/TheLartians/PackageProject.cmake)
- Automatic [documentation](https://thelartians.github.io/ModernCppStarter) and deployment with [Doxygen](https://www.doxygen.nl) and [GitHub Pages](https://pages.github.com)
- Support for [sanitizer tools, and more](#additional-tools) -->

## Usage

### Build and run with Docker (Recommended)

To build a Docker image of the ParaMagneticS toolkit, run the following command:

```bash
docker build -t paramagnetics .
```

The command above builds the docker image. To run a simulation using the built Docker image, run the followin command:

```bash
OMP_PROC_BIND=true;
docker run --rm \
           -v /full_dir_path_to_data_input:/app/data:ro \
           -v /full_dir_path_to_data_output:/app/output \
           paramagnetics
```

The `/full_dir_path_to_data_input` is the full directory to the `input_data.json` file (see **Input Data** section), it should contain a single file.
The `/full_dir_path_to_data_output` is the directory in which the simulation results will be saved.


### Build and run the standalone target

Use the compiling script *compile.sh*:
```bash
./compile.sh
```

or use the following command to build and run the executable target.

```bash
cmake -S standalone -B build/standalone -DCMAKE_BUILD_TYPE=Release -DKokkos_ENABLE_OPENMP=On -DCMAKE_CXX_COMPILER=g++
cmake -S test -B build/test -DCMAKE_BUILD_TYPE=Release -DKokkos_ENABLE_OPENMP=On -DCMAKE_CXX_COMPILER=g++
./build/standalone/Greeter --help
```

### Build and run test suite

Use the following commands from the project's root directory to run the test suite.

```bash
cmake -S test -B build/test -DCMAKE_BUILD_TYPE=Release -DKokkos_ENABLE_OPENMP=On -DCMAKE_CXX_COMPILER=g++
cmake --build build/test -DCMAKE_BUILD_TYPE=Release -DKokkos_ENABLE_OPENMP=On -DCMAKE_CXX_COMPILER=g++
CTEST_OUTPUT_ON_FAILURE=1 cmake --build build/test --target test

# or simply call the executable: 
./build/test/GreeterTests
```

To collect code coverage information, run CMake with the `-DENABLE_TEST_COVERAGE=1` option.

### Build and run the viewer

The viewer draws a scene, the field it makes and the forces in it. It needs Qt 6 and is built
separately, so that everything above builds on a machine without it.

```bash
# Debian and Ubuntu; other systems have their own names for these
sudo apt install qt6-base-dev libqt6opengl6-dev

cmake -S gui -B build/gui -DCMAKE_BUILD_TYPE=Release -DKokkos_ENABLE_OPENMP=On -DCMAKE_CXX_COMPILER=g++
cmake --build build/gui

./build/gui/paramagnetics-viewer arrangements.json
```

Open an input file, press **Run**, and the field and the forces it asks for are simulated on a
thread of their own, so the window stays usable and the run can be stopped. Drag to turn, shift
drag or middle drag to slide, the wheel to zoom, and click a magnet to pick it out. The magnets an
arrangement generated are grouped under it on the left.

The field can be shown as a slice plane, as arrows, or as field lines followed through it. Arrows
are all drawn the same length and the strength is in the colour: a length that followed the
strength would draw one arrow across the whole box and leave every other one too small to see. For
the same reason the colour scale is logarithmic by default and stops short of the largest sample.

It also draws without a window, which is useful on a machine that has none and for making the same
figure repeatedly:

```bash
./build/gui/paramagnetics-viewer arrangements.json --show slice,lines,magnets --draw field.png
```

#### Looking at a run somebody else did

The viewer does not need the simulator. A run on a cluster writes a snapshot, and the snapshot
opens anywhere:

```bash
./build/standalone/Greeter --input arrangements.json --snapshot run.pmsnap
./build/gui/paramagnetics-viewer run.pmsnap
```

A snapshot holds the magnets, the field and the forces, and nothing needs to be simulated again to
look at it. `run.json` instead of `run.pmsnap` writes the readable form, which is worth having for
a small field and hopeless for a large one: a grid of a hundred points a side is three million
numbers.

### Input Data

The input data is a *.json* file that has the following format:

```txt
{
  "magnets": [
    {
      "id": 1,
      "type": "cuboid",
      "parameters": {
        "dimensions": [1, 1, 1],
        "magnetization": [0, 1, 0],
        "position": [0, 0, 0],
        "orientation": [1, 0, 0, 0] 
        }
    }
  ],
   "field_of_view": {
     "x": {
       "min": 2,
       "max": 4,
       "n": 3
    },
     "y": {
       "min": 0,
       "max": 3,
       "n": 4
    },
     "z": {
       "min": 0,
       "max": 10,
       "n": 11
    }
   }
}
```
Note that the **orientation** field in the JSON parameter file represents a quaternion, given as `[w, x, y, z]`.
The **magnetization** field is the magnetic polarization **J** of the magnet, in Tesla.

#### Which quantity is simulated

The field of view computes **B** unless it is asked for something else:

```txt
"field_of_view": {
  "quantity": "H",
  "x": {"min": -0.05, "max": 0.05, "n": 41},
  ...
}
```

| `quantity` | | unit |
| --- | --- | --- |
| `B` | magnetic flux density, the default | Tesla |
| `H` | magnetic field strength, `(B - J) / mu0` | ampere per metre |
| `J` | magnetic polarization: what the magnet is made of where it is, zero elsewhere | Tesla |
| `M` | magnetization, `J / mu0` | ampere per metre |

The last three cost a second look at every magnet, to ask whether the point is inside it, so `B`
stays the default. **H** is the one worth knowing about: inside a magnet it points *against* the
polarization, which is why a magnet demagnetizes itself. Inside a sphere of polarization J it is
exactly `-J / (3 mu0)`, and the tests check that.

Magnets differ in how their geometry is given:

| `type` | geometry | `magnetization` |
| --- | --- | --- |
| `cuboid` | `dimensions`: `[a, b, c]`, the side lengths in metre | `[Jx, Jy, Jz]` in Tesla |
| `sphere` | `dimensions`: the radius in metre, as a number or as `[r]` | `J` along the local z axis, as a number or as `[0, 0, J]` |
| `tetrahedron` | `vertices`: four points in the local frame, in metre | `[Jx, Jy, Jz]` in Tesla |
| `cylinder` | `dimensions`: `[d, h]`, the diameter and the height in metre | `[Jx, Jy, Jz]` in Tesla, or a number for `[0, 0, J]` |
| `cylinder_segment` | `dimensions`: `[r1, r2, h, phi1, phi2]`, the radii and height in metre and the angles in degrees | `[Jx, Jy, Jz]` in Tesla |
| `triangular_mesh` | `vertices` and `faces`, or `triangles` | `[Jx, Jy, Jz]` in Tesla |
| `triangle` | `vertices`: three points in the local frame, in metre | `[Jx, Jy, Jz]` in Tesla |
| `dipole` | none, it is a point | **`moment`**: `[mx, my, mz]` in ampere metre squared |

A sphere is magnetized along its own z axis, so a magnetization that points elsewhere is expressed
by rotating the sphere with its `orientation` rather than by giving a transverse component.

**`triangular_mesh`** is a body of any shape at all, given as a closed surface of triangles, either
as `vertices` plus `faces` of three indices each, or written out directly as `triangles`. The
surface has to be closed, and is checked; one wound inside out is turned the right way round rather
than refused. A tetrahedron is the same thing with four faces, and the two agree to the last digit.

**`cylinder_segment`** is the arc shaped block that real Halbach rings and motor rotors are made of.
It is built as a faceted body rather than from a closed form: the curved walls are cut into
`segments` flat strips, 32 by default. Against magpylib's exact expression for a sector of
r1 = 20 mm, r2 = 30 mm, h = 10 mm over 60°, the worst error is 2.2 % at 4 facets, 0.14 % at 16,
**0.035 % at the default 32**, and 0.009 % at 64 — falling with the square of the facet size, and
already far below any tolerance a magnet is actually made to. Raise `segments` if the field right
against the curved wall matters.

**`triangle`** is a single magnetically charged facet, the piece every polyhedron is built from. It
is a surface and not a body: it has no volume, so it cannot be a force target, and asking for one
says so.

**`dipole`** carries a **`moment` in ampere metre squared**, not a `magnetization` in Tesla — a
magnet of volume V and polarization J has the moment `V * J / mu0`. Writing it as `magnetization`
is refused rather than quietly read as a moment. It is worth having as a deliberate far field
approximation: an array of a thousand magnets seen from a metre away is a thousand dipoles, and
costs a fraction of the shaped kernels to evaluate as such.

```txt
{
  "id": 2,
  "type": "sphere",
  "parameters": {
    "dimensions": 0.5,
    "magnetization": 1.0,
    "position": [0, 0, 0],
    "orientation": [1, 0, 0, 0]
    }
}
```

The four vertices of a tetrahedron are given in its own frame, and `position` and `orientation`
then place it. They are written either as four points or as a flat list of twelve numbers, and
they must not be coplanar. Their winding does not matter.

```txt
{
  "id": 3,
  "type": "tetrahedron",
  "parameters": {
    "vertices": [[0, 0, 0], [1, 0, 0], [0, 1, 0], [0, 0, 1]],
    "magnetization": [0.3, -0.7, 0.5],
    "position": [0, 0, 0],
    "orientation": [1, 0, 0, 0]
    }
}
```

The field of a tetrahedron is the sum of the fields of its four magnetically charged faces, plus
the polarization itself inside the body, following Guptasarma, *Geophysics* 64(1), 1999. Unlike a
cuboid or a sphere, its barycenter is not its position, and it is the barycenter that a torque
refers to by default.

The axis of a cylinder is its own z axis and its center is its `position`, so `orientation` is what
points it elsewhere. A magnetization along the axis and one across it are two different closed
forms, and an arbitrary magnetization is the superposition of the two:

```txt
{
  "id": 4,
  "type": "cylinder",
  "parameters": {
    "dimensions": [1.5, 0.8],
    "magnetization": [0.3, -0.5, 0.8],
    "position": [2, 0, 0],
    "orientation": [1, 0, 0, 0]
    }
}
```

The axial part follows Derby, *American Journal of Physics* 78(3), 2010, and the part across the
axis follows Caciagli, *Journal of Magnetism and Magnetic Materials* 456, 2018. Both reduce to
Bulirsch's complete elliptic integral, which is the only special function the cylinder needs. The
field is not defined on the rim where the hull meets a base, and is reported as zero there.

### Arrangements

A group of identical magnets does not have to be written out one by one. An optional
`arrangements` section describes it by its parameters instead, and the reader turns it into
ordinary magnets that the rest of the library cannot tell apart from listed ones. A file may hold
`magnets`, `arrangements`, or both.

```txt
{
  "arrangements": [
    {
      "id": 100,
      "type": "linear_array",
      "parameters": {
        "count": [4, 2, 1],
        "spacing": [0.03, 0.03, 0.03],
        "alternating": "x",
        "position": [0, 0, 0],
        "orientation": [1, 0, 0, 0],
        "element": {
          "type": "cuboid",
          "parameters": {
            "dimensions": [0.02, 0.02, 0.02],
            "magnetization": [0, 0, 1]
          }
        }
      }
    },
    {
      "id": 200,
      "type": "halbach_ring",
      "parameters": {
        "radius": 0.3,
        "count": 16,
        "order": 1,
        "element": {
          "type": "cuboid",
          "parameters": {
            "dimensions": [0.1, 0.1, 0.1],
            "magnetization": [0, 1, 0]
          }
        }
      }
    }
  ]
}
```

The `element` is the magnet the arrangement repeats, written in the schema of that magnet minus
its `position` and `orientation`, which the arrangement is what decides. It goes through the
reader of its own type, so any magnet type works as an element, and an element that places itself
is refused rather than quietly overwritten. Its `magnetization` is read in its own frame and is
carried along when the arrangement turns it, so a member moves as a rigid body, its geometry and
its polarization together.

The `position` and `orientation` of an arrangement place the assembly as a whole and are both
optional, defaulting to the origin and no rotation. A member is placed in the frame of the
arrangement first and carried into the world second.

| `type` | parameters |
| --- | --- |
| `linear_array` | `count`: members along the local x, y and z as `[nx, ny, nz]`, or a single number for a row. `spacing`: the distance between neighbours in metre as `[dx, dy, dz]`, or a single number for all three. `centered`: optional, `true` by default, so the lattice sits on `position` rather than starting there. `alternating`: optional, `"x"`, `"y"` or `"z"`, turning every other member half a turn about that local axis. |
| `circular_array` | `radius`: of the circle the members sit on, in metre. `count`: members round it. `face`: optional, `"axis"` by default, leaving every member pointing the same way, or `"center"`, carrying the frame of a member round the ring with it. |
| `halbach_ring` | `radius` and `count` as above. `order`: optional, `1` by default, turning the member at the angle `t` by `(order + 1) * t` about the axis of the ring. |
| `halbach_linear` | `count`: members in the row. `spacing`: the distance between neighbours in metre. `steps_per_period`: members per full turn of the polarization, or `wavelength`: the same thing as a length, exactly one of the two. `centered`: optional, `true` by default. |

The members of a lattice come out with x running fastest and z slowest, so the member at
`(ix, iy, iz)` is number `ix + nx * (iy + ny * iz)`. The members of a ring come out in the order
they sit round it, starting on the local x axis and turning towards the local y axis.

`alternating` is a rotation rather than a sign change on the magnetization, which is what keeps a
member a rigid body; naming the axis the magnetization already lies along therefore leaves that
member as it was.

A Halbach ring of order 1 is the dipole ring, whose field is uniform inside it and cancels
outside. Order 2 is the quadrupole, and order -1 leaves every member pointing the same way, which
is what makes a `circular_array` the same ring without the turning: `"face": "axis"` is order -1
and `"face": "center"` is order 0. The order is a whole number, because the polarization has to
come back to itself after a full turn round the ring, and it may be negative.

The axis the field of a ring lies along is set by the element, whose magnetization is read in its
own frame, so a ring of order 1 built from an element polarized along its local y makes a field
along y. Which way along that axis is decided by the ring rather than by the element: an element
polarized along `+y` gives a field along `-y`.

A linear Halbach row turns each member a little further about the local y axis than the one
before, so a polarization along the local z axis sweeps round in the local xz plane and the field
of the row is strong on one side and nearly cancels on the other. Four members to a period is the
usual choice. With a positive `steps_per_period` and an element polarized along its local z, the
strong side is local `-z`; negating `steps_per_period` or `wavelength` mirrors the row and swaps
the two sides. The turn of member `i` is counted off `i` rather than off where it sits, so the
first member is never turned however the row is placed.

Note that `face: "center"` decides which way the *frame* of a member points, and it is the
element that decides what lies along that frame: its magnetization is read in its own frame, so
`[J, 0, 0]` ends up radial and `[0, J, 0]` tangential. An element cannot be given a rotation of its
own, so a shape whose geometry rather than its magnetization has to point at the middle of a ring,
such as a cylinder lying on its side, is not expressible yet.

The magnets an arrangement generates are numbered on from the highest `id` the file uses, so
adding an arrangement never renames a magnet that was already there. They can be named
individually by those ids, or all at once by the arrangement they belong to, see below. See
`arrangements.json` for a complete example.

### Force and torque simulation

Add an optional `force` section to the input file to compute the magnetic force (in Newton) and the
torque (in Newton metre) that the magnets exert on each other:

```txt
"force": {
  "targets": [2],
  "meshing": 1000
}
```

Every target is split into cells that carry a magnetic moment. The field of the sources and its
gradient are evaluated at each cell by finite differences, which gives the force
`F = (grad B) . m` and the torque `T = m x B + (r - pivot) x F` of the cell. The cell contributions
are then summed per target. This is the scheme of the
[magpylib](https://github.com/magpylib/magpylib) `getFT` function, and the cells are distributed
over the available threads in the same way as the observation points of a field simulation.

The fields of the `force` section are:

| Field | Default | Meaning |
| --- | --- | --- |
| `targets` | `"all"` | Magnet ids the force acts on. Either `"all"`, a list of ids, or a list of target objects (see below). |
| `sources` | every other magnet | Magnet ids that generate the field. A target never acts on itself. |
| | | An id in either list may be replaced by `{"arrangement": id}`, which names every member of that arrangement at once. `"all"` covers the generated magnets as well. |
| `meshing` | `1` | Number of cells per target, or `[n1, n2, n3]` cells along the local axes. A spherical magnet is exactly a point dipole and is never split. A tetrahedron is split on a barycentric grid and a cylinder into rings, so both take a cell count and read `[n1, n2, n3]` as their product. A cylinder never yields fewer than two cells, because its split is apportioned over circumference, radius and height. |
| `pivot` | `"centroid"` | Point through which the force contributes to the torque, either `"centroid"` or `[x, y, z]`. |
| `eps` | `1e-3 * magnet size` | Finite difference step in metre used for the field gradient. |

`meshing`, `sources` and `pivot` can also be set per target, by listing target objects instead of
plain ids:

```txt
"force": {
  "targets": [
    {"id": 2, "meshing": [4, 4, 4], "sources": [1]},
    {"id": 3, "meshing": 500, "pivot": [0, 0, 0]}
  ],
  "eps": 1e-4
}
```

A target object may name an arrangement instead of a single magnet, in which case every member of
it becomes a target and they share what the object says:

```txt
"force": {
  "targets": [
    {"id": 1, "meshing": 500},
    {"arrangement": 100, "meshing": 8, "sources": [1]}
  ]
}
```

Note that a whole arrangement is as many targets as it has members, and the cost of a force
simulation is the number of targets times the cells each is split into times the number of
sources, so an arrangement of any size is where that cost starts to be felt.

A larger `meshing` gives a more accurate force, at a cost that grows linearly with the number of
cells. A single cell reduces the target to a point dipole in its center, which is already exact for
a spherical magnet.

The kernels of this library run in single precision, so `eps` cannot be made arbitrarily small
without drowning the finite difference in round-off. The default of one thousandth of the magnet
size keeps both the round-off and the truncation error small; values below `1e-5 * magnet size` are
not recommended.

### Output Data

The main script generates a *.csv* file containing the values of the magnetic field resulting from
the provided magnets in the input JSON file.

When the input file has a `force` section, a second *.csv* file `force_results.csv` is written, with
one row per target and the columns

```txt
magnet_id, Fx, Fy, Fz, Tx, Ty, Tz
```

where the force is given in Newton and the torque in Newton metre.

Neither *.csv* says where anything was measured: the field file is three numbers a sample, in the
order the field of view lays its points out, which is x slowest and z fastest. A snapshot does say,
and is what the viewer opens:

```bash
./build/standalone/Greeter --input arrangements.json --snapshot run.pmsnap
```

A snapshot carries the magnets, their shapes and where they sit, the field together with the box it
was sampled in, and the forces keyed by magnet id.

### Build the documentation

The documentation is automatically built and [published](https://thelartians.github.io/ModernCppStarter) whenever a [GitHub Release](https://help.github.com/en/github/administering-a-repository/managing-releases-in-a-repository) is created.
To manually build documentation, call the following command.

```bash
cmake -S documentation -B build/doc
cmake --build build/doc --target GenerateDocs
# view the docs
open build/doc/doxygen/html/index.html
```

To build the documentation locally, you will need Doxygen, jinja2 and Pygments installed on your system.

<!-- ### Build everything at once

The project also includes an `all` directory that allows building all targets at the same time.
This is useful during development, as it exposes all subprojects to your IDE and avoids redundant builds of the library.

```bash
cmake -S all -B build -DCMAKE_BUILD_TYPE=Release -DKokkos_ENABLE_OPENMP=On
cmake --build build -DCMAKE_BUILD_TYPE=Release -DKokkos_ENABLE_OPENMP=On

# run tests
./build/test/GreeterTests
# format code
cmake --build build --target fix-format
# run standalone
./build/standalone/Greeter --help
# build docs
cmake --build build --target GenerateDocs
``` -->

### Additional tools

The test and standalone subprojects include the [tools.cmake](cmake/tools.cmake) file which is used to import additional tools on-demand through CMake configuration arguments.
The following are currently supported.

#### Sanitizers

Sanitizers can be enabled by configuring CMake with `-DUSE_SANITIZER=<Address | Memory | MemoryWithOrigins | Undefined | Thread | Leak | 'Address;Undefined'>`.

#### Static Analyzers

Static Analyzers can be enabled by setting `-DUSE_STATIC_ANALYZER=<clang-tidy | iwyu | cppcheck>`, or a combination of those in quotation marks, separated by semicolons.
By default, analyzers will automatically find configuration files such as `.clang-format`.
Additional arguments can be passed to the analyzers by setting the `CLANG_TIDY_ARGS`, `IWYU_ARGS` or `CPPCHECK_ARGS` variables.

#### Ccache

Ccache can be enabled by configuring with `-DUSE_CCACHE=<ON | OFF>`.

## References

- **[MagPyLib](https://www.sciencedirect.com/science/article/pii/S2352711020300170)**, [repo](https://github.com/magpylib/magpylib)

- **[MagTetris](https://www.sciencedirect.com/science/article/abs/pii/S1090780723000988?via%3Dihub)**, [repo](https://github.com/BioMed-EM-Lab/MagTetris)

## FAQ

<!-- > Can I use this for header-only libraries?

Yes, however you will need to change the library type to an `INTERFACE` library as documented in the [CMakeLists.txt](CMakeLists.txt).
See [here](https://github.com/TheLartians/StaticTypeInfo) for an example header-only library based on the template.

> I don't need a standalone target / documentation. How can I get rid of it?

Simply remove the standalone / documentation directory and according github workflow file.

> Can I build the standalone and tests at the same time? / How can I tell my IDE about all subprojects?

To keep the template modular, all subprojects derived from the library have been separated into their own CMake modules.
This approach makes it trivial for third-party projects to re-use the projects library code.
To allow IDEs to see the full scope of the project, the template includes the `all` directory that will create a single build for all subprojects.
Use this as the main directory for best IDE support.

> I see you are using `GLOB` to add source files in CMakeLists.txt. Isn't that evil?

Glob is considered bad because any changes to the source file structure [might not be automatically caught](https://cmake.org/cmake/help/latest/command/file.html#filesystem) by CMake's builders and you will need to manually invoke CMake on changes.
  I personally prefer the `GLOB` solution for its simplicity, but feel free to change it to explicitly listing sources.

> I want create additional targets that depend on my library. Should I modify the main CMakeLists to include them?

Avoid including derived projects from the libraries CMakeLists (even though it is a common sight in the C++ world), as this effectively inverts the dependency tree and makes the build system hard to reason about.
Instead, create a new directory or project with a CMakeLists that adds the library as a dependency (e.g. like the [standalone](standalone/CMakeLists.txt) directory).
Depending type it might make sense move these components into a separate repositories and reference a specific commit or version of the library.
This has the advantage that individual libraries and components can be improved and updated independently.

> You recommend to add external dependencies using CPM.cmake. Will this force users of my library to use CPM.cmake as well?

[CPM.cmake](https://github.com/TheLartians/CPM.cmake) should be invisible to library users as it's a self-contained CMake Script.
If problems do arise, users can always opt-out by defining the CMake or env variable [`CPM_USE_LOCAL_PACKAGES`](https://github.com/cpm-cmake/CPM.cmake#options), which will override all calls to `CPMAddPackage` with the according `find_package` call.
This should also enable users to use the project with their favorite external C++ dependency manager, such as vcpkg or Conan.

> Can I configure and build my project offline?

No internet connection is required for building the project, however when using CPM missing dependencies are downloaded at configure time.
To avoid redundant downloads, it's highly recommended to set a CPM.cmake cache directory, e.g.: `export CPM_SOURCE_CACHE=$HOME/.cache/CPM`.
This will enable shallow clones and allow offline configurations dependencies are already available in the cache.

> Can I use CPack to create a package installer for my project?

As there are a lot of possible options and configurations, this is not (yet) in the scope of this template. See the [CPack documentation](https://cmake.org/cmake/help/latest/module/CPack.html) for more information on setting up CPack installers.

> This is too much, I just want to play with C++ code and test some libraries.

Perhaps the [MiniCppStarter](https://github.com/TheLartians/MiniCppStarter) is something for you! -->

## Related projects and alternatives

- [**ModernCppStarter & PVS-Studio Static Code Analyzer**](https://github.com/viva64/pvs-studio-cmake-examples/tree/master/modern-cpp-starter): Official instructions on how to use the ModernCppStarter with the PVS-Studio Static Code Analyzer.
- [**cpp-best-practices/gui_starter_template**](https://github.com/cpp-best-practices/gui_starter_template/): A popular C++ starter project, created in 2017.
- [**filipdutescu/modern-cpp-template**](https://github.com/filipdutescu/modern-cpp-template): A recent starter using a more traditional approach for CMake structure and dependency management.
- [**vector-of-bool/pitchfork**](https://github.com/vector-of-bool/pitchfork/): Pitchfork is a Set of C++ Project Conventions.

<!-- ## Star History

[![Star History Chart](https://api.star-history.com/svg?repos=TheLartians/ModernCppStarter,cpp-best-practices/gui_starter_template,filipdutescu/modern-cpp-template&type=Date)](https://star-history.com/#TheLartians/ModernCppStarter&cpp-best-practices/gui_starter_template&filipdutescu/modern-cpp-template&Date) -->
