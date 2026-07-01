/*******************************************************************************
 * FILE:         display.c
 * MODULE:       Core/Renderer
 * DESCRIPTION:  Double-buffered display environment implementation.
 *               See display.h for the full architecture overview.
 *
 *               This file owns:
 *                 - The two DisplayBuffer instances (A and B).
 *                 - The GPU initialization sequence.
 *                 - The frame begin/end logic (OT DMA, VSync, buffer swap).
 *                 - The packet pool allocator (Pkt_Alloc).
 *
 *               IMPORTANT: On real PSX hardware, Display_Init() must be called
 *               BEFORE any other rendering functions. Calling GPU or DMA
 *               registers before GP1(0x00) (reset) leaves the GPU in an
 *               undefined state from BIOS initialization.
 *
 * DEPENDENCIES: display.h, ot.h, gpu_types.h, mem_map.h
 *******************************************************************************/

#include "display.h"
#include "ot.h"
#include "gpu_types.h"
#include "mem_map.h"

#include <stddef.h> /* for NULL */

/* ---------------------------------------------------------------------------
 * Module globals — the two display buffers and the current index.
 *
 * These are the ONLY mutable globals in the renderer. Everything else
 * operates through pointers into these buffers.
 *
 * g_current_buffer = 0: CPU builds buffer A, GPU displays buffer B.
 * g_current_buffer = 1: CPU builds buffer B, GPU displays buffer A.
 * They flip every Display_EndFrame().
 * --------------------------------------------------------------------------- */

DisplayBuffer g_display_buffers[2];
int           g_current_buffer = 0;

/* ---------------------------------------------------------------------------
 * Private helper: push a DRAWENV to the GPU via GP0 commands.
 *
 * The draw region and draw offset are set by two separate GP0 commands:
 *   GP0(0xE3) + GP0(0xE4): Set drawing area top-left and bottom-right.
 *   GP0(0xE5): Set drawing offset (applied to all vertex screen coords).
 *
 * These are "environment commands" — they do not draw anything, they just
 * configure state for subsequent drawing commands.
 * --------------------------------------------------------------------------- */

static void PushDrawEnv(const DRAWENV* env)
{
    /* GP0(0xE3): Set drawing area top-left corner.
     * Packed as: [command(8)] [Y(10)] [X(10)] in bits [31:0].
     * The drawing area defines the clip rectangle — nothing outside it is drawn. */
    GPU_GP0_Write(0xE3000000u
        | ((uint32_t)(env->draw_y) << 10)
        | ((uint32_t)(env->draw_x)));

    /* GP0(0xE4): Set drawing area bottom-right corner. */
    GPU_GP0_Write(0xE4000000u
        | ((uint32_t)(env->draw_y + env->draw_h - 1u) << 10)
        | ((uint32_t)(env->draw_x + env->draw_w - 1u)));

    /* GP0(0xE5): Set drawing offset.
     * Packed as: [command(8)] [Y_off(11)] [X_off(11)].
     * The GPU adds this offset to every vertex before rasterization, mapping
     * screen-space (0,0) to the correct VRAM draw region top-left. */
    GPU_GP0_Write(0xE5000000u
        | (((uint32_t)(env->off_y) & 0x7FFu) << 11)
        | (((uint32_t)(env->off_x) & 0x7FFu)));
}

/* ---------------------------------------------------------------------------
 * Private helper: push a DISPENV to the GPU via GP1 commands.
 *
 * This changes which region of VRAM is displayed on screen. Calling this
 * at VSync time (from Display_EndFrame) produces the visual buffer swap.
 * --------------------------------------------------------------------------- */

static void PushDispEnv(const DISPENV* env)
{
    /* GP1(0x05): Set display start in VRAM.
     * Packed as: [command(8)] [Y(9)] [X(10)].
     * This tells the GPU where to begin reading VRAM for the TV signal. */
    GPU_GP1_Write(PSX_GP1_DISP_START
        | ((uint32_t)(env->disp_y) << 10)
        | ((uint32_t)(env->disp_x)));

    /* GP1(0x06): Set horizontal display range.
     * 0x0C00200 = standard NTSC 320-pixel active region timing.
     * These magic values come from the PSX hardware reference and set the
     * first and last dot-clock at which video data is output on the HSYNC. */
    GPU_GP1_Write(PSX_GP1_HDISP_RANGE | 0x00C00200u);

    /* GP1(0x07): Set vertical display range.
     * 0x010010 = standard NTSC 240-line active region. */
    GPU_GP1_Write(PSX_GP1_VDISP_RANGE | 0x00010010u);

    /* GP1(0x08): Set display mode.
     * 0x00 = 256x240 NTSC progressive (we use 320 with the H-range above).
     * Full mode word: 320px wide = bit 0 set, 240px = standard. */
    if (!env->is_interlaced) {
        /* 0x08000001 = 320 wide, 240 tall, NTSC, progressive, 15bpp. */
        GPU_GP1_Write(PSX_GP1_DISP_MODE | 0x00000001u);
    } else {
        /* 0x08000025 = 320 wide, 480 interlaced, NTSC, 15bpp. */
        GPU_GP1_Write(PSX_GP1_DISP_MODE | 0x00000025u);
    }
}

/* ---------------------------------------------------------------------------
 * Display_Init
 * --------------------------------------------------------------------------- */

void Display_Init(void)
{
    /* Step 1: Reset the GPU. This clears all GPU state (texture window,
     * drawing area, semi-transparency mode) and stops any running DMA.
     * Must be the very first GPU command issued. */
    GPU_GP1_Write(PSX_GP1_RESET_GPU);

    /* Step 2: Configure DMA direction to "CPU -> GPU" for the OT transfer.
     * GP1(0x04) with value 2 = DMA direction from main RAM to GPU. */
    GPU_GP1_Write(PSX_GP1_DMA_DIR | 0x00000002u);

    /* Step 3: Enable DMA channel 2 (GPU) in the DPCR register.
     * Bits [11:8] of DPCR control channel 2. Setting them to 0b1xxx (bit 3
     * of the nibble = enable) allows the DMA controller to drive the GPU.
     * The value 0x07654321 is the default DPCR with channel 2 enabled. */
#ifndef PSX_HOST_TEST
    PSX_MMIO_REG32(PSX_DMA_DPCR) |= (0x8u << 8);
#endif

    /* Step 4: Initialize Display Buffer A.
     * Buffer A is displayed at VRAM (0,0) and drawn into at VRAM (0,0).
     * When A is the display buffer (g_current_buffer == 1), the TV reads
     * from (0,0). When A is the draw buffer (g_current_buffer == 0), the
     * GPU draws at VRAM (0,0) with screen offset (0,0). */
    g_display_buffers[0].disp.disp_x      = 0;
    g_display_buffers[0].disp.disp_y      = 0;
    g_display_buffers[0].disp.screen_w    = DISPLAY_W;
    g_display_buffers[0].disp.screen_h    = DISPLAY_H;
    g_display_buffers[0].disp.is_pal      = 0;
    g_display_buffers[0].disp.is_interlaced = 0;

    g_display_buffers[0].draw.draw_x      = 0;
    g_display_buffers[0].draw.draw_y      = 0;
    g_display_buffers[0].draw.draw_w      = DISPLAY_W;
    g_display_buffers[0].draw.draw_h      = DISPLAY_H;
    g_display_buffers[0].draw.off_x       = 0;
    g_display_buffers[0].draw.off_y       = 0;
    g_display_buffers[0].draw.clear_color.r = 0;
    g_display_buffers[0].draw.clear_color.g = 0;
    g_display_buffers[0].draw.clear_color.b = 0;

    /* Step 5: Initialize Display Buffer B.
     * Buffer B sits at VRAM (320,0) — immediately right of buffer A.
     * Screen offset (320,0) maps vertex (0,0) to VRAM column 320. */
    g_display_buffers[1].disp.disp_x      = DISPLAY_W; /* 320 */
    g_display_buffers[1].disp.disp_y      = 0;
    g_display_buffers[1].disp.screen_w    = DISPLAY_W;
    g_display_buffers[1].disp.screen_h    = DISPLAY_H;
    g_display_buffers[1].disp.is_pal      = 0;
    g_display_buffers[1].disp.is_interlaced = 0;

    g_display_buffers[1].draw.draw_x      = DISPLAY_W; /* 320 */
    g_display_buffers[1].draw.draw_y      = 0;
    g_display_buffers[1].draw.draw_w      = DISPLAY_W;
    g_display_buffers[1].draw.draw_h      = DISPLAY_H;
    g_display_buffers[1].draw.off_x       = (int16_t)DISPLAY_W; /* 320 */
    g_display_buffers[1].draw.off_y       = 0;
    g_display_buffers[1].draw.clear_color.r = 0;
    g_display_buffers[1].draw.clear_color.g = 0;
    g_display_buffers[1].draw.clear_color.b = 0;

    /* Step 6: Initialize packet pool pointers for both buffers. */
    g_display_buffers[0].pkt_ptr = g_display_buffers[0].pkt_pool;
    g_display_buffers[1].pkt_ptr = g_display_buffers[1].pkt_pool;

    /* Step 7: Clear both OTs to the terminator value. */
    OT_Clear(g_display_buffers[0].ot, OT_ENTRY_COUNT);
    OT_Clear(g_display_buffers[1].ot, OT_ENTRY_COUNT);

    /* Step 8: Push initial display and draw environments.
     * We start with buffer 0 as the draw target, so we push its DRAWENV
     * and buffer 1's DISPENV (the TV shows whatever is in buffer 1 while
     * we build buffer 0 — which is blank/black since we haven't drawn yet). */
    PushDrawEnv(&g_display_buffers[0].draw);
    PushDispEnv(&g_display_buffers[1].disp);

    /* Step 9: Enable display output. Without this, the TV signal is blanked. */
    GPU_GP1_Write(PSX_GP1_DISP_ENABLE | 0x00000000u); /* 0 = enable output */

    g_current_buffer = 0;
}

/* ---------------------------------------------------------------------------
 * Display_BeginFrame
 * --------------------------------------------------------------------------- */

void Display_BeginFrame(void)
{
    DisplayBuffer* buf = &g_display_buffers[g_current_buffer];

    /* Reset the packet pool allocator. All primitives from the PREVIOUS
     * use of this buffer (two frames ago) are implicitly discarded — the
     * GPU has already consumed them, so their memory is safe to reuse. */
    buf->pkt_ptr = buf->pkt_pool;

    /* Clear the OT. All 256 buckets get the terminator value.
     * This must happen before any OT_Add calls this frame. */
    OT_Clear(buf->ot, OT_ENTRY_COUNT);

    /* Push the draw environment to the GPU. This sets the drawing area
     * and offset for all primitives submitted this frame. The GPU applies
     * off_x/off_y to every vertex before rasterizing — this is what maps
     * screen-space (0,0) to the correct VRAM column for this buffer. */
    GPU_WaitIdle();
    PushDrawEnv(&buf->draw);

    /* Clear the screen. This issues a GP0(0x02) fill-rect covering the full
     * draw region with the background color. */
    Display_Clear();
}

/* ---------------------------------------------------------------------------
 * Display_EndFrame
 * --------------------------------------------------------------------------- */

void Display_EndFrame(void)
{
    DisplayBuffer* buf = &g_display_buffers[g_current_buffer];

    /* Step 1: Wait for the GPU to finish drawing the previous frame.
     * The GPU is still working through the OTHER buffer's OT from the
     * previous Display_EndFrame call. We must not start a new DMA
     * until that transfer is completely consumed. */
    DMA2_WaitIdle();
    GPU_WaitIdle();

    /* Step 2: Initiate the DMA transfer of this frame's Ordering Table.
     * The DMA engine reads OT entries starting from the TAIL (highest index)
     * and traverses the linked lists to the GPU. The GPU renders everything
     * it receives — this runs asynchronously while the CPU is free to start
     * building the NEXT frame in the other buffer.
     *
     * DMA2_MADR = physical address of OT tail entry.
     * DMA2_BCR  = 0 (block count is irrelevant in linked-list mode).
     * DMA2_CHCR = 0x01000401 = linked-list from-memory DMA, start bit set. */
#ifndef PSX_HOST_TEST
    PSX_MMIO_REG32(PSX_DMA2_MADR) = (uint32_t)((uintptr_t)OT_GetTail(buf->ot));
    PSX_MMIO_REG32(PSX_DMA2_BCR)  = 0u;
    PSX_MMIO_REG32(PSX_DMA2_CHCR) = PSX_DMA2_CHCR_START;
#endif

    /* Step 3: Wait for VSync. This synchronizes the buffer swap to the TV's
     * vertical blanking interval, preventing tearing. On NTSC, VSync fires
     * at 60Hz. We poll GPU status bit 31 (odd/even interlace field changes
     * every frame) as a VSync indicator in simple polling mode.
     * A more sophisticated implementation would use the VBLANK interrupt. */
#ifndef PSX_HOST_TEST
    {
        /* Wait for the field bit to CHANGE (one full frame period). */
        uint32_t prev_field = PSX_MMIO_REG32(PSX_GPU_GPUSTAT) & PSX_GPUSTAT_ODD_EVEN;
        uint32_t curr_field;
        do {
            curr_field = PSX_MMIO_REG32(PSX_GPU_GPUSTAT) & PSX_GPUSTAT_ODD_EVEN;
        } while (curr_field == prev_field);
    }
#endif

    /* Step 4: Swap the display buffer. Push the DISPENV of the buffer we
     * just submitted to the GPU — the TV now shows that buffer.
     * The other buffer (which we HAVEN'T pushed to the GPU yet) will become
     * the new draw target for the next frame. */
    PushDispEnv(&buf->disp);

    /* Step 5: Flip. Next frame, the CPU builds the other buffer.
     * The GPU will still be drawing THIS buffer's OT asynchronously —
     * we will wait for it again at the TOP of the next Display_EndFrame. */
    g_current_buffer ^= 1;
}

/* ---------------------------------------------------------------------------
 * Display_GetCurrent
 * --------------------------------------------------------------------------- */

DisplayBuffer* Display_GetCurrent(void)
{
    return &g_display_buffers[g_current_buffer];
}

/* ---------------------------------------------------------------------------
 * Display_Clear — Fill the draw buffer with the background color.
 * --------------------------------------------------------------------------- */

void Display_Clear(void)
{
    const DRAWENV*  draw = &g_display_buffers[g_current_buffer].draw;
    const CVECTOR*  col  = &draw->clear_color;

    /* GP0(0x02) Fill Rectangle command.
     * The command takes three words after the command word:
     *   Word 1: RGB color (24-bit, same format as CVECTOR but without cd byte)
     *   Word 2: Top-left corner  (Y<<16 | X)
     *   Word 3: Width and height (H<<16 | W)
     * We write directly to GP0 here rather than building a packet because
     * fill-rect does not go through the OT — it executes immediately. */
    GPU_WaitIdle();
    GPU_GP0_Write(PSX_GP0_FILL_RECT
        | ((uint32_t)col->b << 16)
        | ((uint32_t)col->g << 8)
        | ((uint32_t)col->r));
    GPU_GP0_Write(((uint32_t)draw->draw_y << 16) | (uint32_t)draw->draw_x);
    GPU_GP0_Write(((uint32_t)draw->draw_h << 16) | (uint32_t)draw->draw_w);
}

/* ---------------------------------------------------------------------------
 * Pkt_Alloc — Allocate from the current frame's packet pool.
 * --------------------------------------------------------------------------- */

void* Pkt_Alloc(uint32_t size)
{
    DisplayBuffer* buf  = &g_display_buffers[g_current_buffer];
    void*          ptr;
    uint32_t       used;

    /* Compute how many bytes are already in use. */
    used = (uint32_t)(buf->pkt_ptr - buf->pkt_pool);

    /* Overflow guard: if this allocation would exceed the pool capacity,
     * return NULL. The caller (Prim_AllocXxx) must check for NULL and
     * either skip the primitive or handle the out-of-budget condition. */
    if (used + size > PKT_POOL_SIZE) {
        return (void*)0;
    }

    /* Record the current frontier as the allocated block's start. */
    ptr = (void*)buf->pkt_ptr;

    /* Advance the frontier. No zeroing — callers initialize every field. */
    buf->pkt_ptr += size;

    return ptr;
}
