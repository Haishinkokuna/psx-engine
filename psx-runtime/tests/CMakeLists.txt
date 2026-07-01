################################################################################
# FILE:    CMakeLists.txt
# MODULE:  psx-runtime/tests
# PURPOSE: Host-compiled unit tests for the psx-runtime core modules.
#
#          These tests compile against the HOST compiler (not the MIPS
#          cross-compiler) to allow fast iteration on a development machine.
#          They exercise the logic of fixed-point math and memory allocators
#          without requiring PSX hardware.
#
#          To run:
#            cmake -S psx-runtime -B build/tests -DPSX_HOST_TEST=ON
#            cmake --build build/tests
#            ctest --test-dir build/tests -V
################################################################################

cmake_minimum_required(VERSION 3.20)

# test_fixed: verifies fixed.h, trig.h, vec3.h, mat3.h
add_executable(test_fixed
    test_fixed.cpp
    ${CMAKE_SOURCE_DIR}/core/math/trig.c
)
target_include_directories(test_fixed PRIVATE
    ${CMAKE_SOURCE_DIR}/core/math
    ${CMAKE_SOURCE_DIR}/core/memory
)
target_compile_features(test_fixed PRIVATE cxx_std_17)
add_test(NAME FixedPointMath COMMAND test_fixed)

# test_memory: verifies scratch.h/c and heap.h/c
add_executable(test_memory
    test_memory.cpp
    ${CMAKE_SOURCE_DIR}/core/memory/scratch.c
    ${CMAKE_SOURCE_DIR}/core/memory/heap.c
)
target_include_directories(test_memory PRIVATE
    ${CMAKE_SOURCE_DIR}/core/math
    ${CMAKE_SOURCE_DIR}/core/memory
)
target_compile_features(test_memory PRIVATE cxx_std_17)
add_test(NAME MemoryAllocators COMMAND test_memory)

# test_renderer: verifies OT, packet pool, display buffer swap, and primitive alloc.
# PSX_HOST_TEST stubs out MMIO register writes so the test runs on the host.
add_executable(test_renderer
    test_renderer.cpp
    ${CMAKE_SOURCE_DIR}/core/renderer/display.c
    ${CMAKE_SOURCE_DIR}/core/memory/scratch.c
    ${CMAKE_SOURCE_DIR}/core/memory/heap.c
    ${CMAKE_SOURCE_DIR}/core/math/trig.c
)
target_include_directories(test_renderer PRIVATE
    ${CMAKE_SOURCE_DIR}/core/math
    ${CMAKE_SOURCE_DIR}/core/memory
    ${CMAKE_SOURCE_DIR}/core/renderer
)
target_compile_definitions(test_renderer PRIVATE PSX_HOST_TEST=1)
target_compile_features(test_renderer PRIVATE cxx_std_17)
add_test(NAME Renderer COMMAND test_renderer)
