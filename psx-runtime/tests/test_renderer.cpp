/*******************************************************************************
 * FILE:         test_renderer.cpp
 * MODULE:       Core/Renderer — Host Unit Tests
 * DESCRIPTION:  Verifies the Ordering Table, packet pool allocator, and
 *               double-buffer swap logic on the host platform.
 *
 *               These tests compile with PSX_HOST_TEST=1 defined, which
 *               stubs out all GPU and DMA MMIO writes in display.c. The
 *               rendering LOGIC (OT insertion, linked-list structure, pointer
 *               arithmetic, buffer flip) is still exercised fully.
 *
 *               TESTS COVERED:
 *                 OT_Clear         — all entries set to terminator (0x00FFFFFF)
 *                 OT_Add (single)  — primitive correctly linked into bucket
 *                 OT_Add (chain)   — LIFO ordering within one bucket
 *                 OT_Add (multi-Z) — two primitives at different depths
 *                 OT_GetTail       — returns last OT entry address
 *                 Pkt_Alloc        — advances pointer, no overlap
 *                 Pkt_Alloc        — returns NULL on overflow
 *                 Display_BeginFrame — resets pkt_ptr and OT
 *                 Display_EndFrame   — flips g_current_buffer
 *                 Prim_AllocPolyF3   — cmd byte correct, tag initialized
 *                 Prim_AllocPolyG3   — all cd bytes correct
 *                 Prim_AllocTile     — size and fields correct
 *                 Prim_SetXY0/Color0 — setters correctly write fields
 *
 * DEPENDENCIES: display.h, ot.h, prim.h, gpu_types.h, <cstdio>
 *******************************************************************************/

/* Force the host-test stubs in display.h before any includes. */
#ifndef PSX_HOST_TEST
#define PSX_HOST_TEST 1
#endif

#include <cstdio>
#include <cstdint>
#include <cstring>

extern "C" {
#include "display.h"
#include "ot.h"
#include "prim.h"
#include "gpu_types.h"
}

/* ---------------------------------------------------------------------------
 * Test runner helpers
 * --------------------------------------------------------------------------- */

static int g_pass = 0;
static int g_fail = 0;

static void check(int condition, const char* test_name)
{
    if (condition) {
        printf("[PASS] %s\n", test_name);
        g_pass++;
    } else {
        printf("[FAIL] %s\n", test_name);
        g_fail++;
    }
}

/* ---------------------------------------------------------------------------
 * OT tests — operate on a standalone OT array, not via Display_Init.
 * This isolates OT logic from display initialization.
 * --------------------------------------------------------------------------- */

static void test_ot_clear(void)
{
    OT_Tag ot[OT_ENTRY_COUNT];

    /* Pre-fill with garbage to verify Clear overwrites everything. */
    memset(ot, 0xAA, sizeof(ot));
    OT_Clear(ot, OT_ENTRY_COUNT);

    int all_terminated = 1;
    for (int i = 0; i < OT_ENTRY_COUNT; i++) {
        if (ot[i] != PSX_GP0_OT_TERMINATOR) {
            all_terminated = 0;
            break;
        }
    }
    check(all_terminated, "OT_Clear: all entries set to 0x00FFFFFF");
}

static void test_ot_add_single(void)
{
    OT_Tag  ot[OT_ENTRY_COUNT];
    uint32_t prim_data[4] = { PSX_GP0_OT_TERMINATOR, 0, 0, 0 };

    OT_Clear(ot, OT_ENTRY_COUNT);

    /* Insert the primitive at depth bucket 10. */
    OT_Add(ot, prim_data, 10);

    /* The bucket at Z=10 should now hold the address of prim_data (lower 24 bits). */
    uint32_t expected_addr = (uint32_t)((uintptr_t)prim_data & 0x00FFFFFFu);
    check(ot[10] == expected_addr, "OT_Add: bucket[10] holds primitive address");

    /* The primitive's tag should now hold the terminator (it was the list head). */
    check(prim_data[0] == PSX_GP0_OT_TERMINATOR,
          "OT_Add: primitive tag holds previous bucket value (terminator)");
}

static void test_ot_add_chain(void)
{
    OT_Tag   ot[OT_ENTRY_COUNT];
    uint32_t prim_a[4] = { PSX_GP0_OT_TERMINATOR, 0, 0, 0 };
    uint32_t prim_b[4] = { PSX_GP0_OT_TERMINATOR, 0, 0, 0 };

    OT_Clear(ot, OT_ENTRY_COUNT);

    /* Add A first, then B at the same depth. */
    OT_Add(ot, prim_a, 5);
    OT_Add(ot, prim_b, 5);

    /* After adding B, bucket[5] should point to B (LIFO — last in, first rendered). */
    uint32_t addr_b = (uint32_t)((uintptr_t)prim_b & 0x00FFFFFFu);
    check(ot[5] == addr_b, "OT_Add chain: bucket[5] -> prim_b (LIFO)");

    /* B's tag should point to A (B -> A -> terminator). */
    uint32_t addr_a = (uint32_t)((uintptr_t)prim_a & 0x00FFFFFFu);
    check(prim_b[0] == addr_a, "OT_Add chain: prim_b.tag -> prim_a");

    /* A's tag should still be the terminator. */
    check(prim_a[0] == PSX_GP0_OT_TERMINATOR, "OT_Add chain: prim_a.tag -> terminator");
}

static void test_ot_add_multi_z(void)
{
    OT_Tag   ot[OT_ENTRY_COUNT];
    uint32_t prim_near[4] = { PSX_GP0_OT_TERMINATOR, 0, 0, 0 };
    uint32_t prim_far[4]  = { PSX_GP0_OT_TERMINATOR, 0, 0, 0 };

    OT_Clear(ot, OT_ENTRY_COUNT);

    OT_Add(ot, prim_near, 2);
    OT_Add(ot, prim_far,  200);

    /* Buckets at different depths must be independent. */
    check(ot[2]   == (uint32_t)((uintptr_t)prim_near & 0x00FFFFFFu),
          "OT_Add multi-Z: near primitive in bucket[2]");
    check(ot[200] == (uint32_t)((uintptr_t)prim_far  & 0x00FFFFFFu),
          "OT_Add multi-Z: far primitive in bucket[200]");

    /* The two buckets must not cross-link. */
    check(ot[2] != ot[200], "OT_Add multi-Z: buckets are independent");
}

static void test_ot_get_tail(void)
{
    OT_Tag ot[OT_ENTRY_COUNT];
    OT_Clear(ot, OT_ENTRY_COUNT);

    OT_Tag* tail = OT_GetTail(ot);
    check(tail == &ot[OT_ENTRY_COUNT - 1], "OT_GetTail: returns &ot[255]");
}

/* ---------------------------------------------------------------------------
 * Display / packet pool tests — these call Display_Init().
 * In PSX_HOST_TEST mode, Init stubs out the MMIO writes.
 * --------------------------------------------------------------------------- */

static void test_pkt_alloc_basic(void)
{
    Display_Init();

    void* p1 = Pkt_Alloc(20); /* POLY_F3 size */
    void* p2 = Pkt_Alloc(20);

    check(p1 != (void*)0, "Pkt_Alloc: first allocation returns non-NULL");
    check(p2 != (void*)0, "Pkt_Alloc: second allocation returns non-NULL");
    check(p1 != p2,        "Pkt_Alloc: two allocations return distinct pointers");

    /* p2 should be exactly 20 bytes past p1 (linear allocator, no gaps). */
    intptr_t offset = (uint8_t*)p2 - (uint8_t*)p1;
    check(offset == 20, "Pkt_Alloc: second pointer is 20 bytes past first");
}

static void test_pkt_alloc_overflow(void)
{
    Display_Init();

    /* Exhaust the pool by allocating the maximum size at once.
     * PKT_POOL_SIZE is 32768. Request one byte more than capacity. */
    void* p1 = Pkt_Alloc(PKT_POOL_SIZE);
    check(p1 != (void*)0, "Pkt_Alloc overflow: max-size alloc succeeds");

    void* p2 = Pkt_Alloc(1);
    check(p2 == (void*)0, "Pkt_Alloc overflow: alloc beyond pool returns NULL");
}

static void test_begin_frame_resets_pool(void)
{
    Display_Init();

    void* p1 = Pkt_Alloc(32);
    check(p1 != (void*)0, "BeginFrame reset: initial alloc non-NULL");

    /* BeginFrame should reset the pool pointer to the start of pkt_pool. */
    Display_BeginFrame();

    /* After BeginFrame, the pool is reset — a new alloc should return the
     * same address as p1 (the start of the pool). */
    void* p2 = Pkt_Alloc(32);
    check(p2 == p1, "BeginFrame reset: pkt_ptr reset to pool start");
}

static void test_begin_frame_clears_ot(void)
{
    Display_Init();

    /* Add a primitive to the OT. */
    uint32_t dummy[4] = { PSX_GP0_OT_TERMINATOR, 0, 0, 0 };
    OT_Add(g_display_buffers[g_current_buffer].ot, dummy, 50);

    /* Verify it was added. */
    check(g_display_buffers[g_current_buffer].ot[50] != PSX_GP0_OT_TERMINATOR,
          "OT before BeginFrame: bucket[50] != terminator");

    Display_BeginFrame();

    /* After BeginFrame, the OT should be cleared. */
    check(g_display_buffers[g_current_buffer].ot[50] == PSX_GP0_OT_TERMINATOR,
          "BeginFrame OT clear: bucket[50] == terminator after reset");
}

static void test_end_frame_flips_buffer(void)
{
    Display_Init();

    int initial = g_current_buffer;
    Display_BeginFrame();
    Display_EndFrame();

    check(g_current_buffer != initial, "EndFrame: g_current_buffer flipped");
    check(g_current_buffer == (initial ^ 1), "EndFrame: flipped to correct index");

    /* Flip again — should return to original. */
    Display_BeginFrame();
    Display_EndFrame();
    check(g_current_buffer == initial, "EndFrame: two flips return to original buffer");
}

/* ---------------------------------------------------------------------------
 * Primitive allocation and field setter tests
 * --------------------------------------------------------------------------- */

static void test_prim_alloc_poly_f3(void)
{
    Display_Init();
    Display_BeginFrame();

    POLY_F3* p = Prim_AllocPolyF3();
    check(p != (POLY_F3*)0,                  "Prim_AllocPolyF3: returns non-NULL");
    check(p->col0.cd == GPU_CMD_POLY_F3,     "Prim_AllocPolyF3: command byte correct (0x20)");
    check(*(uint32_t*)p == PSX_GP0_OT_TERMINATOR, "Prim_AllocPolyF3: tag = terminator");

    Prim_SetXY0(p, 100, 50);
    Prim_SetColor0(p, 255, 128, 0);
    check(p->v0.sx == 100,  "Prim_SetXY0: sx written correctly");
    check(p->v0.sy == 50,   "Prim_SetXY0: sy written correctly");
    check(p->col0.r == 255, "Prim_SetColor0: r written correctly");
    check(p->col0.g == 128, "Prim_SetColor0: g written correctly");
    check(p->col0.b == 0,   "Prim_SetColor0: b written correctly");
    /* Verify cd was NOT clobbered by the color setter. */
    check(p->col0.cd == GPU_CMD_POLY_F3, "Prim_SetColor0: cd preserved");
}

static void test_prim_alloc_poly_g3(void)
{
    Display_Init();
    Display_BeginFrame();

    POLY_G3* p = Prim_AllocPolyG3();
    check(p != (POLY_G3*)0,              "Prim_AllocPolyG3: non-NULL");
    check(p->col0.cd == GPU_CMD_POLY_G3, "Prim_AllocPolyG3: col0.cd = 0x30");
    check(p->col1.cd == 0,               "Prim_AllocPolyG3: col1.cd = 0");
    check(p->col2.cd == 0,               "Prim_AllocPolyG3: col2.cd = 0");
}

static void test_prim_alloc_tile(void)
{
    Display_Init();
    Display_BeginFrame();

    TILE* p = Prim_AllocTile();
    check(p != (TILE*)0,               "Prim_AllocTile: non-NULL");
    check(p->col0.cd == GPU_CMD_TILE,  "Prim_AllocTile: cd = 0x60");

    Prim_SetXY0(p, 10, 20);
    Prim_SetWH(p, 64, 32);
    check(p->v0.sx == 10, "Prim_SetXY0 (TILE): sx");
    check(p->v0.sy == 20, "Prim_SetXY0 (TILE): sy");
    check(p->w == 64,     "Prim_SetWH: w");
    check(p->h == 32,     "Prim_SetWH: h");
}

/* ---------------------------------------------------------------------------
 * Main
 * --------------------------------------------------------------------------- */

int main(void)
{
    printf("=== PSX ENGINE: Core/Renderer Unit Tests ===\n\n");

    printf("-- Ordering Table --\n");
    test_ot_clear();
    test_ot_add_single();
    test_ot_add_chain();
    test_ot_add_multi_z();
    test_ot_get_tail();

    printf("\n-- Packet Pool --\n");
    test_pkt_alloc_basic();
    test_pkt_alloc_overflow();

    printf("\n-- Display BeginFrame / EndFrame --\n");
    test_begin_frame_resets_pool();
    test_begin_frame_clears_ot();
    test_end_frame_flips_buffer();

    printf("\n-- Primitive Allocation --\n");
    test_prim_alloc_poly_f3();
    test_prim_alloc_poly_g3();
    test_prim_alloc_tile();

    printf("\n============================================\n");
    printf("Results: %d passed, %d failed.\n", g_pass, g_fail);

    return (g_fail == 0) ? 0 : 1;
}
