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

# Dedicated allocation-failure tests: isolated global new override must never
# leak into the production library or other test executables.
add_executable(newton_selective_fault_tests unit/test_selective_faults.cpp)
target_compile_features(newton_selective_fault_tests PRIVATE cxx_std_20)
# SelectiveTermIndex is header-only. Avoid a mismatched Debug STL ABI by
# compiling this fault-injection executable without linking newton_index.
target_include_directories(newton_selective_fault_tests PRIVATE ${PROJECT_SOURCE_DIR}/include)
if(MSVC)
  # MSVC Debug iterator proxies allocate inside noexcept STL constructors.
  # Injected bad_alloc there terminates before any index rollback is tested.
  # Regular Windows Debug tests still retain full iterator debugging.
  target_compile_definitions(newton_selective_fault_tests PRIVATE
    $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>)
endif()
add_test(NAME newton_selective_fault_tests COMMAND newton_selective_fault_tests)
# Fail a hung fault-injection run promptly; preserve all failure offsets.
set_tests_properties(newton_selective_fault_tests PROPERTIES TIMEOUT 120)
