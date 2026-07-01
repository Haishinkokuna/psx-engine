################################################################################
# FILE:    CMakeLists.txt
# MODULE:  Root / PSX ENGINE
# PURPOSE: Top-level CMake entry point for the PSX ENGINE project.
#
#          Controls which subsystems get configured via PSX_BUILD_TARGET:
#
#            RUNTIME  - Cross-compile psx-runtime for the MIPS PSX target.
#                       Requires the mipsel-unknown-elf GCC toolchain on PATH.
#                       Pass -DCMAKE_TOOLCHAIN_FILE=psx-runtime/cmake/mipsel-toolchain.cmake
#
#            EDITOR   - Build the psx-editor desktop application for the host.
#                       Requires SDL2, OpenGL, and Dear ImGui (fetched automatically).
#
#            ALL      - Configure both. Because RUNTIME and EDITOR require
#                       different compilers, they must be configured in
#                       SEPARATE BUILD DIRECTORIES. Use EDITOR for the default
#                       dev workflow and RUNTIME only when targeting hardware.
#
#          RECOMMENDED WORKFLOW:
#            # Editor + MCP server (most frequent — daily development)
#            cmake -S . -B build/editor -DPSX_BUILD_TARGET=EDITOR
#            cmake --build build/editor
#
#            # Runtime (requires MIPS cross-compiler)
#            cmake -S psx-runtime -B build/runtime \
#              -DCMAKE_TOOLCHAIN_FILE=psx-runtime/cmake/mipsel-toolchain.cmake
#            cmake --build build/runtime
#
#            # Host unit tests (runtime logic, no cross-compiler)
#            cmake -S psx-runtime -B build/tests -DPSX_HOST_TEST=ON
#            cmake --build build/tests && ctest --test-dir build/tests -V
################################################################################

cmake_minimum_required(VERSION 3.20)
project(PSXEngine VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Build target selector
# ---------------------------------------------------------------------------

set(PSX_BUILD_TARGET "EDITOR" CACHE STRING
    "Which subsystem to configure: RUNTIME | EDITOR | ALL")
set_property(CACHE PSX_BUILD_TARGET PROPERTY STRINGS RUNTIME EDITOR ALL)

message(STATUS "PSX Engine build target: ${PSX_BUILD_TARGET}")

# ---------------------------------------------------------------------------
# Subsystem configuration
# ---------------------------------------------------------------------------

if(PSX_BUILD_TARGET STREQUAL "RUNTIME" OR PSX_BUILD_TARGET STREQUAL "ALL")
    # Runtime is cross-compiled — delegate to psx-runtime's own CMakeLists
    # which enforces the toolchain and sets up the MIPS compiler flags.
    add_subdirectory(psx-runtime)
endif()

if(PSX_BUILD_TARGET STREQUAL "EDITOR" OR PSX_BUILD_TARGET STREQUAL "ALL")
    # Editor and MCP server are compiled for the host machine.
    add_subdirectory(psx-mcp-server)
    add_subdirectory(psx-editor)
endif()
