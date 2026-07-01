/*******************************************************************************
 * FILE:         PsxArtifactRenderer.cpp
 * MODULE:       Editor/Renderer
 * DESCRIPTION:  Implementation of the PSX artifact simulation renderer.
 *               See PsxArtifactRenderer.h for the full architecture overview.
 *
 * DEPENDENCIES: PsxArtifactRenderer.h, OpenGL 3.3 (via glad or system header),
 *               <cstdio>, <fstream>, <sstream>, <cmath>
 *******************************************************************************/

#include "PsxArtifactRenderer.h"

/* Platform-agnostic OpenGL headers.
 * SDL2 provides SDL_opengl.h which includes the correct platform header. */
#include <SDL_opengl.h>

#include <cstdio>
#include <cstring>
#include <cmath>
#include <fstream>
#include <sstream>

#include <imgui.h>

extern "C" {
    #include "../../../psx-runtime/core/scene/scene.h"
    #include "../../../psx-runtime/core/memory/heap.h"
}

/* ---------------------------------------------------------------------------
 * A minimal column-major 4x4 float matrix (host-side only — this is the
 * EDITOR running on modern hardware, not PSX runtime code).
 * We only need it to build the MVP matrix for the OpenGL rendering preview.
 * --------------------------------------------------------------------------- */

struct Mat4f {
    float m[16]; /* Column-major */

    static Mat4f Identity()
    {
        Mat4f r;
        memset(r.m, 0, sizeof(r.m));
        r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
        return r;
    }

    static Mat4f Mul(const Mat4f& a, const Mat4f& b)
    {
        Mat4f r;
        memset(r.m, 0, sizeof(r.m));
        for (int col = 0; col < 4; ++col)
            for (int row = 0; row < 4; ++row)
                for (int k = 0; k < 4; ++k)
                    r.m[col * 4 + row] += a.m[k * 4 + row] * b.m[col * 4 + k];
        return r;
    }

    static Mat4f RotX(float angle_rad)
    {
        Mat4f r = Identity();
        float c = cosf(angle_rad), s = sinf(angle_rad);
        r.m[5] =  c; r.m[9]  = -s;
        r.m[6] =  s; r.m[10] =  c;
        return r;
    }

    static Mat4f RotY(float angle_rad)
    {
        Mat4f r = Identity();
        float c = cosf(angle_rad), s = sinf(angle_rad);
        r.m[0] =  c; r.m[8]  =  s;
        r.m[2] = -s; r.m[10] =  c;
        return r;
    }

    /* Perspective projection matrix (column-major, OpenGL convention) */
    static Mat4f Perspective(float fov_rad, float aspect, float near_z, float far_z)
    {
        Mat4f r;
        memset(r.m, 0, sizeof(r.m));
        float f = 1.0f / tanf(fov_rad * 0.5f);
        r.m[0]  =  f / aspect;
        r.m[5]  =  f;
        r.m[10] = (far_z + near_z) / (near_z - far_z);
        r.m[11] = -1.0f;
        r.m[14] = (2.0f * far_z * near_z) / (near_z - far_z);
        return r;
    }

    /* Simple translation */
    static Mat4f Translate(float tx, float ty, float tz)
    {
        Mat4f r = Identity();
        r.m[12] = tx; r.m[13] = ty; r.m[14] = tz;
        return r;
    }
};

/* ---------------------------------------------------------------------------
 * Cube geometry: 36 vertices (6 faces * 2 triangles * 3 verts), interleaved:
 * [x, y, z,  u, v,  r, g, b]  — 8 floats per vertex
 * Each face has a distinct color to make Gouraud shading visible.
 * --------------------------------------------------------------------------- */

static const float CUBE_VERTICES[] = {
    /* Position         UV        Color (R,G,B) */
    /* Front face (+Z) — red */
    -0.5f,-0.5f, 0.5f,  0,0,  0.8f,0.2f,0.2f,
     0.5f,-0.5f, 0.5f,  1,0,  0.8f,0.2f,0.2f,
     0.5f, 0.5f, 0.5f,  1,1,  0.8f,0.2f,0.2f,
    -0.5f,-0.5f, 0.5f,  0,0,  0.8f,0.2f,0.2f,
     0.5f, 0.5f, 0.5f,  1,1,  0.8f,0.2f,0.2f,
    -0.5f, 0.5f, 0.5f,  0,1,  0.8f,0.2f,0.2f,
    /* Back face (-Z) — blue */
     0.5f,-0.5f,-0.5f,  0,0,  0.2f,0.2f,0.8f,
    -0.5f,-0.5f,-0.5f,  1,0,  0.2f,0.2f,0.8f,
    -0.5f, 0.5f,-0.5f,  1,1,  0.2f,0.2f,0.8f,
     0.5f,-0.5f,-0.5f,  0,0,  0.2f,0.2f,0.8f,
    -0.5f, 0.5f,-0.5f,  1,1,  0.2f,0.2f,0.8f,
     0.5f, 0.5f,-0.5f,  0,1,  0.2f,0.2f,0.8f,
    /* Left face (-X) — green */
    -0.5f,-0.5f,-0.5f,  0,0,  0.2f,0.8f,0.2f,
    -0.5f,-0.5f, 0.5f,  1,0,  0.2f,0.8f,0.2f,
    -0.5f, 0.5f, 0.5f,  1,1,  0.2f,0.8f,0.2f,
    -0.5f,-0.5f,-0.5f,  0,0,  0.2f,0.8f,0.2f,
    -0.5f, 0.5f, 0.5f,  1,1,  0.2f,0.8f,0.2f,
    -0.5f, 0.5f,-0.5f,  0,1,  0.2f,0.8f,0.2f,
    /* Right face (+X) — yellow */
     0.5f,-0.5f, 0.5f,  0,0,  0.8f,0.8f,0.2f,
     0.5f,-0.5f,-0.5f,  1,0,  0.8f,0.8f,0.2f,
     0.5f, 0.5f,-0.5f,  1,1,  0.8f,0.8f,0.2f,
     0.5f,-0.5f, 0.5f,  0,0,  0.8f,0.8f,0.2f,
     0.5f, 0.5f,-0.5f,  1,1,  0.8f,0.8f,0.2f,
     0.5f, 0.5f, 0.5f,  0,1,  0.8f,0.8f,0.2f,
    /* Top face (+Y) — cyan */
    -0.5f, 0.5f, 0.5f,  0,0,  0.2f,0.8f,0.8f,
     0.5f, 0.5f, 0.5f,  1,0,  0.2f,0.8f,0.8f,
     0.5f, 0.5f,-0.5f,  1,1,  0.2f,0.8f,0.8f,
    -0.5f, 0.5f, 0.5f,  0,0,  0.2f,0.8f,0.8f,
     0.5f, 0.5f,-0.5f,  1,1,  0.2f,0.8f,0.8f,
    -0.5f, 0.5f,-0.5f,  0,1,  0.2f,0.8f,0.8f,
    /* Bottom face (-Y) — magenta */
    -0.5f,-0.5f,-0.5f,  0,0,  0.8f,0.2f,0.8f,
     0.5f,-0.5f,-0.5f,  1,0,  0.8f,0.2f,0.8f,
     0.5f,-0.5f, 0.5f,  1,1,  0.8f,0.2f,0.8f,
    -0.5f,-0.5f,-0.5f,  0,0,  0.8f,0.2f,0.8f,
     0.5f,-0.5f, 0.5f,  1,1,  0.8f,0.2f,0.8f,
    -0.5f,-0.5f, 0.5f,  0,1,  0.8f,0.2f,0.8f,
};

/* ---------------------------------------------------------------------------
 * Constructor / Destructor
 * --------------------------------------------------------------------------- */

PsxArtifactRenderer::PsxArtifactRenderer()
    : m_shader_program(0)
    , m_fbo(0)
    , m_fbo_texture(0)
    , m_fbo_depth_rbo(0)
    , m_cube_vao(0)
    , m_cube_vbo(0)
    , m_loc_mvp(-1)
    , m_loc_use_texture(-1)
    , m_loc_dither(-1)
    , m_viewport_w(320)
    , m_viewport_h(240)
    , m_dither_enabled(true)
    , m_initialized(false)
{}

PsxArtifactRenderer::~PsxArtifactRenderer()
{
    Shutdown();
}

/* ---------------------------------------------------------------------------
 * LoadFile — Read an entire text file into a std::string.
 * --------------------------------------------------------------------------- */

std::string PsxArtifactRenderer::LoadFile(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        fprintf(stderr, "[PsxRenderer] ERROR: Cannot open shader file: %s\n", path.c_str());
        return "";
    }
    std::stringstream buf;
    buf << file.rdbuf();
    return buf.str();
}

/* ---------------------------------------------------------------------------
 * CompileShader — Compile a single GLSL stage. Returns 0 on failure.
 * --------------------------------------------------------------------------- */

uint32_t PsxArtifactRenderer::CompileShader(uint32_t type, const char* source)
{
    uint32_t id = glCreateShader(type);
    glShaderSource(id, 1, &source, nullptr);
    glCompileShader(id);

    /* Check for compilation errors and print the info log. */
    int success = 0;
    glGetShaderiv(id, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[1024];
        glGetShaderInfoLog(id, sizeof(log), nullptr, log);
        fprintf(stderr, "[PsxRenderer] Shader compile error:\n%s\n", log);
        glDeleteShader(id);
        return 0;
    }
    return id;
}

/* ---------------------------------------------------------------------------
 * LinkProgram — Link vertex and fragment shaders into a program.
 * --------------------------------------------------------------------------- */

uint32_t PsxArtifactRenderer::LinkProgram(uint32_t vert_id, uint32_t frag_id)
{
    uint32_t program = glCreateProgram();
    glAttachShader(program, vert_id);
    glAttachShader(program, frag_id);
    glLinkProgram(program);

    int success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[1024];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        fprintf(stderr, "[PsxRenderer] Program link error:\n%s\n", log);
        glDeleteProgram(program);
        return 0;
    }

    /* Shaders can be deleted once linked — the program owns the binary. */
    glDeleteShader(vert_id);
    glDeleteShader(frag_id);
    return program;
}

/* ---------------------------------------------------------------------------
 * Init
 * --------------------------------------------------------------------------- */

bool PsxArtifactRenderer::Init(const std::string& vert_path, const std::string& frag_path)
{
    /* Load shader sources from disk. */
    std::string vert_src = LoadFile(vert_path);
    std::string frag_src = LoadFile(frag_path);

    if (vert_src.empty() || frag_src.empty()) {
        return false;
    }

    /* Compile and link. */
    uint32_t vert_id = CompileShader(GL_VERTEX_SHADER,   vert_src.c_str());
    uint32_t frag_id = CompileShader(GL_FRAGMENT_SHADER, frag_src.c_str());

    if (vert_id == 0 || frag_id == 0) {
        return false;
    }

    m_shader_program = LinkProgram(vert_id, frag_id);
    if (m_shader_program == 0) {
        return false;
    }

    /* Cache uniform locations. -1 means the uniform was optimized out or
     * missing — log a warning but don't fail Init entirely. */
    m_loc_mvp         = glGetUniformLocation(m_shader_program, "uMVP");
    m_loc_use_texture = glGetUniformLocation(m_shader_program, "uUseTexture");
    m_loc_dither      = glGetUniformLocation(m_shader_program, "uDitherEnabled");

    /* Build the cube VAO/VBO. */
    BuildCubeGeometry();

    /* Create the initial FBO at the default viewport size. */
    CreateFramebuffer(m_viewport_w, m_viewport_h);

    glEnable(GL_DEPTH_TEST);

    m_initialized = true;
    fprintf(stdout, "[PsxRenderer] Initialized. Viewport: %dx%d\n",
            m_viewport_w, m_viewport_h);
    return true;
}

/* ---------------------------------------------------------------------------
 * Shutdown
 * --------------------------------------------------------------------------- */

void PsxArtifactRenderer::Shutdown()
{
    if (!m_initialized) return;

    DestroyFramebuffer();

    if (m_cube_vao) { glDeleteVertexArrays(1, &m_cube_vao); m_cube_vao = 0; }
    if (m_cube_vbo) { glDeleteBuffers(1, &m_cube_vbo);      m_cube_vbo = 0; }
    if (m_shader_program) { glDeleteProgram(m_shader_program); m_shader_program = 0; }

    m_initialized = false;
}

/* ---------------------------------------------------------------------------
 * CreateFramebuffer / DestroyFramebuffer
 * --------------------------------------------------------------------------- */

void PsxArtifactRenderer::CreateFramebuffer(int w, int h)
{
    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    /* Color attachment texture — the rendered PSX preview. */
    glGenTextures(1, &m_fbo_texture);
    glBindTexture(GL_TEXTURE_2D, m_fbo_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); /* No smoothing — PSX was nearest */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_fbo_texture, 0);

    /* Depth renderbuffer. */
    glGenRenderbuffers(1, &m_fbo_depth_rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, m_fbo_depth_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_fbo_depth_rbo);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "[PsxRenderer] ERROR: Framebuffer is incomplete!\n");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void PsxArtifactRenderer::DestroyFramebuffer()
{
    if (m_fbo_depth_rbo) { glDeleteRenderbuffers(1, &m_fbo_depth_rbo); m_fbo_depth_rbo = 0; }
    if (m_fbo_texture)   { glDeleteTextures(1, &m_fbo_texture);         m_fbo_texture   = 0; }
    if (m_fbo)           { glDeleteFramebuffers(1, &m_fbo);             m_fbo           = 0; }
}

/* ---------------------------------------------------------------------------
 * BuildCubeGeometry
 * --------------------------------------------------------------------------- */

void PsxArtifactRenderer::BuildCubeGeometry()
{
    glGenVertexArrays(1, &m_cube_vao);
    glGenBuffers(1, &m_cube_vbo);

    glBindVertexArray(m_cube_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_cube_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(CUBE_VERTICES), CUBE_VERTICES, GL_STATIC_DRAW);

    /* Stride = 8 floats per vertex = 32 bytes */
    const int STRIDE = 8 * sizeof(float);

    /* layout(location=0): position (vec3), offset 0 */
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, STRIDE, (void*)0);
    glEnableVertexAttribArray(0);

    /* layout(location=1): texcoord (vec2), offset 12 bytes */
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, STRIDE, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    /* layout(location=2): color (vec3), offset 20 bytes */
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, STRIDE, (void*)(5 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
}

/* ---------------------------------------------------------------------------
 * Resize
 * --------------------------------------------------------------------------- */

void PsxArtifactRenderer::Resize(int width, int height)
{
    if (width == m_viewport_w && height == m_viewport_h) return;
    m_viewport_w = width;
    m_viewport_h = height;
    DestroyFramebuffer();
    CreateFramebuffer(width, height);
}

/* ---------------------------------------------------------------------------
 * BeginScene
 * --------------------------------------------------------------------------- */

void PsxArtifactRenderer::BeginScene()
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, m_viewport_w, m_viewport_h);

    /* PSX background color was typically black. */
    glClearColor(0.04f, 0.04f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

/* ---------------------------------------------------------------------------
 * DrawDebugCube
 * --------------------------------------------------------------------------- */

void PsxArtifactRenderer::DrawDebugCube(float rot_x, float rot_y)
{
    if (!m_initialized) return;

    /* Build MVP: perspective projection * view (translate back) * rotation */
    float aspect = (m_viewport_h > 0)
                 ? (float)m_viewport_w / (float)m_viewport_h
                 : (320.0f / 240.0f);

    float fov_rad = 3.14159265f * 60.0f / 180.0f; /* 60-degree FOV */

    Mat4f proj  = Mat4f::Perspective(fov_rad, aspect, 0.1f, 100.0f);
    Mat4f view  = Mat4f::Translate(0.0f, 0.0f, -3.0f);
    Mat4f rot   = Mat4f::Mul(Mat4f::RotX(rot_x * 3.14159265f / 180.0f),
                              Mat4f::RotY(rot_y * 3.14159265f / 180.0f));
    Mat4f mvp   = Mat4f::Mul(proj, Mat4f::Mul(view, rot));

    glUseProgram(m_shader_program);

    /* Upload MVP matrix (column-major, as stored in Mat4f). */
    if (m_loc_mvp >= 0) {
        glUniformMatrix4fv(m_loc_mvp, 1, GL_FALSE, mvp.m);
    }

    /* No texture for the debug cube — use vertex colors (Gouraud). */
    if (m_loc_use_texture >= 0) {
        glUniform1i(m_loc_use_texture, 0);
    }

    /* Set dither toggle. */
    if (m_loc_dither >= 0) {
        glUniform1i(m_loc_dither, m_dither_enabled ? 1 : 0);
    }

    /* Draw 36 vertices (6 faces * 2 triangles * 3 verts). */
    glBindVertexArray(m_cube_vao);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

/* ---------------------------------------------------------------------------
 * EndScene
 * --------------------------------------------------------------------------- */

uint32_t PsxArtifactRenderer::EndScene()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return m_fbo_texture;
}

/* ---------------------------------------------------------------------------
 * SetDitherEnabled
 * --------------------------------------------------------------------------- */

void PsxArtifactRenderer::SetDitherEnabled(bool enabled)
{
    m_dither_enabled = enabled;
}

/* ---------------------------------------------------------------------------
 * RenderSceneGraph
 * --------------------------------------------------------------------------- */

void PsxArtifactRenderer::RenderSceneGraph()
{
    ImGui::Text("Active Entities: %u / %u", g_scene.active_count, MAX_ENTITIES);
    ImGui::Separator();

    if (ImGui::BeginChild("EntityTree", ImVec2(0, 0), true)) {
        for (uint32_t i = 0; i < g_scene.active_count; i++) {
            Entity* ent = Scene_GetEntityByIndex(i);
            if (!ent) continue;

            char label[64];
            snprintf(label, sizeof(label), "Entity %u (Mesh: %u)", ent->id, ent->mesh_id);

            if (ImGui::TreeNode(label)) {
                ImGui::Text("Position: %.2f, %.2f, %.2f", 
                    (float)ent->transform.position.vx / 4096.0f,
                    (float)ent->transform.position.vy / 4096.0f,
                    (float)ent->transform.position.vz / 4096.0f);
                ImGui::TreePop();
            }
        }
    }
    ImGui::EndChild();
}

/* ---------------------------------------------------------------------------
 * RenderMemoryMap
 * --------------------------------------------------------------------------- */

void PsxArtifactRenderer::RenderMemoryMap()
{
    ImGui::Text("Memory Usage (Main RAM & SPU RAM)");
    ImGui::Separator();

    /* 2MB Main RAM Progress */
    float main_usage = (float)g_heap_allocated / (2.0f * 1024.0f * 1024.0f);
    char main_overlay[64];
    snprintf(main_overlay, sizeof(main_overlay), "Main RAM: %u KB / 2048 KB", g_heap_allocated / 1024);
    ImGui::ProgressBar(main_usage, ImVec2(-1.0f, 0.0f), main_overlay);

    /* 512KB SPU RAM Progress (Mocked for now since spu_alloc is static in spu.c) */
    float spu_usage = 0.0f; /* We'd expose g_spu_alloc_ptr if we really needed it */
    ImGui::ProgressBar(spu_usage, ImVec2(-1.0f, 0.0f), "SPU RAM: 0 KB / 512 KB");

    /* 1KB Scratchpad Progress */
    ImGui::ProgressBar(0.0f, ImVec2(-1.0f, 0.0f), "Scratchpad: 0 Bytes / 1024 Bytes");
}

/* ---------------------------------------------------------------------------
 * RenderVRAMView
 * --------------------------------------------------------------------------- */

void PsxArtifactRenderer::RenderVRAMView()
{
    ImGui::Text("PSX VRAM Viewer (1024x512)");
    ImGui::Separator();

    ImVec2 avail = ImGui::GetContentRegionAvail();
    
    /* Calculate aspect ratio preserving size (2:1) */
    float aspect = 1024.0f / 512.0f;
    ImVec2 size = avail;
    if (size.x / aspect > size.y) {
        size.x = size.y * aspect;
    } else {
        size.y = size.x / aspect;
    }

    /* We just draw a colored rectangle to mock the VRAM preview since we 
     * don't have a real 1024x512 VRAM texture in our OpenGL mock yet. */
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 p1 = ImVec2(p0.x + size.x, p0.y + size.y);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(p0, p1, IM_COL32(20, 20, 40, 255));
    draw_list->AddRect(p0, p1, IM_COL32(255, 255, 255, 100));

    /* Draw mock display buffers */
    ImVec2 db1_0 = ImVec2(p0.x, p0.y);
    ImVec2 db1_1 = ImVec2(p0.x + (320.0f/1024.0f)*size.x, p0.y + (240.0f/512.0f)*size.y);
    draw_list->AddRectFilled(db1_0, db1_1, IM_COL32(80, 40, 40, 150));
    draw_list->AddText(ImVec2(db1_0.x + 4, db1_0.y + 4), IM_COL32(255,255,255,255), "DispBuf 1");

    ImVec2 db2_0 = ImVec2(p0.x, p0.y + (256.0f/512.0f)*size.y);
    ImVec2 db2_1 = ImVec2(p0.x + (320.0f/1024.0f)*size.x, p0.y + ((256.0f+240.0f)/512.0f)*size.y);
    draw_list->AddRectFilled(db2_0, db2_1, IM_COL32(40, 80, 40, 150));
    draw_list->AddText(ImVec2(db2_0.x + 4, db2_0.y + 4), IM_COL32(255,255,255,255), "DispBuf 2");

    ImGui::Dummy(size); /* Occupy space */
}
