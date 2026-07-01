################################################################################
# FILE:    CMakeLists.txt
# MODULE:  psx-mcp-server/tests
# PURPOSE: Unit tests for the MCP server JSON-RPC dispatcher.
#
#          Run with:
#            cmake -S . -B build/editor -DPSX_BUILD_TARGET=EDITOR -DPSX_BUILD_TESTS=ON
#            cmake --build build/editor
#            ctest --test-dir build/editor -R McpDispatch -V
################################################################################

cmake_minimum_required(VERSION 3.20)

add_executable(test_mcp_dispatch
    test_dispatch.cpp
    ${CMAKE_SOURCE_DIR}/psx-mcp-server/src/McpServer.cpp
    ${CMAKE_SOURCE_DIR}/psx-mcp-server/src/tools/CoreTools.cpp
)

target_include_directories(test_mcp_dispatch PRIVATE
    ${CMAKE_SOURCE_DIR}/psx-mcp-server/include
)

target_link_libraries(test_mcp_dispatch PRIVATE
    nlohmann_json::nlohmann_json
)

target_compile_features(test_mcp_dispatch PRIVATE cxx_std_17)

enable_testing()
add_test(NAME McpDispatch COMMAND test_mcp_dispatch)
