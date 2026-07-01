#version 330 core
/*******************************************************************************
 * FILE:         psxArtifact.frag
 * MODULE:       Editor/Renderer
 * DESCRIPTION:  Fragment shader implementing PSX color quantization artifacts.
 *
 *               The PSX framebuffer stores pixels in 15bpp (5 bits per channel:
 *               R5G5B5 with 1 bit for masking). This gives only 32 levels per
 *               channel (0-31), compared to modern 8bpp (0-255). The result is
 *               visible color banding — smooth gradients become stepped.
 *
 *               This shader simulates that color banding by quantizing the
 *               rendered color to 5 bits per channel before output:
 *
 *                 quantized = floor(linear_color * 31.0 + 0.5) / 31.0
 *
 *               The "+ 0.5 / 31.0" term rounds to the nearest 5-bit value
 *               rather than always truncating (which would make darks too dark).
 *
 *               DITHERING (optional, controlled by uDitherEnabled):
 *                 The real PSX applied a 4x4 Bayer dithering matrix to soften
 *                 the color quantization stepping. We implement the same matrix
 *                 here. When enabled, it closely matches hardware output.
 *
 *               TEXTURE SAMPLING:
 *                 If a texture is bound (uUseTexture == 1), the base color is
 *                 sampled from the texture. Otherwise, the vertex color (Gouraud
 *                 shading simulation) is used directly. This matches how the
 *                 PSX GPU handled textured vs. flat/Gouraud polygons.
 *
 * INPUTS:  vTexCoord        - UV from vertex shader (affine, noperspective)
 *          vColor           - Vertex color from vertex shader (Gouraud)
 * OUTPUTS: fragColor        - Quantized 15bpp-simulated RGBA output
 *******************************************************************************/

noperspective in vec2 vTexCoord;
in vec3 vColor;

out vec4 fragColor;

uniform sampler2D uTexture;
uniform int       uUseTexture;    /* 1 = sample texture, 0 = use vertex color */
uniform int       uDitherEnabled; /* 1 = apply PSX Bayer dithering, 0 = skip  */

/* ---------------------------------------------------------------------------
 * PSX 4x4 Bayer dithering matrix.
 * Values are in the range [0, 15] — the PSX used a 16-step dither offset.
 * These are normalized to [0, 1] for use as per-channel bias.
 * Source: No$Psx technical reference, GPU section.
 * --------------------------------------------------------------------------- */
const float BAYER[16] = float[16](
     0.0/16.0,  8.0/16.0,  2.0/16.0, 10.0/16.0,
    12.0/16.0,  4.0/16.0, 14.0/16.0,  6.0/16.0,
     3.0/16.0, 11.0/16.0,  1.0/16.0,  9.0/16.0,
    15.0/16.0,  7.0/16.0, 13.0/16.0,  5.0/16.0
);

/* ---------------------------------------------------------------------------
 * psx_quantize — Quantize a linear [0,1] color value to 5-bit precision.
 *
 * floor(v * 31.0 + 0.5) produces an integer in [0, 31].
 * Dividing by 31.0 maps back to [0, 1] with 32 discrete levels.
 * The 0.5 bias implements round-to-nearest rather than floor (truncate).
 * --------------------------------------------------------------------------- */
float psx_quantize(float v)
{
    return floor(v * 31.0 + 0.5) / 31.0;
}

void main()
{
    /* --- Base color selection ---
     * Fetch from texture or use interpolated vertex color. */
    vec3 base_color;
    if (uUseTexture == 1) {
        vec4 tex_sample = texture(uTexture, vTexCoord);
        base_color = tex_sample.rgb;
    } else {
        base_color = vColor;
    }

    /* --- Optional dithering ---
     * The PSX dither matrix offsets each pixel's color slightly before
     * quantization, softening banding. The matrix tiles every 4 pixels.
     * gl_FragCoord.xy gives screen-space pixel position (integer centers). */
    if (uDitherEnabled == 1) {
        /* Determine which dither matrix cell this pixel falls in. */
        int px = int(mod(gl_FragCoord.x, 4.0));
        int py = int(mod(gl_FragCoord.y, 4.0));
        float dither_bias = BAYER[py * 4 + px];

        /* Scale the dither bias to the per-channel step size (1/31).
         * The PSX used a fixed 4-bit dither offset applied before 5-bit quantize.
         * We approximate this as a +/- half-step bias. */
        float step = 1.0 / 31.0;
        base_color += (dither_bias - 0.5) * step;
        base_color = clamp(base_color, 0.0, 1.0);
    }

    /* --- 15bpp Quantization ---
     * Reduce each channel to 5 bits (32 levels) to simulate PSX framebuffer. */
    vec3 quantized;
    quantized.r = psx_quantize(base_color.r);
    quantized.g = psx_quantize(base_color.g);
    quantized.b = psx_quantize(base_color.b);

    fragColor = vec4(quantized, 1.0);
}
