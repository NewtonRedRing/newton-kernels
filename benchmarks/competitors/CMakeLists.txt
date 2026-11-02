# Entirely opt-in: no external libraries are required to build NewtonKernels.
option(NEWTON_COMP_BENCH_WITH_BOOST "Enable Boost.Graph competitor" ON)
option(NEWTON_COMP_BENCH_WITH_VINCE "Enable Vince's CSV Parser competitor" OFF)
set(NEWTON_VINCE_SOURCE_DIR "" CACHE PATH "Existing external csv-parser checkout; avoids any network fetch")

add_executable(newton_compare comparison.cpp)
target_compile_features(newton_compare PRIVATE cxx_std_20)
target_link_libraries(newton_compare PRIVATE newton_simd newton_parsing newton_graph)

if(NEWTON_COMP_BENCH_WITH_BOOST)
  find_package(Boost 1.71 QUIET)
  if(Boost_FOUND)
    target_include_directories(newton_compare SYSTEM PRIVATE ${Boost_INCLUDE_DIRS})
    target_compile_definitions(newton_compare PRIVATE NEWTON_COMP_BENCH_BOOST)
  else()
    message(WARNING "Boost headers unavailable; build will omit Boost comparator. Use system Boost development package.")
  endif()
endif()

if(NEWTON_COMP_BENCH_WITH_VINCE)
  if(NEWTON_VINCE_SOURCE_DIR)
    set(_newton_csv_source "${NEWTON_VINCE_SOURCE_DIR}")
  else()
    include(FetchContent)
    FetchContent_Declare(newton_vince
      GIT_REPOSITORY https://github.com/vincentlaucsb/csv-parser.git
      GIT_TAG 86f1cab4fcb50a936475b5f778e8ee8c5dfe2259
      GIT_SHALLOW FALSE)
    FetchContent_GetProperties(newton_vince)
    if(NOT newton_vince_POPULATED)
      FetchContent_Populate(newton_vince)
    endif()
    set(_newton_csv_source "${newton_vince_SOURCE_DIR}")
  endif()
  if(NOT EXISTS "${_newton_csv_source}/include/csv.hpp")
    message(FATAL_ERROR "Expected csv-parser header at ${_newton_csv_source}/include/csv.hpp")
  endif()
  # The external library is built independently; its source is never vendored here.
  set(CSV_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
  set(CSV_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  add_subdirectory("${_newton_csv_source}" "${CMAKE_CURRENT_BINARY_DIR}/vince-build" EXCLUDE_FROM_ALL)
  if(TARGET csv)
    target_link_libraries(newton_compare PRIVATE csv)
  elseif(TARGET csv::csv)
    target_link_libraries(newton_compare PRIVATE csv::csv)
  else()
    message(FATAL_ERROR "csv-parser CMake target not found")
  endif()
  target_compile_definitions(newton_compare PRIVATE NEWTON_COMP_BENCH_VINCE)
endif()

# Independent C++20 standard-library baselines for the five additional modules.
# These need no external code or network access.
add_executable(newton_compare_extended extended.cpp)
target_compile_features(newton_compare_extended PRIVATE cxx_std_20)
target_link_libraries(newton_compare_extended PRIVATE newton_core newton_memory newton_index newton_analytics newton_parallel)
