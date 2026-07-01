/*******************************************************************************
 * FILE:         scene.h
 * MODULE:       Core/Scene
 * DESCRIPTION:  The global game world container.
 *
 *               The Scene manages a flat, statically-allocated array of Entities.
 *               This flat structure avoids pointer chasing and heap fragmentation,
 *               which is critical for performance on the PSX.
 *
 *               The Scene also maintains a pointer to the active Camera.
 *
 * DEPENDENCIES: entity.h, camera.h
 *******************************************************************************/

#ifndef PSX_SCENE_H
#define PSX_SCENE_H

#include <stdint.h>
#include "entity.h"
#include "camera.h"

/* The maximum number of entities the scene can hold at once.
 * 256 is a solid default for a PSX game. */
#ifndef MAX_ENTITIES
#define MAX_ENTITIES 256
#endif

/* ---------------------------------------------------------------------------
 * Scene
 *
 * The master container for all game objects.
 * --------------------------------------------------------------------------- */

typedef struct {
    Entity   entities[MAX_ENTITIES];
    uint32_t active_count;
    uint32_t next_id_counter;
    Camera*  active_camera;
} Scene;

/* ---------------------------------------------------------------------------
 * Global Scene Instance (defined in scene.c)
 * --------------------------------------------------------------------------- */

extern Scene g_scene;

/* ---------------------------------------------------------------------------
 * Scene_Init
 *
 * Initializes the global scene, clearing all entities and resetting counters.
 * --------------------------------------------------------------------------- */

void Scene_Init(void);

/* ---------------------------------------------------------------------------
 * Scene_SpawnEntity
 *
 * Finds an inactive slot in the entity array, marks it active and visible,
 * assigns it a unique ID, and returns a pointer to it.
 *
 * @return Pointer to the new Entity, or NULL if MAX_ENTITIES is reached.
 * --------------------------------------------------------------------------- */

Entity* Scene_SpawnEntity(void);

/* ---------------------------------------------------------------------------
 * Scene_DestroyEntity
 *
 * Marks an entity as inactive, freeing its slot for future spawns.
 *
 * @param id  The unique ID of the entity to destroy.
 * --------------------------------------------------------------------------- */

void Scene_DestroyEntity(uint32_t id);

/* ---------------------------------------------------------------------------
 * Scene_GetEntity
 *
 * Looks up an active entity by its unique ID.
 *
 * @param id  The unique ID to find.
 * @return Pointer to the Entity, or NULL if not found or inactive.
 * --------------------------------------------------------------------------- */

Entity* Scene_GetEntity(uint32_t id);

/* ---------------------------------------------------------------------------
 * Scene_GetEntityByIndex
 *
 * Retrieves an entity directly by its flat array index.
 * Useful for bulk iteration (like the MCP list_entities tool).
 *
 * @param index  The array index [0, MAX_ENTITIES-1].
 * @return Pointer to the Entity, or NULL if index out of bounds.
 * --------------------------------------------------------------------------- */

Entity* Scene_GetEntityByIndex(uint32_t index);

#endif /* PSX_SCENE_H */
