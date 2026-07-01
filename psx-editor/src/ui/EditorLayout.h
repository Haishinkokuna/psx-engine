/*******************************************************************************
 * FILE:         EditorLayout.h
 * MODULE:       Editor/UI
 * DESCRIPTION:  Dear ImGui docking layout manager for the PSX Editor.
 *
 *               Defines the three-panel editor layout:
 *
 *                 +---------------------------+-------------+
 *                 |                           |             |
 *                 |   Scene Viewport          |  Inspector  |
 *                 |   (PSX artifact render)   |  Panel      |
 *                 |                           |             |
 *                 +---------------------------+-------------+
 *                 |   Console / Log Panel                   |
 *                 +-----------------------------------------+
 *
 *               The docking layout is set up once on the first frame using
 *               ImGui's DockBuilder API (available when IMGUI_HAS_DOCK is
 *               defined). Subsequent frames just render into the named
 *               dock nodes — ImGui persists the layout automatically in
 *               imgui.ini.
 *
 *               PANEL RESPONSIBILITIES:
 *                 Viewport   — Hosts the PSX renderer FBO via ImGui::Image().
 *                              Reports current panel size so the renderer can
 *                              resize its FBO to match.
 *                 Inspector  — Placeholder for per-entity property editing.
 *                              Exposes renderer toggles (dither on/off) for now.
 *                 Console    — Scrolling text log for engine messages.
 *
 * DEPENDENCIES: Dear ImGui, PsxArtifactRenderer.h
 *******************************************************************************/

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "renderer/PsxArtifactRenderer.h"

/*
 * EditorLayout — manages the ImGui docking layout and panel rendering.
 */
class EditorLayout
{
public:
    EditorLayout();
    ~EditorLayout() = default;

    /* -----------------------------------------------------------------------
     * Init — Must be called once after ImGui context has been created.
     * Enables the ImGui docking feature and loads the docking layout.
     * ----------------------------------------------------------------------- */
    void Init();

    /* -----------------------------------------------------------------------
     * BeginFrame — Call at the start of each frame before any ImGui rendering.
     * Sets up the fullscreen dockspace that panels attach to.
     * ----------------------------------------------------------------------- */
    void BeginFrame();

    /* -----------------------------------------------------------------------
     * RenderScenePanel — Draws the Scene Viewport panel.
     *
     * Displays the rendered FBO texture from the PsxArtifactRenderer.
     * Reports the panel's content region size (used to resize the FBO).
     *
     * @param renderer         The renderer whose FBO to display.
     * @param out_panel_width  Output: current panel content width in pixels.
     * @param out_panel_height Output: current panel content height in pixels.
     * ----------------------------------------------------------------------- */
    void RenderScenePanel(PsxArtifactRenderer& renderer,
                          int& out_panel_width,
                          int& out_panel_height);

    /* -----------------------------------------------------------------------
     * RenderInspectorPanel — Draws the entity inspector / settings panel.
     *
     * @param renderer       Renderer whose settings (dither etc.) are exposed.
     * @param out_rot_x      Output: current debug cube X rotation (degrees).
     * @param out_rot_y      Output: current debug cube Y rotation (degrees).
     * ----------------------------------------------------------------------- */
    void RenderInspectorPanel(PsxArtifactRenderer& renderer,
                              float& out_rot_x,
                              float& out_rot_y);

    /* -----------------------------------------------------------------------
     * RenderConsolePanel — Draws the scrolling log console.
     * ----------------------------------------------------------------------- */
    void RenderConsolePanel();

    /* -----------------------------------------------------------------------
     * EndFrame — Call at the end of each frame after all panels are rendered.
     * ----------------------------------------------------------------------- */
    void EndFrame();

    /* -----------------------------------------------------------------------
     * Log — Append a message to the console panel.
     * Thread-safe: may be called from any thread (uses a simple mutex-free
     * ring buffer — the editor is single-threaded).
     * ----------------------------------------------------------------------- */
    void Log(const std::string& message);

private:
    /* Tracks whether the docking layout has been built (first-frame init). */
    bool m_layout_built;

    /* Rotation angles for the debug cube (controlled via inspector sliders). */
    float m_rot_x;
    float m_rot_y;

    /* Console message ring buffer. Capped at MAX_LOG_LINES to prevent
     * unbounded memory growth in long editor sessions. */
    static const int MAX_LOG_LINES = 500;
    std::vector<std::string> m_log_lines;
    bool m_scroll_to_bottom;

    /* Build the initial docking layout (called once on the first frame). */
    void BuildDockLayout(uint32_t dockspace_id);
};
