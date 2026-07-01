/*******************************************************************************
 * FILE:         McpServer.h
 * MODULE:       MCP Server
 * DESCRIPTION:  JSON-RPC 2.0 MCP server class for the PSX Engine editor.
 *
 *               The McpServer runs as an in-process thread alongside the editor.
 *               External AI agents (Claude, Gemini, or any MCP-compatible client)
 *               send JSON-RPC requests via stdin and receive responses via stdout.
 *
 *               PROTOCOL — JSON-RPC 2.0:
 *                 Request:   { "jsonrpc":"2.0", "id":N, "method":"...", "params":{} }
 *                 Success:   { "jsonrpc":"2.0", "id":N, "result": <value> }
 *                 Error:     { "jsonrpc":"2.0", "id":N, "error": { "code":N, "message":"..." } }
 *                 Notify:    { "jsonrpc":"2.0", "method":"...", "params":{} }  (no id, no response)
 *
 *               STANDARD ERROR CODES:
 *                 -32700  Parse error     — Invalid JSON received
 *                 -32600  Invalid request — Missing "jsonrpc", "method", etc.
 *                 -32601  Method not found — No registered handler for method
 *                 -32602  Invalid params  — Handler rejected the params shape
 *                 -32603  Internal error  — Handler threw an uncaught exception
 *
 *               TOOL REGISTRATION:
 *                 Call RegisterTool() before Start() to add tool handlers.
 *                 The handler function receives the "params" JSON object and
 *                 returns the "result" JSON object (or throws on error).
 *
 *               THREADING:
 *                 Start() launches a background std::thread that blocks on
 *                 std::cin. The editor's main thread calls Stop() on shutdown.
 *                 The tool handler functions are invoked on the server thread
 *                 — ensure any engine state they touch is thread-safe (or
 *                 queue the call back to the main thread via a mutex-protected
 *                 command queue in future sprints).
 *
 * DEPENDENCIES: ToolSchema.h, nlohmann/json, <thread>, <mutex>
 *******************************************************************************/

#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <nlohmann/json.hpp>
#include "ToolSchema.h"

/*
 * McpServer — JSON-RPC 2.0 dispatcher with stdio transport.
 */
class McpServer
{
public:
    McpServer();
    ~McpServer();

    /* Non-copyable — owns a thread and mutex. */
    McpServer(const McpServer&) = delete;
    McpServer& operator=(const McpServer&) = delete;

    /* -----------------------------------------------------------------------
     * RegisterTool — Register a tool with its schema and handler.
     *
     * Must be called BEFORE Start(). Thread-safe to call from any thread
     * during setup (no concurrent access at registration time).
     *
     * @param def  ToolDefinition containing name, schema, and handler.
     * ----------------------------------------------------------------------- */
    void RegisterTool(ToolDefinition def);

    /* -----------------------------------------------------------------------
     * Start — Launch the background JSON-RPC listener thread.
     * The thread reads one complete JSON object per line from stdin.
     * ----------------------------------------------------------------------- */
    void Start();

    /* -----------------------------------------------------------------------
     * Stop — Signal the listener thread to terminate and join it.
     * Call this before the process exits.
     * ----------------------------------------------------------------------- */
    void Stop();

    /* -----------------------------------------------------------------------
     * Dispatch — Parse and dispatch a raw JSON-RPC message string.
     *
     * This is the core of the server — it parses the JSON, looks up the
     * method, calls the handler, and returns the full JSON-RPC response
     * envelope as a string. Can be called directly for testing.
     *
     * @param raw_json  The raw JSON string received from the transport.
     * @return          A complete JSON-RPC response string (or empty for notifications).
     * ----------------------------------------------------------------------- */
    std::string Dispatch(const std::string& raw_json);

    /* -----------------------------------------------------------------------
     * GetToolList — Return the list of all registered tool schemas.
     * Suitable for responding to an "rpc.discover" or "list_tools" method.
     * ----------------------------------------------------------------------- */
    nlohmann::json GetToolList() const;

    /* -----------------------------------------------------------------------
     * IsRunning — Returns true if the server thread is active.
     * ----------------------------------------------------------------------- */
    bool IsRunning() const { return m_running.load(); }

private:
    /* -----------------------------------------------------------------------
     * Private helpers
     * ----------------------------------------------------------------------- */

    /* Build a JSON-RPC 2.0 success response envelope. */
    static nlohmann::json MakeSuccess(const nlohmann::json& id,
                                      const nlohmann::json& result);

    /* Build a JSON-RPC 2.0 error response envelope. */
    static nlohmann::json MakeError(const nlohmann::json& id,
                                    int code,
                                    const std::string& message);

    /* Background thread loop — reads lines from stdin and calls Dispatch. */
    void ServerThreadLoop();

    /* -----------------------------------------------------------------------
     * State
     * ----------------------------------------------------------------------- */

    /* Tool registry: method name -> ToolDefinition (handler + schema). */
    std::unordered_map<std::string, ToolDefinition> m_tools;

    /* Background listener thread. */
    std::thread m_thread;

    /* Protects m_tools during concurrent access (unlikely in current design,
     * but included as good hygiene for future dynamic registration). */
    mutable std::mutex m_tools_mutex;

    /* Atomic flag — set to false by Stop() to signal thread termination. */
    std::atomic<bool> m_running;
};
