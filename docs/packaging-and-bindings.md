# Packaging and optional in-process language interfaces

The default build is a set of independently linkable C++20 static libraries.
It has **no central runtime**, daemon, JIT, background profiling or mandatory
Python/Rust/Arrow dependencies. All language integrations are opt-in and remain
inside the caller's process.

## Native CMake (verified in CI)

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
cmake --install build --prefix "$HOME/.local"
```

Use `find_package(NewtonKernels CONFIG REQUIRED)` and link just the desired
`NewtonKernels::analytics`, `::columnar`, `::simd`, etc. CI also builds a
**separate installed-package consumer** after installation.

## Conan 2 (local recipe)

```sh
conan create . --build=missing
```

The `conanfile.py` is a local developer recipe; it has not been published
to ConanCenter. It installs the project's native CMake exports. This recipe
has not been independently run in the bundled CI environment.

## vcpkg (local overlay port)

```sh
vcpkg install newton-kernels --overlay-ports=packaging/vcpkg/ports
```

Run from the NewtonKernels checkout. This is a local overlay and **not**
an upstream vcpkg registry submission. The overlay is provided for testing;
the actual registry integration must be validated in a vcpkg-equipped
environment before it can be advertised as supported.

## Optional C ABI

```sh
cmake -S . -B build-c -DCMAKE_BUILD_TYPE=Release \
  -DNEWTON_BUILD_C_API=ON -DNEWTON_BUILD_TESTS=ON
cmake --build build-c --parallel
ctest --test-dir build-c --output-on-failure
```

`#include <newton/c_api/newton_c.h>` exposes a versioned minimal interface:
`newton_count_byte`, `newton_sum_gt_u64`. Functions return status codes
instead of throwing. The C ABI library is a shared library **only when opted in**.
The C++ default remains static; no additional process or executor is started.

## Python

```sh
python3 bindings/python/test_smoke.py ./build-c/libnewton_c.so
```

For application use, import `NewtonKernels` from
`bindings/python/newton_kernels.py` and pass the installed shared-library
path. This ctypes adapter copies immutable Python bytes into a ctypes buffer;
its overhead should be measured independently. It is an optional example,
not a package published on PyPI.

## Rust

```sh
cargo check --manifest-path bindings/rust/newton-kernels-native/Cargo.toml
RUSTFLAGS="-Lnative=$PWD/build-c" cargo build \
  --manifest-path bindings/rust/newton-kernels-native/Cargo.toml
```

The minimal crate provides `count_byte` and `sum_gt` as safe wrappers
over an explicitly linked native C ABI. The crate is not on crates.io.
Cross-OS linker paths and shared-library deployment are still application
responsibilities. No threads or global scheduler are created by the bindings.

## Limitations

These are initial integration bridges, not comprehensive Python or Rust
implementations of every kernel. Conan/vcpkg registry submission, API/ABI
stabilization, wheel building, GPU kernels and production package support
remain separate, gated workstreams.
