/*******************************************************************************
 * FILE:         EditorLayout.cpp
 * MODULE:       Editor/UI
 * DESCRIPTION:  Implementation of the Dear ImGui docking layout and panel
 *               rendering for the PSX Editor. See EditorLayout.h for overview.
 *
 * DEPENDENCIES: EditorLayout.h, imgui.h, imgui_internal.h (for DockBuilder)
 *******************************************************************************/

#include "EditorLayout.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <cstdio>

EditorLayout::EditorLayout()
    : m_layout_built(false)
    , m_rot_x(15.0f)
    , m_rot_y(30.0f)
    , m_scroll_to_bottom(false)
{
    m_log_lines.reserve(MAX_LOG_LINES);
}

void EditorLayout::Init()
{
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    /* Attempt to load retro PSX font */
    ImFont* font = io.Fonts->AddFontFromFileTTF("assets/psx_font.ttf", 16.0f);
    if (!font) {
        /* Fallback if missing */
        io.Fonts->AddFontDefault();
    }

    Log("[PSX Editor] Editor initialized. Welcome.");
    Log("[PSX Editor] Debug cube rendering with PSX artifact simulation.");
}

void EditorLayout::BeginFrame()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags host_flags =
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_NoBackground;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("##DockspaceHost", nullptr, host_flags);
    ImGui::PopStyleVar(3);

    ImGuiID dockspace_id = ImGui::GetID("PSXEditorDockspace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

    if (!m_layout_built) {
        BuildDockLayout(dockspace_id);
        m_layout_built = true;
    }

    ImGui::End();
}

void EditorLayout::BuildDockLayout(uint32_t dockspace_id)
{
    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id,
        ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->WorkSize);

    ImGuiID dock_right;
    ImGuiID dock_left = ImGui::DockBuilderSplitNode(
        dockspace_id, ImGuiDir_Left, 0.75f, nullptr, &dock_right);

    ImGuiID dock_viewport;
    ImGuiID dock_console = ImGui::DockBuilderSplitNode(
        dock_left, ImGuiDir_Down, 0.22f, nullptr, &dock_viewport);

    ImGui::DockBuilderDockWindow("Scene Viewport", dock_viewport);
    ImGui::DockBuilderDockWindow("Inspector",      dock_right);
    ImGui::DockBuilderDockWindow("Console",        dock_console);

    ImGui::DockBuilderFinish(dockspace_id);
}

void EditorLayout::RenderScenePanel(PsxArtifactRenderer& renderer,
                                     int& out_panel_width,
                                     int& out_panel_height)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("Scene Viewport");
    ImGui::PopStyleVar();

    ImVec2 region = ImGui::GetContentRegionAvail();
    out_panel_width  = (int)region.x;
    out_panel_height = (int)region.y;

    uint32_t tex_id = renderer.EndScene();
    ImGui::Image(
        (ImTextureID)(uintptr_t)tex_id,
        region,
        ImVec2(0, 1),
        ImVec2(1, 0)
    );

    /* Overlay: display mode label */
    ImVec2 overlay_pos = ImGui::GetItemRectMin();
    overlay_pos.x += 8.0f;
    overlay_pos.y += 8.0f;
    ImGui::GetForegroundDrawList()->AddText(
        overlay_pos,
        IM_COL32(200, 200, 200, 180),
        "PSX 320x240 Simulation"
    );

    ImGui::End();
}

void EditorLayout::RenderInspectorPanel(PsxArtifactRenderer& renderer,
                                         float& out_rot_x,
                                         float& out_rot_y)
{
    ImGui::Begin("Inspector");

    ImGui::TextDisabled("PSX Engine v0.1.0");
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Render Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        static bool dither_state = true;
        if (ImGui::Checkbox("Bayer Dithering (PSX)", &dither_state)) {
            renderer.SetDitherEnabled(dither_state);
            Log(dither_state ? "[Renderer] Dithering ON" : "[Renderer] Dithering OFF");
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(
                "The PSX GPU applied a 4x4 Bayer dither pass\n"
                "before writing to the 15bpp framebuffer.\n"
                "Toggle to compare quantized vs. dithered output.");
        }

        ImGui::Separator();
        ImGui::Text("Viewport: %dx%d",
            renderer.GetViewportWidth(),
            renderer.GetViewportHeight());
    }

    if (ImGui::CollapsingHeader("Debug Cube")) {
        ImGui::SliderFloat("Rotation X", &m_rot_x, -180.0f, 180.0f, "%.1f deg");
        ImGui::SliderFloat("Rotation Y", &m_rot_y, -180.0f, 180.0f, "%.1f deg");

        if (ImGui::Button("Reset Rotation")) {
            m_rot_x = 15.0f;
            m_rot_y = 30.0f;
        }
    }

    if (ImGui::CollapsingHeader("Scene Graph", ImGuiTreeNodeFlags_DefaultOpen)) {
        renderer.RenderSceneGraph();
    }

    if (ImGui::CollapsingHeader("Memory Map", ImGuiTreeNodeFlags_DefaultOpen)) {
        renderer.RenderMemoryMap();
    }

    if (ImGui::CollapsingHeader("VRAM Viewer", ImGuiTreeNodeFlags_DefaultOpen)) {
        renderer.RenderVRAMView();
    }

    if (ImGui::CollapsingHeader("PSX Hardware Specs")) {
        ImGui::BulletText("CPU:  MIPS R3000A @ 33.868 MHz");
        ImGui::BulletText("RAM:  2 MB Main / 1 KB Scratchpad");
        ImGui::BulletText("VRAM: 1 MB (1024x512 @ 15bpp)");
        ImGui::BulletText("GTE:  Coprocessor 2 (Fixed-point 3D)");
        ImGui::BulletText("No FPU. No float. Fixed-point only.");
    }

    out_rot_x = m_rot_x;
    out_rot_y = m_rot_y;

    ImGui::End();
}

void EditorLayout::RenderConsolePanel()
{
    ImGui::Begin("Console");

    if (ImGui::SmallButton("Clear")) {
        m_log_lines.clear();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%zu lines", m_log_lines.size());
    ImGui::Separator();

    ImGui::BeginChild("##console_scroll",
                       ImVec2(0.0f, 0.0f),
                       false,
                       ImGuiWindowFlags_HorizontalScrollbar);

    for (const std::string& line : m_log_lines) {
        if (line.find("[FAIL]") != std::string::npos ||
            line.find("ERROR") != std::string::npos) {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(220, 80, 80, 255));
        } else if (line.find("WARN") != std::string::npos) {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(220, 180, 50, 255));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(180, 210, 180, 255));
        }
        ImGui::TextUnformatted(line.c_str());
        ImGui::PopStyleColor();
    }

    if (m_scroll_to_bottom) {
        ImGui::SetScrollHereY(1.0f);
        m_scroll_to_bottom = false;
    }

    ImGui::EndChild();
    ImGui::End();
}

void EditorLayout::EndFrame()
{
    /* Reserved for per-frame layout state teardown. */
}

void EditorLayout::Log(const std::string& message)
{
    if ((int)m_log_lines.size() >= MAX_LOG_LINES) {
        m_log_lines.erase(m_log_lines.begin());
    }
    m_log_lines.push_back(message);
    m_scroll_to_bottom = true;
}
