/*******************************************************************************
 * FILE:         ToolSchema.h
 * MODULE:       MCP Server
 * DESCRIPTION:  MCP tool definition types and the six initial tool schemas
 *               that external AI agents can invoke against the PSX Editor.
 *
 *               MODEL CONTEXT PROTOCOL (MCP) TOOL SCHEMA FORMAT:
 *                 Each tool exposed by the MCP server must have a JSON schema
 *                 describing its name, human-readable description, and the
 *                 structure of its parameters. This schema is what external
 *                 AI agents read to understand how to call the tool.
 *
 *               JSON-RPC 2.0 CALL FORMAT:
 *                 {
 *                   "jsonrpc": "2.0",
 *                   "id": 1,
 *                   "method": "create_entity",
 *                   "params": {
 *                     "name": "player",
 *                     "type": "mesh",
 *                     "position": [0, 0, 0]
 *                   }
 *                 }
 *
 *               INITIAL TOOLS (this sprint — all stubs):
 *                 create_entity   — Add a new entity to the scene
 *                 delete_entity   — Remove an entity from the scene
 *                 list_entities   — Return all entities in the current scene
 *                 link_nodes      — Connect two nodes in the visual script graph
 *                 bake_assets     — Trigger the asset compilation pipeline
 *                 get_scene_state — Return a full JSON snapshot of the scene
 *
 * DEPENDENCIES: nlohmann/json, <string>
 *******************************************************************************/

#pragma once

#include <string>
#include <functional>
#include <nlohmann/json.hpp>

/* ---------------------------------------------------------------------------
 * ToolHandler — The function signature for a registered MCP tool handler.
 *
 * @param params  The "params" object from the JSON-RPC request.
 * @return        A JSON value that becomes the "result" in the response.
 *                Throw std::runtime_error to produce a JSON-RPC error response.
 * --------------------------------------------------------------------------- */

using ToolHandler = std::function<nlohmann::json(nlohmann::json params)>;

/* ---------------------------------------------------------------------------
 * ToolDefinition — One complete MCP tool registration record.
 *
 * 'schema' follows the MCP tool schema convention:
 *   {
 *     "name":        "tool_name",
 *     "description": "Human-readable description for the AI agent.",
 *     "inputSchema": {
 *       "type": "object",
 *       "properties": { ... }
 *     }
 *   }
 * --------------------------------------------------------------------------- */

struct ToolDefinition {
    std::string    name;        /* Unique tool identifier (JSON-RPC method name) */
    nlohmann::json schema;      /* Full MCP tool schema as JSON                   */
    ToolHandler    handler;     /* Handler function invoked on dispatch            */
};

/* ---------------------------------------------------------------------------
 * TOOL SCHEMAS — Declared as extern constants, defined in CoreTools.cpp.
 * These are the JSON schema objects for each tool. External agents enumerate
 * them via the McpServer::GetToolList() method.
 * --------------------------------------------------------------------------- */

/* Tool: create_entity
 * Creates a new entity in the scene with a name, type, and initial position. */
extern const nlohmann::json SCHEMA_CREATE_ENTITY;

/* Tool: delete_entity
 * Removes the entity with the given name from the current scene. */
extern const nlohmann::json SCHEMA_DELETE_ENTITY;

/* Tool: list_entities
 * Returns an array of all entities in the current scene. No parameters. */
extern const nlohmann::json SCHEMA_LIST_ENTITIES;

/* Tool: link_nodes
 * Connects an output pin on one visual script node to an input pin on another. */
extern const nlohmann::json SCHEMA_LINK_NODES;

/* Tool: bake_assets
 * Triggers the asset pipeline: compiles textures, geometry, and level data
 * into PSX-native binary format ready for the cross-compiler. */
extern const nlohmann::json SCHEMA_BAKE_ASSETS;

/* Tool: get_scene_state
 * Returns a complete JSON snapshot of the current scene: entities, nodes,
 * and render settings. Useful for agents that need to understand context
 * before making further edits. */
extern const nlohmann::json SCHEMA_GET_SCENE_STATE;
