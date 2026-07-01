/*******************************************************************************
 * FILE:         tmd.h
 * MODULE:       Core/Assets
 * DESCRIPTION:  TMD (3D Model) parser.
 *
 *               TMD is the standard Sony 3D geometry format. It consists of:
 *               1. Header
 *               2. Object Table (list of 3D objects in the file)
 *               3. Object definition (pointers to vert/normal/prim arrays)
 *               4. Primitive definitions (POLY_F3, POLY_GT4, etc.)
 *               5. Vertices (SVECTOR)
 *               6. Normals (SVECTOR)
 *******************************************************************************/

#ifndef PSX_TMD_H
#define PSX_TMD_H

#include <stdint.h>
#include "../renderer/display.h"
#include "../scene/transform.h"
#include "../gte/gte.h"

/* ---------------------------------------------------------------------------
 * TMD Structures
 * --------------------------------------------------------------------------- */

/* Standard TMD Header */
typedef struct {
    uint32_t id;         /* Should be 0x00000041 */
    uint32_t flags;
    uint32_t num_objs;   /* Number of objects in the file */
} TMD_Header;

/* Object block defining where its arrays start */
typedef struct {
    uint32_t vert_top;   /* Offset to vertices (later converted to pointer) */
    uint32_t num_verts;
    uint32_t norm_top;   /* Offset to normals */
    uint32_t num_norms;
    uint32_t prim_top;   /* Offset to primitives */
    uint32_t num_prims;
    int32_t  scale;
} TMD_Object;

/* Primitive header (every TMD primitive starts with this) */
typedef struct {
    uint8_t  olen;       /* Length of primitive data in 32-bit words */
    uint8_t  ilen;
    uint8_t  flag;       /* Rendering flags */
    uint8_t  mode;       /* Primitive type (e.g., 0x20 = POLY_F3) */
} TMD_Prim;

/* TMD version of a flat triangle definition */
typedef struct {
    TMD_Prim head;
    uint8_t  r0, g0, b0, pad; /* Color */
    uint16_t n0, v0;          /* Normal index, Vertex 0 index */
    uint16_t v1, v2;          /* Vertex 1, Vertex 2 index */
} TMD_P_F3;

/* ---------------------------------------------------------------------------
 * TMD API
 * --------------------------------------------------------------------------- */

/* Parses a raw byte buffer containing a .TMD file.
 * This converts all relative offsets in the file into absolute memory pointers
 * so the data can be rendered quickly.
 * Returns the number of objects on success, 0 on failure. */
uint32_t TMD_Load(void* file_data);

/* Retrieves a specific object from a loaded TMD file. */
TMD_Object* TMD_GetObject(void* file_data, uint32_t obj_index);

/* Draws a TMD object into the current DisplayBuffer.
 * For this initial version, it only renders POLY_F3 (Flat Triangles). */
void TMD_Draw(const TMD_Object* obj, const Transform* t, DisplayBuffer* buf);

#endif /* PSX_TMD_H */
