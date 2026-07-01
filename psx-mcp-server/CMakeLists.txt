################################################################################
# FILE:    CMakeLists.txt
# MODULE:  psx-mcp-server
# PURPOSE: Configures the PSX Engine MCP (Model Context Protocol) server
#          as a STATIC library that links into the editor executable.
#
#          DEPENDENCIES (pulled via FetchContent):
#            - nlohmann/json v3.11.x -- header-only JSON library for C++17.
#              Used for parsing/serializing JSON-RPC 2.0 messages.
#
#          TRANSPORT:
#            This sprint implements the stdio transport only (reads from stdin,
#            writes to stdout). The WebSocket upgrade path is stubbed.
#
# USAGE:   target_link_libraries(psx_editor PRIVATE psx_mcp_server)
################################################################################

cmake_minimum_required(VERSION 3.20)
project(PSXMcpServer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install    OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nlohmann_json)

add_library(psx_mcp_server STATIC
    src/McpServer.cpp
    src/tools/CoreTools.cpp
)

target_include_directories(psx_mcp_server PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(psx_mcp_server PUBLIC
    nlohmann_json::nlohmann_json
)

if(PSX_BUILD_TESTS)
    add_subdirectory(tests)
endif()
