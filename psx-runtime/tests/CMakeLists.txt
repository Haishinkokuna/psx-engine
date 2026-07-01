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

# test_gte: validates the software GTE pipeline (RTPS/RTPT, NCLIP, AVSZ3,
# GTE_TransformTriangle) on the host. PSX_HOST_TEST activates the software
# register file (g_gte_regs) and disables the MIPS inline assembly paths.
add_executable(test_gte
    test_gte.cpp
    ${CMAKE_SOURCE_DIR}/core/gte/gte_regs.c
    ${CMAKE_SOURCE_DIR}/core/renderer/display.c
    ${CMAKE_SOURCE_DIR}/core/memory/scratch.c
    ${CMAKE_SOURCE_DIR}/core/memory/heap.c
    ${CMAKE_SOURCE_DIR}/core/math/trig.c
)
target_include_directories(test_gte PRIVATE
    ${CMAKE_SOURCE_DIR}/core/math
    ${CMAKE_SOURCE_DIR}/core/memory
    ${CMAKE_SOURCE_DIR}/core/renderer
    ${CMAKE_SOURCE_DIR}/core/gte
)
target_compile_definitions(test_gte PRIVATE PSX_HOST_TEST=1)
target_compile_features(test_gte PRIVATE cxx_std_17)
add_test(NAME GTE COMMAND test_gte)

# test_scene: validates the scene graph, entity lifecycle, transform math
add_executable(test_scene
    test_scene.cpp
    ${CMAKE_SOURCE_DIR}/core/scene/transform.c
    ${CMAKE_SOURCE_DIR}/core/scene/camera.c
    ${CMAKE_SOURCE_DIR}/core/scene/entity.c
    ${CMAKE_SOURCE_DIR}/core/scene/scene.c
    ${CMAKE_SOURCE_DIR}/core/math/trig.c
)
target_include_directories(test_scene PRIVATE
    ${CMAKE_SOURCE_DIR}/core/math
    ${CMAKE_SOURCE_DIR}/core/scene
)
target_compile_definitions(test_scene PRIVATE PSX_HOST_TEST=1)
target_compile_features(test_scene PRIVATE cxx_std_17)
add_test(NAME Scene COMMAND test_scene)

# test_input: validates the digital joypad driver
add_executable(test_input
    test_input.cpp
    ${CMAKE_SOURCE_DIR}/core/input/joypad.c
)
target_include_directories(test_input PRIVATE
    ${CMAKE_SOURCE_DIR}/core/input
)
target_compile_definitions(test_input PRIVATE PSX_HOST_TEST=1)
target_compile_features(test_input PRIVATE cxx_std_17)
add_test(NAME Input COMMAND test_input)

# test_assets: validates TIM/TMD parsing and pointer relinking
add_executable(test_assets
    test_assets.cpp
    ${CMAKE_SOURCE_DIR}/core/assets/cdrom.c
    ${CMAKE_SOURCE_DIR}/core/assets/tim.c
    ${CMAKE_SOURCE_DIR}/core/assets/tmd.c
    ${CMAKE_SOURCE_DIR}/core/gte/gte_regs.c
    ${CMAKE_SOURCE_DIR}/core/renderer/display.c
    ${CMAKE_SOURCE_DIR}/core/memory/scratch.c
    ${CMAKE_SOURCE_DIR}/core/memory/heap.c
    ${CMAKE_SOURCE_DIR}/core/math/trig.c
    ${CMAKE_SOURCE_DIR}/core/scene/transform.c
)
target_include_directories(test_assets PRIVATE
    ${CMAKE_SOURCE_DIR}/core/assets
    ${CMAKE_SOURCE_DIR}/core/math
    ${CMAKE_SOURCE_DIR}/core/memory
    ${CMAKE_SOURCE_DIR}/core/renderer
    ${CMAKE_SOURCE_DIR}/core/gte
    ${CMAKE_SOURCE_DIR}/core/scene
)
target_compile_definitions(test_assets PRIVATE PSX_HOST_TEST=1)
target_compile_features(test_assets PRIVATE cxx_std_17)
add_test(NAME Assets COMMAND test_assets)



