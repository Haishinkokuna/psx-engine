/*******************************************************************************
 * FILE:         prim.h
 * MODULE:       Core/Renderer
 * DESCRIPTION:  GPU primitive allocation and initialization helpers.
 *
 *               This header provides typed allocation functions for every GPU
 *               primitive type defined in gpu_types.h. Each function:
 *                 1. Calls Pkt_Alloc() to claim the right number of bytes from
 *                    the current frame's packet pool.
 *                 2. Stamps the correct GPU command byte into the first color
 *                    word's 'cd' field (which the GPU reads as the command).
 *                 3. Initializes the 'tag' word to the OT terminator
 *                    (0x00FFFFFF) so the packet is a valid standalone list node
 *                    even before OT_Add is called.
 *                 4. Returns a typed pointer to the allocated packet.
 *
 *               WHY HELPERS INSTEAD OF DIRECT STRUCT USE?
 *               The GPU command byte position and size values are tricky to get
 *               right. A POLY_G3 where col0.cd is accidentally 0 instead of
 *               GPU_CMD_POLY_G3 will cause the GPU to misinterpret the command,
 *               producing corrupted output or a hung packet stream — silent
 *               bugs that are very hard to diagnose on real hardware. These
 *               helpers enforce correctness at the call site.
 *
 *               FIELD INITIALIZATION HELPERS:
 *               Setters for common fields (vertex coords, colors, UVs) are
 *               provided as inline functions. They exist to keep game code
 *               readable and to hide the int16_t casts on screen coordinates.
 *
 *               USAGE PATTERN (one flat triangle per frame):
 *
 *                 POLY_F3* p = Prim_AllocPolyF3();
 *                 if (p) {
 *                     Prim_SetXY0(p, 100, 50);
 *                     Prim_SetXY1(p, 150, 150);
 *                     Prim_SetXY2(p, 50,  150);
 *                     Prim_SetColor0(p, 255, 0, 0);
 *                     OT_Add(Display_GetCurrent()->ot, p, z_depth);
 *                 }
 *
 * DEPENDENCIES: gpu_types.h, display.h (for Pkt_Alloc), ot.h, <stdint.h>
 *******************************************************************************/

#ifndef PSX_PRIM_H
#define PSX_PRIM_H

#include <stdint.h>
#include "gpu_types.h"
#include "display.h"

/* ---------------------------------------------------------------------------
 * Internal macro: initialize any primitive's tag to the OT terminator.
 * This marks the packet as "end of list" until OT_Add wires it into a bucket.
 * --------------------------------------------------------------------------- */

#define PRIM_INIT_TAG(p)    (((uint32_t*)(p))[0] = PSX_GP0_OT_TERMINATOR)

/* ===========================================================================
 * Allocation functions — one per primitive type.
 * Returns NULL if the packet pool is exhausted.
 * =========================================================================== */

/* ---------------------------------------------------------------------------
 * Prim_AllocPolyF3 — Allocate a flat-shaded untextured triangle.
 * --------------------------------------------------------------------------- */

static inline POLY_F3* Prim_AllocPolyF3(void)
{
    POLY_F3* p = (POLY_F3*)Pkt_Alloc(sizeof(POLY_F3));
    if (p) {
        PRIM_INIT_TAG(p);
        p->col0.cd = GPU_CMD_POLY_F3;
    }
    return p;
}

/* ---------------------------------------------------------------------------
 * Prim_AllocPolyG3 — Allocate a Gouraud-shaded untextured triangle.
 * --------------------------------------------------------------------------- */

static inline POLY_G3* Prim_AllocPolyG3(void)
{
    POLY_G3* p = (POLY_G3*)Pkt_Alloc(sizeof(POLY_G3));
    if (p) {
        PRIM_INIT_TAG(p);
        p->col0.cd = GPU_CMD_POLY_G3;
        /* col1.cd and col2.cd must be 0 — the GPU command is set only once. */
        p->col1.cd = 0;
        p->col2.cd = 0;
    }
    return p;
}

/* ---------------------------------------------------------------------------
 * Prim_AllocPolyFT3 — Allocate a flat-shaded textured triangle.
 * --------------------------------------------------------------------------- */

static inline POLY_FT3* Prim_AllocPolyFT3(void)
{
    POLY_FT3* p = (POLY_FT3*)Pkt_Alloc(sizeof(POLY_FT3));
    if (p) {
        PRIM_INIT_TAG(p);
        p->col0.cd = GPU_CMD_POLY_FT3;
        p->pad     = 0;
    }
    return p;
}

/* ---------------------------------------------------------------------------
 * Prim_AllocPolyGT3 — Allocate a Gouraud-shaded textured triangle.
 * --------------------------------------------------------------------------- */

static inline POLY_GT3* Prim_AllocPolyGT3(void)
{
    POLY_GT3* p = (POLY_GT3*)Pkt_Alloc(sizeof(POLY_GT3));
    if (p) {
        PRIM_INIT_TAG(p);
        p->col0.cd = GPU_CMD_POLY_GT3;
        p->col1.cd = 0;
        p->col2.cd = 0;
        p->pad     = 0;
    }
    return p;
}

/* ---------------------------------------------------------------------------
 * Prim_AllocPolyF4 — Allocate a flat-shaded untextured quad.
 * --------------------------------------------------------------------------- */

static inline POLY_F4* Prim_AllocPolyF4(void)
{
    POLY_F4* p = (POLY_F4*)Pkt_Alloc(sizeof(POLY_F4));
    if (p) {
        PRIM_INIT_TAG(p);
        p->col0.cd = GPU_CMD_POLY_F4;
    }
    return p;
}

/* ---------------------------------------------------------------------------
 * Prim_AllocPolyG4 — Allocate a Gouraud-shaded untextured quad.
 * --------------------------------------------------------------------------- */

static inline POLY_G4* Prim_AllocPolyG4(void)
{
    POLY_G4* p = (POLY_G4*)Pkt_Alloc(sizeof(POLY_G4));
    if (p) {
        PRIM_INIT_TAG(p);
        p->col0.cd = GPU_CMD_POLY_G4;
        p->col1.cd = 0;
        p->col2.cd = 0;
        p->col3.cd = 0;
    }
    return p;
}

/* ---------------------------------------------------------------------------
 * Prim_AllocPolyFT4 — Allocate a flat-shaded textured quad.
 * --------------------------------------------------------------------------- */

static inline POLY_FT4* Prim_AllocPolyFT4(void)
{
    POLY_FT4* p = (POLY_FT4*)Pkt_Alloc(sizeof(POLY_FT4));
    if (p) {
        PRIM_INIT_TAG(p);
        p->col0.cd = GPU_CMD_POLY_FT4;
        p->pad0    = 0;
        p->pad1    = 0;
    }
    return p;
}

/* ---------------------------------------------------------------------------
 * Prim_AllocPolyGT4 — Allocate a Gouraud-shaded textured quad.
 * --------------------------------------------------------------------------- */

static inline POLY_GT4* Prim_AllocPolyGT4(void)
{
    POLY_GT4* p = (POLY_GT4*)Pkt_Alloc(sizeof(POLY_GT4));
    if (p) {
        PRIM_INIT_TAG(p);
        p->col0.cd = GPU_CMD_POLY_GT4;
        p->col1.cd = 0;
        p->col2.cd = 0;
        p->col3.cd = 0;
        p->pad0    = 0;
        p->pad1    = 0;
    }
    return p;
}

/* ---------------------------------------------------------------------------
 * Prim_AllocLineF2 — Allocate a flat-shaded line (2 endpoints).
 * --------------------------------------------------------------------------- */

static inline LINE_F2* Prim_AllocLineF2(void)
{
    LINE_F2* p = (LINE_F2*)Pkt_Alloc(sizeof(LINE_F2));
    if (p) {
        PRIM_INIT_TAG(p);
        p->col0.cd = GPU_CMD_LINE_F2;
    }
    return p;
}

/* ---------------------------------------------------------------------------
 * Prim_AllocTile — Allocate a filled rectangle primitive.
 * --------------------------------------------------------------------------- */

static inline TILE* Prim_AllocTile(void)
{
    TILE* p = (TILE*)Pkt_Alloc(sizeof(TILE));
    if (p) {
        PRIM_INIT_TAG(p);
        p->col0.cd = GPU_CMD_TILE;
    }
    return p;
}

/* ---------------------------------------------------------------------------
 * Prim_AllocSprt — Allocate a variable-size sprite primitive.
 * --------------------------------------------------------------------------- */

static inline SPRT* Prim_AllocSprt(void)
{
    SPRT* p = (SPRT*)Pkt_Alloc(sizeof(SPRT));
    if (p) {
        PRIM_INIT_TAG(p);
        p->col0.cd = GPU_CMD_SPRT;
    }
    return p;
}

/* ===========================================================================
 * Field setter macros and inline functions.
 *
 * These exist so game code reads naturally:
 *   Prim_SetXY0(p, x, y);  instead of  p->v0.sx = x; p->v0.sy = y;
 *
 * Using macros (not functions) here avoids struct type dependency — the
 * same SetXY0 pattern applies to POLY_F3, POLY_G3, etc. without overloads.
 * =========================================================================== */

/* Set vertex screen coordinates by index (0, 1, 2, or 3). */
#define Prim_SetXY0(p, x, y)   do { (p)->v0.sx = (int16_t)(x); (p)->v0.sy = (int16_t)(y); } while(0)
#define Prim_SetXY1(p, x, y)   do { (p)->v1.sx = (int16_t)(x); (p)->v1.sy = (int16_t)(y); } while(0)
#define Prim_SetXY2(p, x, y)   do { (p)->v2.sx = (int16_t)(x); (p)->v2.sy = (int16_t)(y); } while(0)
#define Prim_SetXY3(p, x, y)   do { (p)->v3.sx = (int16_t)(x); (p)->v3.sy = (int16_t)(y); } while(0)

/* Set flat color (col0) — used for POLY_F3, POLY_F4, LINE_F2, TILE, SPRT.
 * The 'cd' byte is NOT overwritten here — it was set by Prim_AllocXxx. */
#define Prim_SetColor0(p, r, g, b) \
    do { (p)->col0.r = (uint8_t)(r); (p)->col0.g = (uint8_t)(g); (p)->col0.b = (uint8_t)(b); } while(0)

/* Set per-vertex colors for Gouraud primitives. */
#define Prim_SetColor1(p, r, g, b) \
    do { (p)->col1.r = (uint8_t)(r); (p)->col1.g = (uint8_t)(g); (p)->col1.b = (uint8_t)(b); } while(0)
#define Prim_SetColor2(p, r, g, b) \
    do { (p)->col2.r = (uint8_t)(r); (p)->col2.g = (uint8_t)(g); (p)->col2.b = (uint8_t)(b); } while(0)
#define Prim_SetColor3(p, r, g, b) \
    do { (p)->col3.r = (uint8_t)(r); (p)->col3.g = (uint8_t)(g); (p)->col3.b = (uint8_t)(b); } while(0)

/* Set TILE dimensions. */
#define Prim_SetWH(p, w, h) \
    do { (p)->w = (uint16_t)(w); (p)->h = (uint16_t)(h); } while(0)

/* Set texture UV for vertex 0 of textured primitives. */
#define Prim_SetUV0(p, u, v)  do { (p)->u0 = (uint8_t)(u); (p)->v0_uv = (uint8_t)(v); } while(0)
#define Prim_SetUV1(p, u, v)  do { (p)->u1 = (uint8_t)(u); (p)->v1_uv = (uint8_t)(v); } while(0)
#define Prim_SetUV2(p, u, v)  do { (p)->u2 = (uint8_t)(u); (p)->v2_uv = (uint8_t)(v); } while(0)
#define Prim_SetUV3(p, u, v)  do { (p)->u3 = (uint8_t)(u); (p)->v3_uv = (uint8_t)(v); } while(0)

/* Set CLUT and TPAGE descriptors for textured primitives.
 * Use GPU_MAKE_CLUT and GPU_MAKE_TPAGE from gpu_types.h to build these. */
#define Prim_SetCLUT(p, clut_val)   ((p)->clut  = (uint16_t)(clut_val))
#define Prim_SetTPage(p, tpage_val) ((p)->tpage = (uint16_t)(tpage_val))

#endif /* PSX_PRIM_H */
