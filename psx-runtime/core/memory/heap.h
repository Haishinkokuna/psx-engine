/*******************************************************************************
 * FILE:         heap.h
 * MODULE:       Core/Memory
 * DESCRIPTION:  Main RAM pool allocator API — first-fit freelist with
 *               in-band block headers and adjacent-block coalescing.
 *
 *               MEMORY BUDGET:
 *                 Total main RAM: 2 MB (0x80000000 - 0x801FFFFF).
 *                 Kernel + vectors occupy the first 64 KB.
 *                 Stack reserves the top 64 KB (grows downward from ~0x801FFF00).
 *                 USABLE HEAP: from 0x80010000, approximately 1.875 MB.
 *                   PSX_HEAP_SIZE (defined in mem_map.h) gives the exact byte count.
 *
 *               ALLOCATOR DESIGN — First-Fit Freelist:
 *                 Free blocks are stored as a singly-linked list. Each free
 *                 block's first 8 bytes store a FreeBlock header:
 *                   { uint32_t size; FreeBlock* next; }
 *                 This is called "in-band" storage — no separate metadata
 *                 array. The block's own memory holds the freelist node.
 *
 *                 Allocation: walk the list until a block >= requested size
 *                 is found. Split if the remaining space is large enough to
 *                 form a new free block (>= MIN_BLOCK_SIZE). O(n) worst case.
 *
 *                 Free + Coalesce: locate the freed block's position in the
 *                 list (by address order), reinsert it, and merge with
 *                 adjacent free neighbors. This prevents fragmentation.
 *
 *               NAMING CONVENTION:
 *                 Functions are named PSX_HeapAlloc / PSX_HeapFree (not
 *                 malloc/free) to prevent accidental linkage against libc.
 *                 If you see malloc() anywhere in psx-runtime, that is a bug.
 *
 * DEPENDENCIES: mem_map.h, <stdint.h>
 *******************************************************************************/

#ifndef PSX_HEAP_H
#define PSX_HEAP_H

#include <stdint.h>
#include "mem_map.h"

/* ---------------------------------------------------------------------------
 * Heap_Init — Initialize the heap allocator.
 *
 * Sets up the initial free block covering the entire usable heap region.
 * Must be called exactly once before any PSX_HeapAlloc calls.
 * --------------------------------------------------------------------------- */

void Heap_Init(void);

/* ---------------------------------------------------------------------------
 * PSX_HeapAlloc — Allocate 'size' bytes from the main RAM heap.
 *
 * Uses a first-fit freelist strategy. The returned pointer is 8-byte
 * aligned (header overhead is subtracted before returning to caller).
 *
 * @param size  Bytes to allocate. Will be padded to 8-byte boundary.
 * @return      Pointer to usable memory, or NULL if heap is exhausted.
 *
 * PERFORMANCE WARNING: PSX_HeapAlloc is O(n) in the number of free blocks.
 * In a PSX game, heavy per-frame allocation from the heap is an anti-pattern.
 * Allocate large blocks at level load and sub-allocate within them. Use
 * Scratch_Alloc for per-frame temporary buffers.
 * --------------------------------------------------------------------------- */

void* PSX_HeapAlloc(uint32_t size);

/* ---------------------------------------------------------------------------
 * PSX_HeapFree — Return a previously allocated block to the heap.
 *
 * The block is reinserted into the freelist in address order. Adjacent
 * free blocks are coalesced to form a larger free block, preventing
 * fragmentation over time.
 *
 * @param ptr  Must be a pointer previously returned by PSX_HeapAlloc.
 *             Passing NULL is a no-op (safe). Passing any other value
 *             is undefined behavior and will corrupt the heap.
 * --------------------------------------------------------------------------- */

void PSX_HeapFree(void* ptr);

/* ---------------------------------------------------------------------------
 * Heap_GetFreeBytes — Return total free bytes currently in the heap.
 *
 * Walks the entire freelist and sums block sizes. O(n). Intended for
 * debug/profiling only — do not call this in performance-critical code.
 * --------------------------------------------------------------------------- */

uint32_t Heap_GetFreeBytes(void);

/* ---------------------------------------------------------------------------
 * Heap_GetLargestFreeBlock — Return the size of the largest contiguous
 * free block. This indicates the maximum single allocation possible.
 *
 * If this value is much smaller than Heap_GetFreeBytes(), fragmentation
 * has occurred and block coalescing was insufficient for the allocation
 * pattern. Consider revising the allocation strategy.
 * --------------------------------------------------------------------------- */

uint32_t Heap_GetLargestFreeBlock(void);

#endif /* PSX_HEAP_H */
