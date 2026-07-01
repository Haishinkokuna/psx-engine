/*******************************************************************************
 * FILE:         cdrom.h
 * MODULE:       Core/Assets
 * DESCRIPTION:  CD-ROM reading abstraction.
 *
 *               On the PSX, reading from the CD-ROM drive requires navigating
 *               the ISO9660 filesystem, issuing sector reads, and waiting for
 *               the DMA to transfer data to RAM.
 *
 *               For this initial module, we provide a synchronous ReadFile
 *               interface to keep things simple.
 *******************************************************************************/

#ifndef PSX_CDROM_H
#define PSX_CDROM_H

#include <stdint.h>

/* ---------------------------------------------------------------------------
 * CD_Init
 *
 * Initializes the CD-ROM hardware subsystem.
 * --------------------------------------------------------------------------- */
void CD_Init(void);

/* ---------------------------------------------------------------------------
 * CD_ReadFile
 *
 * Synchronously reads an entire file from the CD into memory.
 * Note: On actual hardware, 'buffer' must be aligned to a 2048-byte sector
 * boundary for DMA transfer. We allocate via Heap_Alloc in practice.
 *
 * @param path    Path to the file on the CD (e.g., "\\MODEL.TMD;1")
 * @param buffer  Pre-allocated buffer large enough to hold the file.
 * @return        Number of bytes read, or 0 on failure.
 * --------------------------------------------------------------------------- */
uint32_t CD_ReadFile(const char* path, void* buffer);

/* ---------------------------------------------------------------------------
 * CD_GetFileSize
 *
 * Queries the filesystem for the exact byte size of a file.
 * --------------------------------------------------------------------------- */
uint32_t CD_GetFileSize(const char* path);

#endif /* PSX_CDROM_H */
