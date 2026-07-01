################################################################################
# FILE:    CMakeLists.txt
# MODULE:  Core/Input
# PURPOSE: Defines the psxengine_input STATIC library target.
################################################################################

cmake_minimum_required(VERSION 3.20)

add_library(psxengine_input STATIC
    joypad.c
)

target_include_directories(psxengine_input PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
