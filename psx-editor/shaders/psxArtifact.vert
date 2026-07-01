#version 330 core
/*******************************************************************************
 * FILE:         psxArtifact.vert
 * MODULE:       Editor/Renderer
 * DESCRIPTION:  Vertex shader implementing the PSX vertex-snapping artifact.
 *
 *               The original PSX GPU rasterizer operated entirely in integer
 *               screen-space coordinates. Vertices were projected and then
 *               truncated to integer pixels before rasterization. This caused
 *               the characteristic "wobbly" or "swimming" geometry that PSX
 *               games are known for — polygons visibly shift between integer
 *               pixel positions as the camera or object moves.
 *
 *               SIMULATION TECHNIQUE:
 *                 1. Transform vertex to clip space normally via the MVP matrix.
 *                 2. Perform a manual perspective divide to get NDC (Normalized
 *                    Device Coordinates, range -1.0 to +1.0).
 *                 3. Scale NDC to PSX screen resolution (320 x 240), snap to
 *                    integer pixel coordinates via floor(), then scale back.
 *                 4. Multiply back by the original W to restore clip-space
 *                    for the rasterizer pipeline (which will divide by W again).
 *
 *               WHY MULTIPLY BACK BY W?
 *                 OpenGL's rasterizer automatically divides gl_Position by its
 *                 W component. If we set gl_Position = vec4(snapped_ndc, 1.0),
 *                 OpenGL would NOT apply the correct depth values. By restoring
 *                 clipPos = vec4(snapped_ndc * W, depth, W), the hardware
 *                 divide produces the correct snapped NDC + correct depth.
 *
 *               UV COORDINATES — noperspective:
 *                 The PSX did NOT perform perspective-correct UV interpolation
 *                 (a technique added in later hardware). UVs were interpolated
 *                 linearly across the screen — the "affine" method. This causes
 *                 the characteristic texture warping on large triangles.
 *                 We replicate this with the 'noperspective' qualifier.
 *
 * INPUTS:  aPos      - Object-space vertex position (vec3)
 *          aTexCoord - Texture coordinates (vec2)
 * OUTPUTS: vTexCoord - Affine (non-perspective) UV for fragment shader
 *          vColor    - Vertex color for Gouraud shading simulation
 *******************************************************************************/

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec3 aColor;

/* noperspective disables perspective-correct interpolation for these varyings.
 * This is the hardware-level mechanism that produces affine UV warping. */
noperspective out vec2 vTexCoord;
out vec3 vColor;

uniform mat4 uMVP;

/* PSX display resolution — the grid we snap to. Half-res in NDC space
 * (NDC goes -1 to +1, so full width = 2.0 / PSX_WIDTH pixels per unit). */
const float PSX_WIDTH  = 320.0;
const float PSX_HEIGHT = 240.0;

void main()
{
    /* Standard MVP transform to clip space. */
    vec4 clipPos = uMVP * vec4(aPos, 1.0);

    /* --- Vertex snapping pass ---
     * Divide by W to get NDC (-1 to +1 on both axes). */
    vec2 ndc = clipPos.xy / clipPos.w;

    /* Scale NDC to PSX pixel coordinates and snap to integer grid.
     * NDC range [-1, 1] maps to pixel range [-160, 160] on X (half-width).
     * floor(x + 0.5) = round-to-nearest-integer. */
    float half_w = PSX_WIDTH  * 0.5;
    float half_h = PSX_HEIGHT * 0.5;

    vec2 psx_pixels = floor(ndc * vec2(half_w, half_h) + 0.5);

    /* Convert back to NDC. */
    vec2 snapped_ndc = psx_pixels / vec2(half_w, half_h);

    /* Restore clip space by multiplying by original W.
     * The rasterizer will divide by W again — giving us the snapped NDC. */
    gl_Position = vec4(snapped_ndc * clipPos.w, clipPos.z, clipPos.w);

    /* Pass through UVs and vertex color. noperspective means UVs will NOT
     * be divided by W during interpolation, simulating affine mapping. */
    vTexCoord = aTexCoord;
    vColor    = aColor;
}
