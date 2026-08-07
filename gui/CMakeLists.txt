cmake_minimum_required(VERSION 3.16...3.22)

project(ParaMagneticSViewer LANGUAGES CXX)

# ---- Import tools ----

include(../cmake/tools.cmake)

# ---- Dependencies ----

include(../cmake/CPM.cmake)

CPMAddPackage(NAME Greeter SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/..)

find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Widgets OpenGL OpenGLWidgets)

qt_standard_project_setup()

# ---- Create the viewer ----

file(GLOB sources CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp)
file(GLOB headers CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/source/*.h)

qt_add_executable(${PROJECT_NAME} ${sources} ${headers})

set_target_properties(${PROJECT_NAME} PROPERTIES
  CXX_STANDARD 17
  OUTPUT_NAME "paramagnetics-viewer"
)

target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wpedantic -Wextra)

# No Qt in the library, no Kokkos in the viewer's own sources: everything the
# viewer asks of the simulation goes through greeter/view and greeter/service,
# neither of which names a Kokkos type. That the two header sets stay clean is
# checked by the GreeterViewBoundary target in test/CMakeLists.txt, which
# compiles them with Kokkos absent from the include path.
#
# Kokkos is still linked, because the library it is linked into is a static
# one and its symbols have to be resolved. That is a link time fact and not a
# compile time dependency: no source file below includes a Kokkos header.
target_link_libraries(${PROJECT_NAME} PRIVATE
  Greeter::Greeter
  Kokkos::kokkos
  nlohmann_json::nlohmann_json
  Qt6::Widgets
  Qt6::OpenGLWidgets
)

# ---- Tests ----

# Turning a scene into triangles, following a field line and working out what
# a colour stands for are ordinary computations that need no window, and they
# are the half of the viewer worth testing. Note what this target does not
# link: no Qt. If the geometry ever grows a dependency on it, this stops
# building, which is the same trick the view layer's boundary guard plays
# with Kokkos.
# doctest 2.4.9 asks for a CMake older than CMake 4 will agree to, so it is
# told what floor to assume. The test build does the same from its command
# line; doing it here means the viewer configures without one.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

CPMAddPackage("gh:doctest/doctest@2.4.9")

file(GLOB test_sources CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp)

add_executable(ParaMagneticSViewerTests
  ${test_sources}
  ${CMAKE_CURRENT_SOURCE_DIR}/source/GeometryBuilder.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/source/ColorMap.cpp
)

set_target_properties(ParaMagneticSViewerTests PROPERTIES CXX_STANDARD 17)

target_include_directories(ParaMagneticSViewerTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)

target_compile_options(ParaMagneticSViewerTests PRIVATE -Wall -Wpedantic -Wextra)

target_link_libraries(ParaMagneticSViewerTests PRIVATE
  doctest::doctest
  Greeter::Greeter
  Kokkos::kokkos
)
