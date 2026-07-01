/*******************************************************************************
 * FILE:         gpu_types.h
 * MODULE:       Core/Renderer
 * DESCRIPTION:  PSX GPU primitive type definitions. These are the raw packet
 *               structs that the CPU builds in main RAM and the GPU consumes
 *               via DMA when Display_EndFrame() fires the OT transfer.
 *
 *               PACKET ANATOMY:
 *                 Every GPU packet begins with a 32-bit 'tag' word:
 *                   Bits [31:24] = GPU command code (e.g., 0x20 = flat triangle)
 *                   Bits [23:00] = Address of the NEXT packet in the OT linked
 *                                 list (0x00FFFFFF = OT end terminator).
 *               The GPU reads each tag, executes the embedded command using
 *               the words that follow, then jumps to the next packet address
 *               stored in the tag. This continues until it hits 0x00FFFFFF.
 *
 *               WORD ALIGNMENT:
 *               ALL structs in this file MUST be 4-byte aligned and their
 *               total size MUST be a multiple of 4 bytes. The GPU DMA engine
 *               operates in 32-bit words only. Misaligned or odd-sized packets
 *               will silently corrupt the packet stream.
 *
 *               COORDINATE SPACE:
 *               Vertices (sx, sy) are in SCREEN SPACE — already projected by
 *               the GTE. They are int16_t in the range [0, 1023] X [0, 511].
 *               The GTE outputs these directly. DO NOT pass world-space or
 *               fixed-point FP12_4 coordinates here; those must be converted
 *               by the GTE projection pipeline first.
 *
 *               COLOR FORMAT:
 *               CVECTOR stores R, G, B as uint8_t (0-255) plus a 'cd' byte
 *               which is the GPU command code in the first color word. The
 *               Gouraud-shaded primitives store one CVECTOR per vertex.
 *
 *               TEXTURE COORDINATES:
 *               POLY_FTx and POLY_GTx types carry UV coordinates (0-255 range)
 *               and a TPAGE (texture page) word. CLUT coordinates are stored
 *               as a packed uint16_t (clut_x >> 4, clut_y) in the clut field.
 *
 *               NAMING CONVENTION:
 *               Names follow PSn00bSDK/LibGS conventions so code written for
 *               this engine can be more easily referenced against PSX documents.
 *
 * DEPENDENCIES: <stdint.h>
 *******************************************************************************/

#ifndef PSX_GPU_TYPES_H
#define PSX_GPU_TYPES_H

#include <stdint.h>

/* ===========================================================================
 * Primitive helper types
 * =========================================================================== */

/*
 * CVECTOR — RGB color with a command byte in the fourth slot.
 *
 * The GPU reads the command byte 'cd' from the high byte of the FIRST
 * color word in a packet (the 'tag' word contains the link address, so the
 * first DATA word for a polygon is always this color + command byte).
 *
 * For flat-shaded primitives: cd = GPU command (e.g., 0x20 for flat tri).
 * For Gouraud: only the first vertex's CVECTOR carries cd; subsequent
 * CVECTORs have cd = 0 (padding — the GPU command is already set).
 */
typedef struct {
    uint8_t r;   /* Red   channel (0-255) */
    uint8_t g;   /* Green channel (0-255) */
    uint8_t b;   /* Blue  channel (0-255) */
    uint8_t cd;  /* GPU command code in first color word; 0 for subsequent ones */
} CVECTOR;

/*
 * SVECTOR — Signed 16-bit 2D screen-space coordinate pair.
 * Matches the format the GTE outputs after projection.
 * The 'pad' field keeps the struct at 4 bytes (GPU reads 32-bit words).
 */
typedef struct {
    int16_t  sx;   /* Screen X (0-319 for 320-wide display) */
    int16_t  sy;   /* Screen Y (0-239 for 240-tall display) */
} SVECTOR;

/*
 * TCOORD — Texture coordinate pair and associated TPAGE/CLUT words.
 * UV values are in the range [0, 255] within the texture page's local space.
 */
typedef struct {
    uint8_t  u;    /* U (horizontal) texture coordinate (0-255) */
    uint8_t  v;    /* V (vertical)   texture coordinate (0-255) */
} TCOORD;

/* ===========================================================================
 * GPU Primitive Command Codes
 *
 * These are the upper-byte values embedded in the 'tag' or 'col0.cd' field
 * of each primitive. The GPU dispatches on this byte.
 *
 * Format: 0xCC where C = upper nibble (primitive class) + attributes.
 * Bit 25 of the full command word = Gouraud shading (1) vs. flat (0).
 * Bit 26 = textured (1) vs. untextured (0).
 * Bit 27 = semi-transparent (1) vs. opaque (0).
 * Bit 28 = quad (1) vs. triangle (0) [for polygon commands].
 * =========================================================================== */

#define GPU_CMD_POLY_F3     0x20U  /* Flat-shaded  triangle  (untextured)        */
#define GPU_CMD_POLY_G3     0x30U  /* Gouraud      triangle  (untextured)        */
#define GPU_CMD_POLY_FT3    0x24U  /* Flat-shaded  triangle  (textured)          */
#define GPU_CMD_POLY_GT3    0x34U  /* Gouraud      triangle  (textured)          */
#define GPU_CMD_POLY_F4     0x28U  /* Flat-shaded  quad      (untextured)        */
#define GPU_CMD_POLY_G4     0x38U  /* Gouraud      quad      (untextured)        */
#define GPU_CMD_POLY_FT4    0x2CU  /* Flat-shaded  quad      (textured)          */
#define GPU_CMD_POLY_GT4    0x3CU  /* Gouraud      quad      (textured)          */
#define GPU_CMD_LINE_F2     0x40U  /* Flat-shaded  line      (2 verts)           */
#define GPU_CMD_TILE        0x60U  /* Filled rectangle (fastest 2D fill command) */
#define GPU_CMD_SPRT        0x64U  /* Variable-size textured sprite              */
#define GPU_CMD_SPRT_8      0x74U  /* Fixed  8x8  textured sprite                */
#define GPU_CMD_SPRT_16     0x7CU  /* Fixed 16x16 textured sprite                */

/* ===========================================================================
 * POLY_F3 — Flat-shaded untextured triangle
 *
 * Layout in memory (6 words = 24 bytes):
 *   Word 0: tag     (link address | packet length)
 *   Word 1: col0    (r, g, b, GPU_CMD_POLY_F3)
 *   Word 2: v0      (sx0, sy0)
 *   Word 3: v1      (sx1, sy1)
 *   Word 4: v2      (sx2, sy2)
 *
 * GPU renders one solid color across the entire triangle. No perspective
 * correction (the PSX GPU never does that). Fastest polygon type.
 * =========================================================================== */

typedef struct {
    uint32_t tag;   /* OT link address | cmd length in upper byte */
    CVECTOR  col0;  /* Flat color; col0.cd MUST be GPU_CMD_POLY_F3 */
    SVECTOR  v0;    /* Vertex 0 (screen space) */
    SVECTOR  v1;    /* Vertex 1 (screen space) */
    SVECTOR  v2;    /* Vertex 2 (screen space) */
} POLY_F3;          /* Total: 20 bytes */

/* ===========================================================================
 * POLY_G3 — Gouraud-shaded untextured triangle
 *
 * Layout in memory (8 words = 32 bytes):
 *   Word 0: tag
 *   Word 1: col0    (r, g, b, GPU_CMD_POLY_G3) + v0 (sx0, sy0)
 *   Word 2: col1    (r, g, b, 0x00)            + v1 (sx1, sy1)
 *   Word 3: col2    (r, g, b, 0x00)            + v2 (sx2, sy2)
 *
 * The GPU interpolates color linearly across the triangle from three
 * distinct vertex colors. More expensive than POLY_F3 due to color
 * interpolation, but gives smooth shading without textures.
 *
 * NOTE: The color and vertex for each Gouraud vertex are interleaved —
 * col then vert — not separate arrays. The struct layout enforces this.
 * =========================================================================== */

typedef struct {
    uint32_t tag;
    CVECTOR  col0;  /* col0.cd MUST be GPU_CMD_POLY_G3 */
    SVECTOR  v0;
    CVECTOR  col1;  /* col1.cd = 0 (padding) */
    SVECTOR  v1;
    CVECTOR  col2;  /* col2.cd = 0 (padding) */
    SVECTOR  v2;
} POLY_G3;          /* Total: 32 bytes */

/* ===========================================================================
 * POLY_FT3 — Flat-shaded textured triangle
 *
 * Adds UV coordinates and texture page/CLUT information to POLY_F3.
 * The color modulates the texture (multiply blend). Set to 0x808080 for
 * unmodulated (pure texture color). The GPU command includes the T bit.
 *
 * CLUT field: packed as ((clut_y << 6) | (clut_x >> 4)).
 * TPAGE field: packed texture page word — use the GPU helper macros to build.
 * =========================================================================== */

typedef struct {
    uint32_t tag;
    CVECTOR  col0;      /* Flat modulation color; col0.cd = GPU_CMD_POLY_FT3 */
    SVECTOR  v0;        /* Vertex 0 screen coords */
    uint8_t  u0;        /* UV for vertex 0 */
    uint8_t  v0_uv;
    uint16_t clut;      /* CLUT origin (packed: ((cy<<6)|(cx>>4))) */
    SVECTOR  v1;
    uint8_t  u1;
    uint8_t  v1_uv;
    uint16_t tpage;     /* Texture page descriptor */
    SVECTOR  v2;
    uint8_t  u2;
    uint8_t  v2_uv;
    uint16_t pad;       /* Padding to keep 4-byte alignment */
} POLY_FT3;             /* Total: 36 bytes */

/* ===========================================================================
 * POLY_GT3 — Gouraud-shaded textured triangle
 *
 * Combines per-vertex colors (Gouraud) with UV texture coordinates.
 * This is the richest (and most expensive) triangle type.
 * =========================================================================== */

typedef struct {
    uint32_t tag;
    CVECTOR  col0;      /* col0.cd = GPU_CMD_POLY_GT3 */
    SVECTOR  v0;
    uint8_t  u0;
    uint8_t  v0_uv;
    uint16_t clut;
    CVECTOR  col1;      /* col1.cd = 0 */
    SVECTOR  v1;
    uint8_t  u1;
    uint8_t  v1_uv;
    uint16_t tpage;
    CVECTOR  col2;      /* col2.cd = 0 */
    SVECTOR  v2;
    uint8_t  u2;
    uint8_t  v2_uv;
    uint16_t pad;
} POLY_GT3;             /* Total: 48 bytes */

/* ===========================================================================
 * POLY_F4 — Flat-shaded untextured quad
 * POLY_G4 — Gouraud-shaded untextured quad
 *
 * Quads are drawn as two triangles (v0v1v2 and v1v2v3) by the GPU.
 * Vertex order is: top-left, top-right, bottom-left, bottom-right.
 * This specific order is required by the GPU — other orders produce
 * a "butterfly" (bowtie) polygon rather than a rectangle.
 * =========================================================================== */

typedef struct {
    uint32_t tag;
    CVECTOR  col0;      /* col0.cd = GPU_CMD_POLY_F4 */
    SVECTOR  v0;
    SVECTOR  v1;
    SVECTOR  v2;
    SVECTOR  v3;
} POLY_F4;              /* Total: 24 bytes */

typedef struct {
    uint32_t tag;
    CVECTOR  col0;      /* col0.cd = GPU_CMD_POLY_G4 */
    SVECTOR  v0;
    CVECTOR  col1;
    SVECTOR  v1;
    CVECTOR  col2;
    SVECTOR  v2;
    CVECTOR  col3;
    SVECTOR  v3;
} POLY_G4;              /* Total: 40 bytes */

/* ===========================================================================
 * POLY_FT4 — Flat-shaded textured quad
 * POLY_GT4 — Gouraud-shaded textured quad
 * =========================================================================== */

typedef struct {
    uint32_t tag;
    CVECTOR  col0;
    SVECTOR  v0;
    uint8_t  u0; uint8_t v0_uv; uint16_t clut;
    SVECTOR  v1;
    uint8_t  u1; uint8_t v1_uv; uint16_t tpage;
    SVECTOR  v2;
    uint8_t  u2; uint8_t v2_uv; uint16_t pad0;
    SVECTOR  v3;
    uint8_t  u3; uint8_t v3_uv; uint16_t pad1;
} POLY_FT4;             /* Total: 52 bytes */

typedef struct {
    uint32_t tag;
    CVECTOR  col0;
    SVECTOR  v0;
    uint8_t  u0; uint8_t v0_uv; uint16_t clut;
    CVECTOR  col1;
    SVECTOR  v1;
    uint8_t  u1; uint8_t v1_uv; uint16_t tpage;
    CVECTOR  col2;
    SVECTOR  v2;
    uint8_t  u2; uint8_t v2_uv; uint16_t pad0;
    CVECTOR  col3;
    SVECTOR  v3;
    uint8_t  u3; uint8_t v3_uv; uint16_t pad1;
} POLY_GT4;             /* Total: 64 bytes */

/* ===========================================================================
 * LINE_F2 — Flat-shaded line (2 endpoints)
 *
 * Draws a single anti-aliased (actually aliased on PSX — no AA hardware) line
 * between two screen-space points. Used for wireframe debugging and HUD
 * vector elements (crosshairs, UI boxes without fill).
 * =========================================================================== */

typedef struct {
    uint32_t tag;
    CVECTOR  col0;      /* col0.cd = GPU_CMD_LINE_F2 */
    SVECTOR  v0;        /* First endpoint */
    SVECTOR  v1;        /* Second endpoint */
} LINE_F2;              /* Total: 16 bytes */

/* ===========================================================================
 * TILE — Variable-size filled rectangle (fastest 2D fill)
 *
 * The TILE command directly fills a screen-space rectangle with a solid
 * color. This is the fastest GPU operation — no rasterization math, just
 * a rectangle blit. Use it for:
 *   - UI background panels
 *   - Shadow blobs (semi-transparent TILE with DRAW_MODE blend)
 *   - Health bars, map overlays
 *   - Clearing specific VRAM regions
 *
 * W and H are in pixels. Neither can be zero.
 * =========================================================================== */

typedef struct {
    uint32_t tag;
    CVECTOR  col0;      /* col0.cd = GPU_CMD_TILE */
    SVECTOR  v0;        /* Top-left corner (screen space) */
    uint16_t w;         /* Width  in pixels */
    uint16_t h;         /* Height in pixels */
} TILE;                 /* Total: 16 bytes */

/* ===========================================================================
 * SPRT — Variable-size textured sprite (axis-aligned, no rotation)
 *
 * Renders a rectangle sampled from VRAM at position (u, v) within the
 * current texture page. The rectangle is always axis-aligned — sprites on
 * the PSX cannot be rotated in hardware (game code approximates rotation
 * by selecting from a pre-rendered rotation atlas, or by using textured
 * polygons for the rare case where per-frame rotation is needed).
 *
 * SPRT_8 and SPRT_16 variants have fixed 8x8 and 16x16 sizes and use
 * slightly different command codes (no W/H fields needed by the GPU).
 * =========================================================================== */

typedef struct {
    uint32_t tag;
    CVECTOR  col0;      /* Color modulation; col0.cd = GPU_CMD_SPRT */
    SVECTOR  v0;        /* Top-left corner (screen space) */
    uint8_t  u0;        /* Horizontal offset into texture page */
    uint8_t  v0_uv;     /* Vertical   offset into texture page */
    uint16_t clut;      /* CLUT descriptor for 4bpp/8bpp textures */
    uint16_t w;         /* Sprite width  in pixels */
    uint16_t h;         /* Sprite height in pixels */
} SPRT;                 /* Total: 20 bytes */

/* ===========================================================================
 * Convenience macros for CLUT and TPAGE encoding.
 *
 * The GPU's CLUT field is a packed uint16_t:
 *   Bits [15:6] = CLUT Y position in VRAM (row / 1)
 *   Bits  [5:0] = CLUT X position in VRAM (column / 16)
 *
 * The GPU's TPAGE field encodes:
 *   Bits  [3:0] = Texture page X base (N * 64 pixels)
 *   Bit      4  = Texture page Y base (0=row0, 1=row256)
 *   Bits  [6:5] = Semi-transparency mode (0-3)
 *   Bits  [8:7] = Color mode (0=4bpp, 1=8bpp, 2=15bpp)
 *   Bit      9  = Dither enable
 * =========================================================================== */

/* Pack a CLUT descriptor from VRAM coordinates.
 * clut_x must be a multiple of 16. clut_y is a VRAM row. */
#define GPU_MAKE_CLUT(clut_x, clut_y)  \
    ((uint16_t)(((clut_y) << 6) | ((clut_x) >> 4)))

/* Pack a texture page descriptor.
 * tp_x: texture page base in VRAM X (0-15, each step = 64 pixels)
 * tp_y: 0 or 1 (VRAM row 0 or row 256)
 * semi: semi-transparency mode (0-3)
 * mode: color depth (0=4bpp, 1=8bpp, 2=15bpp) */
#define GPU_MAKE_TPAGE(tp_x, tp_y, semi, mode) \
    ((uint16_t)(((tp_x) & 0xF) | (((tp_y) & 0x1) << 4) | \
                (((semi) & 0x3) << 5) | (((mode) & 0x3) << 7)))

#endif /* PSX_GPU_TYPES_H */
