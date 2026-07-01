/*******************************************************************************
 * FILE:         McpServer.cpp
 * MODULE:       MCP Server
 * DESCRIPTION:  Implementation of the McpServer JSON-RPC 2.0 dispatcher
 *               and stdio transport. See McpServer.h for the full design doc.
 *
 * DEPENDENCIES: McpServer.h, ToolSchema.h, nlohmann/json,
 *               <iostream>, <thread>, <mutex>
 *******************************************************************************/

#include "McpServer.h"

#include <iostream>
#include <sstream>
#include <cstdio>
#include <stdexcept>

using json = nlohmann::json;

/* ---------------------------------------------------------------------------
 * JSON-RPC 2.0 error codes — defined here to avoid magic numbers in the code.
 * --------------------------------------------------------------------------- */

static const int RPC_ERR_PARSE_ERROR     = -32700;
static const int RPC_ERR_INVALID_REQUEST = -32600;
static const int RPC_ERR_METHOD_NOT_FOUND= -32601;
static const int RPC_ERR_INVALID_PARAMS  = -32602;
static const int RPC_ERR_INTERNAL_ERROR  = -32603;

/* ---------------------------------------------------------------------------
 * Constructor / Destructor
 * --------------------------------------------------------------------------- */

McpServer::McpServer()
    : m_running(false)
{}

McpServer::~McpServer()
{
    Stop();
}

/* ---------------------------------------------------------------------------
 * MakeSuccess — Build a JSON-RPC 2.0 success response.
 * --------------------------------------------------------------------------- */

json McpServer::MakeSuccess(const json& id, const json& result)
{
    return {
        {"jsonrpc", "2.0"},
        {"id",      id},
        {"result",  result}
    };
}

/* ---------------------------------------------------------------------------
 * MakeError — Build a JSON-RPC 2.0 error response.
 * --------------------------------------------------------------------------- */

json McpServer::MakeError(const json& id, int code, const std::string& message)
{
    return {
        {"jsonrpc", "2.0"},
        {"id",      id},
        {"error", {
            {"code",    code},
            {"message", message}
        }}
    };
}

/* ---------------------------------------------------------------------------
 * RegisterTool
 * --------------------------------------------------------------------------- */

void McpServer::RegisterTool(ToolDefinition def)
{
    std::lock_guard<std::mutex> lock(m_tools_mutex);
    m_tools[def.name] = std::move(def);
}

/* ---------------------------------------------------------------------------
 * Dispatch — Core JSON-RPC dispatch logic.
 *
 * 1. Parse the raw JSON string.
 * 2. Validate the JSON-RPC envelope (jsonrpc version, method field).
 * 3. Look up the method in the tool registry.
 * 4. Call the handler with params.
 * 5. Build and return the response envelope.
 * --------------------------------------------------------------------------- */

std::string McpServer::Dispatch(const std::string& raw_json)
{
    json request;
    json id = nullptr; /* id remains null if parsing fails (notification or parse error) */

    /* --- Step 1: Parse JSON --- */
    try {
        request = json::parse(raw_json);
    } catch (const json::parse_error& e) {
        /* Malformed JSON — return a parse error. id is null per JSON-RPC spec. */
        json err = MakeError(nullptr, RPC_ERR_PARSE_ERROR,
                             std::string("Parse error: ") + e.what());
        return err.dump();
    }

    /* --- Step 2: Extract id (may be absent for notifications) --- */
    if (request.contains("id")) {
        id = request["id"];
    }

    /* --- Step 3: Validate required fields --- */
    if (!request.contains("jsonrpc") || request["jsonrpc"] != "2.0") {
        return MakeError(id, RPC_ERR_INVALID_REQUEST,
                         "Invalid request: 'jsonrpc' field must be '2.0'").dump();
    }
    if (!request.contains("method") || !request["method"].is_string()) {
        return MakeError(id, RPC_ERR_INVALID_REQUEST,
                         "Invalid request: 'method' field is required and must be a string.").dump();
    }

    std::string method = request["method"];

    /* Notification: requests without an 'id' do not receive a response.
     * We still process them, but return an empty string to signal "no reply". */
    bool is_notification = !request.contains("id");

    /* Handle the built-in list_tools method — enumerates registered tools. */
    if (method == "list_tools" || method == "rpc.discover") {
        if (is_notification) return "";
        return MakeSuccess(id, GetToolList()).dump();
    }

    /* --- Step 4: Look up handler --- */
    ToolHandler handler;
    {
        std::lock_guard<std::mutex> lock(m_tools_mutex);
        auto it = m_tools.find(method);
        if (it == m_tools.end()) {
            if (is_notification) return "";
            return MakeError(id, RPC_ERR_METHOD_NOT_FOUND,
                             "Method not found: '" + method + "'").dump();
        }
        handler = it->second.handler;
    }

    /* Extract params (may be absent — default to empty object). */
    json params = request.value("params", json::object());

    /* --- Step 5: Call handler and build response --- */
    try {
        json result = handler(params);
        if (is_notification) return "";
        return MakeSuccess(id, result).dump();
    } catch (const std::runtime_error& e) {
        /* Handler signaled a parameter or validation error. */
        if (is_notification) return "";
        return MakeError(id, RPC_ERR_INVALID_PARAMS, e.what()).dump();
    } catch (const std::exception& e) {
        /* Handler threw an unexpected exception — internal error. */
        if (is_notification) return "";
        return MakeError(id, RPC_ERR_INTERNAL_ERROR,
                         std::string("Internal error: ") + e.what()).dump();
    }
}

/* ---------------------------------------------------------------------------
 * GetToolList — Return all registered tool schemas as a JSON array.
 * --------------------------------------------------------------------------- */

json McpServer::GetToolList() const
{
    std::lock_guard<std::mutex> lock(m_tools_mutex);
    json tools_array = json::array();

    for (const auto& pair : m_tools) {
        tools_array.push_back(pair.second.schema);
    }

    return tools_array;
}

/* ---------------------------------------------------------------------------
 * ServerThreadLoop — Background thread: read stdin line-by-line, dispatch.
 *
 * The MCP stdio transport convention is one JSON object per line.
 * Each line is passed to Dispatch(). The response (if any) is written to
 * stdout followed by a newline — stdout must be flushed after each response
 * so the client does not block waiting for buffered data.
 * --------------------------------------------------------------------------- */

void McpServer::ServerThreadLoop()
{
    fprintf(stdout, "[MCP] Server started (stdio transport). Ready.\n");
    fflush(stdout);

    std::string line;
    while (m_running.load() && std::getline(std::cin, line)) {
        /* Skip empty lines. */
        if (line.empty()) continue;

        /* Dispatch the request and get the response. */
        std::string response = Dispatch(line);

        /* Empty response = notification (no reply expected). */
        if (!response.empty()) {
            /* Write the complete JSON response on a single line, then flush.
             * Flushing is critical — without it, the client blocks indefinitely
             * waiting for data that is stuck in the OS write buffer. */
            std::cout << response << "\n";
            std::cout.flush();
        }
    }

    fprintf(stdout, "[MCP] Server thread exiting.\n");
    fflush(stdout);
}

/* ---------------------------------------------------------------------------
 * Start
 * --------------------------------------------------------------------------- */

void McpServer::Start()
{
    if (m_running.load()) {
        fprintf(stderr, "[MCP] Start() called but server is already running.\n");
        return;
    }

    m_running.store(true);
    m_thread = std::thread(&McpServer::ServerThreadLoop, this);
}

/* ---------------------------------------------------------------------------
 * Stop
 * --------------------------------------------------------------------------- */

void McpServer::Stop()
{
    if (!m_running.load()) return;

    m_running.store(false);

    /* The background thread is blocked on std::getline(std::cin).
     * There is no clean cross-platform way to unblock a blocking read
     * without closing stdin. We detach rather than join to avoid hanging
     * the main thread on editor close. In a future sprint, switch to
     * a non-blocking read loop with a stop token (C++20 std::jthread). */
    if (m_thread.joinable()) {
        m_thread.detach();
    }
}
