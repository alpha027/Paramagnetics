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

- Elementary magnets: cuboids, spheres, tetrahedra and cylinders
- Parallel magnetic force and torque simulation between magnets
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

A magnet is a `cuboid`, a `sphere`, a `tetrahedron` or a `cylinder`. They differ in how their
geometry is given:

| `type` | geometry | `magnetization` |
| --- | --- | --- |
| `cuboid` | `dimensions`: `[a, b, c]`, the side lengths in metre | `[Jx, Jy, Jz]` in Tesla |
| `sphere` | `dimensions`: the radius in metre, as a number or as `[r]` | `J` along the local z axis, as a number or as `[0, 0, J]` |
| `tetrahedron` | `vertices`: four points in the local frame, in metre | `[Jx, Jy, Jz]` in Tesla |
| `cylinder` | `dimensions`: `[d, h]`, the diameter and the height in metre | `[Jx, Jy, Jz]` in Tesla, or a number for `[0, 0, J]` |

A sphere is magnetized along its own z axis, so a magnetization that points elsewhere is expressed
by rotating the sphere with its `orientation` rather than by giving a transverse component.

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
