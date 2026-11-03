# Local overlay port. Install with --overlay-ports=packaging/vcpkg/ports
# from the NewtonKernels checkout; no remote source download required.
get_filename_component(SOURCE_PATH "${CURRENT_PORT_DIR}/../../../.." ABSOLUTE)
vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DNEWTON_BUILD_TESTS=OFF
        -DNEWTON_BUILD_BENCHMARKS=OFF
        -DNEWTON_BUILD_EXAMPLES=OFF
        -DNEWTON_BUILD_C_API=OFF
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/NewtonKernels)
file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/debug/include"
    "${CURRENT_PACKAGES_DIR}/debug/share")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
