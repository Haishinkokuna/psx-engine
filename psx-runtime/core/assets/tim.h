/*******************************************************************************
 * FILE:         tim.h
 * MODULE:       Core/Assets
 * DESCRIPTION:  TIM (Texture IMage) parser.
 *
 *               TIM is the standard Sony texture format. It contains
 *               optional Color Lookup Table (CLUT) data and the raw
 *               pixel image data.
 *******************************************************************************/

#ifndef PSX_TIM_H
#define PSX_TIM_H

#include <stdint.h>

/* ---------------------------------------------------------------------------
 * TIM Structures
 * Note: Struct packing is important if mapping directly to file bytes, but
 * the PSX toolchain generally aligns structs properly for TIM.
 * --------------------------------------------------------------------------- */

/* Magic number = 0x00000010 */
#define TIM_MAGIC 0x00000010

typedef struct {
    uint32_t magic;
    uint32_t bpp;    /* 0=4-bit, 1=8-bit, 2=16-bit, 3=24-bit */
} TIM_Header;

typedef struct {
    uint32_t size;   /* Size of CLUT block in bytes */
    uint16_t x, y;   /* VRAM X, Y coordinate */
    uint16_t w, h;   /* Width and height */
    /* CLUT pixel data follows */
} TIM_ClutInfo;

typedef struct {
    uint32_t size;   /* Size of image block in bytes */
    uint16_t x, y;   /* VRAM X, Y coordinate */
    uint16_t w, h;   /* Width (in 16-bit words) and height */
    /* Image pixel data follows */
} TIM_ImageInfo;

/* High-level parsed container */
typedef struct {
    uint32_t bpp;
    
    /* CLUT data (NULL if no CLUT) */
    TIM_ClutInfo* clut;
    uint16_t*     clut_data;
    
    /* Image data */
    TIM_ImageInfo* image;
    uint16_t*      image_data;
} TIM_Parsed;

/* ---------------------------------------------------------------------------
 * TIM API
 * --------------------------------------------------------------------------- */

/* Parses a raw byte buffer containing a .TIM file into the TIM_Parsed struct. 
 * Returns 1 on success, 0 on failure. */
int TIM_Load(void* file_data, TIM_Parsed* out_tim);

/* Uploads the parsed TIM (both CLUT and Image) to VRAM. */
void TIM_UploadToVRAM(const TIM_Parsed* tim);

#endif /* PSX_TIM_H */
