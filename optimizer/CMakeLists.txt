cmake_minimum_required(VERSION 3.14...3.22)

project(HalbachOptimizer LANGUAGES CXX)

# --- Import tools ----

include(../cmake/tools.cmake)

# ---- Dependencies ----

include(../cmake/CPM.cmake)

CPMAddPackage(
  GITHUB_REPOSITORY jarro2783/cxxopts
  VERSION 3.0.0
  OPTIONS "CXXOPTS_BUILD_EXAMPLES NO" "CXXOPTS_BUILD_TESTS NO" "CXXOPTS_ENABLE_INSTALL YES"
)

CPMAddPackage(NAME Greeter SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/..)

# ---- Create the optimizer executable ----

file(GLOB sources CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp)

add_executable(${PROJECT_NAME} ${sources})

set_target_properties(
  ${PROJECT_NAME} PROPERTIES CXX_STANDARD 17 OUTPUT_NAME "halbach-optimizer"
)

target_link_libraries(${PROJECT_NAME} Greeter::Greeter cxxopts)
target_link_libraries(${PROJECT_NAME} Kokkos::kokkos)
target_link_libraries(${PROJECT_NAME} nlohmann_json::nlohmann_json)

find_package(Python3 QUIET COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
  enable_testing()
  add_test(NAME optimizer-cli COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/test/smoke.py $<TARGET_FILE:${PROJECT_NAME}>)
  set_tests_properties(optimizer-cli PROPERTIES ENVIRONMENT "OMP_NUM_THREADS=2;OMP_PROC_BIND=false")
endif()
