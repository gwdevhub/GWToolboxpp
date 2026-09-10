// GWToolboxpp as a gw_in_browser wasm mod - the MVP entry point (mod_init contract, not a port of main.cpp/GWToolbox.cpp's DllMain/MinHook/D3D9 model); proves mod_init -> GW::Initialize() -> ImGui -> real GL draws end to end with a static demo window, real modules are deliberate follow-up work.

#include "stdafx.h"

#include <GWCA/GWCA.h>
#include <GWCA/Managers/RenderMgr.h>
#include <GWCA/Utilities/Version.h>

#include <Logger.h>

#include <chrono>
#include <cstdio>

#include <GLES3/gl3.h>

extern "C" {
    __attribute__((import_module("env"), import_name("emscripten_sleep")))
    void emscripten_sleep(unsigned int ms);
}

// FPS root cause (bisected via these toggles, see git history for the A/B trail): RenderDrawData's 6 blend-state glGetIntegerv backups were expensive under real GPU load, likely ANGLE resolving bound D3D11 state; fixed at the JS layer (gllib.js's SYNTHESIZED_BLEND_STATE). Toggles kept off in case of regression.
#define GWTOOLBOX_WASM_DISABLE_IMGUI 0
#define GWTOOLBOX_WASM_PROFILE_IMGUI 0
#define GWTOOLBOX_WASM_SKIP_RENDERDRAWDATA 0
#define GWTOOLBOX_WASM_TEST_STATE_QUERIES_ONLY 0
#define GWTOOLBOX_WASM_TEST_OBJECT_QUERIES_ONLY 0
#define GWTOOLBOX_WASM_TEST_SCALAR_QUERIES_ONLY 0
#define GWTOOLBOX_WASM_TEST_ISENABLED_ONLY 0
#define GWTOOLBOX_WASM_TEST_GETINTEGERV_SCALAR_ONLY 0
#define GWTOOLBOX_WASM_TEST_VIEWPORT_SCISSOR_ONLY 0
#define GWTOOLBOX_WASM_TEST_BLEND_SCALARS_ONLY 0
#define GWTOOLBOX_WASM_TEST_ACTIVE_TEXTURE_ONLY 0
#define GWTOOLBOX_WASM_TEST_BLEND_ENUMS_ONLY 0

// Still choppy plus visible corruption even with blend-state synthesis fixed, so the getter cost wasn't the whole story - isolates SetupRenderState's unconditional GL setters alone (no getters/VAO/restore) to test if they alone reproduce either symptom.
#define GWTOOLBOX_WASM_TEST_SETUPRENDERSTATE_SETTERS_ONLY 0

// Confirmed: SetupRenderState's setters alone, unrestored, are fast and clean - the game re-specifies its own state before drawing, so the remaining cost/corruption must be in actual drawing work; this adds a minimal self-contained draw to isolate that.
// Confirmed: that real draw, properly restored, still duplicated on screen - not a restore bug. Root cause: the old GR-queue hook fired once per GW::Render::VirtualDeviceRenderer command_buffer (3x/frame); fixed in GWCA by hooking FrCache_RenderAll instead, gated once per real frame.
#define GWTOOLBOX_WASM_TEST_SETUP_PLUS_DRAW_NO_RESTORE 1

namespace {
    bool g_imgui_ready = false;

    void EnsureImGuiInitialized()
    {
        if (g_imgui_ready) return;
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();

        // CreateFontsTexture sets GL_UNPACK_ROW_LENGTH and never restores it - plain global state, would corrupt GW's own later texture uploads if it relies on a non-default value (matches reported sheared/tiling textures).
        GLint prev_unpack_row_length = 0;
        glGetIntegerv(GL_UNPACK_ROW_LENGTH, &prev_unpack_row_length);

        ImGui_ImplOpenGL3_Init("#version 300 es");

        glPixelStorei(GL_UNPACK_ROW_LENGTH, prev_unpack_row_length);

        g_imgui_ready = true;
        Log::Log("GWToolboxpp wasm: ImGui + GLES3 backend initialised");
    }

#if GWTOOLBOX_WASM_TEST_SETUP_PLUS_DRAW_NO_RESTORE
    // Deliberately not ImGui's own shader/buffers - exercises the same GL call category as RenderDrawData without any of its backup/restore machinery.
    GLuint g_test_program = 0;
    GLuint g_test_vao = 0;
    GLuint g_test_vbo = 0;
    GLuint g_test_texture = 0;
    GLint g_test_tex_uniform_loc = -1;

    void CheckShaderCompile(GLuint shader, const char* label)
    {
        GLint ok = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[512];
            GLsizei len = 0;
            glGetShaderInfoLog(shader, sizeof(log), &len, log);
            Log::Error("GWToolboxpp wasm: %s shader compile failed: %s", label, log);
        }
    }

    void EnsureTestDrawInitialized()
    {
        if (g_test_program) return;
        // Samples a real 2x2 texture, not a solid-color shader - isolates whether actual sampling (not just a bound texture object) triggers the reported corruption.
        const char* vs_src =
            "#version 300 es\n"
            "layout(location=0) in vec2 aPos;\n"
            "out vec2 vUV;\n"
            "void main() { vUV = aPos * 0.5 + 0.5; gl_Position = vec4(aPos, 0.0, 1.0); }\n";
        const char* fs_src =
            "#version 300 es\n"
            "precision mediump float;\n"
            "in vec2 vUV;\n"
            "uniform sampler2D uTex;\n"
            "out vec4 FragColor;\n"
            "void main() { FragColor = texture(uTex, vUV); }\n";
        GLuint vs = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vs, 1, &vs_src, nullptr);
        glCompileShader(vs);
        CheckShaderCompile(vs, "vertex");
        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fs, 1, &fs_src, nullptr);
        glCompileShader(fs);
        CheckShaderCompile(fs, "fragment");
        g_test_program = glCreateProgram();
        glAttachShader(g_test_program, vs);
        glAttachShader(g_test_program, fs);
        glLinkProgram(g_test_program);
        GLint link_ok = 0;
        glGetProgramiv(g_test_program, GL_LINK_STATUS, &link_ok);
        if (!link_ok) {
            char log[512];
            GLsizei len = 0;
            glGetProgramInfoLog(g_test_program, sizeof(log), &len, log);
            Log::Error("GWToolboxpp wasm: test program link failed: %s", log);
        }
        glDeleteShader(vs);
        glDeleteShader(fs);
        g_test_tex_uniform_loc = glGetUniformLocation(g_test_program, "uTex");

        glGenBuffers(1, &g_test_vbo);

        glGenTextures(1, &g_test_texture);
        glBindTexture(GL_TEXTURE_2D, g_test_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        // Bright red/green checker so it's unmistakable if it renders correctly vs. bleeding/tiling.
        static const unsigned char pixels[2 * 2 * 4] = {
            255, 0, 0, 255,   0, 255, 0, 255,
            0, 255, 0, 255,   255, 0, 0, 255,
        };
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

        Log::Log("GWToolboxpp wasm: test draw program/buffer/texture initialised (program=%u)", g_test_program);
    }
#endif

    // Fires once per real displayed frame (GWCA's RenderMgr.cpp hooks FrCache_RenderAll for this now, not the raw GR-queue hook); lazily initialised since GlesDevice* only exists once this has fired once.
    void __cdecl OnRenderFinished(GW::Render::GlesDevice*)
    {
#if GWTOOLBOX_WASM_TEST_STATE_QUERIES_ONLY
        // Same pnames RenderDrawData backs up, minus GlVersion>=330/310-gated ones that don't apply to our "#version 300 es" init.
        GLint gi;
        glGetIntegerv(GL_ACTIVE_TEXTURE, &gi);
        glGetIntegerv(GL_CURRENT_PROGRAM, &gi);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &gi);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &gi);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &gi);
        GLint viewport[4]; glGetIntegerv(GL_VIEWPORT, viewport);
        GLint scissor[4]; glGetIntegerv(GL_SCISSOR_BOX, scissor);
        glGetIntegerv(GL_BLEND_SRC_RGB, &gi);
        glGetIntegerv(GL_BLEND_DST_RGB, &gi);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &gi);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &gi);
        glGetIntegerv(GL_BLEND_EQUATION_RGB, &gi);
        glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &gi);
        (void)glIsEnabled(GL_BLEND);
        (void)glIsEnabled(GL_CULL_FACE);
        (void)glIsEnabled(GL_DEPTH_TEST);
        (void)glIsEnabled(GL_STENCIL_TEST);
        (void)glIsEnabled(GL_SCISSOR_TEST);
        static bool logged_once = false;
        if (!logged_once) {
            Log::Log("GWToolboxpp wasm: OnGlesRender firing, state-queries-only diagnostic build");
            logged_once = true;
        }
        return;
#elif GWTOOLBOX_WASM_TEST_OBJECT_QUERIES_ONLY
        GLint gi;
        glGetIntegerv(GL_CURRENT_PROGRAM, &gi);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &gi);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &gi);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &gi);
        static bool logged_once = false;
        if (!logged_once) {
            Log::Log("GWToolboxpp wasm: OnGlesRender firing, object-queries-only diagnostic build");
            logged_once = true;
        }
        return;
#elif GWTOOLBOX_WASM_TEST_SCALAR_QUERIES_ONLY
        GLint gi;
        glGetIntegerv(GL_ACTIVE_TEXTURE, &gi);
        GLint viewport[4]; glGetIntegerv(GL_VIEWPORT, viewport);
        GLint scissor[4]; glGetIntegerv(GL_SCISSOR_BOX, scissor);
        glGetIntegerv(GL_BLEND_SRC_RGB, &gi);
        glGetIntegerv(GL_BLEND_DST_RGB, &gi);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &gi);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &gi);
        glGetIntegerv(GL_BLEND_EQUATION_RGB, &gi);
        glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &gi);
        (void)glIsEnabled(GL_BLEND);
        (void)glIsEnabled(GL_CULL_FACE);
        (void)glIsEnabled(GL_DEPTH_TEST);
        (void)glIsEnabled(GL_STENCIL_TEST);
        (void)glIsEnabled(GL_SCISSOR_TEST);
        static bool logged_once = false;
        if (!logged_once) {
            Log::Log("GWToolboxpp wasm: OnGlesRender firing, scalar-queries-only diagnostic build");
            logged_once = true;
        }
        return;
#elif GWTOOLBOX_WASM_TEST_ISENABLED_ONLY
        (void)glIsEnabled(GL_BLEND);
        (void)glIsEnabled(GL_CULL_FACE);
        (void)glIsEnabled(GL_DEPTH_TEST);
        (void)glIsEnabled(GL_STENCIL_TEST);
        (void)glIsEnabled(GL_SCISSOR_TEST);
        static bool logged_once = false;
        if (!logged_once) {
            Log::Log("GWToolboxpp wasm: OnGlesRender firing, isEnabled-only diagnostic build");
            logged_once = true;
        }
        return;
#elif GWTOOLBOX_WASM_TEST_GETINTEGERV_SCALAR_ONLY
        GLint gi;
        glGetIntegerv(GL_ACTIVE_TEXTURE, &gi);
        GLint viewport[4]; glGetIntegerv(GL_VIEWPORT, viewport);
        GLint scissor[4]; glGetIntegerv(GL_SCISSOR_BOX, scissor);
        glGetIntegerv(GL_BLEND_SRC_RGB, &gi);
        glGetIntegerv(GL_BLEND_DST_RGB, &gi);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &gi);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &gi);
        glGetIntegerv(GL_BLEND_EQUATION_RGB, &gi);
        glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &gi);
        static bool logged_once = false;
        if (!logged_once) {
            Log::Log("GWToolboxpp wasm: OnGlesRender firing, getIntegerv-scalar-only diagnostic build");
            logged_once = true;
        }
        return;
#elif GWTOOLBOX_WASM_TEST_VIEWPORT_SCISSOR_ONLY
        GLint viewport[4]; glGetIntegerv(GL_VIEWPORT, viewport);
        GLint scissor[4]; glGetIntegerv(GL_SCISSOR_BOX, scissor);
        static bool logged_once = false;
        if (!logged_once) {
            Log::Log("GWToolboxpp wasm: OnGlesRender firing, viewport-scissor-only diagnostic build");
            logged_once = true;
        }
        return;
#elif GWTOOLBOX_WASM_TEST_BLEND_SCALARS_ONLY
        GLint gi;
        glGetIntegerv(GL_ACTIVE_TEXTURE, &gi);
        glGetIntegerv(GL_BLEND_SRC_RGB, &gi);
        glGetIntegerv(GL_BLEND_DST_RGB, &gi);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &gi);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &gi);
        glGetIntegerv(GL_BLEND_EQUATION_RGB, &gi);
        glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &gi);
        static bool logged_once = false;
        if (!logged_once) {
            Log::Log("GWToolboxpp wasm: OnGlesRender firing, blend-scalars-only diagnostic build");
            logged_once = true;
        }
        return;
#elif GWTOOLBOX_WASM_TEST_ACTIVE_TEXTURE_ONLY
        GLint gi;
        glGetIntegerv(GL_ACTIVE_TEXTURE, &gi);
        static bool logged_once = false;
        if (!logged_once) {
            Log::Log("GWToolboxpp wasm: OnGlesRender firing, active-texture-only diagnostic build");
            logged_once = true;
        }
        return;
#elif GWTOOLBOX_WASM_TEST_BLEND_ENUMS_ONLY
        GLint gi;
        glGetIntegerv(GL_BLEND_SRC_RGB, &gi);
        glGetIntegerv(GL_BLEND_DST_RGB, &gi);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &gi);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &gi);
        glGetIntegerv(GL_BLEND_EQUATION_RGB, &gi);
        glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &gi);
        static bool logged_once = false;
        if (!logged_once) {
            Log::Log("GWToolboxpp wasm: OnGlesRender firing, blend-enums-only diagnostic build");
            logged_once = true;
        }
        return;
#elif GWTOOLBOX_WASM_TEST_SETUPRENDERSTATE_SETTERS_ONLY
        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_STENCIL_TEST);
        glEnable(GL_SCISSOR_TEST);
        glViewport(0, 0, static_cast<GLsizei>(GW::Render::GetViewportWidth()),
                   static_cast<GLsizei>(GW::Render::GetViewportHeight()));
        static bool logged_once = false;
        if (!logged_once) {
            Log::Log("GWToolboxpp wasm: OnGlesRender firing, SetupRenderState-setters-only diagnostic build (state left as-is, not restored)");
            logged_once = true;
        }
        return;
#elif GWTOOLBOX_WASM_TEST_SETUP_PLUS_DRAW_NO_RESTORE
        EnsureTestDrawInitialized();
        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_STENCIL_TEST);
        glEnable(GL_SCISSOR_TEST);
        const GLsizei fb_w = static_cast<GLsizei>(GW::Render::GetViewportWidth());
        const GLsizei fb_h = static_cast<GLsizei>(GW::Render::GetViewportHeight());
        glViewport(0, 0, fb_w, fb_h);
        glScissor(0, 0, fb_w, fb_h);

        // Confirmed: this same draw fully unrestored was fast but corrupted - GL_ARRAY_BUFFER/GL_CURRENT_PROGRAM are the suspect global (not per-VAO) state, restored here to test if that alone clears it.
        GLint prev_array_buffer = 0, prev_program = 0, prev_active_texture = 0, prev_texture = 0;
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &prev_array_buffer);
        glGetIntegerv(GL_CURRENT_PROGRAM, &prev_program);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &prev_active_texture);
        glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_texture);

        glGenVertexArrays(1, &g_test_vao);
        glBindVertexArray(g_test_vao);
        glBindBuffer(GL_ARRAY_BUFFER, g_test_vbo);
        // A quarter-screen quad, textured with the checker - big enough to clearly see tiling/bleeding, unlike the earlier tiny triangle.
        static const float verts[12] = {
            -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f,
            -1.0f, -1.0f,  0.0f, 0.0f,  -1.0f, 0.0f,
        };
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STREAM_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        glUseProgram(g_test_program);
        glBindTexture(GL_TEXTURE_2D, g_test_texture);
        if (g_test_tex_uniform_loc >= 0)
            glUniform1i(g_test_tex_uniform_loc, 0);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        const GLenum draw_err = glGetError();
        glDeleteVertexArrays(1, &g_test_vao);

        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(prev_texture));
        glActiveTexture(static_cast<GLenum>(prev_active_texture));
        glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(prev_array_buffer));
        glUseProgram(static_cast<GLuint>(prev_program));

        static bool logged_once = false;
        if (!logged_once) {
            Log::Log("GWToolboxpp wasm: test draw fb=%dx%d program=%u tex=%u err=0x%x",
                      fb_w, fb_h, g_test_program, g_test_texture, draw_err);
            logged_once = true;
        }
        return;
#elif GWTOOLBOX_WASM_DISABLE_IMGUI
        static bool logged_once = false;
        if (!logged_once) {
            Log::Log("GWToolboxpp wasm: OnGlesRender firing, ImGui disabled (diagnostic build)");
            logged_once = true;
        }
        return;
#else
        EnsureImGuiInitialized();

        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(static_cast<float>(GW::Render::GetViewportWidth()),
                                 static_cast<float>(GW::Render::GetViewportHeight()));
        // Fixed 60fps assumption - real per-frame delta timing is unported lifecycle work; fine for a static demo window.
        io.DeltaTime = 1.0f / 60.0f;

#if GWTOOLBOX_WASM_PROFILE_IMGUI
        using clock = std::chrono::steady_clock;
        const auto t0 = clock::now();
#endif

        ImGui_ImplOpenGL3_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("GWToolboxpp (wasm MVP)");
        ImGui::Text("Rendering pipeline is alive:");
        ImGui::BulletText("mod_init -> GW::Render::SetGlesRenderCallback");
        ImGui::BulletText("ImGui -> imgui_impl_opengl3");
        ImGui::BulletText("gw_in_browser's gllib.js -> the game's own WebGL2 context");
        ImGui::End();

#if GWTOOLBOX_WASM_PROFILE_IMGUI
        const auto t1 = clock::now();
#endif

        ImGui::Render();

#if GWTOOLBOX_WASM_PROFILE_IMGUI
        const auto t2 = clock::now();
#endif

#if !GWTOOLBOX_WASM_SKIP_RENDERDRAWDATA
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
#endif

#if GWTOOLBOX_WASM_PROFILE_IMGUI
        const auto t3 = clock::now();
        static double sum_newframe_ms = 0, sum_render_ms = 0, sum_renderdrawdata_ms = 0;
        static int sample_count = 0;
        sum_newframe_ms += std::chrono::duration<double, std::milli>(t1 - t0).count();
        sum_render_ms += std::chrono::duration<double, std::milli>(t2 - t1).count();
        sum_renderdrawdata_ms += std::chrono::duration<double, std::milli>(t3 - t2).count();
        if (++sample_count >= 120) {
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                "GWToolboxpp wasm: OnGlesRender/120 frames avg ms -- "
                "NewFrame+widgets: %.3f  ImGui::Render: %.3f  RenderDrawData: %.3f  total: %.3f",
                sum_newframe_ms / sample_count, sum_render_ms / sample_count,
                sum_renderdrawdata_ms / sample_count,
                (sum_newframe_ms + sum_render_ms + sum_renderdrawdata_ms) / sample_count);
            Log::Log(buf);
            sum_newframe_ms = sum_render_ms = sum_renderdrawdata_ms = 0;
            sample_count = 0;
        }
#endif
#endif
    }
} // namespace

extern "C" void mod_init()
{
    Log::Log("GWToolboxpp wasm: mod_init starting");

    if (!GW::Initialize()) {
        Log::Error("GWToolboxpp wasm: GW::Initialize() failed");
        return;
    }
    if (!GW::AbiVersionMatches()) {
        Log::Error("GWToolboxpp wasm: GWCA ABI mismatch -- vendored headers and "
                    "gwca.wasm disagree (re-run tools/update_gwca.py)");
        return;
    }
    GW::EnableHooks();

    if (GW::Render::GetBackend() != GW::Render::Backend::GLES3) {
        Log::Error("GWToolboxpp wasm: renderer backend is not GLES3 -- nothing to "
                    "hook (Param::GetFlag(10) picked D3D9? on a wasm client that "
                    "shouldn't happen)");
        return;
    }
    GW::Render::EnableHooks();
    // GWCA's RenderMgr.cpp fires this from FrCache_RenderAll (same anchor GameWorldCompositor.cpp uses natively), gated once per real frame - not the old GR-queue hook, which fired up to 3x. See RenderMgr.cpp.
    GW::Render::SetGlesRenderCallback(OnRenderFinished);

    Log::Log("GWToolboxpp wasm: render callback registered (kRenderFinished), staying resident");

    // Returning would retire this module in the loader's registry, but the render callback needs it resident; no termination/unload path exists yet, parking here for the MVP.
    for (;;) {
        emscripten_sleep(1000);
    }
}
