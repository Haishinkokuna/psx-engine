/*******************************************************************************
 * FILE:         scene.c
 * MODULE:       Core/Scene
 * DESCRIPTION:  Global game world container implementation.
 *
 * DEPENDENCIES: scene.h
 *******************************************************************************/

#include "scene.h"
#include <stddef.h> /* for NULL */

/* The single global scene instance. */
Scene g_scene;

/* ---------------------------------------------------------------------------
 * Scene_Init
 * --------------------------------------------------------------------------- */

void Scene_Init(void)
{
    int i;
    for (i = 0; i < MAX_ENTITIES; i++) {
        Entity_Init(&g_scene.entities[i], 0);
    }
    
    g_scene.active_count = 0;
    g_scene.next_id_counter = 1; /* ID 0 is invalid/unassigned */
    g_scene.active_camera = NULL;
}

/* ---------------------------------------------------------------------------
 * Scene_SpawnEntity
 * --------------------------------------------------------------------------- */

Entity* Scene_SpawnEntity(void)
{
    int i;
    
    if (g_scene.active_count >= MAX_ENTITIES) {
        return NULL; /* Scene is full */
    }

    /* Find the first inactive slot */
    for (i = 0; i < MAX_ENTITIES; i++) {
        if (!(g_scene.entities[i].flags & ENTITY_FLAG_ACTIVE)) {
            
            /* Initialize it */
            Entity_Init(&g_scene.entities[i], g_scene.next_id_counter++);
            
            /* Mark as active and visible */
            g_scene.entities[i].flags |= (ENTITY_FLAG_ACTIVE | ENTITY_FLAG_VISIBLE);
            
            g_scene.active_count++;
            
            return &g_scene.entities[i];
        }
    }
    
    return NULL; /* Should never reach here if active_count check passed */
}

/* ---------------------------------------------------------------------------
 * Scene_DestroyEntity
 * --------------------------------------------------------------------------- */

void Scene_DestroyEntity(uint32_t id)
{
    int i;
    
    if (id == 0) return; /* Invalid ID */
    
    for (i = 0; i < MAX_ENTITIES; i++) {
        if ((g_scene.entities[i].flags & ENTITY_FLAG_ACTIVE) && 
             g_scene.entities[i].id == id) {
             
            /* Clear all flags to mark it inactive */
            g_scene.entities[i].flags = 0;
            g_scene.entities[i].id = 0;
            
            g_scene.active_count--;
            break;
        }
    }
}

/* ---------------------------------------------------------------------------
 * Scene_GetEntity
 * --------------------------------------------------------------------------- */

Entity* Scene_GetEntity(uint32_t id)
{
    int i;
    
    if (id == 0) return NULL;
    
    for (i = 0; i < MAX_ENTITIES; i++) {
        if ((g_scene.entities[i].flags & ENTITY_FLAG_ACTIVE) && 
             g_scene.entities[i].id == id) {
            return &g_scene.entities[i];
        }
    }
    
    return NULL;
}

/* ---------------------------------------------------------------------------
 * Scene_GetEntityByIndex
 * --------------------------------------------------------------------------- */

Entity* Scene_GetEntityByIndex(uint32_t index)
{
    if (index >= MAX_ENTITIES) {
        return NULL;
    }
    
    return &g_scene.entities[index];
}
