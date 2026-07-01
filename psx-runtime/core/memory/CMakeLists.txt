################################################################################
# FILE:    CMakeLists.txt
# MODULE:  Core/Memory
# PURPOSE: Defines the psxengine_memory STATIC library target.
#
#          Compiles scratch.c and heap.c into a static library that other
#          runtime modules link against. Depends on psxengine_math for the
#          fixed-point types used in allocation size calculations.
#
# USAGE:   target_link_libraries(my_target PRIVATE psxengine_memory)
################################################################################

cmake_minimum_required(VERSION 3.20)

add_library(psxengine_memory STATIC
    scratch.c
    heap.c
)

target_include_directories(psxengine_memory PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../math   # For fixed.h (mem_map.h uses stdint.h only)
)

# psxengine_math provides fixed.h types transitively to anyone using memory.
target_link_libraries(psxengine_memory PUBLIC psxengine_math)
