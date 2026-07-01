/*******************************************************************************
 * FILE:         tim.c
 * MODULE:       Core/Assets
 * DESCRIPTION:  TIM parser implementation.
 *******************************************************************************/

#include "tim.h"
#include <stddef.h>

/* ---------------------------------------------------------------------------
 * TIM_Load
 * --------------------------------------------------------------------------- */

int TIM_Load(void* file_data, TIM_Parsed* out_tim)
{
    if (!file_data || !out_tim) return 0;
    
    uint32_t* ptr = (uint32_t*)file_data;
    
    /* 1. Read Header */
    TIM_Header* header = (TIM_Header*)ptr;
    if (header->magic != TIM_MAGIC) {
        return 0; /* Not a valid TIM */
    }
    
    out_tim->bpp = header->bpp & 3; /* Bit 0-1 represent BPP */
    int has_clut = (header->bpp & 8) != 0; /* Bit 3 represents CLUT presence */
    
    ptr += 2; /* Skip magic and bpp */
    
    /* 2. Read CLUT (if present) */
    if (has_clut) {
        out_tim->clut = (TIM_ClutInfo*)ptr;
        out_tim->clut_data = (uint16_t*)(ptr + 2); /* Skip size, x, y, w, h */
        
        /* Advance ptr by CLUT size (size is in bytes, ptr is uint32_t) */
        ptr += out_tim->clut->size / 4;
    } else {
        out_tim->clut = NULL;
        out_tim->clut_data = NULL;
    }
    
    /* 3. Read Image */
    out_tim->image = (TIM_ImageInfo*)ptr;
    out_tim->image_data = (uint16_t*)(ptr + 2);
    
    return 1;
}

/* ---------------------------------------------------------------------------
 * TIM_UploadToVRAM
 * --------------------------------------------------------------------------- */

void TIM_UploadToVRAM(const TIM_Parsed* tim)
{
    if (!tim) return;

#ifdef PSX_HOST_TEST
    /* In the host editor, texture uploading will be simulated by the OpenGL 
     * artifact renderer (which tracks VRAM states). For now, it's a no-op 
     * as we only test parsing. */
#else
    /* Future hardware DMA (LoadImage) */
    /*
    RECT rect;
    if (tim->clut) {
        rect.x = tim->clut->x;
        rect.y = tim->clut->y;
        rect.w = tim->clut->w;
        rect.h = tim->clut->h;
        LoadImage(&rect, (uint32_t*)tim->clut_data);
    }
    
    rect.x = tim->image->x;
    rect.y = tim->image->y;
    rect.w = tim->image->w;
    rect.h = tim->image->h;
    LoadImage(&rect, (uint32_t*)tim->image_data);
    
    DrawSync(0); // Wait for DMA to finish
    */
#endif
}
