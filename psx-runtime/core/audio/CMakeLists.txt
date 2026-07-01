################################################################################
# FILE:    CMakeLists.txt
# MODULE:  Core/Audio
# PURPOSE: Defines the psxengine_audio STATIC library target.
################################################################################

cmake_minimum_required(VERSION 3.20)

add_library(psxengine_audio STATIC
    spu.c
)

target_include_directories(psxengine_audio PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
