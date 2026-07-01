################################################################################
# FILE:    CMakeLists.txt
# MODULE:  Core/Assets
# PURPOSE: Defines the psxengine_assets STATIC library target.
################################################################################

cmake_minimum_required(VERSION 3.20)

add_library(psxengine_assets STATIC
    cdrom.c
    tim.c
    tmd.c
)

target_include_directories(psxengine_assets PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Assets module depends on Renderer, Scene, and GTE.
target_link_libraries(psxengine_assets PUBLIC
    psxengine_renderer
    psxengine_scene
    psxengine_gte
)
