################################################################################
# FILE:    CMakeLists.txt
# MODULE:  psx-editor
# PURPOSE: Configures the PSX Engine desktop editor application.
#
#          The editor is a host-native application (Windows/Linux/macOS).
#          It uses Dear ImGui for the UI, SDL2 for windowing and input, and
#          OpenGL 3.3 for rendering — including the PSX artifact simulation
#          shaders (vertex jitter, affine UV, 5-bit color banding).
#
#          DEPENDENCIES (pulled automatically via CMake FetchContent):
#            - Dear ImGui v1.91.x (ocornut/imgui on GitHub)
#            - SDL2 v2.30.x (libsdl-org/SDL on GitHub)
#
#          OpenGL is expected to be installed on the host system.
#          On Windows: typically bundled with the GPU driver (opengl32.lib).
#          On Linux: install libgl1-mesa-dev.
#
#          BUILD:
#            cmake -S . -B build/editor -DPSX_BUILD_TARGET=EDITOR
#            cmake --build build/editor
#            ./build/editor/psx-editor/psx_editor
################################################################################

cmake_minimum_required(VERSION 3.20)
project(PSXEditor VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Find OpenGL (required, must be installed on host)
# ---------------------------------------------------------------------------

find_package(OpenGL REQUIRED)

# ---------------------------------------------------------------------------
# FetchContent: Dear ImGui
# We pull the source directly and compile the necessary backends alongside
# our own sources. No pre-built binaries — ImGui is always compiled fresh.
# ---------------------------------------------------------------------------

include(FetchContent)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.5
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

# ---------------------------------------------------------------------------
# FetchContent: SDL2
# ---------------------------------------------------------------------------

FetchContent_Declare(
    SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-2.30.6
    GIT_SHALLOW    TRUE
)
# SDL2 has its own CMakeLists — configure it quietly.
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON  CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SDL2)

# ---------------------------------------------------------------------------
# ImGui source files
# We compile the core ImGui files + the SDL2 and OpenGL3 backends.
# The backends are in imgui/backends/ — we add only the two we use.
# ---------------------------------------------------------------------------

set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

# ---------------------------------------------------------------------------
# psx_editor executable
# ---------------------------------------------------------------------------

add_executable(psx_editor
    src/main.cpp
    src/renderer/PsxArtifactRenderer.cpp
    src/ui/EditorLayout.cpp
    ${IMGUI_SOURCES}
)

target_include_directories(psx_editor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(psx_editor PRIVATE
    SDL2::SDL2-static
    OpenGL::GL
    psx_mcp_server
)

# On Windows, SDL2 needs the WinMain entry point adapter.
if(WIN32)
    target_link_libraries(psx_editor PRIVATE SDL2::SDL2main)
endif()

# ---------------------------------------------------------------------------
# Copy shaders to the build output directory so the executable can find them
# via a relative path at runtime.
# ---------------------------------------------------------------------------

add_custom_command(TARGET psx_editor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:psx_editor>/shaders
    COMMENT "Copying PSX artifact shaders to build output..."
)
