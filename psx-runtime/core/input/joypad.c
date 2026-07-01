/*******************************************************************************
 * FILE:         joypad.c
 * MODULE:       Core/Input
 * DESCRIPTION:  PSX standard digital controller driver implementation.
 *
 *               Includes a host-test path to allow the editor/MCP server
 *               to inject fake button presses into the engine.
 *******************************************************************************/

#include "joypad.h"
#include <stddef.h> /* for NULL */

/* ---------------------------------------------------------------------------
 * State Storage
 * --------------------------------------------------------------------------- */

static JoypadState g_pads[2];

#ifdef PSX_HOST_TEST
/* Host injection buffer */
static uint16_t g_host_injected_buttons[2];
#endif

/* ---------------------------------------------------------------------------
 * Internal Helper
 * --------------------------------------------------------------------------- */

static void UpdatePort(int port, uint16_t current_buttons)
{
    /* Calculate edge triggers */
    uint16_t changed = g_pads[port].buttons ^ current_buttons;
    g_pads[port].buttons_pressed  = changed & current_buttons;
    g_pads[port].buttons_released = changed & ~current_buttons;
    
    /* Store current state */
    g_pads[port].buttons = current_buttons;
}

/* ---------------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------------- */

void Joypad_Init(void)
{
    g_pads[0].buttons = 0;
    g_pads[0].buttons_pressed = 0;
    g_pads[0].buttons_released = 0;

    g_pads[1].buttons = 0;
    g_pads[1].buttons_pressed = 0;
    g_pads[1].buttons_released = 0;

#ifdef PSX_HOST_TEST
    g_host_injected_buttons[0] = 0;
    g_host_injected_buttons[1] = 0;
#else
    /* Future hardware SIO0 initialization goes here */
#endif
}

void Joypad_Update(void)
{
#ifdef PSX_HOST_TEST
    /* On host, pull from the injection buffer */
    UpdatePort(0, g_host_injected_buttons[0]);
    UpdatePort(1, g_host_injected_buttons[1]);
#else
    /* Future hardware SIO0 polling goes here.
     * The hardware protocol requires sending a command byte (0x01)
     * and clocking in the 16-bit response. */
    
    /* uint16_t hw_pad1 = read_sio0_port1(); */
    /* uint16_t hw_pad2 = read_sio0_port2(); */
    
    /* Remember: hardware returns 0 for pressed. We invert it:
     * UpdatePort(0, ~hw_pad1); 
     */
#endif
}

const JoypadState* Joypad_GetState(int port)
{
    if (port < 0 || port > 1) {
        return NULL;
    }
    return &g_pads[port];
}

#ifdef PSX_HOST_TEST
void Joypad_HostInject(int port, uint16_t buttons)
{
    if (port >= 0 && port <= 1) {
        g_host_injected_buttons[port] = buttons;
    }
}
#endif
