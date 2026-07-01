/*******************************************************************************
 * FILE:         entity.c
 * MODULE:       Core/Scene
 * DESCRIPTION:  Entity initialization.
 *
 * DEPENDENCIES: entity.h
 *******************************************************************************/

#include "entity.h"

/* ---------------------------------------------------------------------------
 * Entity_Init
 * --------------------------------------------------------------------------- */

void Entity_Init(Entity* ent, uint32_t id)
{
    ent->id = id;
    Transform_Init(&ent->transform);
    ent->mesh_id = 0;
    ent->flags = 0; /* Clear all flags, making it inactive by default */
}
