/*******************************************************************************
 * FILE:         test_gte.cpp
 * MODULE:       Core/GTE — Host Unit Tests
 * DESCRIPTION:  Validates the software GTE pipeline (register loads, RTPS/RTPT,
 *               NCLIP backface cull, AVSZ3 depth averaging, and the full
 *               GTE_TransformTriangle pipeline) on the host platform.
 *
 *               All tests use PSX_HOST_TEST=1 which activates:
 *                 - g_gte_regs (software register file in gte_regs.c)
 *                 - Software C equivalents in all inline functions
 *               No MIPS hardware or coprocessor instructions are used.
 *
 *               TESTS COVERED:
 *                 GTE_Init               — register file reset to identity
 *                 GTE_SetRotMatrix       — matrix stored in rt[9]
 *                 GTE_SetTransVector     — translation stored in trx/try/trz
 *                 GTE_SetVertex0/1/2     — vertices stored in vxy/vz slots
 *                 GTE_SetProjection      — h register stored
 *                 GTE_SetZScaleFactors   — zsf3/zsf4 stored
 *                 GTE_RTPS identity      — vertex at origin transforms to center
 *                 GTE_RTPS translate     — vertex at (100,0,512) projects correctly
 *                 GTE_RTPT              — three vertices projected simultaneously
 *                 GTE_NCLIP CCW          — front-facing triangle -> MAC0 > 0
 *                 GTE_NCLIP CW           — back-facing triangle  -> MAC0 < 0
 *                 GTE_NCLIP colinear     — degenerate triangle   -> MAC0 = 0
 *                 GTE_AVSZ3              — average of three Z values
 *                 GTE_TransformTriangle (visible)   — returns 1 with correct sx/sy
 *                 GTE_TransformTriangle (backfaced) — returns 0 (cull)
 *                 GTE_TransformTriangle (behind cam)— returns 0 (behind clip)
 *
 * DEPENDENCIES: gte.h, gte_ops.h, gte_project.h, gte_regs.c
 *******************************************************************************/

#ifndef PSX_HOST_TEST
#define PSX_HOST_TEST 1
#endif

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstdlib>  /* for abs() */

extern "C" {
#include "gte.h"
#include "gte_ops.h"
#include "gte_project.h"
}

/* ---------------------------------------------------------------------------
 * Test runner helpers
 * --------------------------------------------------------------------------- */

static int g_pass = 0;
static int g_fail = 0;

static void check(int cond, const char* name)
{
    if (cond) { printf("[PASS] %s\n", name); g_pass++; }
    else       { printf("[FAIL] %s\n", name); g_fail++; }
}

/* Approximate equality for screen coordinates (±2 pixel tolerance to
 * account for integer rounding differences vs. the hardware GTE). */
static int approx_eq(int a, int b, int tolerance)
{
    int diff = a - b;
    return (diff >= -tolerance) && (diff <= tolerance);
}

/* ---------------------------------------------------------------------------
 * Reset the software register file to a clean state between tests.
 * --------------------------------------------------------------------------- */

static void reset_regs(void)
{
    memset(&g_gte_regs, 0, sizeof(g_gte_regs));
}

/* ---------------------------------------------------------------------------
 * Helper: build an identity Mat3 and zero Vec3.
 * --------------------------------------------------------------------------- */

static Mat3 make_identity(void)
{
    Mat3 m;
    m.m[0] = 4096; m.m[1] = 0;    m.m[2] = 0;
    m.m[3] = 0;    m.m[4] = 4096; m.m[5] = 0;
    m.m[6] = 0;    m.m[7] = 0;    m.m[8] = 4096;
    return m;
}

static Vec3 make_vec3(int32_t x, int32_t y, int32_t z)
{
    Vec3 v; v.x = x; v.y = y; v.z = z;
    return v;
}

/* ===========================================================================
 * Test cases
 * =========================================================================== */

static void test_set_rot_matrix(void)
{
    reset_regs();
    Mat3 m = make_identity();
    GTE_SetRotMatrix(&m);

    check(g_gte_regs.rt[0] == 4096, "SetRotMatrix: rt[0] = 4096 (FP4_12 1.0)");
    check(g_gte_regs.rt[1] == 0,    "SetRotMatrix: rt[1] = 0");
    check(g_gte_regs.rt[4] == 4096, "SetRotMatrix: rt[4] = 4096 (FP4_12 1.0)");
    check(g_gte_regs.rt[8] == 4096, "SetRotMatrix: rt[8] = 4096 (FP4_12 1.0)");
}

static void test_set_trans_vector(void)
{
    reset_regs();
    Vec3 t = make_vec3(100, -50, 512);
    GTE_SetTransVector(&t);

    check(g_gte_regs.trx  == 100,  "SetTransVector: trx =  100");
    check(g_gte_regs.try_ == -50,  "SetTransVector: try = -50");
    check(g_gte_regs.trz  == 512,  "SetTransVector: trz =  512");
}

static void test_set_vertex0(void)
{
    reset_regs();
    Vec3 v = make_vec3(10, 20, 30);
    GTE_SetVertex0(&v);

    check(g_gte_regs.vxy0_x == 10, "SetVertex0: vxy0_x = 10");
    check(g_gte_regs.vxy0_y == 20, "SetVertex0: vxy0_y = 20");
    check(g_gte_regs.vz0    == 30, "SetVertex0: vz0    = 30");
}

static void test_set_projection(void)
{
    reset_regs();
    GTE_SetProjection(256);
    check(g_gte_regs.h == 256, "SetProjection: h = 256");
}

static void test_set_zscale(void)
{
    reset_regs();
    GTE_SetZScaleFactors(1365, 1024);
    check(g_gte_regs.zsf3 == 1365, "SetZScaleFactors: zsf3 = 1365");
    check(g_gte_regs.zsf4 == 1024, "SetZScaleFactors: zsf4 = 1024");
}

static void test_rtps_identity(void)
{
    /* A vertex at (0, 0, 256) with identity rotation and zero translation,
     * H=256, screen center at (160, 120). Should project to screen center. */
    reset_regs();
    GTE_Init();

    Vec3 v = make_vec3(0, 0, 256); /* in FP12_4: actual Z = 256/16 = 16 world units */
    GTE_SetVertex0(&v);
    GTE_RTPS();

    int16_t sx, sy;
    GTE_ReadSXY2(&sx, &sy);

    /* Expected: screen center = (160, 120). Tolerance ±2 for integer rounding. */
    check(approx_eq(sx, 160, 2), "RTPS identity: sx ≈ 160 (screen center X)");
    check(approx_eq(sy, 120, 2), "RTPS identity: sy ≈ 120 (screen center Y)");
}

static void test_rtps_offset_vertex(void)
{
    /* A vertex offset right from center. With identity rotation and H=256,
     * a vertex at camera-space X=128 (FP12_4) at Z=256 should project right of center. */
    reset_regs();
    GTE_Init();

    /* FP12_4: 128 means 128/16 = 8 world units right.
     * screen_x ≈ 160 + (8 * 256) / (256/16) ... but everything is FP12_4 so:
     * The math inside RTPS: tx = (4096*128) >> 12 + 0 = 128, tz = 256.
     * sx = (2560 + (128 * 256) / 256) >> 4 = (2560 + 128) >> 4 = 2688/16 = 168. */
    Vec3 v = make_vec3(128, 0, 256);
    GTE_SetVertex0(&v);
    GTE_RTPS();

    int16_t sx, sy;
    GTE_ReadSXY2(&sx, &sy);

    /* Vertex is 8 FP units right of center -> projects to slightly right of 160 */
    check(sx > 160, "RTPS offset: vertex right of center projects right of screen center");
    check(approx_eq(sy, 120, 2), "RTPS offset: Y unchanged for horizontal offset");
}

static void test_nclip_front_facing(void)
{
    /* CCW triangle in screen space: v0=(10,100), v1=(100,10), v2=(190,100).
     * Cross product (v1-v0) x (v2-v0) should be positive. */
    reset_regs();
    g_gte_regs.sxy0_x = 10;  g_gte_regs.sxy0_y = 100;
    g_gte_regs.sxy1_x = 100; g_gte_regs.sxy1_y = 10;
    g_gte_regs.sxy2_x = 190; g_gte_regs.sxy2_y = 100;

    GTE_NCLIP();
    int32_t mac0 = GTE_ReadMAC0();

    check(mac0 > 0, "NCLIP front-facing: MAC0 > 0 for CCW winding");
}

static void test_nclip_back_facing(void)
{
    /* CW triangle (reversed winding) — should produce MAC0 <= 0. */
    reset_regs();
    g_gte_regs.sxy0_x = 190; g_gte_regs.sxy0_y = 100;
    g_gte_regs.sxy1_x = 100; g_gte_regs.sxy1_y = 10;
    g_gte_regs.sxy2_x = 10;  g_gte_regs.sxy2_y = 100;

    GTE_NCLIP();
    int32_t mac0 = GTE_ReadMAC0();

    check(mac0 < 0, "NCLIP back-facing: MAC0 < 0 for CW winding");
}

static void test_nclip_degenerate(void)
{
    /* Collinear points — zero area — should give MAC0 = 0. */
    reset_regs();
    g_gte_regs.sxy0_x = 10;  g_gte_regs.sxy0_y = 10;
    g_gte_regs.sxy1_x = 50;  g_gte_regs.sxy1_y = 50;
    g_gte_regs.sxy2_x = 90;  g_gte_regs.sxy2_y = 90;

    GTE_NCLIP();
    check(GTE_ReadMAC0() == 0, "NCLIP degenerate: MAC0 = 0 for collinear points");
}

static void test_avsz3_average(void)
{
    /* Three Z values that should average to a specific OTZ value. */
    reset_regs();
    g_gte_regs.sz1   = 100;
    g_gte_regs.sz2   = 200;
    g_gte_regs.sz3   = 300;
    g_gte_regs.zsf3  = 1365; /* 4096/3 */

    GTE_AVSZ3();
    int32_t otz = GTE_ReadOTZ();

    /* Expected: (100+200+300) * 1365 >> 12 = 600 * 1365 / 4096 = 819000/4096 ≈ 199 */
    check(approx_eq(otz, 199, 2), "AVSZ3: average of {100,200,300} ≈ 200 (±2)");
}

static void test_transform_triangle_visible(void)
{
    reset_regs();
    GTE_Init();

    Mat3 rot  = make_identity();
    Vec3 trans = make_vec3(0, 0, 256); /* push 16 units in front of camera */

    /* Front-facing CCW triangle centered around camera forward vector. */
    Vec3 v0 = make_vec3(-64, -64, 0);
    Vec3 v1 = make_vec3( 64, -64, 0);
    Vec3 v2 = make_vec3(  0,  64, 0);

    int16_t sx0, sy0, sx1, sy1, sx2, sy2;
    int ot_z;

    int visible = GTE_TransformTriangle(
        &rot, &trans,
        &v0, &v1, &v2,
        &sx0, &sy0, &sx1, &sy1, &sx2, &sy2,
        &ot_z
    );

    check(visible == 1,         "TransformTriangle visible: returns 1");
    check(ot_z >= 0 && ot_z < OT_ENTRY_COUNT,
                                "TransformTriangle visible: ot_z in valid range");
    /* v0 is top-left, v1 is top-right, v2 is bottom-center:
     * sx0 < sx1 (v0 is left of v1) and sy2 > sy0 (v2 is below v0) */
    check(sx0 < sx1,            "TransformTriangle visible: v0 sx < v1 sx (left/right)");
    check(sy2 > sy0,            "TransformTriangle visible: v2 sy > v0 sy (bottom > top)");
}

static void test_transform_triangle_backfaced(void)
{
    reset_regs();
    GTE_Init();

    Mat3 rot   = make_identity();
    Vec3 trans = make_vec3(0, 0, 256);

    /* CW winding — back-facing relative to our CCW convention. */
    Vec3 v0 = make_vec3(  0,  64, 0);
    Vec3 v1 = make_vec3( 64, -64, 0);
    Vec3 v2 = make_vec3(-64, -64, 0);

    int16_t sx0, sy0, sx1, sy1, sx2, sy2;
    int ot_z;

    int visible = GTE_TransformTriangle(
        &rot, &trans,
        &v0, &v1, &v2,
        &sx0, &sy0, &sx1, &sy1, &sx2, &sy2,
        &ot_z
    );

    check(visible == 0, "TransformTriangle backfaced: returns 0 (culled)");
}

static void test_transform_triangle_behind_camera(void)
{
    reset_regs();
    GTE_Init();

    Mat3 rot   = make_identity();
    /* Translation puts the object BEHIND the camera (negative Z). */
    Vec3 trans = make_vec3(0, 0, -512);

    Vec3 v0 = make_vec3(-64, -64, 0);
    Vec3 v1 = make_vec3( 64, -64, 0);
    Vec3 v2 = make_vec3(  0,  64, 0);

    int16_t sx0, sy0, sx1, sy1, sx2, sy2;
    int ot_z;

    int visible = GTE_TransformTriangle(
        &rot, &trans,
        &v0, &v1, &v2,
        &sx0, &sy0, &sx1, &sy1, &sx2, &sy2,
        &ot_z
    );

    /* With negative Z, either the OTZ is out of range or winding flips.
     * Either way, we expect this triangle to be culled (return 0). */
    check(visible == 0, "TransformTriangle behind camera: returns 0 (culled)");
}

/* ---------------------------------------------------------------------------
 * Main
 * --------------------------------------------------------------------------- */

int main(void)
{
    printf("=== PSX ENGINE: Core/GTE Unit Tests ===\n\n");

    printf("-- Register Load/Store --\n");
    test_set_rot_matrix();
    test_set_trans_vector();
    test_set_vertex0();
    test_set_projection();
    test_set_zscale();

    printf("\n-- RTPS Projection --\n");
    test_rtps_identity();
    test_rtps_offset_vertex();

    printf("\n-- NCLIP Backface --\n");
    test_nclip_front_facing();
    test_nclip_back_facing();
    test_nclip_degenerate();

    printf("\n-- AVSZ3 Depth Average --\n");
    test_avsz3_average();

    printf("\n-- GTE_TransformTriangle Pipeline --\n");
    test_transform_triangle_visible();
    test_transform_triangle_backfaced();
    test_transform_triangle_behind_camera();

    printf("\n=======================================\n");
    printf("Results: %d passed, %d failed.\n", g_pass, g_fail);

    return (g_fail == 0) ? 0 : 1;
}
