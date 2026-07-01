/*******************************************************************************
 * FILE:         scratch.c
 * MODULE:       Core/Memory
 * DESCRIPTION:  Scratchpad RAM bump allocator implementation.
 *
 *               Manages the 1 KB CPU Data Cache / Scratchpad at 0x1F800000.
 *               This file owns the mutable allocator state (g_scratch_ptr).
 *               All functions operate on that single global — thread safety
 *               is not a concern because the PSX has one CPU and no OS
 *               thread scheduler.
 *
 *               IMPLEMENTATION NOTES:
 *                 - g_scratch_ptr is typed as uint32_t (an address integer).
 *                   We cast to (void*) only at the moment of returning to
 *                   the caller, keeping the arithmetic clean and portable.
 *                 - Alignment is forced via bitmask on every alloc. The mask
 *                   (size + 3) & ~3 rounds up to the nearest 4-byte boundary.
 *                 - The overflow guard compares the post-alloc pointer against
 *                   the hard ceiling (base + size). If exceeded, the pointer
 *                   is NOT advanced and NULL is returned.
 *
 * DEPENDENCIES: scratch.h, mem_map.h
 *******************************************************************************/

#include "scratch.h"
#include "mem_map.h"

/* ---------------------------------------------------------------------------
 * Module-private state.
 * g_scratch_ptr: current allocation frontier, expressed as an address integer.
 * Starts at PSX_SCRATCH_BASE after Scratch_Init().
 * --------------------------------------------------------------------------- */

static uint32_t g_scratch_ptr = 0u;

/* ---------------------------------------------------------------------------
 * Scratch_Init
 * --------------------------------------------------------------------------- */

void Scratch_Init(void)
{
    /* Reset the frontier to the physical base of the scratchpad region.
     * This is idempotent — safe to call multiple times. */
    g_scratch_ptr = PSX_SCRATCH_BASE;
}

/* ---------------------------------------------------------------------------
 * Scratch_Alloc
 * --------------------------------------------------------------------------- */

void* Scratch_Alloc(uint32_t size)
{
    uint32_t aligned_size;
    uint32_t alloc_addr;

    /* Round the requested size up to the next 4-byte boundary.
     * Adding 3 and masking with ~3 is the standard branchless power-of-two
     * ceiling trick. Example: size=5 -> (5+3)&~3 = 8&~3 = 8. */
    aligned_size = (size + 3u) & ~3u;

    /* Guard: reject allocations that would exceed the 1 KB scratchpad.
     * We check the prospective end address against the ceiling constant. */
    if ((g_scratch_ptr + aligned_size) > (PSX_SCRATCH_BASE + PSX_SCRATCH_SIZE)) {
        /* Allocation would overflow. Return NULL and leave state unchanged.
         * The caller must handle this — no fallback allocation happens here. */
        return (void*)0;
    }

    /* Record the start of this allocation block. */
    alloc_addr = g_scratch_ptr;

    /* Advance the frontier past this block. */
    g_scratch_ptr += aligned_size;

    /* Cast the raw integer address to a void pointer for the caller.
     * On MIPS, this is a no-op — addresses are just 32-bit integers. */
    return (void*)alloc_addr;
}

/* ---------------------------------------------------------------------------
 * Scratch_Reset
 * --------------------------------------------------------------------------- */

void Scratch_Reset(void)
{
    /* Rewind to base. All prior allocations are implicitly invalidated.
     * We do NOT zero the scratchpad memory on reset — that would waste
     * CPU cycles. Callers must initialize their allocations before use. */
    g_scratch_ptr = PSX_SCRATCH_BASE;
}

/* ---------------------------------------------------------------------------
 * Scratch_GetUsed
 * --------------------------------------------------------------------------- */

uint32_t Scratch_GetUsed(void)
{
    /* Bytes consumed = current frontier minus base address. */
    return g_scratch_ptr - PSX_SCRATCH_BASE;
}

/* ---------------------------------------------------------------------------
 * Scratch_GetFree
 * --------------------------------------------------------------------------- */

uint32_t Scratch_GetFree(void)
{
    /* Bytes remaining = total capacity minus bytes already allocated. */
    return PSX_SCRATCH_SIZE - Scratch_GetUsed();
}
