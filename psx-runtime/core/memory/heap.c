/*******************************************************************************
 * FILE:         heap.c
 * MODULE:       Core/Memory
 * DESCRIPTION:  Main RAM first-fit freelist heap allocator implementation.
 *
 *               Block layout in memory (8 bytes overhead per allocated block):
 *
 *               +-------------------------------+  <- block base address
 *               |  uint32_t  block_size         |  4 bytes: total block size
 *               |  uint32_t  magic / in_use flag|  4 bytes: state sentinel
 *               +-------------------------------+  <- pointer returned to caller
 *               |  user data ...                |
 *               +-------------------------------+
 *
 *               A "free" block uses the same 8-byte header but the second
 *               4 bytes store a pointer to the next free block (FreeBlock*),
 *               forming the freelist chain. When a block is allocated, that
 *               second word is overwritten with HEAP_MAGIC (0xDEADBEEF) to
 *               detect double-free and use-after-free corruption in debug
 *               builds.
 *
 *               COALESCING:
 *                 The freelist is kept sorted by ascending address. On PSX_HeapFree,
 *                 we walk the list to find the predecessor by address, insert the
 *                 freed block, then check if we can merge it with:
 *                   a) Its next neighbor (if next_block is directly adjacent)
 *                   b) Its previous neighbor (if prev_block ends exactly at us)
 *
 * DEPENDENCIES: heap.h, mem_map.h
 *******************************************************************************/

#include "heap.h"
#include "mem_map.h"

/* ---------------------------------------------------------------------------
 * Internal constants
 * --------------------------------------------------------------------------- */

/* Sentinel value stored in the second header word of an ALLOCATED block.
 * If PSX_HeapFree ever sees a block whose magic is wrong, the heap is corrupt. */
#define HEAP_MAGIC          0xDEADBEEFu

/* Total overhead per allocation (two uint32_t words = 8 bytes).
 * The usable payload begins immediately after these two words. */
#define HEAP_HEADER_SIZE    8u

/* Minimum block size. A free block must be at least large enough to hold
 * the 8-byte header plus 8 bytes of payload. Splitting produces a new free
 * block only if the leftover is >= MIN_BLOCK_SIZE. */
#define MIN_BLOCK_SIZE      (HEAP_HEADER_SIZE + 8u)

/* ---------------------------------------------------------------------------
 * Internal types
 * --------------------------------------------------------------------------- */

/* FreeBlock — the header layout of a free block in the freelist.
 * Lives in the first 8 bytes of the free block's memory.
 * 'size' includes the header itself. 'next' is the next free block pointer. */
typedef struct FreeBlock {
    uint32_t        size;   /* Total byte size of this block (incl. header) */
    struct FreeBlock* next; /* Next free block in address-sorted freelist     */
} FreeBlock;

/* AllocHeader — the header of an ALLOCATED block.
 * Layout matches FreeBlock in the first word (size is the same).
 * Second word holds HEAP_MAGIC to detect corruption. */
typedef struct {
    uint32_t size;  /* Total byte size of this block (incl. header) */
    uint32_t magic; /* HEAP_MAGIC sentinel — checked on free         */
} AllocHeader;

/* ---------------------------------------------------------------------------
 * Module-private state
 * g_freelist_head: pointer to the first free block (lowest address).
 * Initialized by Heap_Init to point to the entire usable heap region.
 * --------------------------------------------------------------------------- */

static FreeBlock* g_freelist_head = (FreeBlock*)0;

/* ---------------------------------------------------------------------------
 * Heap_Init
 * --------------------------------------------------------------------------- */

void Heap_Init(void)
{
    /* Cast the heap base address to a FreeBlock pointer. This is the
     * initial single free block that covers the entire usable heap.
     * PSX_HEAP_BASE and PSX_HEAP_SIZE come from mem_map.h. */
    FreeBlock* initial = (FreeBlock*)PSX_HEAP_BASE;

    /* The block's 'size' covers the full heap. Allocations will chip
     * pieces off the front (or wherever first-fit finds a match). */
    initial->size = PSX_HEAP_SIZE;
    initial->next = (FreeBlock*)0;

    g_freelist_head = initial;
}

/* ---------------------------------------------------------------------------
 * PSX_HeapAlloc
 * --------------------------------------------------------------------------- */

void* PSX_HeapAlloc(uint32_t size)
{
    FreeBlock* prev;
    FreeBlock* curr;
    uint32_t   total_size;

    if (size == 0u) {
        return (void*)0;
    }

    /* Pad requested size to an 8-byte boundary for alignment.
     * Add the 8-byte header overhead — we are allocating a block of
     * (header + payload), not just payload alone. */
    size      = (size + 7u) & ~7u;           /* align payload to 8 bytes */
    total_size = size + HEAP_HEADER_SIZE;     /* add header overhead       */

    /* Walk the freelist (first-fit) looking for a block large enough. */
    prev = (FreeBlock*)0;
    curr = g_freelist_head;

    while (curr != (FreeBlock*)0) {
        if (curr->size >= total_size) {
            /* Found a fitting block. */
            FreeBlock* leftover = (FreeBlock*)0;

            if ((curr->size - total_size) >= MIN_BLOCK_SIZE) {
                /* The remaining space after our allocation is large enough
                 * to form a new, usable free block. Split here.
                 * The new free block begins immediately after our allocation. */
                leftover = (FreeBlock*)((uint8_t*)curr + total_size);
                leftover->size = curr->size - total_size;
                leftover->next = curr->next;
            }

            /* Unlink 'curr' from the freelist. If we split, wire in the
             * leftover; otherwise skip directly to curr->next. */
            if (prev != (FreeBlock*)0) {
                prev->next = (leftover != (FreeBlock*)0) ? leftover : curr->next;
            } else {
                g_freelist_head = (leftover != (FreeBlock*)0) ? leftover : curr->next;
            }

            /* Stamp the allocated block header. 'size' field holds the
             * total block size (including header). Magic marks it allocated. */
            {
                AllocHeader* hdr = (AllocHeader*)curr;
                hdr->size  = total_size;
                hdr->magic = HEAP_MAGIC;

                /* Return a pointer to the payload — just past the 8-byte header. */
                return (void*)((uint8_t*)curr + HEAP_HEADER_SIZE);
            }
        }

        prev = curr;
        curr = curr->next;
    }

    /* No fitting block found — heap is exhausted or too fragmented. */
    return (void*)0;
}

/* ---------------------------------------------------------------------------
 * PSX_HeapFree
 * --------------------------------------------------------------------------- */

void PSX_HeapFree(void* ptr)
{
    AllocHeader* hdr;
    FreeBlock*   freed;
    FreeBlock*   prev;
    FreeBlock*   curr;

    if (ptr == (void*)0) {
        return; /* NULL free is explicitly a no-op. */
    }

    /* Step back 8 bytes to find the block header. */
    hdr = (AllocHeader*)((uint8_t*)ptr - HEAP_HEADER_SIZE);

    /* Corruption check: the magic sentinel must still be intact.
     * A mismatch means a double-free, heap overflow, or use-after-free.
     * We do nothing rather than crash — in a debug build you would assert here. */
    if (hdr->magic != HEAP_MAGIC) {
        return;
    }

    /* Clear the magic so a double-free will be caught next time. */
    hdr->magic = 0u;

    /* Treat this block as a FreeBlock from here on. */
    freed       = (FreeBlock*)hdr;
    freed->size = hdr->size;
    freed->next = (FreeBlock*)0;

    /* Insert the freed block into the address-sorted freelist.
     * We need to find the predecessor block (prev) such that:
     *   prev < freed < prev->next
     * This maintains the sorted invariant needed for coalescing. */
    prev = (FreeBlock*)0;
    curr = g_freelist_head;

    while (curr != (FreeBlock*)0 && curr < freed) {
        prev = curr;
        curr = curr->next;
    }

    /* Wire in the freed block between prev and curr. */
    freed->next = curr;

    if (prev != (FreeBlock*)0) {
        prev->next = freed;
    } else {
        g_freelist_head = freed;
    }

    /* --- Coalesce with next neighbor ---
     * If the block immediately after 'freed' in memory is also free
     * (i.e., it is exactly where freed->next points), merge them into
     * one larger block. */
    if (freed->next != (FreeBlock*)0) {
        FreeBlock* next_block = freed->next;
        /* "Adjacent" means: freed ends exactly where next_block begins. */
        if ((uint8_t*)freed + freed->size == (uint8_t*)next_block) {
            freed->size += next_block->size;
            freed->next  = next_block->next;
        }
    }

    /* --- Coalesce with previous neighbor ---
     * If 'prev' ends exactly where 'freed' begins, merge them. */
    if (prev != (FreeBlock*)0) {
        if ((uint8_t*)prev + prev->size == (uint8_t*)freed) {
            prev->size += freed->size;
            prev->next  = freed->next;
        }
    }
}

/* ---------------------------------------------------------------------------
 * Heap_GetFreeBytes
 * --------------------------------------------------------------------------- */

uint32_t Heap_GetFreeBytes(void)
{
    FreeBlock* curr = g_freelist_head;
    uint32_t   total = 0u;

    while (curr != (FreeBlock*)0) {
        total += curr->size;
        curr   = curr->next;
    }

    return total;
}

/* ---------------------------------------------------------------------------
 * Heap_GetLargestFreeBlock
 * --------------------------------------------------------------------------- */

uint32_t Heap_GetLargestFreeBlock(void)
{
    FreeBlock* curr    = g_freelist_head;
    uint32_t   largest = 0u;

    while (curr != (FreeBlock*)0) {
        if (curr->size > largest) {
            largest = curr->size;
        }
        curr = curr->next;
    }

    /* Return usable payload size (subtract header overhead). */
    return (largest > HEAP_HEADER_SIZE) ? (largest - HEAP_HEADER_SIZE) : 0u;
}
