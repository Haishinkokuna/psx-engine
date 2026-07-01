/*******************************************************************************
 * FILE:         test_dispatch.cpp
 * MODULE:       MCP Server — Unit Tests
 * DESCRIPTION:  Unit tests for the McpServer JSON-RPC 2.0 dispatcher.
 *               Tests the Dispatch() function directly (no network or stdin
 *               required). Verifies:
 *                 - Valid request dispatching and result structure
 *                 - Parse error handling (malformed JSON)
 *                 - Invalid request handling (missing jsonrpc version)
 *                 - Method not found (unregistered method name)
 *                 - Handler parameter validation errors
 *                 - list_tools built-in method
 *                 - Notification handling (no id = no response)
 *
 * DEPENDENCIES: McpServer.h, ToolSchema.h, nlohmann/json, <cassert>, <cstdio>
 *******************************************************************************/

#include <cstdio>
#include <cassert>
#include <string>
#include <stdexcept>

#include "McpServer.h"
#include "ToolSchema.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;

/* Forward declaration — defined in CoreTools.cpp */
void RegisterCoreTools(McpServer& server);

/* ---------------------------------------------------------------------------
 * Simple test runner helpers
 * --------------------------------------------------------------------------- */

static int g_pass = 0;
static int g_fail = 0;

static void check(bool condition, const char* test_name)
{
    if (condition) {
        printf("[PASS] %s\n", test_name);
        g_pass++;
    } else {
        printf("[FAIL] %s\n", test_name);
        g_fail++;
    }
}

/* ---------------------------------------------------------------------------
 * Utility: dispatch a JSON object through the server and parse the response.
 * --------------------------------------------------------------------------- */

static json dispatch_json(McpServer& server, const json& request)
{
    std::string response_str = server.Dispatch(request.dump());
    if (response_str.empty()) return json(nullptr);
    return json::parse(response_str);
}

/* ---------------------------------------------------------------------------
 * Tests
 * --------------------------------------------------------------------------- */

static void test_valid_list_entities(McpServer& server)
{
    json req = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "list_entities"},
        {"params", json::object()}
    };

    json resp = dispatch_json(server, req);

    check(resp.contains("jsonrpc"),    "list_entities: response has 'jsonrpc'");
    check(resp["jsonrpc"] == "2.0",    "list_entities: jsonrpc == 2.0");
    check(resp.contains("id"),         "list_entities: response has 'id'");
    check(resp["id"] == 1,             "list_entities: id echoed correctly");
    check(resp.contains("result"),     "list_entities: response has 'result'");
    check(!resp.contains("error"),     "list_entities: no 'error' field on success");
}

static void test_create_entity_valid(McpServer& server)
{
    json req = {
        {"jsonrpc", "2.0"},
        {"id", 2},
        {"method", "create_entity"},
        {"params", {
            {"name", "test_cube"},
            {"type", "mesh"},
            {"position", {0, 0, -100}}
        }}
    };

    json resp = dispatch_json(server, req);

    check(resp.contains("result"),        "create_entity valid: has result");
    check(!resp.contains("error"),        "create_entity valid: no error");
    check(resp["result"]["status"] == "stub",
                                          "create_entity valid: stub status returned");
}

static void test_create_entity_missing_param(McpServer& server)
{
    /* Missing 'position' parameter — handler should throw, producing an error response. */
    json req = {
        {"jsonrpc", "2.0"},
        {"id", 3},
        {"method", "create_entity"},
        {"params", {
            {"name", "bad_entity"},
            {"type", "mesh"}
            /* position deliberately omitted */
        }}
    };

    json resp = dispatch_json(server, req);

    check(resp.contains("error"),              "create_entity missing param: has error");
    check(resp["error"]["code"] == -32602,     "create_entity missing param: code -32602");
    check(!resp.contains("result"),            "create_entity missing param: no result");
}

static void test_parse_error(McpServer& server)
{
    /* Feed malformed JSON. */
    std::string response_str = server.Dispatch("{this is not valid json]");
    json resp = json::parse(response_str);

    check(resp.contains("error"),           "parse error: has error");
    check(resp["error"]["code"] == -32700,  "parse error: code -32700");
    check(resp["id"].is_null(),             "parse error: id is null");
}

static void test_invalid_request_no_version(McpServer& server)
{
    json req = {
        {"id", 10},
        {"method", "list_entities"},
        {"params", json::object()}
        /* 'jsonrpc' field deliberately absent */
    };

    json resp = dispatch_json(server, req);

    check(resp.contains("error"),              "invalid request: has error");
    check(resp["error"]["code"] == -32600,     "invalid request: code -32600");
}

static void test_method_not_found(McpServer& server)
{
    json req = {
        {"jsonrpc", "2.0"},
        {"id", 5},
        {"method", "nonexistent_tool"},
        {"params", json::object()}
    };

    json resp = dispatch_json(server, req);

    check(resp.contains("error"),              "method not found: has error");
    check(resp["error"]["code"] == -32601,     "method not found: code -32601");
}

static void test_list_tools(McpServer& server)
{
    json req = {
        {"jsonrpc", "2.0"},
        {"id", 6},
        {"method", "list_tools"},
        {"params", json::object()}
    };

    json resp = dispatch_json(server, req);

    check(resp.contains("result"),             "list_tools: has result");
    check(resp["result"].is_array(),           "list_tools: result is array");
    /* We registered 6 tools — verify at least some are present. */
    check(resp["result"].size() >= 6,          "list_tools: at least 6 tools returned");
}

static void test_notification_no_response(McpServer& server)
{
    /* A notification has no 'id' field — the server must not send a response. */
    json req = {
        {"jsonrpc", "2.0"},
        /* id deliberately absent */
        {"method", "list_entities"},
        {"params", json::object()}
    };

    std::string response_str = server.Dispatch(req.dump());

    check(response_str.empty(), "notification: no response sent (empty string returned)");
}

/* ---------------------------------------------------------------------------
 * Main
 * --------------------------------------------------------------------------- */

int main(void)
{
    printf("=== PSX ENGINE: MCP Server Dispatch Tests ===\n\n");

    McpServer server;
    RegisterCoreTools(server);

    printf("-- Valid Dispatch --\n");
    test_valid_list_entities(server);
    test_create_entity_valid(server);

    printf("\n-- Parameter Validation --\n");
    test_create_entity_missing_param(server);

    printf("\n-- JSON-RPC Error Codes --\n");
    test_parse_error(server);
    test_invalid_request_no_version(server);
    test_method_not_found(server);

    printf("\n-- Built-in Methods --\n");
    test_list_tools(server);
    test_notification_no_response(server);

    printf("\n=============================================\n");
    printf("Results: %d passed, %d failed.\n", g_pass, g_fail);

    return (g_fail == 0) ? 0 : 1;
}
