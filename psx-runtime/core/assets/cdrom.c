/*******************************************************************************
 * FILE:         cdrom.c
 * MODULE:       Core/Assets
 * DESCRIPTION:  CD-ROM reading abstraction implementation.
 *******************************************************************************/

#include "cdrom.h"
#include <stddef.h>

#ifdef PSX_HOST_TEST
#include <stdio.h>
#include <string.h>

/* On host, we simulate the CD filesystem using the local 'assets' folder */
static const char* HOST_ASSETS_DIR = "assets/";

#else
/* Hardware headers for the CD-ROM driver would go here */
#endif

/* ---------------------------------------------------------------------------
 * CD_Init
 * --------------------------------------------------------------------------- */

void CD_Init(void)
{
#ifdef PSX_HOST_TEST
    /* Host environment requires no init */
#else
    /* Future hardware init (e.g. CdInit()) */
#endif
}

/* ---------------------------------------------------------------------------
 * CD_GetFileSize
 * --------------------------------------------------------------------------- */

uint32_t CD_GetFileSize(const char* path)
{
#ifdef PSX_HOST_TEST
    char full_path[256];
    snprintf(full_path, sizeof(full_path), "%s%s", HOST_ASSETS_DIR, path);
    
    FILE* f = fopen(full_path, "rb");
    if (!f) return 0;
    
    fseek(f, 0, SEEK_END);
    uint32_t size = ftell(f);
    fclose(f);
    return size;
#else
    /* Future ISO9660 directory record lookup (CdSearchFile) */
    return 0;
#endif
}

/* ---------------------------------------------------------------------------
 * CD_ReadFile
 * --------------------------------------------------------------------------- */

uint32_t CD_ReadFile(const char* path, void* buffer)
{
    if (!buffer) return 0;
    
#ifdef PSX_HOST_TEST
    char full_path[256];
    snprintf(full_path, sizeof(full_path), "%s%s", HOST_ASSETS_DIR, path);
    
    FILE* f = fopen(full_path, "rb");
    if (!f) return 0;
    
    fseek(f, 0, SEEK_END);
    uint32_t size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    size_t read_bytes = fread(buffer, 1, size, f);
    fclose(f);
    
    return (uint32_t)read_bytes;
#else
    /* Future hardware read (CdControl, CdRead, CdReadSync) */
    return 0;
#endif
}
