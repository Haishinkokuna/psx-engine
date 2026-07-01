/*******************************************************************************
 * FILE:         entity.h
 * MODULE:       Core/Scene
 * DESCRIPTION:  Game world entity representation.
 *
 *               An Entity ties together a spatial Transform, a visual mesh ID,
 *               and flags that dictate its behavior in the engine.
 *
 * DEPENDENCIES: transform.h
 *******************************************************************************/

#ifndef PSX_ENTITY_H
#define PSX_ENTITY_H

#include <stdint.h>
#include "transform.h"

/* ---------------------------------------------------------------------------
 * Entity Flags
 * --------------------------------------------------------------------------- */

#define ENTITY_FLAG_ACTIVE   (1 << 0)  /* Is this entity slot in use? */
#define ENTITY_FLAG_VISIBLE  (1 << 1)  /* Should the renderer draw it? */

/* ---------------------------------------------------------------------------
 * Entity
 *
 * id:       A unique handle for identifying this entity globally (e.g. over RPC).
 * transform: Spatial state (position, rotation, scale).
 * mesh_id:  Identifier referencing a loaded 3D mesh asset.
 * flags:    Bitmask of ENTITY_FLAG_*.
 * --------------------------------------------------------------------------- */

typedef struct {
    uint32_t  id;
    Transform transform;
    uint32_t  mesh_id;
    uint32_t  flags;
} Entity;

/* ---------------------------------------------------------------------------
 * Entity_Init
 *
 * Initializes an entity to a blank state (inactive, origin transform).
 * --------------------------------------------------------------------------- */

void Entity_Init(Entity* ent, uint32_t id);

#endif /* PSX_ENTITY_H */
