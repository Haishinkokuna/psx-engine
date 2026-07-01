/*******************************************************************************
 * FILE:         display.h
 * MODULE:       Core/Renderer
 * DESCRIPTION:  Double-buffered display environment management for the PSX.
 *
 *               DOUBLE BUFFERING OVERVIEW:
 *               The PSX renders into VRAM using two independent frame buffers
 *               (A and B). While the GPU is drawing into buffer B, the display
 *               hardware is reading buffer A and sending it to the TV. Each
 *               VSync (60Hz NTSC / 50Hz PAL), the buffers swap roles. This
 *               eliminates tearing — the TV never reads a buffer that is being
 *               written to simultaneously.
 *
 *               VRAM LAYOUT (320x240 double-buffered):
 *
 *                 VRAM (1024 x 512 pixels @ 16bpp)
 *                 X=0      X=320    X=640   X=1023
 *                 +--------+--------+--------+
 *                 | Buf A  | Buf B  | CLUT / | Y=0
 *                 | 320x240| 320x240| TPages |
 *                 |        |        |        | Y=239
 *                 +--------+--------+--------+
 *                 |   Texture Pages (free)   | Y=240
 *                 |                          | Y=511
 *                 +--------+--------+--------+
 *
 *               Buffer A occupies VRAM (0,0)-(319,239).
 *               Buffer B occupies VRAM (320,0)-(639,239).
 *               Texture pages begin at X=640 or Y=240.
 *
 *               DISPLAY ENVIRONMENT (DISPENV):
 *               Tells the GPU which VRAM region to send to the TV and at
 *               what screen offset/resolution.
 *
 *               DRAW ENVIRONMENT (DRAWENV):
 *               Tells the GPU which VRAM region to draw into and where
 *               the (0,0) drawing origin is mapped in VRAM.
 *
 *               PACKET POOL:
 *               Each DisplayBuffer owns a flat byte array (PKT_POOL_SIZE bytes)
 *               that acts as a per-frame linear allocator. Pkt_Alloc() advances
 *               a pointer through this pool. At the start of each frame, the
 *               pointer is reset to the beginning of the pool (all previous
 *               frame's packets are implicitly discarded). This makes allocation
 *               O(1) and avoids fragmentation entirely.
 *
 *               PKT_POOL_SIZE = 32 KB. Approximate capacity:
 *                 ~1600 POLY_F3  (20 bytes each)
 *                 ~1000 POLY_G3  (32 bytes each)
 *                 ~ 900 POLY_FT3 (36 bytes each)
 *               This is generous for a PSX title, which typically renders
 *               200-800 polygons per frame in a 3D scene.
 *
 * DEPENDENCIES: ot.h, gpu_types.h, mem_map.h, <stdint.h>
 *******************************************************************************/

#ifndef PSX_DISPLAY_H
#define PSX_DISPLAY_H

#include <stdint.h>
#include "ot.h"
#include "gpu_types.h"
#include "mem_map.h"

/* ---------------------------------------------------------------------------
 * Display configuration constants
 * --------------------------------------------------------------------------- */

/* Packet pool size per buffer. 32 KB. Override with -DPKT_POOL_SIZE=N. */
#ifndef PKT_POOL_SIZE
#define PKT_POOL_SIZE   ((uint32_t)0x8000U)
#endif

/* Display resolution. The PSX supports 256/320/368/512/640 wide modes.
 * 320x240 is standard for most 3D games (NTSC). */
#define DISPLAY_W       PSX_DISPLAY_WIDTH
#define DISPLAY_H       PSX_DISPLAY_HEIGHT

/* ---------------------------------------------------------------------------
 * DISPENV — Display environment.
 *
 * Configures the GPU's display output: which VRAM region is the "screen" and
 * how it maps to the TV output. Set via GP1 commands.
 *
 * FIELD DESCRIPTIONS:
 *   disp_x, disp_y: Top-left corner of the display region in VRAM (pixels).
 *   screen_w, screen_h: Displayed resolution in pixels.
 *   is_pal: 0 = NTSC (60Hz), 1 = PAL (50Hz). Must match hardware region.
 *   is_interlaced: 0 = progressive (320x240), 1 = interlaced (320x480).
 *     Interlaced doubles vertical resolution but flickers on CRT. Most PSX
 *     games use progressive (non-interlaced) for stability.
 * --------------------------------------------------------------------------- */

typedef struct {
    uint16_t disp_x;       /* VRAM X of the top-left of the display region */
    uint16_t disp_y;       /* VRAM Y of the top-left of the display region */
    uint16_t screen_w;     /* Display width  (pixels on screen)             */
    uint16_t screen_h;     /* Display height (pixels on screen)             */
    uint8_t  is_pal;       /* 0 = NTSC, 1 = PAL                             */
    uint8_t  is_interlaced;/* 0 = progressive, 1 = interlaced               */
    uint16_t pad;          /* Alignment padding                             */
} DISPENV;

/* ---------------------------------------------------------------------------
 * DRAWENV — Draw environment.
 *
 * Configures the GPU's drawing context: where in VRAM polygons are rasterized,
 * what the drawing offset is, and the clipping rectangle.
 *
 * FIELD DESCRIPTIONS:
 *   draw_x, draw_y: Top-left of the draw region in VRAM (pixels).
 *   draw_w, draw_h: Width and height of the draw region (defines clip rect).
 *   off_x, off_y:  Drawing offset applied to all vertex coordinates before
 *     rasterization. Set to (draw_x, draw_y) so that vertex coordinates
 *     passed in screen space (0,0)-(319,239) map to the correct VRAM region.
 *   clear_color: Background color for GP0 fill-rect clear commands.
 * --------------------------------------------------------------------------- */

typedef struct {
    uint16_t draw_x;       /* VRAM X of the drawing region top-left */
    uint16_t draw_y;       /* VRAM Y of the drawing region top-left */
    uint16_t draw_w;       /* Drawing region width  (clip rect right edge)  */
    uint16_t draw_h;       /* Drawing region height (clip rect bottom edge) */
    int16_t  off_x;        /* X offset applied to all vertex screen coords  */
    int16_t  off_y;        /* Y offset applied to all vertex screen coords  */
    CVECTOR  clear_color;  /* Background fill color (used in Display_Clear) */
} DRAWENV;

/* ---------------------------------------------------------------------------
 * DisplayBuffer — One complete rendering buffer (half of the double-buffer).
 *
 * A DisplayBuffer is everything needed to render one frame:
 *   - DISPENV: which VRAM region the TV reads.
 *   - DRAWENV: which VRAM region the GPU draws into.
 *   - OT:      the depth-sorted primitive list for this frame.
 *   - pkt_pool: the flat byte buffer for all GPU packets this frame.
 *   - pkt_ptr:  allocation frontier into pkt_pool.
 * --------------------------------------------------------------------------- */

typedef struct {
    DISPENV  disp;               /* Display configuration (TV output region)     */
    DRAWENV  draw;               /* Draw configuration (GPU draw region + clip)  */
    OT_Tag   ot[OT_ENTRY_COUNT]; /* Ordering Table — 256 depth buckets           */
    uint8_t  pkt_pool[PKT_POOL_SIZE]; /* Per-frame GPU packet memory pool        */
    uint8_t* pkt_ptr;            /* Current allocation frontier in pkt_pool      */
} DisplayBuffer;

/* ---------------------------------------------------------------------------
 * Global display state (defined in display.c)
 *
 * g_display_buffers[2]: The two display buffers (A and B).
 * g_current_buffer:     Index (0 or 1) of the buffer the CPU is BUILDING.
 *   The GPU is simultaneously drawing the OTHER buffer (1-g_current_buffer).
 * --------------------------------------------------------------------------- */

extern DisplayBuffer g_display_buffers[2];
extern int           g_current_buffer;

/* ---------------------------------------------------------------------------
 * Display_Init — Initialize the PSX GPU and set up both display buffers.
 *
 * SEQUENCE:
 *   1. GP1(0x00) — Reset GPU (clears all state, stops any in-progress DMA).
 *   2. GP1(0x08) — Set display mode: 320x240 NTSC progressive.
 *   3. GP1(0x06/0x07) — Set H and V display ranges for NTSC timing.
 *   4. Initialize DISPENV and DRAWENV for both buffers A (0,0) and B (320,0).
 *   5. Call Display_ApplyEnv() to push the initial environments to the GPU.
 *   6. GP1(0x03) — Enable display output.
 *
 * Must be called exactly once at program startup, before any rendering.
 * --------------------------------------------------------------------------- */

void Display_Init(void);

/* ---------------------------------------------------------------------------
 * Display_BeginFrame — Prepare the current buffer for a new frame.
 *
 * SEQUENCE:
 *   1. Clear the OT (OT_Clear).
 *   2. Reset pkt_ptr to the start of pkt_pool.
 *   3. Push DRAWENV to the GPU (sets drawing region and offset).
 *   4. Optionally clear the screen with a GP0 fill-rect command.
 *
 * Called at the top of the game loop, before any OT_Add calls.
 * --------------------------------------------------------------------------- */

void Display_BeginFrame(void);

/* ---------------------------------------------------------------------------
 * Display_EndFrame — Submit the completed OT to the GPU and swap buffers.
 *
 * SEQUENCE:
 *   1. Wait for the GPU to finish drawing the PREVIOUS frame (GPU idle poll).
 *   2. Initiate DMA transfer of the current OT to the GPU (DMA channel 2).
 *   3. Wait for VSync (H-blank counter or GPU interlace field).
 *   4. Push the DISPENV to the GPU (swaps which buffer is on screen).
 *   5. Flip g_current_buffer (0->1 or 1->0) for the next frame.
 *
 * Must be called exactly once per frame, after all OT_Add calls.
 * --------------------------------------------------------------------------- */

void Display_EndFrame(void);

/* ---------------------------------------------------------------------------
 * Display_GetCurrent — Return the buffer currently being BUILT by the CPU.
 *
 * Game code should call this to get a pointer to the active DisplayBuffer
 * for the current frame. Do NOT cache this pointer across frames — it flips
 * every frame.
 *
 * @return Pointer to the active DisplayBuffer.
 * --------------------------------------------------------------------------- */

DisplayBuffer* Display_GetCurrent(void);

/* ---------------------------------------------------------------------------
 * Display_Clear — Fill the current draw buffer with the DRAWENV clear color.
 *
 * Issues a GP0 fill-rect command covering the entire draw region. This is
 * faster than clearing with a POLY_F4 — the fill-rect command bypasses the
 * normal rasterizer pipeline and writes directly to VRAM.
 *
 * Call this during Display_BeginFrame or after it, before any OT_Add calls.
 * --------------------------------------------------------------------------- */

void Display_Clear(void);

/* ---------------------------------------------------------------------------
 * Pkt_Alloc — Allocate 'size' bytes from the current frame's packet pool.
 *
 * This is the fastest possible allocator — a pointer advance with no
 * branching in the common case. The overflow check ensures that if game
 * code submits too many primitives, the pool returns NULL rather than
 * overrunning into adjacent memory.
 *
 * Pool memory is NOT zeroed on allocation. Callers (Prim_AllocXxx)
 * must initialize every field before calling OT_Add.
 *
 * @param size  Bytes to allocate. Must be a multiple of 4 (4-byte alignment).
 * @return      Pointer to the allocated block, or NULL if pool is exhausted.
 * --------------------------------------------------------------------------- */

void* Pkt_Alloc(uint32_t size);

/* ---------------------------------------------------------------------------
 * Private: GPU register write helpers (used only in display.c)
 * These inline wrappers exist to make the GP0/GP1 write sequences readable.
 * They write directly to the MMIO registers defined in mem_map.h.
 * --------------------------------------------------------------------------- */

#ifdef PSX_HOST_TEST
/* On the host test build, MMIO writes are no-ops. The renderer logic
 * (OT, packet allocation, buffer flip) is still exercised; only the
 * actual GPU writes are suppressed since we have no PSX hardware. */
#define GPU_GP0_Write(cmd)  ((void)(cmd))
#define GPU_GP1_Write(cmd)  ((void)(cmd))
#define DMA2_WaitIdle()     ((void)0)
#define GPU_WaitIdle()      ((void)0)
#else
#include "mem_map.h"
#define GPU_GP0_Write(cmd)  (PSX_MMIO_REG32(PSX_GPU_GP0) = (uint32_t)(cmd))
#define GPU_GP1_Write(cmd)  (PSX_MMIO_REG32(PSX_GPU_GP1) = (uint32_t)(cmd))
/* Poll CHCR bit 24 until the DMA engine reports "not busy". */
#define DMA2_WaitIdle()     \
    do {} while (PSX_MMIO_REG32(PSX_DMA2_CHCR) & PSX_DMA2_CHCR_BUSY)
/* Poll GPUSTAT bit 28 until the GPU is ready for a new command. */
#define GPU_WaitIdle()      \
    do {} while (!(PSX_MMIO_REG32(PSX_GPU_GPUSTAT) & PSX_GPUSTAT_READY_CMD))
#endif

#endif /* PSX_DISPLAY_H */
