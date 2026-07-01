/*******************************************************************************
 * FILE:         scratch.h
 * MODULE:       Core/Memory
 * DESCRIPTION:  Scratchpad RAM bump allocator API.
 *
 *               The PSX CPU Data Cache (1 KB at 0x1F800000) is repurposed as
 *               fast Scratchpad RAM. Access is single-cycle — no DRAM latency,
 *               no bus arbitration, no cache miss. It is the fastest memory
 *               available on the hardware.
 *
 *               ALLOCATOR DESIGN — Linear / Bump Allocator:
 *                 A bump allocator maintains a single pointer. Allocation
 *                 advances the pointer by the requested size. There is NO
 *                 individual free — the only supported operation is a full
 *                 reset (rewind to the base address).
 *
 *                 This is perfect for per-frame usage:
 *                   1. At the start of each frame, call Scratch_Reset().
 *                   2. Allocate temporary buffers freely during frame work.
 *                   3. Data is implicitly discarded at the next Reset().
 *
 *               ALIGNMENT:
 *                 All allocations are aligned to 4 bytes via bitmask.
 *                 This prevents bus errors on 32-bit MIPS loads/stores.
 *                 Requested size is padded up to the next multiple of 4:
 *                   aligned_size = (size + 3) & ~3
 *
 *               CAPACITY: 1024 bytes total. No waste — Scratch_Alloc returns
 *               NULL if the request would overflow. Always check the return.
 *
 * DEPENDENCIES: mem_map.h, <stdint.h>
 *******************************************************************************/

#ifndef PSX_SCRATCH_H
#define PSX_SCRATCH_H

#include <stdint.h>
#include "mem_map.h"

/* ---------------------------------------------------------------------------
 * Scratch_Init — Initialize the scratchpad allocator.
 *
 * Must be called once before any Scratch_Alloc calls. Resets the internal
 * pointer to the scratchpad base address. Safe to call multiple times.
 * --------------------------------------------------------------------------- */

void Scratch_Init(void);

/* ---------------------------------------------------------------------------
 * Scratch_Alloc — Allocate 'size' bytes from scratchpad RAM.
 *
 * The returned pointer is 4-byte aligned. The actual allocated block may be
 * slightly larger than 'size' due to alignment padding — this is invisible
 * to the caller but accounts for the full capacity budget.
 *
 * @param size  Number of bytes to allocate (will be padded to 4-byte multiple)
 * @return      Pointer into scratchpad memory, or NULL if capacity exceeded.
 *              NEVER dereference NULL — check the return value.
 * --------------------------------------------------------------------------- */

void* Scratch_Alloc(uint32_t size);

/* ---------------------------------------------------------------------------
 * Scratch_Reset — Rewind the allocator to the base address.
 *
 * All previously allocated scratchpad memory is implicitly released.
 * Call this at the start of each frame (or when the current scratchpad
 * scope ends) to reclaim the full 1 KB for new allocations.
 *
 * CAUTION: Any pointers returned by Scratch_Alloc before this call are
 * now dangling. Do not read or write through them after Scratch_Reset.
 * --------------------------------------------------------------------------- */

void Scratch_Reset(void);

/* ---------------------------------------------------------------------------
 * Scratch_GetUsed — Return the number of bytes currently allocated.
 *
 * Useful for profiling and debugging — log this at frame end to understand
 * peak scratchpad usage without needing a separate profiling system.
 * --------------------------------------------------------------------------- */

uint32_t Scratch_GetUsed(void);

/* ---------------------------------------------------------------------------
 * Scratch_GetFree — Return the number of bytes still available.
 * --------------------------------------------------------------------------- */

uint32_t Scratch_GetFree(void);

#endif /* PSX_SCRATCH_H */
