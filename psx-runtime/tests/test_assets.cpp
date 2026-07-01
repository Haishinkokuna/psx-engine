/*******************************************************************************
 * FILE:         test_assets.cpp
 * MODULE:       Tests
 * DESCRIPTION:  Host-compiled unit tests for the Asset Pipeline module.
 *               Simulates reading TIM and TMD from raw bytes.
 *******************************************************************************/

#include <iostream>
#include <cassert>

/* Include the C headers from the runtime */
extern "C" {
    #include "tim.h"
    #include "tmd.h"
}

/* ---------------------------------------------------------------------------
 * Test Data
 * --------------------------------------------------------------------------- */

/* Minimal valid TIM file (8-bit with CLUT) */
static uint32_t raw_tim[] = {
    0x00000010, /* Magic */
    0x00000009, /* BPP = 1 (8-bit), Has CLUT = 8. 8|1 = 9 */
    
    /* CLUT Block */
    12,         /* Size of CLUT block in bytes (12 bytes = 3 words) */
    0x00010000, /* X = 0, Y = 1 */
    0x00010002, /* W = 2, H = 1 */
    0x12345678, /* 2 CLUT colors (16-bit each) */
    
    /* Image Block */
    12,         /* Size of Image block in bytes */
    0x00020000, /* X = 0, Y = 2 */
    0x00020002, /* W = 2 (in 16-bit words = 4 bytes), H = 2 */
    0xAABBCCDD, /* Image data row 1 */
    0xEEFF0011  /* Image data row 2 */
};

/* Minimal valid TMD file (1 object, 0 primitives, just testing header logic) */
static uint32_t raw_tmd[] = {
    0x00000041, /* Magic */
    0x00000000, /* Flags */
    0x00000001, /* 1 Object */
    
    /* Object Table (relative to start of object table, which is &raw_tmd[3])
     * Just one object, offset = 4 bytes (the table itself is 1 pointer long) */
    0x00000004, 
    
    /* Object 0 Definition (starts at &raw_tmd[4]) */
    0x0000001C, /* vert_top offset = 28 bytes from &raw_tmd[3] */
    0x00000001, /* 1 vertex */
    0x00000024, /* norm_top offset = 36 bytes */
    0x00000001, /* 1 normal */
    0x0000002C, /* prim_top offset = 44 bytes */
    0x00000000, /* 0 prims for this test */
    0x00000000, /* scale = 0 */
    
    /* Vertex 0 (SVECTOR: x, y, z, pad) */
    0x00100020, /* X=32, Y=16 */
    0x00000030, /* Z=48, pad=0 */
    
    /* Normal 0 (SVECTOR) */
    0x10000000, /* X=0, Y=4096 */
    0x00000000  /* Z=0, pad=0 */
};

/* ---------------------------------------------------------------------------
 * Test Functions
 * --------------------------------------------------------------------------- */

void test_tim_parse()
{
    std::cout << "  [TEST] TIM Parser...\n";

    TIM_Parsed tim;
    int res = TIM_Load(raw_tim, &tim);
    
    assert(res == 1);
    assert(tim.bpp == 1); /* 8-bit */
    
    assert(tim.clut != NULL);
    assert(tim.clut->size == 12);
    assert(tim.clut->w == 2);
    
    assert(tim.image != NULL);
    assert(tim.image->size == 12);
    assert(tim.image->w == 2);

    std::cout << "    Passed.\n";
}

void test_tmd_parse()
{
    std::cout << "  [TEST] TMD Parser (Pointer Re-linking)...\n";

    /* We need to copy raw_tmd because TMD_Load modifies pointers in-place */
    uint32_t tmd_buffer[64];
    for (int i = 0; i < 20; i++) {
        tmd_buffer[i] = raw_tmd[i];
    }
    
    uint32_t num_objs = TMD_Load(tmd_buffer);
    assert(num_objs == 1);
    
    TMD_Object* obj = TMD_GetObject(tmd_buffer, 0);
    assert(obj != NULL);
    assert(obj->num_verts == 1);
    
    /* Verify the pointers were correctly re-linked to absolute memory addresses.
     * vert_top was 28 bytes from the object table (&tmd_buffer[3]).
     * So vert_top should point to &tmd_buffer[3] + 28 bytes = &tmd_buffer[10] */
    
    SVECTOR* verts = (SVECTOR*)(uintptr_t)obj->vert_top;
    assert(verts->vx == 32);
    assert(verts->vy == 16);
    assert(verts->vz == 48);

    std::cout << "    Passed.\n";
}

/* ---------------------------------------------------------------------------
 * Main
 * --------------------------------------------------------------------------- */

int main()
{
    std::cout << "=== Running Assets Unit Tests ===\n";

    test_tim_parse();
    test_tmd_parse();

    std::cout << "=== All Assets Tests Passed ===\n";
    return 0;
}
