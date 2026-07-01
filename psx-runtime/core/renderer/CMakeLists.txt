################################################################################
# FILE:    CMakeLists.txt
# MODULE:  Core/Renderer
# PURPOSE: Defines the psxengine_renderer STATIC library target.
#
#          Compiles display.c into a static library. The headers (gpu_types.h,
#          ot.h, prim.h) are interface-only (inline/macro), so they require
#          no compiled translation units of their own.
#
#          PSX_HOST_TEST mode:
#            When -DPSX_HOST_TEST=ON is set, the GPU and DMA MMIO writes in
#            display.c are compiled as no-ops via the PSX_HOST_TEST guard in
#            display.h. This allows the renderer logic (OT, packet allocation,
#            buffer swap) to be unit-tested on the host without PSX hardware.
#
# USAGE:   target_link_libraries(my_target PRIVATE psxengine_renderer)
################################################################################

cmake_minimum_required(VERSION 3.20)

add_library(psxengine_renderer STATIC
    display.c
)

target_include_directories(psxengine_renderer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Renderer depends on math (for FP types used by GTE callers) and memory
# (for mem_map.h hardware addresses).
target_link_libraries(psxengine_renderer PUBLIC
    psxengine_math
    psxengine_memory
)

# Propagate PSX_HOST_TEST to renderer sources so the MMIO stubs activate.
if(PSX_HOST_TEST)
    target_compile_definitions(psxengine_renderer PUBLIC PSX_HOST_TEST=1)
endif()
