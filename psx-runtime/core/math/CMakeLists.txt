################################################################################
# FILE:    CMakeLists.txt
# MODULE:  Core/Math
# PURPOSE: Defines the psxengine_math INTERFACE library target.
#
#          This is a header-only library — there is no compiled object code
#          except for trig.c (which defines the sin LUT). The INTERFACE keyword
#          means any target that links psxengine_math automatically gets the
#          correct include paths propagated through CMake's dependency system.
#
#          The sin LUT (trig.c) IS compiled — it produces a .rodata section
#          in the final binary containing the 512-byte lookup table. To keep
#          psxengine_math as close to INTERFACE as possible, trig.c is compiled
#          into a small OBJECT library and bundled via $<TARGET_OBJECTS:...>.
#
# USAGE:   target_link_libraries(my_target PRIVATE psxengine_math)
################################################################################

cmake_minimum_required(VERSION 3.20)

# ---------------------------------------------------------------------------
# Compile the sin LUT as a separate OBJECT library.
# This lets the main INTERFACE target stay "header-only" conceptually while
# still including the one compiled translation unit (trig.c).
# ---------------------------------------------------------------------------

add_library(psxengine_math_trig OBJECT
    trig.c
)

target_include_directories(psxengine_math_trig PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# ---------------------------------------------------------------------------
# The main INTERFACE target. Consumers link this and get:
#   - Include paths for all math headers (fixed.h, vec3.h, mat3.h, trig.h)
#   - The compiled sin LUT object from psxengine_math_trig
# ---------------------------------------------------------------------------

add_library(psxengine_math INTERFACE)

target_include_directories(psxengine_math INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Pull in the compiled LUT object via generator expression.
target_link_libraries(psxengine_math INTERFACE
    $<TARGET_OBJECTS:psxengine_math_trig>
)

# ---------------------------------------------------------------------------
# Host-build test toggle (controlled from the runtime parent CMakeLists).
# When PSX_HOST_TEST=ON the math module builds a test executable against
# the host compiler so we can verify the fixed-point math without hardware.
# ---------------------------------------------------------------------------

if(PSX_HOST_TEST)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../tests
                     ${CMAKE_CURRENT_BINARY_DIR}/tests_math)
endif()
