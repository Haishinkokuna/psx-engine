/*******************************************************************************
 * FILE:         joypad.h
 * MODULE:       Core/Input
 * DESCRIPTION:  PSX standard digital controller driver interface.
 *
 *               The original 1994 PSX controller contains 14 digital buttons
 *               (plus Select/Start). This interface abstracts the SIO0
 *               serial reading into a clean 16-bit bitmask per port.
 *
 *               By convention, a pressed button is represented by a 0 bit
 *               in hardware, but we invert it in software so that
 *               1 = PRESSED, 0 = RELEASED for easier bitwise AND checks.
 *******************************************************************************/

#ifndef PSX_JOYPAD_H
#define PSX_JOYPAD_H

#include <stdint.h>

/* ---------------------------------------------------------------------------
 * Standard Digital Controller Bitmasks
 * Note: These match the standard hardware bit layout (inverted for ease of use).
 * --------------------------------------------------------------------------- */

#define PAD_SELECT   (1 << 0)
#define PAD_L3       (1 << 1) /* Reserved for DualShock */
#define PAD_R3       (1 << 2) /* Reserved for DualShock */
#define PAD_START    (1 << 3)
#define PAD_UP       (1 << 4)
#define PAD_RIGHT    (1 << 5)
#define PAD_DOWN     (1 << 6)
#define PAD_LEFT     (1 << 7)
#define PAD_L2       (1 << 8)
#define PAD_R2       (1 << 9)
#define PAD_L1       (1 << 10)
#define PAD_R1       (1 << 11)
#define PAD_TRIANGLE (1 << 12)
#define PAD_CIRCLE   (1 << 13)
#define PAD_CROSS    (1 << 14)
#define PAD_SQUARE   (1 << 15)

/* ---------------------------------------------------------------------------
 * JoypadState
 * Represents the current instantaneous state of a controller port.
 * --------------------------------------------------------------------------- */

typedef struct {
    uint16_t buttons;         /* Bitmask of all currently held buttons */
    uint16_t buttons_pressed; /* Buttons pressed EXACTLY this frame */
    uint16_t buttons_released;/* Buttons released EXACTLY this frame */
} JoypadState;

/* ---------------------------------------------------------------------------
 * Joypad API
 * --------------------------------------------------------------------------- */

/* Initialize the serial interface to start polling controllers. */
void Joypad_Init(void);

/* Poll the hardware and update the state structs for Port 1 and Port 2. 
 * Should be called once per frame. */
void Joypad_Update(void);

/* Get the state for a specific port (0 = Port 1, 1 = Port 2). */
const JoypadState* Joypad_GetState(int port);

#ifdef PSX_HOST_TEST
/* Host hook to inject simulated button presses when running the editor. */
void Joypad_HostInject(int port, uint16_t buttons);
#endif

#endif /* PSX_JOYPAD_H */
