add_executable(newton_unit_tests unit/test_main.cpp)
target_compile_features(newton_unit_tests PRIVATE cxx_std_20)
target_link_libraries(newton_unit_tests PRIVATE newton_core newton_memory newton_simd
  newton_parsing newton_index newton_analytics newton_graph newton_parallel)
add_test(NAME newton_unit_tests COMMAND newton_unit_tests)

add_executable(newton_integration_tests integration/test_pipeline.cpp)
target_compile_features(newton_integration_tests PRIVATE cxx_std_20)
target_link_libraries(newton_integration_tests PRIVATE newton_memory newton_parsing newton_analytics)
add_test(NAME newton_integration_tests COMMAND newton_integration_tests)

add_executable(newton_algorithm_tests unit/test_algorithms.cpp)
target_link_libraries(newton_algorithm_tests PRIVATE newton_graph newton_index)
add_test(NAME newton_algorithm_tests COMMAND newton_algorithm_tests)

# Independently implemented, allocator-free primitive correctness and differential tests.
add_executable(newton_primitive_tests unit/test_native_primitives.cpp)
target_compile_features(newton_primitive_tests PRIVATE cxx_std_20)
target_link_libraries(newton_primitive_tests PRIVATE newton_analytics newton_parsing)
add_test(NAME newton_primitive_tests COMMAND newton_primitive_tests)

add_executable(newton_portfolio_tests unit/test_native_portfolio.cpp)
target_compile_features(newton_portfolio_tests PRIVATE cxx_std_20)
target_link_libraries(newton_portfolio_tests PRIVATE newton_graph newton_analytics newton_index newton_parsing newton_simd newton_parallel)
add_test(NAME newton_portfolio_tests COMMAND newton_portfolio_tests)

add_executable(newton_columnar_tests unit/test_columnar.cpp)
target_compile_features(newton_columnar_tests PRIVATE cxx_std_20)
target_link_libraries(newton_columnar_tests PRIVATE newton_analytics newton_parsing)
add_test(NAME newton_columnar_tests COMMAND newton_columnar_tests)

# Audit edge-case regression tests for graph, executor, mapping and columnar.
add_executable(newton_audit_edge_tests unit/test_audit_edges.cpp)
target_compile_features(newton_audit_edge_tests PRIVATE cxx_std_20)
target_link_libraries(newton_audit_edge_tests PRIVATE newton_graph newton_parallel newton_memory newton_columnar)
add_test(NAME newton_audit_edge_tests COMMAND newton_audit_edge_tests)

# The allocation override remains isolated to this test process.
add_executable(newton_radix_heap_fault_tests unit/test_radix_heap_faults.cpp)
target_compile_features(newton_radix_heap_fault_tests PRIVATE cxx_std_20)
# This test uses the header-only RadixHeap implementation. Do not link the
# independently compiled graph library when testing MSVC's Debug STL with a
# different iterator-debug setting: that would create an ABI mismatch.
target_include_directories(newton_radix_heap_fault_tests PRIVATE ${PROJECT_SOURCE_DIR}/include)
if(MSVC)
  # MSVC Debug STL default vector constructors allocate iterator proxies
  # inside noexcept functions. Injecting bad_alloc into those infrastructure
  # allocations calls terminate instead of testing RadixHeap exception safety.
  # Other regular Debug test targets retain normal iterator debugging.
  target_compile_definitions(newton_radix_heap_fault_tests PRIVATE
    $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>)
endif()
add_test(NAME newton_radix_heap_fault_tests COMMAND newton_radix_heap_fault_tests)
# A diagnostic failure must not occupy a Windows runner for 25 minutes.
# Every injected allocation offset remains covered.
set_tests_properties(newton_radix_heap_fault_tests PROPERTIES TIMEOUT 120)

# Fault-injection test: failed allocations must not consume an ID or
# leave partial positional postings in the in-memory index.
add_executable(newton_index_fault_tests unit/test_index_faults.cpp)
target_compile_features(newton_index_fault_tests PRIVATE cxx_std_20)
if(MSVC)
  # Compile this index implementation alongside its injected-allocation test,
  # keeping the STL iterator-debug ABI consistent. Other Windows Debug tests
  # still compile and link the production newton_index library unchanged.
  target_sources(newton_index_fault_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/src/index/inverted_index.cpp)
  target_include_directories(newton_index_fault_tests PRIVATE ${PROJECT_SOURCE_DIR}/include)
  # Debug iterator proxy allocation can throw from noexcept STL constructors
  # before the index reaches an operation with a catchable allocation failure.
  target_compile_definitions(newton_index_fault_tests PRIVATE
    $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>)
else()
  target_link_libraries(newton_index_fault_tests PRIVATE newton_index)
endif()
add_test(NAME newton_index_fault_tests COMMAND newton_index_fault_tests)
# Do not obscure an MSVC Debug hang behind the default 25-minute timeout.
# The full allocation-failure sweep remains enabled.
set_tests_properties(newton_index_fault_tests PROPERTIES TIMEOUT 120)
