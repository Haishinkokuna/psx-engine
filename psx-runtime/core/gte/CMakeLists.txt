################################################################################
# FILE:    CMakeLists.txt
# MODULE:  Core/GTE
# PURPOSE: Defines the psxengine_gte STATIC library target.
#
#          Most of the GTE interface is header-only (inline functions and
#          inline assembly in gte.h, gte_ops.h, gte_project.h). The only
#          compiled source file is gte_regs.c, which defines the software
#          register file (g_gte_regs) used when PSX_HOST_TEST=1.
#
#          On the real PSX cross-compile build, gte_regs.c compiles to an
#          empty translation unit (the #ifdef PSX_HOST_TEST guard excludes
#          the only meaningful content). The linker removes it with
#          --gc-sections. This keeps the GTE target compilable in both modes.
#
# USAGE:   target_link_libraries(my_target PRIVATE psxengine_gte)
################################################################################

cmake_minimum_required(VERSION 3.20)

add_library(psxengine_gte STATIC
    gte_regs.c
)

target_include_directories(psxengine_gte PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# GTE depends on the math module for Vec3 and Mat3 type definitions.
target_link_libraries(psxengine_gte PUBLIC
    psxengine_math
)

# The GTE renderer pipeline also references the renderer OT header.
# We add the renderer include path without a full link dependency to avoid
# a circular dependency (renderer uses GTE, GTE headers reference OT_ENTRY_COUNT).
target_include_directories(psxengine_gte PUBLIC
    ${CMAKE_SOURCE_DIR}/core/renderer
    ${CMAKE_SOURCE_DIR}/core/memory
)

if(PSX_HOST_TEST)
    target_compile_definitions(psxengine_gte PUBLIC PSX_HOST_TEST=1)
endif()
