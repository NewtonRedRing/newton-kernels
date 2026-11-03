cmake_minimum_required(VERSION 3.21)
project(NewtonKernels VERSION 0.1.0 LANGUAGES CXX DESCRIPTION "Portable high-performance C++ data processing and graph kernels")

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CTest)

option(NEWTON_BUILD_TESTS "Build unit and integration tests" ${BUILD_TESTING})
option(NEWTON_BUILD_EXAMPLES "Build usage examples" ON)
option(NEWTON_BUILD_BENCHMARKS "Build deterministic microbenchmarks" OFF)
option(NEWTON_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(NEWTON_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

option(NEWTON_ENABLE_IPO "Enable opt-in link-time/interprocedural optimization" OFF)
if(NEWTON_ENABLE_IPO)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT _newton_ipo_supported OUTPUT _newton_ipo_error LANGUAGES CXX)
  if(NOT _newton_ipo_supported)
    message(FATAL_ERROR "NEWTON_ENABLE_IPO unsupported: ${_newton_ipo_error}")
  endif()
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

# Profile-guided optimization is a build-time, completely optional workflow.
# No profiling runs, files, or services are required at application runtime.
set(NEWTON_PGO_MODE "OFF" CACHE STRING "PGO mode: OFF, GENERATE or USE")
set_property(CACHE NEWTON_PGO_MODE PROPERTY STRINGS OFF GENERATE USE)
set(NEWTON_PGO_DATA "" CACHE FILEPATH "GCC profile directory, or Clang .profraw/.profdata path")
if(NOT NEWTON_PGO_MODE STREQUAL "OFF")
  if(NOT NEWTON_PGO_MODE MATCHES "^(GENERATE|USE)$")
    message(FATAL_ERROR "NEWTON_PGO_MODE must be OFF, GENERATE or USE")
  endif()
  if(NOT NEWTON_PGO_DATA)
    message(FATAL_ERROR "NEWTON_PGO_DATA must be supplied for non-OFF PGO")
  endif()
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(NEWTON_PGO_MODE STREQUAL "GENERATE")
      add_compile_options("-fprofile-generate=${NEWTON_PGO_DATA}")
      add_link_options("-fprofile-generate=${NEWTON_PGO_DATA}")
    else()
      add_compile_options("-fprofile-use=${NEWTON_PGO_DATA}" -fprofile-correction)
      add_link_options("-fprofile-use=${NEWTON_PGO_DATA}")
    endif()
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    if(NEWTON_PGO_MODE STREQUAL "GENERATE")
      add_compile_options("-fprofile-instr-generate=${NEWTON_PGO_DATA}")
      add_link_options("-fprofile-instr-generate=${NEWTON_PGO_DATA}")
    else()
      add_compile_options("-fprofile-instr-use=${NEWTON_PGO_DATA}")
      add_link_options("-fprofile-instr-use=${NEWTON_PGO_DATA}")
    endif()
  else()
    message(FATAL_ERROR "PGO wrapper currently supports only GCC and Clang")
  endif()
endif()

if(NEWTON_ENABLE_SANITIZERS)
  if(MSVC)
    message(FATAL_ERROR "NEWTON_ENABLE_SANITIZERS currently requires GCC or Clang")
  endif()
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

function(newton_module name)
  add_library(newton_${name} STATIC ${ARGN})
  add_library(NewtonKernels::${name} ALIAS newton_${name})
  set_target_properties(newton_${name} PROPERTIES EXPORT_NAME ${name} OUTPUT_NAME newton_${name})
  target_compile_features(newton_${name} PUBLIC cxx_std_20)
  target_include_directories(newton_${name} PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
      $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
  if(MSVC)
    target_compile_options(newton_${name} PRIVATE /W4)
    if(NEWTON_WARNINGS_AS_ERRORS)
      target_compile_options(newton_${name} PRIVATE /WX)
    endif()
  else()
    target_compile_options(newton_${name} PRIVATE -Wall -Wextra -Wpedantic)
    if(NEWTON_WARNINGS_AS_ERRORS)
      target_compile_options(newton_${name} PRIVATE -Werror)
    endif()
  endif()
endfunction()

newton_module(core src/core/cpu_features.cpp)
newton_module(memory src/memory/mapped_file.cpp)
newton_module(simd src/simd/byte_scan.cpp)
newton_module(parsing src/parsing/csv_parser.cpp)
newton_module(index src/index/inverted_index.cpp)
newton_module(analytics src/analytics/percentile.cpp)
newton_module(graph src/graph/graph.cpp)
newton_module(parallel src/parallel/executor.cpp)

target_link_libraries(newton_simd PUBLIC newton_core)
target_link_libraries(newton_parsing PUBLIC newton_simd)
find_package(Threads REQUIRED)
target_link_libraries(newton_parallel PUBLIC Threads::Threads)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS newton_core newton_memory newton_simd newton_parsing
  newton_index newton_analytics newton_graph newton_parallel
  EXPORT NewtonKernelsTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(EXPORT NewtonKernelsTargets
  NAMESPACE NewtonKernels::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NewtonKernels)
configure_package_config_file(cmake/NewtonKernelsConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/NewtonKernelsConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NewtonKernels)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/NewtonKernelsConfigVersion.cmake
  VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/NewtonKernelsConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/NewtonKernelsConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NewtonKernels)
install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/NewtonKernels)

if(NEWTON_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
if(NEWTON_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()
if(NEWTON_BUILD_BENCHMARKS)
  add_subdirectory(benchmarks)
endif()
