/*******************************************************************************
 * FILE:         ot.h
 * MODULE:       Core/Renderer
 * DESCRIPTION:  Ordering Table (OT) — the PSX GPU's depth-sorting mechanism.
 *
 *               WHAT IS AN ORDERING TABLE?
 *               The PSX GPU has no Z-buffer. Depth sorting is the programmer's
 *               responsibility. The Ordering Table is an array of 32-bit "link
 *               words" — one per depth bucket. Each bucket is the head of a
 *               singly-linked list of GPU primitive packets. Deeper (farther)
 *               primitives go into higher-indexed buckets; closer primitives
 *               go into lower-indexed buckets.
 *
 *               When Display_EndFrame() fires the DMA transfer, the GPU reads
 *               the OT from the LAST entry (index OT_ENTRY_COUNT-1, i.e., the
 *               farthest) toward index 0 (the nearest). This implements a
 *               back-to-front painter's algorithm: far objects are drawn first
 *               and near objects paint over them.
 *
 *               OT LINK WORD FORMAT:
 *               Each uint32_t OT entry holds the 24-bit physical address of
 *               the first primitive packet in that bucket (shifted right by 0,
 *               since the PSX bus addresses are 32-bit and the upper byte is
 *               always 0x00 in the KSEG1 uncached address range used for DMA).
 *               The value 0x00FFFFFF is the terminator — it signals the GPU
 *               that this bucket's list is empty (or the full OT is done).
 *
 *               OT_Add OPERATION:
 *               1. The primitive's 'tag' word currently holds 0x00FFFFFF.
 *               2. We write the current bucket head address into 'tag' (so
 *                  the primitive points to whoever was previously at this Z).
 *               3. We write the primitive's address into the bucket (so the
 *                  bucket now points to the new primitive as its list head).
 *               This is an O(1) prepend. Multiple primitives at the same Z
 *               depth are rendered in LIFO order — last added draws on top.
 *
 *               DEPTH CALCULATION:
 *               The Z value passed to OT_Add should be in the range
 *               [0, OT_ENTRY_COUNT-1]. It is typically derived from the GTE's
 *               average projected Z of a polygon's vertices after perspective
 *               divide. The caller is responsible for clamping.
 *
 *               SCRATCHPAD USAGE:
 *               A 256-entry OT at uint32_t = 1,024 bytes — exactly fits the
 *               1 KB scratchpad. In performance-critical code, the OT can be
 *               placed in scratchpad for faster CPU access during insertion.
 *               This header provides OT_ENTRY_SIZE for that calculation.
 *
 * DEPENDENCIES: <stdint.h>, mem_map.h (for PSX_GP0_OT_TERMINATOR)
 *******************************************************************************/

#ifndef PSX_OT_H
#define PSX_OT_H

#include <stdint.h>
#include "mem_map.h"

/* ---------------------------------------------------------------------------
 * OT Configuration
 *
 * OT_ENTRY_COUNT: Number of depth buckets. 256 is the standard for PSX games.
 * A power-of-two count is not required by the hardware but is conventional.
 * Override at compile time with -DOT_ENTRY_COUNT=N if a scene requires
 * coarser (64) or finer (512) depth resolution.
 *
 * OT_ENTRY_SIZE: Size of the full OT array in bytes.
 * A 256-entry OT fits exactly in the 1KB scratchpad — this is intentional.
 * --------------------------------------------------------------------------- */

#ifndef OT_ENTRY_COUNT
#define OT_ENTRY_COUNT  256
#endif

#define OT_ENTRY_SIZE   ((uint32_t)(OT_ENTRY_COUNT * sizeof(uint32_t)))

/* ---------------------------------------------------------------------------
 * OT_Tag — The type of a single Ordering Table entry.
 *
 * The value stored is the raw 24-bit physical address of the packet at the
 * head of this bucket's list, OR the terminator value 0x00FFFFFF.
 * The upper byte (bits [31:24]) is unused by the GPU in linked-list mode.
 * --------------------------------------------------------------------------- */

typedef uint32_t OT_Tag;

/* ---------------------------------------------------------------------------
 * OT_Clear — Reset all OT entries to the terminator value.
 *
 * Must be called at the start of every frame before any OT_Add calls.
 * Initializes all buckets to empty (no primitives at any depth).
 *
 * Cost: 256 stores = 256 memory write cycles. Fast, but still avoidable
 * if using DMA-based clearing. For now, we clear with a simple loop.
 *
 * @param ot    Pointer to the OT array (must be OT_ENTRY_COUNT uint32_ts).
 * @param count Number of entries (pass OT_ENTRY_COUNT).
 * --------------------------------------------------------------------------- */

static inline void OT_Clear(OT_Tag* ot, int count)
{
    int i;
    /* Fill every bucket with the GPU linked-list terminator (0x00FFFFFF).
     * The GPU stops traversal when it reads this value in a tag/link word. */
    for (i = 0; i < count; i++) {
        ot[i] = PSX_GP0_OT_TERMINATOR;
    }
}

/* ---------------------------------------------------------------------------
 * OT_Add — Insert a GPU primitive packet into an OT depth bucket.
 *
 * This is the hot path — called once per visible polygon per frame.
 * The implementation must be as lean as possible: three pointer operations,
 * no branches, no function call overhead (hence static inline).
 *
 * IMPLEMENTATION NOTE ON ADDRESS ENCODING:
 * The PSX GPU DMA uses physical KSEG1 addresses (0xA0000000 range).
 * For the OT link to work, addresses must be the actual 32-bit RAM address
 * with bit [28] set (KSEG1 = 0xA0xxxxxx rather than KSEG0 = 0x80xxxxxx).
 * On real PSX hardware, the linker script places packet buffers in KSEG1
 * uncached RAM. In the host test build, raw pointer values are used (the
 * DMA is not actually triggered, so the exact encoding does not matter).
 * This is a known simplification for unit-testability.
 *
 * @param ot   Pointer to the OT array (base of OT_ENTRY_COUNT entries).
 * @param prim Pointer to the GPU primitive packet to insert.
 *             Must be a valid, freshly-allocated packet with its 'tag'
 *             word initialized (e.g., by Prim_AllocXxx).
 * @param z    Depth bucket index. Must be in [0, OT_ENTRY_COUNT-1].
 *             0 = nearest (drawn last, on top).
 *             OT_ENTRY_COUNT-1 = farthest (drawn first, behind everything).
 *             The caller is responsible for clamping z to this range.
 * --------------------------------------------------------------------------- */

static inline void OT_Add(OT_Tag* ot, void* prim, int z)
{
    /* Cast the packet to a uint32_t pointer so we can access its tag word.
     * Every GPU primitive struct has 'tag' as its first field. */
    uint32_t* tag = (uint32_t*)prim;

    /* Step 1: Write the current bucket head address into the primitive's tag.
     *         The primitive now "points to" the previous head of this bucket.
     *         The upper byte of the tag will be overwritten with the packet
     *         length word by the GPU — we only set bits [23:0] here. */
    *tag = ot[z] & 0x00FFFFFFu;

    /* Step 2: Write the primitive's address into the bucket as the new head.
     *         Only bits [23:0] of the address are meaningful to the GPU.
     *         The 0x00FFFFFF mask on the bucket value above preserves the
     *         lower 24 bits of whatever was there, clearing the command byte. */
    ot[z] = (uint32_t)((uintptr_t)prim & 0x00FFFFFFu);
}

/* ---------------------------------------------------------------------------
 * OT_GetTail — Return a pointer to the farthest OT entry (last entry).
 *
 * The DMA transfer starts at the TAIL of the OT (farthest depth bucket)
 * and the GPU reads backward toward entry 0. This function returns the
 * address that should be passed to the DMA MADR register.
 *
 * @param ot  Pointer to the OT array.
 * @return    Pointer to the last OT entry (ot[OT_ENTRY_COUNT-1]).
 * --------------------------------------------------------------------------- */

static inline OT_Tag* OT_GetTail(OT_Tag* ot)
{
    /* The tail is the highest-indexed bucket — the farthest depth. */
    return &ot[OT_ENTRY_COUNT - 1];
}

#endif /* PSX_OT_H */
