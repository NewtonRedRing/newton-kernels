# Getting started

Build on Linux, macOS or Windows with CMake >= 3.21 and a C++20 compiler. Enable tests with `-DNEWTON_BUILD_TESTS=ON` and examples with `-DNEWTON_BUILD_EXAMPLES=ON`.

For a local installation run `cmake --install build --prefix <prefix>`, then point `CMAKE_PREFIX_PATH` at `<prefix>` when configuring the consuming project. Import targets as `NewtonKernels::core`, `NewtonKernels::memory`, `NewtonKernels::simd`, `NewtonKernels::parsing`, `NewtonKernels::index`, `NewtonKernels::analytics`, `NewtonKernels::graph`, and `NewtonKernels::parallel`.

For example, include `<newton/parsing/csv_parser.hpp>` and link `NewtonKernels::parsing`; the callback receives each decoded row, and must copy any data it wishes to retain. Use `for_each_delimited_record` only when records contain no quotes or escaped delimiters, and do not retain the borrowed views after the source buffer is released.
