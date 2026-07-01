/*******************************************************************************
 * FILE:         test_input.cpp
 * MODULE:       Tests
 * DESCRIPTION:  Host-compiled unit tests for the Joypad Driver module.
 *******************************************************************************/

#include <iostream>
#include <cassert>

/* Include the C headers from the runtime */
extern "C" {
    #include "joypad.h"
}

/* ---------------------------------------------------------------------------
 * Test Functions
 * --------------------------------------------------------------------------- */

void test_joypad_initialization()
{
    std::cout << "  [TEST] Joypad Initialization...\n";

    Joypad_Init();
    
    const JoypadState* state1 = Joypad_GetState(0);
    const JoypadState* state2 = Joypad_GetState(1);
    
    assert(state1 != NULL);
    assert(state2 != NULL);
    assert(state1->buttons == 0);
    assert(state1->buttons_pressed == 0);
    assert(state1->buttons_released == 0);

    assert(Joypad_GetState(2) == NULL);

    std::cout << "    Passed.\n";
}

void test_joypad_host_injection()
{
    std::cout << "  [TEST] Joypad Host Injection...\n";

    Joypad_Init();
    
    /* Simulate pressing CROSS and UP on frame 1 */
    Joypad_HostInject(0, PAD_CROSS | PAD_UP);
    Joypad_Update();
    
    const JoypadState* s1 = Joypad_GetState(0);
    assert(s1->buttons == (PAD_CROSS | PAD_UP));
    assert(s1->buttons_pressed == (PAD_CROSS | PAD_UP)); /* Freshly pressed */
    assert(s1->buttons_released == 0);
    
    /* Frame 2: Hold CROSS, release UP, press SQUARE */
    Joypad_HostInject(0, PAD_CROSS | PAD_SQUARE);
    Joypad_Update();
    
    assert(s1->buttons == (PAD_CROSS | PAD_SQUARE));
    assert(s1->buttons_pressed == PAD_SQUARE);
    assert(s1->buttons_released == PAD_UP);
    
    /* Frame 3: Release all */
    Joypad_HostInject(0, 0);
    Joypad_Update();
    
    assert(s1->buttons == 0);
    assert(s1->buttons_pressed == 0);
    assert(s1->buttons_released == (PAD_CROSS | PAD_SQUARE));

    std::cout << "    Passed.\n";
}

/* ---------------------------------------------------------------------------
 * Main
 * --------------------------------------------------------------------------- */

int main()
{
    std::cout << "=== Running Input Unit Tests ===\n";

    test_joypad_initialization();
    test_joypad_host_injection();

    std::cout << "=== All Input Tests Passed ===\n";
    return 0;
}
