from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
import os

class NewtonKernelsConan(ConanFile):
    name = "newton-kernels"
    version = "0.1.0"
    license = "MIT"
    url = "https://github.com/NewtonRedRing/newton-kernels"
    description = "Modular, hardware-aware C++20 static execution kernels"
    topics = ("cpp20", "simd", "analytics", "graph", "csv")
    settings = "os", "compiler", "build_type", "arch"
    package_type = "static-library"
    exports_sources = "CMakeLists.txt", "cmake/*", "include/*", "src/*", "LICENSE"

    def layout(self):
        cmake_layout(self)

    def generate(self):
        toolchain = CMakeToolchain(self)
        toolchain.variables["NEWTON_BUILD_TESTS"] = False
        toolchain.variables["NEWTON_BUILD_EXAMPLES"] = False
        toolchain.variables["NEWTON_BUILD_BENCHMARKS"] = False
        toolchain.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.set_property("cmake_find_mode", "none")
        self.cpp_info.builddirs.append(os.path.join("lib", "cmake", "NewtonKernels"))
        self.cpp_info.libs = [
            "newton_analytics", "newton_parsing", "newton_simd",
            "newton_core", "newton_graph", "newton_index",
            "newton_memory", "newton_parallel",
        ]
