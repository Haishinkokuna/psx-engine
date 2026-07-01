/*******************************************************************************
 * FILE:         PsxArtifactRenderer.h
 * MODULE:       Editor/Renderer
 * DESCRIPTION:  PSX artifact simulation renderer for the desktop editor.
 *
 *               This renderer runs on the host GPU (modern OpenGL 3.3) but
 *               applies a GLSL shader pipeline that simulates the visual
 *               characteristics of original PSX hardware rendering:
 *
 *                 1. Vertex snapping — geometry snapped to 320x240 integer grid.
 *                 2. Affine UV mapping — texture coordinates interpolated without
 *                    perspective division (noperspective qualifier).
 *                 3. 5-bit color quantization — R5G5B5 framebuffer simulation.
 *                 4. Optional 4x4 Bayer dithering — matches hardware dither pass.
 *
 *               The renderer owns the OpenGL shader program, the framebuffer
 *               (for rendering the scene before ImGui overlay), and a simple
 *               demo geometry (a cube) so the editor has something to show
 *               before any actual scene data exists.
 *
 *               ARCHITECTURE:
 *                 PsxArtifactRenderer is constructed once at editor startup.
 *                 Each frame, the editor calls:
 *                   1. renderer.BeginScene(viewport_width, viewport_height)
 *                   2. renderer.DrawDebugCube(rotation_x, rotation_y)
 *                   3. renderer.EndScene() -> returns the FBO texture ID
 *                 The returned texture ID is displayed in the ImGui Scene panel
 *                 via ImGui::Image().
 *
 * DEPENDENCIES: OpenGL 3.3, <string>, <cstdint>
 *******************************************************************************/

#pragma once

#include <cstdint>
#include <string>

/*
 * PsxArtifactRenderer — Manages the OpenGL resources and shader pipeline for
 * PSX-style artifact rendering inside the desktop editor viewport.
 */
class PsxArtifactRenderer
{
public:
    /* Constructor / destructor — does NOT allocate GL resources.
     * Call Init() after a valid OpenGL context has been created. */
    PsxArtifactRenderer();
    ~PsxArtifactRenderer();

    /* Non-copyable — owns GPU resources (VAO, VBO, FBO, shader program). */
    PsxArtifactRenderer(const PsxArtifactRenderer&) = delete;
    PsxArtifactRenderer& operator=(const PsxArtifactRenderer&) = delete;

    /* -----------------------------------------------------------------------
     * Init — Initialize all OpenGL resources.
     *
     * Must be called once, after SDL_GL_CreateContext() has succeeded.
     * Loads and compiles the vertex/fragment shaders from the paths provided.
     *
     * @param vert_path  Filesystem path to psxArtifact.vert (relative OK)
     * @param frag_path  Filesystem path to psxArtifact.frag (relative OK)
     * @return           true on success, false if shader compilation fails.
     *                   On failure, check stderr for the GLSL error log.
     * ----------------------------------------------------------------------- */
    bool Init(const std::string& vert_path, const std::string& frag_path);

    /* -----------------------------------------------------------------------
     * Shutdown — Release all OpenGL resources.
     * Safe to call multiple times. Called automatically by destructor.
     * ----------------------------------------------------------------------- */
    void Shutdown();

    /* -----------------------------------------------------------------------
     * Resize — Recreate the framebuffer for the given viewport dimensions.
     * Call whenever the ImGui scene panel is resized.
     * ----------------------------------------------------------------------- */
    void Resize(int width, int height);

    /* -----------------------------------------------------------------------
     * BeginScene — Bind the FBO and clear for rendering.
     * ----------------------------------------------------------------------- */
    void BeginScene();

    /* -----------------------------------------------------------------------
     * DrawDebugCube — Render a simple colored cube with PSX artifact shader.
     *
     * This placeholder geometry demonstrates the artifact pipeline (vertex
     * snapping, affine UV) before real scene data is available.
     *
     * @param rot_x  Rotation angle around X axis (degrees, host float OK here)
     * @param rot_y  Rotation angle around Y axis (degrees)
     * ----------------------------------------------------------------------- */
    void DrawDebugCube(float rot_x, float rot_y);

    /* -----------------------------------------------------------------------
     * EndScene — Unbind the FBO and return the rendered texture ID.
     *
     * @return  OpenGL texture ID suitable for ImGui::Image().
     *          Cast to (ImTextureID) / (void*) before passing to ImGui.
     * ----------------------------------------------------------------------- */
    uint32_t EndScene();

    /* -----------------------------------------------------------------------
     * SetDitherEnabled — Toggle Bayer dithering pass.
     * ----------------------------------------------------------------------- */
    void SetDitherEnabled(bool enabled);

    /* -----------------------------------------------------------------------
     * IsInitialized — Returns true if Init() succeeded.
     * ----------------------------------------------------------------------- */
    bool IsInitialized() const { return m_initialized; }

    /* Current viewport dimensions (updated by Resize). */
    int GetViewportWidth()  const { return m_viewport_w; }
    int GetViewportHeight() const { return m_viewport_h; }

private:
    /* -----------------------------------------------------------------------
     * Private helpers
     * ----------------------------------------------------------------------- */

    /* Load a text file from disk. Returns empty string on failure. */
    static std::string LoadFile(const std::string& path);

    /* Compile a single shader stage. Returns 0 on failure. */
    static uint32_t CompileShader(uint32_t type, const char* source);

    /* Link the full program from compiled stages. Returns 0 on failure. */
    static uint32_t LinkProgram(uint32_t vert_id, uint32_t frag_id);

    /* Create / recreate the FBO and its color attachment texture. */
    void CreateFramebuffer(int w, int h);
    void DestroyFramebuffer();

    /* Build the cube VAO/VBO (called once in Init). */
    void BuildCubeGeometry();

    /* -----------------------------------------------------------------------
     * OpenGL resource handles
     * ----------------------------------------------------------------------- */

    uint32_t m_shader_program; /* Linked GLSL program (vert + frag)    */
    uint32_t m_fbo;            /* Framebuffer Object for offscreen pass */
    uint32_t m_fbo_texture;    /* Color attachment texture of the FBO   */
    uint32_t m_fbo_depth_rbo;  /* Depth renderbuffer attached to FBO    */
    uint32_t m_cube_vao;       /* Cube vertex array object              */
    uint32_t m_cube_vbo;       /* Cube vertex buffer object             */

    /* -----------------------------------------------------------------------
     * Uniform locations (cached at Init to avoid glGetUniformLocation every
     * frame — that function involves a hash lookup on some drivers).
     * ----------------------------------------------------------------------- */

    int m_loc_mvp;
    int m_loc_use_texture;
    int m_loc_dither;

    /* -----------------------------------------------------------------------
     * State
     * ----------------------------------------------------------------------- */

    int  m_viewport_w;     /* Current FBO width  */
    int  m_viewport_h;     /* Current FBO height */
    bool m_dither_enabled; /* Bayer dither toggle */
    bool m_initialized;    /* True after successful Init() */
};
