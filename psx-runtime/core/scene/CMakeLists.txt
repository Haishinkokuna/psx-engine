################################################################################
# FILE:    CMakeLists.txt
# MODULE:  Core/Scene
# PURPOSE: Defines the psxengine_scene STATIC library target.
################################################################################

cmake_minimum_required(VERSION 3.20)

add_library(psxengine_scene STATIC
    transform.c
    camera.c
    entity.c
    scene.c
)

target_include_directories(psxengine_scene PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Scene depends on math (Vec3, Mat3, Trig) and eventually renderer for drawing.
target_link_libraries(psxengine_scene PUBLIC
    psxengine_math
    psxengine_renderer
)
