/*******************************************************************************
 * FILE:         test_audio.cpp
 * MODULE:       Tests
 * DESCRIPTION:  Host-compiled unit tests for the Audio (SPU) module.
 *******************************************************************************/

#include <iostream>
#include <cassert>

/* Include the C headers from the runtime */
extern "C" {
    #include "spu.h"
}

/* ---------------------------------------------------------------------------
 * Test Data
 * --------------------------------------------------------------------------- */

/* Minimal valid VAG file */
static uint32_t raw_vag[] = {
    0x56414770, /* "VAGp" */
    0x00000000, /* Version */
    0x00000000, /* Reserved */
    0x00000800, /* Data size: 2048 bytes (Big Endian) */
    0x0000AC44, /* Sample rate: 44100 (Big Endian) */
    0x00000000, 0x00000000, 0x00000000, /* Reserved2 */
    0x54455354, 0x00000000, 0x00000000, 0x00000000 /* Name: "TEST" */
    /* ... 2048 bytes of ADPCM would follow ... */
};

/* ---------------------------------------------------------------------------
 * Test Functions
 * --------------------------------------------------------------------------- */

void test_spu_initialization()
{
    std::cout << "  [TEST] SPU Initialization & Reset...\n";

    SPU_Init();
    
    /* Play 1 voice to ensure it gets allocated */
    int v = SPU_PlayVoice(0x1000, 0x1000, 0x3FFF, 0x3FFF);
    assert(v == 0);
    
    /* Stop all, then allocate again, should be voice 0 */
    SPU_StopAll();
    v = SPU_PlayVoice(0x1000, 0x1000, 0x3FFF, 0x3FFF);
    assert(v == 0);

    std::cout << "    Passed.\n";
}

void test_spu_voice_allocation()
{
    std::cout << "  [TEST] SPU Voice Allocation...\n";

    SPU_Init();
    
    /* Allocate all 24 voices */
    for (int i = 0; i < SPU_NUM_VOICES; i++) {
        int v = SPU_PlayVoice(0x1000 + (i * 8), 0x1000, 0x3FFF, 0x3FFF);
        assert(v == i);
    }
    
    /* Try to allocate 25th voice, should fail */
    int overflow = SPU_PlayVoice(0x2000, 0x1000, 0x3FFF, 0x3FFF);
    assert(overflow == -1);
    
    /* Free voice 12, try allocating again */
    SPU_StopVoice(12);
    int new_voice = SPU_PlayVoice(0x2000, 0x1000, 0x3FFF, 0x3FFF);
    assert(new_voice == 12);

    std::cout << "    Passed.\n";
}

void test_spu_vag_loading()
{
    std::cout << "  [TEST] SPU VAG Loading (RAM Allocation)...\n";

    SPU_Init();
    
    /* First file should load at 0x1000 */
    uint32_t addr1 = SPU_LoadVAG(raw_vag);
    assert(addr1 == 0x1000);
    
    /* Because raw_vag data_size is 2048 (0x800), next file should be at 0x1800 */
    uint32_t addr2 = SPU_LoadVAG(raw_vag);
    assert(addr2 == 0x1800);
    
    /* And the next at 0x2000 */
    uint32_t addr3 = SPU_LoadVAG(raw_vag);
    assert(addr3 == 0x2000);

    std::cout << "    Passed.\n";
}

/* ---------------------------------------------------------------------------
 * Main
 * --------------------------------------------------------------------------- */

int main()
{
    std::cout << "=== Running Audio Unit Tests ===\n";

    test_spu_initialization();
    test_spu_voice_allocation();
    test_spu_vag_loading();

    std::cout << "=== All Audio Tests Passed ===\n";
    return 0;
}
