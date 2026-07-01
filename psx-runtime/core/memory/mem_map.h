/*******************************************************************************
 * FILE:         mem_map.h
 * MODULE:       Core/Memory
 * DESCRIPTION:  PlayStation 1 hardware memory map constants. Every physical
 *               address used anywhere in the runtime is defined here and
 *               nowhere else. This is the single source of truth for the
 *               memory layout — no magic numbers anywhere else in the codebase.
 *
 *               PSX Memory Layout Summary:
 *
 *               0x00000000 - 0x001FFFFF  Main RAM (2 MB)
 *                 0x00000000 - 0x0000FFFF  BIOS kernel + exception vectors
 *                 0x00010000 - 0x001F0000  Usable heap space (~1.875 MB)
 *                 0x001F0000 - 0x001FFFFF  Stack (grows down from top)
 *
 *               0x1F000000 - 0x1F7FFFFF  Hardware I/O registers (peripherals)
 *               0x1F800000 - 0x1F8003FF  CPU Data Cache / Scratchpad RAM (1 KB)
 *               0x1F801000 - 0x1F803FFF  I/O Ports (DMA, GPU, SPU, CDROM, etc.)
 *               0x1FC00000 - 0x1FC7FFFF  BIOS ROM (512 KB, read-only)
 *
 *               VRAM is accessed via the GPU I/O ports, NOT mapped to CPU
 *               address space directly. VRAM constants here refer to the
 *               GPU-internal coordinate system (0,0) to (1023,511).
 *
 * DEPENDENCIES: <stdint.h>
 *******************************************************************************/

#ifndef PSX_MEM_MAP_H
#define PSX_MEM_MAP_H

#include <stdint.h>

/* ---------------------------------------------------------------------------
 * Main System RAM
 * The PSX has 2 MB of general-purpose DRAM (KSEG0 cached alias).
 * The uncached alias (KSEG1) starts at 0xA0000000 and is used for DMA
 * transfers where cache coherency would otherwise cause stale reads.
 * --------------------------------------------------------------------------- */

#define PSX_RAM_BASE           ((uint32_t)0x80000000U) /* KSEG0 cached base   */
#define PSX_RAM_BASE_UNCACHED  ((uint32_t)0xA0000000U) /* KSEG1 uncached base */
#define PSX_RAM_SIZE           ((uint32_t)0x00200000U) /* 2 MB = 2,097,152 B  */

/* The kernel occupies the first 64 KB. Never write here from game code.
 * Exception vectors (0x00000080 etc.) also live in this region. */
#define PSX_KERNEL_RESERVE     ((uint32_t)0x00010000U) /* 64 KB kernel region */

/* Safe heap base: immediately above the kernel reserve.
 * Stack grows downward from near the top of RAM, so leave a buffer. */
#define PSX_HEAP_BASE          (PSX_RAM_BASE + PSX_KERNEL_RESERVE)

/* Reserve 64 KB at the top of RAM for the stack. The default stack pointer
 * in PSn00bSDK is set to 0x801FFF00, so we conservatively give it 64 KB.
 * Adjust PSX_STACK_RESERVE if the game's call stack is deeper. */
#define PSX_STACK_RESERVE      ((uint32_t)0x00010000U) /* 64 KB stack reserve */

/* Usable heap bytes = Total RAM - kernel reserve - stack reserve */
#define PSX_HEAP_SIZE          (PSX_RAM_SIZE - PSX_KERNEL_RESERVE - PSX_STACK_RESERVE)

/* ---------------------------------------------------------------------------
 * Scratchpad RAM (CPU Data Cache repurposed as fast scratchpad)
 * Located at a fixed physical address. Access is single-cycle — no bus
 * arbitration, no cache miss penalty. This is the fastest memory on the PSX.
 * Use it for: intermediate transform matrices, sorting scratch buffers,
 * per-frame temporary data that does NOT need to persist between frames.
 * --------------------------------------------------------------------------- */

#define PSX_SCRATCH_BASE       ((uint32_t)0x1F800000U) /* Physical address    */
#define PSX_SCRATCH_SIZE       ((uint32_t)0x00000400U) /* 1 KB = 1,024 bytes  */

/* ---------------------------------------------------------------------------
 * VRAM (Video RAM — internal to GPU, not CPU-addressable)
 * These constants describe the GPU's internal coordinate system.
 * VRAM is a flat 1 MB frame buffer addressed as a 1024x512 grid of 16bpp
 * pixels. Textures, CLUTs, and frame buffers all live here.
 * --------------------------------------------------------------------------- */

#define PSX_VRAM_WIDTH         ((uint32_t)1024U)   /* VRAM width in pixels   */
#define PSX_VRAM_HEIGHT        ((uint32_t)512U)    /* VRAM height in pixels  */
#define PSX_VRAM_SIZE          ((uint32_t)0x00100000U) /* 1 MB                */
#define PSX_VRAM_BPP           16                  /* Bits per pixel (15bpp + mask) */

/* Display buffer layout (standard double-buffered 320x240 setup):
 * Frame buffer A: VRAM origin (0,0)
 * Frame buffer B: VRAM (0,256) — below frame A  */
#define PSX_DISPLAY_WIDTH      320U
#define PSX_DISPLAY_HEIGHT     240U
#define PSX_FRAMEBUF_A_X       0U
#define PSX_FRAMEBUF_A_Y       0U
#define PSX_FRAMEBUF_B_X       0U
#define PSX_FRAMEBUF_B_Y       256U

/* ---------------------------------------------------------------------------
 * I/O Port Base Addresses (read/write these via volatile uint32_t*)
 * These are documented in the PSXTEK / No$Psx technical reference.
 * Listed here for completeness; individual subsystem headers (GPU, SPU, etc.)
 * will define the full register sets within these ranges.
 * --------------------------------------------------------------------------- */

#define PSX_IO_BASE            ((uint32_t)0x1F801000U)
#define PSX_GPU_BASE           ((uint32_t)0x1F801810U) /* GP0 / GP1 commands  */
#define PSX_DMA_BASE           ((uint32_t)0x1F801080U) /* DMA controller      */
#define PSX_CDROM_BASE         ((uint32_t)0x1F801800U) /* CD-ROM interface    */
#define PSX_SPU_BASE           ((uint32_t)0x1F801C00U) /* Sound Processing    */
#define PSX_TIMER_BASE         ((uint32_t)0x1F801100U) /* Hardware timers     */
#define PSX_JOY_BASE           ((uint32_t)0x1F801040U) /* Joypad / SIO        */

/* ---------------------------------------------------------------------------
 * BIOS ROM
 * Read-only 512 KB. Never write here. Referenced only for documentation.
 * --------------------------------------------------------------------------- */

#define PSX_BIOS_BASE          ((uint32_t)0x1FC00000U)
#define PSX_BIOS_SIZE          ((uint32_t)0x00080000U) /* 512 KB              */

/* ---------------------------------------------------------------------------
 * Helper macro: cast an address constant to a volatile pointer for MMIO use.
 * Volatile ensures the compiler does not optimize out hardware register reads.
 * --------------------------------------------------------------------------- */

#define PSX_MMIO_REG32(addr)   (*((volatile uint32_t*)(addr)))
#define PSX_MMIO_REG16(addr)   (*((volatile uint16_t*)(addr)))
#define PSX_MMIO_REG8(addr)    (*((volatile uint8_t*)(addr)))

#endif /* PSX_MEM_MAP_H */
