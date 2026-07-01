################################################################################
# FILE:    CMakeLists.txt
# MODULE:  psx-runtime (parent)
# PURPOSE: Configures the cross-compiled MIPS PSX runtime library.
#
#          This CMakeLists is the entry point for all psx-runtime submodules.
#          It applies the MIPS cross-compile toolchain and assembles the
#          core submodule targets.
#
#          TO CROSS-COMPILE:
#            cmake -S psx-runtime -B build/runtime \
#              -DCMAKE_TOOLCHAIN_FILE=cmake/mipsel-toolchain.cmake
#            cmake --build build/runtime
#
#          TO BUILD HOST TESTS (no cross-compiler needed):
#            cmake -S psx-runtime -B build/runtime-tests \
#              -DPSX_HOST_TEST=ON
#            cmake --build build/runtime-tests
#            ctest --test-dir build/runtime-tests -V
################################################################################

cmake_minimum_required(VERSION 3.20)
project(PSXRuntime VERSION 0.1.0 LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# Host-test mode toggle.
# When enabled: build for the HOST compiler (no cross-compiler needed).
# Test executables under psx-runtime/tests/ validate logic on the dev machine.
# ---------------------------------------------------------------------------

option(PSX_HOST_TEST "Build host-compiled unit tests (no cross-compiler)" OFF)

if(NOT PSX_HOST_TEST)
    # Verify the cross-compiler is available. Fail early with a helpful message.
    if(NOT CMAKE_CROSSCOMPILING)
        message(WARNING
            "psx-runtime is being configured without the MIPS toolchain file.\n"
            "Pass -DCMAKE_TOOLCHAIN_FILE=cmake/mipsel-toolchain.cmake to target hardware.\n"
            "Or pass -DPSX_HOST_TEST=ON to build host-side unit tests only.")
    endif()
endif()

# ---------------------------------------------------------------------------
# Core submodules — order matters (math has no deps; memory depends on math;
# renderer depends on both math and memory).
# ---------------------------------------------------------------------------

add_subdirectory(core/math)
add_subdirectory(core/memory)
add_subdirectory(core/renderer)
add_subdirectory(core/gte)
add_subdirectory(core/scene)

# ---------------------------------------------------------------------------
# Tests (host-compiled only — not cross-compiled to MIPS)
# ---------------------------------------------------------------------------

if(PSX_HOST_TEST)
    enable_testing()
    add_subdirectory(tests)
endif()
