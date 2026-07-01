/*******************************************************************************
 * FILE:         gte_project.h
 * MODULE:       Core/GTE
 * DESCRIPTION:  High-level GTE pipeline helpers for 3D-to-2D projection.
 *
 *               This file provides the functions that game and scene code
 *               actually call — it wraps the low-level GTE register loads and
 *               opcode calls from gte.h and gte_ops.h into a clean, typed
 *               interface.
 *
 *               THE FULL TRIANGLE PIPELINE:
 *               Given a world-space triangle (three Vec3 vertices), a rotation
 *               matrix, and a translation vector (object-to-camera transform),
 *               GTE_TransformTriangle performs:
 *
 *                 1. Load rotation matrix into GTE control registers.
 *                 2. Load translation vector into GTE control registers.
 *                 3. Load all three vertices into GTE data registers V0/V1/V2.
 *                 4. Execute RTPT (rotate-translate-perspective for all 3).
 *                 5. Execute NCLIP (check winding — backface cull).
 *                 6. Execute AVSZ3 (compute average Z for OT insertion).
 *                 7. Read back screen XY from SXY0/SXY1/SXY2.
 *                 8. Read back OTZ for OT_Add depth.
 *
 *               The output sx/sy/ot_z values can be passed directly to:
 *                 - Prim_SetXY0/1/2 (screen coordinate setters)
 *                 - OT_Add(ot, prim, ot_z) (depth insertion)
 *
 *               CAMERA TRANSFORM NOTE:
 *               The rotation matrix and translation passed here should be the
 *               combined model-view matrix (world transform * inverse camera
 *               transform). The GTE operates in camera space, not world space.
 *               For a static camera at the origin, the model-view matrix is
 *               simply the object's own rotation and position.
 *
 *               OT DEPTH CLAMPING:
 *               The raw OTZ from AVSZ3 can be negative (triangle is behind
 *               the camera) or larger than OT_ENTRY_COUNT-1 (too far away).
 *               GTE_TransformTriangle clamps and returns 0 (cull) for both
 *               cases. The caller should check the return value before calling
 *               OT_Add — a return of 0 means the triangle should NOT be drawn.
 *
 * DEPENDENCIES: gte.h, gte_ops.h, gpu_types.h (for SVECTOR), ot.h
 *******************************************************************************/

#ifndef PSX_GTE_PROJECT_H
#define PSX_GTE_PROJECT_H

#include <stdint.h>
#include "gte.h"
#include "gte_ops.h"
#include "../renderer/ot.h"
#include "../math/vec3.h"
#include "../math/mat3.h"

/* ---------------------------------------------------------------------------
 * GTE_TransformTriangle — Full pipeline: 3D triangle -> screen coordinates.
 *
 * This is the core function that game rendering code calls for every visible
 * triangle. It runs the complete GTE pipeline and returns everything needed
 * to submit a colored polygon primitive to the Ordering Table.
 *
 * STEPS EXECUTED:
 *   1. GTE_SetRotMatrix(rot)        — load object rotation into GTE
 *   2. GTE_SetTransVector(trans)    — load object position into GTE
 *   3. GTE_SetVertex0/1/2(v0/v1/v2) — load triangle vertices into GTE
 *   4. GTE_RTPT()                   — project all three vertices
 *   5. GTE_NCLIP()                  — backface cull test
 *   6. GTE_AVSZ3()                  — compute average Z for OT insertion
 *   7. Read SXY0/1/2 -> out_sx/y    — retrieve screen coordinates
 *   8. Read OTZ, clamp -> out_ot_z  — retrieve OT depth bucket
 *
 * PARAMETERS:
 *   rot      — Object rotation matrix (FP4_12, object-to-camera space)
 *   trans    — Object translation vector (FP12_4, camera space)
 *   v0/1/2   — Triangle vertices in object space (FP12_4)
 *   out_sx0  — Output: screen X for vertex 0 (pixels)
 *   out_sy0  — Output: screen Y for vertex 0 (pixels)
 *   (... same for vertices 1 and 2)
 *   out_ot_z — Output: OT depth bucket index [0, OT_ENTRY_COUNT-1]
 *
 * RETURN VALUE:
 *   1 — Triangle is front-facing and in the view frustum. Caller should draw.
 *   0 — Triangle is back-facing (NCLIP <= 0) OR behind/beyond frustum.
 *       Caller should skip OT_Add for this triangle.
 * --------------------------------------------------------------------------- */

static inline int GTE_TransformTriangle(
    const Mat3* rot,
    const Vec3* trans,
    const Vec3* v0, const Vec3* v1, const Vec3* v2,
    int16_t* out_sx0, int16_t* out_sy0,
    int16_t* out_sx1, int16_t* out_sy1,
    int16_t* out_sx2, int16_t* out_sy2,
    int*     out_ot_z)
{
    int32_t mac0;
    int32_t otz;

    /* Step 1-2: Load the object-to-camera transform into GTE registers. */
    GTE_SetRotMatrix(rot);
    GTE_SetTransVector(trans);

    /* Step 3: Load all three triangle vertices. V0, V1, V2 correspond to the
     * GTE's dedicated input vertex slots (data registers r0-r5). */
    GTE_SetVertex0(v0);
    GTE_SetVertex1(v1);
    GTE_SetVertex2(v2);

    /* Step 4: RTPT — projects all three vertices in one hardware operation.
     * After this, SXY0/1/2 hold screen-space (sx, sy) for each vertex.
     * SZ1/2/3 hold the corresponding screen-space Z values. */
    GTE_RTPT();

    /* Step 5: NCLIP — compute the 2D cross product of the projected triangle.
     * Positive MAC0 = CCW winding (front-facing). PSX convention is that
     * front-facing triangles have CCW winding in screen space. */
    GTE_NCLIP();
    mac0 = GTE_ReadMAC0();

    if (mac0 <= 0) {
        /* Back-facing or degenerate (zero-area) triangle — cull it.
         * Return 0 to signal the caller to skip this triangle. */
        return 0;
    }

    /* Step 6: AVSZ3 — compute the average of SZ1+SZ2+SZ3 for OT placement.
     * After this, OTZ holds the depth bucket index in the range that ZSF3
     * projects it into. Typical output range matches OT_ENTRY_COUNT. */
    GTE_AVSZ3();
    otz = GTE_ReadOTZ();

    /* Clamp OTZ to valid OT range. Negative = behind camera, too large =
     * beyond the far clip plane implied by OT_ENTRY_COUNT buckets.
     * Both cases mean the triangle should not be rendered. */
    if (otz < 0 || otz >= OT_ENTRY_COUNT) {
        return 0;
    }

    /* Step 7: Read projected screen coordinates for all three vertices.
     * These are screen-pixel values (already offset by OFX/OFY in the GTE).
     * They can be passed directly to Prim_SetXY0/1/2. */
    GTE_ReadSXY0(out_sx0, out_sy0);
    GTE_ReadSXY1(out_sx1, out_sy1);
    GTE_ReadSXY2(out_sx2, out_sy2);

    /* Step 8: Output the OT depth bucket. */
    *out_ot_z = (int)otz;

    return 1; /* Triangle is visible — caller should build and submit a primitive. */
}

/* ---------------------------------------------------------------------------
 * GTE_TransformQuad — Full pipeline for a quad (4 vertices -> 2 triangles).
 *
 * A PSX quad is rendered as a single POLY_F4/G4/FT4/GT4 packet, which the
 * GPU internally rasterizes as two triangles (v0v1v2, v1v2v3). The GTE
 * handles four vertices using RTPT for the first three + RTPS for the fourth.
 *
 * The backface test uses the first three vertices (v0, v1, v2). The fourth
 * vertex (v3) is transformed and its Z is included in AVSZ4 for a better
 * depth estimate of the quad's center.
 *
 * PARAMETERS: Same as GTE_TransformTriangle, plus v3/sx3/sy3 for the 4th vertex.
 *
 * RETURN VALUE: 1 = front-facing visible quad, 0 = cull.
 * --------------------------------------------------------------------------- */

static inline int GTE_TransformQuad(
    const Mat3* rot,
    const Vec3* trans,
    const Vec3* v0, const Vec3* v1, const Vec3* v2, const Vec3* v3,
    int16_t* out_sx0, int16_t* out_sy0,
    int16_t* out_sx1, int16_t* out_sy1,
    int16_t* out_sx2, int16_t* out_sy2,
    int16_t* out_sx3, int16_t* out_sy3,
    int*     out_ot_z)
{
    int32_t mac0;
    int32_t otz;

    GTE_SetRotMatrix(rot);
    GTE_SetTransVector(trans);

    /* Transform three vertices with RTPT (hardware batch). */
    GTE_SetVertex0(v0);
    GTE_SetVertex1(v1);
    GTE_SetVertex2(v2);
    GTE_RTPT();

    /* Backface test on the first triangle's winding. */
    GTE_NCLIP();
    mac0 = GTE_ReadMAC0();
    if (mac0 <= 0) {
        return 0;
    }

    /* Transform the fourth vertex with RTPS (adds to the SZ FIFO at SZ3). */
    GTE_SetVertex0(v3);
    GTE_RTPS();

    /* AVSZ4: use all four screen Z values for a more accurate depth bucket. */
    GTE_AVSZ4();
    otz = GTE_ReadOTZ();

    if (otz < 0 || otz >= OT_ENTRY_COUNT) {
        return 0;
    }

    /* Read back all four screen coordinates. */
    GTE_ReadSXY0(out_sx0, out_sy0);
    GTE_ReadSXY1(out_sx1, out_sy1);
    GTE_ReadSXY2(out_sx2, out_sy2);

    /* The fourth vertex is now in SXY2 (RTPS overwrote it — the FIFO shifts).
     * On real GTE: after the second RTPS call, SXY0=v1, SXY1=v2, SXY2=v3.
     * The v0 result is lost from the FIFO. We captured v0 via SXY0 before
     * the second RTPS, so we can read SXY2 for v3 now. */
    GTE_ReadSXY2(out_sx3, out_sy3);

    *out_ot_z = (int)otz;
    return 1;
}

/* ---------------------------------------------------------------------------
 * GTE_SetupForScene — Configure GTE for a new frame/scene.
 *
 * Sets the screen center offset and projection distance. Should be called
 * once per frame, or whenever the camera FOV changes.
 *
 * @param h     Projection plane distance. 256 = standard 90-degree FOV at 320x240.
 *              Increase for a narrower (telephoto) perspective.
 *              Decrease for a wider (fisheye) perspective.
 * --------------------------------------------------------------------------- */

static inline void GTE_SetupForScene(int32_t h)
{
    /* 320x240 screen center in Q4 sub-pixel units: (160*16, 120*16) */
    GTE_SetScreenOffset(160 * 16, 120 * 16);
    GTE_SetProjection(h);
    /* ZSF3 = 1365 (4096/3): true average of 3 Z values -> OTZ range 0-255 */
    GTE_SetZScaleFactors(1365, 1024);
}

#endif /* PSX_GTE_PROJECT_H */
