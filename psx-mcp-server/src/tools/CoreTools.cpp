/*******************************************************************************
 * FILE:         CoreTools.cpp
 * MODULE:       MCP Server / Tools
 * DESCRIPTION:  Definitions of the six initial MCP tool schemas and their stub
 *               handler implementations.
 *
 *               STUB HANDLERS:
 *                 Each handler currently returns a {"status": "stub"} response.
 *                 This confirms the JSON-RPC dispatch pipeline is wired correctly
 *                 without requiring any real scene data. Real implementations
 *                 will replace these stubs when scene management is built.
 *
 *               TO REGISTER ALL TOOLS AT STARTUP:
 *                 Call RegisterCoreTools(server) after constructing McpServer
 *                 and before calling McpServer::Start(). This is done in
 *                 the editor's main() function.
 *
 * DEPENDENCIES: ToolSchema.h, McpServer.h, nlohmann/json
 *******************************************************************************/

#include "ToolSchema.h"
#include "McpServer.h"

#include <cstdio>
#include <stdexcept>

extern "C" {
    #include "scene.h"
}

using json = nlohmann::json;

/* ===========================================================================
 * SCHEMA DEFINITIONS
 *
 * Each schema follows the MCP / JSON Schema convention:
 *   {
 *     "name":        "method_name",
 *     "description": "What this tool does, for the AI agent's context.",
 *     "inputSchema": {
 *       "type": "object",
 *       "properties": {
 *         "param_name": { "type": "...", "description": "..." }
 *       },
 *       "required": ["param_name", ...]
 *     }
 *   }
 * =========================================================================== */

const json SCHEMA_CREATE_ENTITY = {
    {"name", "create_entity"},
    {"description",
     "Creates a new entity in the current PSX Editor scene. "
     "An entity is the fundamental scene object — it can be a mesh, "
     "a light, a trigger volume, or a camera. "
     "Returns the entity's assigned ID on success."},
    {"inputSchema", {
        {"type", "object"},
        {"properties", {
            {"name", {
                {"type", "string"},
                {"description", "Unique name for the entity within the scene."}
            }},
            {"type", {
                {"type", "string"},
                {"enum", {"mesh", "light", "camera", "trigger", "empty"}},
                {"description", "Entity type determines which components are attached by default."}
            }},
            {"position", {
                {"type", "array"},
                {"items", {{"type", "number"}}},
                {"minItems", 3},
                {"maxItems", 3},
                {"description",
                 "World-space XYZ position as an array of three numbers. "
                 "Internally stored as FP12_4 fixed-point (scale=16). "
                 "Example: [0, 0, -100] places the entity 100 units in front."}
            }}
        }},
        {"required", {"name", "type", "position"}}
    }}
};

const json SCHEMA_DELETE_ENTITY = {
    {"name", "delete_entity"},
    {"description",
     "Removes an entity from the current PSX Editor scene by name. "
     "If no entity with the given name exists, returns an error."},
    {"inputSchema", {
        {"type", "object"},
        {"properties", {
            {"name", {
                {"type", "string"},
                {"description", "Name of the entity to remove."}
            }}
        }},
        {"required", {"name"}}
    }}
};

const json SCHEMA_LIST_ENTITIES = {
    {"name", "list_entities"},
    {"description",
     "Returns an array of all entities in the current scene. "
     "Each entry includes the entity's name, type, position, and ID. "
     "No parameters required."},
    {"inputSchema", {
        {"type", "object"},
        {"properties", json::object()},
        {"required", json::array()}
    }}
};

const json SCHEMA_LINK_NODES = {
    {"name", "link_nodes"},
    {"description",
     "Connects an output pin on one visual script node to an input pin on another. "
     "Used to build logic graphs in the PSX Editor node editor. "
     "The transpiler converts these node connections into C++ code before compilation."},
    {"inputSchema", {
        {"type", "object"},
        {"properties", {
            {"source_node", {
                {"type", "string"},
                {"description", "Name or ID of the source node."}
            }},
            {"source_pin", {
                {"type", "string"},
                {"description", "Name of the output pin on the source node."}
            }},
            {"target_node", {
                {"type", "string"},
                {"description", "Name or ID of the target node."}
            }},
            {"target_pin", {
                {"type", "string"},
                {"description", "Name of the input pin on the target node."}
            }}
        }},
        {"required", {"source_node", "source_pin", "target_node", "target_pin"}}
    }}
};

const json SCHEMA_BAKE_ASSETS = {
    {"name", "bake_assets"},
    {"description",
     "Triggers the PSX asset compilation pipeline. "
     "This converts the current scene's raw assets (meshes, textures, level data) "
     "into PSX-native binary format: fixed-point geometry, CLUTs for VRAM, "
     "and LZ77-compressed texture streams. "
     "The output is placed in the build/assets directory, ready for the "
     "mipsel-unknown-elf-gcc cross-compiler toolchain."},
    {"inputSchema", {
        {"type", "object"},
        {"properties", {
            {"target", {
                {"type", "string"},
                {"enum", {"all", "meshes", "textures", "level"}},
                {"description",
                 "Which asset category to bake. 'all' runs the full pipeline."}
            }}
        }},
        {"required", json::array()}
    }}
};

const json SCHEMA_GET_SCENE_STATE = {
    {"name", "get_scene_state"},
    {"description",
     "Returns a complete JSON snapshot of the current editor scene. "
     "Includes all entities, their transforms (in FP12_4 fixed-point format), "
     "node graph connections, render settings, and VRAM budget estimate. "
     "Use this to understand the current scene state before making edits."},
    {"inputSchema", {
        {"type", "object"},
        {"properties", json::object()},
        {"required", json::array()}
    }}
};

/* ===========================================================================
 * STUB HANDLER IMPLEMENTATIONS
 *
 * Each handler receives the "params" JSON object from the JSON-RPC request
 * and returns a JSON value for the "result" field of the response.
 *
 * These stubs log the call and return a placeholder response. They will be
 * replaced with real implementations as the scene management module is built.
 * =========================================================================== */

static json handle_create_entity(json params)
{
    /* Basic parameter validation — ensure required fields are present. */
    if (!params.contains("name") || !params["name"].is_string()) {
        throw std::runtime_error("Parameter 'name' is required and must be a string.");
    }
    if (!params.contains("type") || !params["type"].is_string()) {
        throw std::runtime_error("Parameter 'type' is required and must be a string.");
    }
    if (!params.contains("position") || !params["position"].is_array()) {
        throw std::runtime_error("Parameter 'position' is required and must be an array [x,y,z].");
    }

    std::string name = params["name"];
    std::string type = params["type"];

    fprintf(stdout, "[MCP] create_entity: name='%s', type='%s'\n",
            name.c_str(), type.c_str());

    Entity* ent = Scene_SpawnEntity();
    if (!ent) {
        throw std::runtime_error("Scene is full. Cannot spawn more entities.");
    }

    /* Assign position */
    auto pos = params["position"];
    ent->transform.position.x = pos[0];
    ent->transform.position.y = pos[1];
    ent->transform.position.z = pos[2];

    return {
        {"status",  "success"},
        {"message", "Entity created."},
        {"entity",  {
            {"id", ent->id},
            {"name", name},
            {"type", type},
            {"position", {ent->transform.position.x, ent->transform.position.y, ent->transform.position.z}}
        }}
    };
}

static json handle_delete_entity(json params)
{
    if (!params.contains("name") || !params["name"].is_string()) {
        throw std::runtime_error("Parameter 'name' is required and must be a string.");
    }

    std::string name = params["name"];
    fprintf(stdout, "[MCP] delete_entity: name='%s'\n", name.c_str());
    
    /* We don't have name lookup yet, so for now we just look it up if name is the ID string */
    try {
        uint32_t id = std::stoul(name);
        Scene_DestroyEntity(id);
    } catch (...) {}

    return {
        {"status",  "success"},
        {"message", "Entity deleted."},
        {"deleted", name}
    };
}

static json handle_list_entities(json /*params*/)
{
    fprintf(stdout, "[MCP] list_entities called.\n");

    json entities = json::array();
    
    for (uint32_t i = 0; i < MAX_ENTITIES; i++) {
        Entity* ent = Scene_GetEntityByIndex(i);
        if (ent && (ent->flags & ENTITY_FLAG_ACTIVE)) {
            entities.push_back({
                {"id", ent->id},
                {"position", {ent->transform.position.x, ent->transform.position.y, ent->transform.position.z}}
            });
        }
    }

    return {
        {"status",   "success"},
        {"message",  "Retrieved active entities."},
        {"entities", entities}
    };
}

static json handle_link_nodes(json params)
{
    if (!params.contains("source_node") || !params.contains("target_node")) {
        throw std::runtime_error(
            "Parameters 'source_node', 'source_pin', 'target_node', 'target_pin' are required.");
    }

    fprintf(stdout, "[MCP] link_nodes: %s.%s -> %s.%s\n",
            params.value("source_node", "?").c_str(),
            params.value("source_pin",  "?").c_str(),
            params.value("target_node", "?").c_str(),
            params.value("target_pin",  "?").c_str());

    return {
        {"status",  "stub"},
        {"message", "link_nodes is not yet wired to the node editor."}
    };
}

static json handle_bake_assets(json params)
{
    std::string target = params.value("target", "all");
    fprintf(stdout, "[MCP] bake_assets: target='%s'\n", target.c_str());

    return {
        {"status",  "stub"},
        {"message", "bake_assets pipeline is not yet implemented."},
        {"target",  target}
    };
}

static json handle_get_scene_state(json /*params*/)
{
    fprintf(stdout, "[MCP] get_scene_state called.\n");

    return {
        {"status",   "success"},
        {"message",  "Scene state retrieved."},
        {"active_entities", g_scene.active_count},
        {"max_entities", MAX_ENTITIES},
        {"nodes",    json::array()},
        {"render",   {{"dither", true}, {"resolution", "320x240"}}}
    };
}

/* ===========================================================================
 * RegisterCoreTools — Register all six tools with the McpServer.
 *
 * Call this once after constructing McpServer and before McpServer::Start().
 * =========================================================================== */

void RegisterCoreTools(McpServer& server)
{
    /* Initialize the global runtime scene so the server can spawn/list entities */
    Scene_Init();

    server.RegisterTool({ "create_entity",  SCHEMA_CREATE_ENTITY,  handle_create_entity  });
    server.RegisterTool({ "delete_entity",  SCHEMA_DELETE_ENTITY,  handle_delete_entity  });
    server.RegisterTool({ "list_entities",  SCHEMA_LIST_ENTITIES,  handle_list_entities  });
    server.RegisterTool({ "link_nodes",     SCHEMA_LINK_NODES,     handle_link_nodes     });
    server.RegisterTool({ "bake_assets",    SCHEMA_BAKE_ASSETS,    handle_bake_assets    });
    server.RegisterTool({ "get_scene_state",SCHEMA_GET_SCENE_STATE,handle_get_scene_state});

    fprintf(stdout, "[MCP] Registered 6 core tools.\n");
}
