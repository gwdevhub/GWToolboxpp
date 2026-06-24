#include "stdafx.h"

#include <DirectXMath.h>

#include <GWCA/GameEntities/Camera.h>
#include <GWCA/Managers/CameraMgr.h>
#include <GWCA/Managers/RenderMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/GameContainers/Array.h>
#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Scanner.h>

#include <Utils/GameWorldCompositor.h>

// The shared world shaders live with the original in-game renderer; both consumers use them.
#include "Widgets/Minimap/Shaders/game_world_renderer_vs.h"
#include "Widgets/Minimap/Shaders/game_world_renderer_dotted_ps.h"

namespace {
    // === FrCache compositor: draw registered overlays between GW's world and UI passes ===
    // GW's deferred command buffer holds FRCACHE_GPU_RENDER (3D world) then frame entries (HUD);
    // split at the boundary, flush the world, run the callbacks, then let GW render the HUD on top.
    enum FrCacheEntryType : uint32_t {
        FRCACHE_GPU_RENDER = 0,
        FRCACHE_FRAME_CALLBACK = 1,
        FRCACHE_CLIENT_VIEWPORT = 2,
        FRCACHE_FRAME_VIEWPORT = 3,
    };
    struct FrCacheBufferEntry {
        FrCacheEntryType type;
        uint32_t index;
        uint32_t param;
    };
    using FrCacheRenderFn = void(__cdecl*)(uint32_t, uint32_t);
    FrCacheRenderFn FrCacheRenderAll_Func = nullptr;
    FrCacheRenderFn FrCacheRenderAll_Ret = nullptr;
    GW::Array<FrCacheBufferEntry>* render_buffer = nullptr;
    GW::Array<GW::UI::Frame*>* frame_array = nullptr;
    bool compositor_scanned = false;
    bool compositor_failed = false;
    bool compositor_hooked = false;
    bool drawn_this_frame = false; // guard: draw only in the first world pass per frame

    // Registered overlay draws, invoked in registration order between world and HUD.
    std::vector<std::pair<int, GameWorldCompositor::DrawCallback>> callbacks;
    int next_token = 1;

    // === shared world-draw pipeline ===
    IDirect3DVertexShader9* vshader = nullptr;
    IDirect3DPixelShader9* pshader = nullptr;
    IDirect3DVertexDeclaration9* vertex_declaration = nullptr;
    bool need_configure_pipeline = true;

    void RunCallbacks(IDirect3DDevice9* device)
    {
        for (auto& entry : callbacks) {
            if (entry.second) {
                entry.second(device);
            }
        }
    }

    bool ScanCompositor()
    {
        if (compositor_scanned) return !compositor_failed;
        compositor_scanned = true;

        // FrCache_RenderAll asserts "frame" in FrCache.cpp at line 0x9e.
        const auto render_all = GW::Scanner::ToFunctionStart(
            GW::Scanner::FindAssertion("FrCache.cpp", "frame", 0x9e, 0), 0x200);
        if (!render_all) {
            compositor_failed = true;
            return false;
        }
        // FrCache_Render is the only caller of FrCache_RenderAll: scan back for the CALL.
        uintptr_t render_addr = 0;
        for (auto a = render_all - 5; a > render_all - 0x100; a--) {
            if (*reinterpret_cast<uint8_t*>(a) == 0xE8 && GW::Scanner::FunctionFromNearCall(a) == render_all) {
                render_addr = GW::Scanner::ToFunctionStart(a);
                break;
            }
        }
        if (!render_addr) {
            compositor_failed = true;
            return false;
        }
        // FrCache_Render opens with CMP [RenderFrameList.m_size], 0: the 4 bytes at +5 are &RenderFrameList.m_size; Array<T> globals are consecutive 16-byte structs, m_size at +8.
        const auto frame_list_size_addr = *reinterpret_cast<uintptr_t*>(render_addr + 5);
        const auto frame_list_base = frame_list_size_addr - 8;
        // Consecutive 16-byte Array<T> globals: RenderFrameList, FrameArray(+0x10), ..., RenderBuffer(+0x30).
        frame_array = reinterpret_cast<GW::Array<GW::UI::Frame*>*>(frame_list_base + 0x10);
        render_buffer = reinterpret_cast<GW::Array<FrCacheBufferEntry>*>(frame_list_base + 0x30);
        FrCacheRenderAll_Func = reinterpret_cast<FrCacheRenderFn>(render_all);
        if (!render_buffer || !FrCacheRenderAll_Func) {
            compositor_failed = true;
            return false;
        }
        return true;
    }

    void __cdecl OnFrCacheRenderAll(uint32_t param_1, uint32_t param_2)
    {
        GW::Hook::EnterHook();
        IDirect3DDevice9* device = GW::Render::GetDevice();

        // Nothing to do (no overlays, unusable buffer/device) -> run the original untouched.
        if (callbacks.empty() || !render_buffer || !device) {
            FrCacheRenderAll_Ret(param_1, param_2);
            GW::Hook::LeaveHook();
            return;
        }

        auto& buffer = *render_buffer;

        // The main 3D scene is the FIRST GPU_RENDER block; everything after is HUD - including the 2D HUD and
        // agent/UI 3D renders, which all correctly draw over us. Split right after the first GPU block. (Skipping
        // further consecutive GPU blocks would swallow GPU-rendered HUD entries into the world side, so our
        // overlays would then draw on top of them.) UI-only/sub-render passes have no GPU block, so boundary
        // stays == size and is skipped below.
        uint32_t boundary = buffer.size();
        for (uint32_t i = 0; i < buffer.size(); i++) {
            if (buffer[i].type == FRCACHE_GPU_RENDER) {
                boundary = i + 1;
                break;
            }
        }

        // Only the main world-then-HUD pass draws; others pass through, and the guard draws exactly once per frame.
        if (boundary == 0 || boundary >= buffer.size() || drawn_this_frame) {
            FrCacheRenderAll_Ret(param_1, param_2);
            GW::Hook::LeaveHook();
            return;
        }

        auto* const orig_buffer = buffer.m_buffer;
        const auto orig_size = buffer.m_size;

        // 1) world portion
        buffer.m_buffer = orig_buffer;
        buffer.m_size = boundary;
        FrCacheRenderAll_Ret(param_1, param_2);

        // 2) flush the world into the back/depth buffer, then run the registered overlays
        buffer.m_buffer = orig_buffer;
        buffer.m_size = orig_size;
        GW::Render::FlushCommandQueue();
        RunCallbacks(device);
        drawn_this_frame = true;

        // 3) HUD portion (drawn on top of the overlays)
        buffer.m_buffer = orig_buffer + boundary;
        buffer.m_size = orig_size - boundary;
        FrCacheRenderAll_Ret(param_1, param_2);

        // restore the buffer for GW
        buffer.m_buffer = orig_buffer;
        buffer.m_size = orig_size;

        GW::Hook::LeaveHook();
    }

    void EnsureHook()
    {
        if (compositor_hooked || compositor_failed) return;
        if (!ScanCompositor()) return;
        if (GW::Hook::CreateHook(reinterpret_cast<void**>(&FrCacheRenderAll_Func), OnFrCacheRenderAll,
                                 reinterpret_cast<void**>(&FrCacheRenderAll_Ret)) != 0) {
            compositor_failed = true;
            return;
        }
        GW::Hook::EnableHooks(FrCacheRenderAll_Func);
        compositor_hooked = true;
    }

    void RemoveHook()
    {
        if (compositor_hooked && FrCacheRenderAll_Func) {
            GW::Hook::RemoveHook(FrCacheRenderAll_Func);
            compositor_hooked = false;
        }
    }

    bool ConfigureProgrammablePipeline(IDirect3DDevice9* device)
    {
        constexpr D3DVERTEXELEMENT9 decl[] = {
            {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
            {0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
            D3DDECL_END()};
        if (device->CreateVertexDeclaration(decl, &vertex_declaration) != D3D_OK) {
            return false;
        }
        if (device->CreateVertexShader(reinterpret_cast<const DWORD*>(&game_world_renderer_vs), &vshader) != D3D_OK) {
            return false;
        }
        if (device->CreatePixelShader(reinterpret_cast<const DWORD*>(&game_world_renderer_dotted_ps), &pshader) != D3D_OK) {
            return false;
        }
        need_configure_pipeline = false;
        return true;
    }

    bool SetWorldTransform(IDirect3DDevice9* device, const float z_near, const float z_far)
    {
        // Build view/proj matrices matching GW's world-render camera.
        constexpr auto vertex_shader_view_matrix_offset = 0u;
        constexpr auto vertex_shader_proj_matrix_offset = 4u;

        const auto cam = GW::CameraMgr::GetCamera();
        if (!cam) {
            return false;
        }

        DirectX::XMFLOAT4X4A mat_view{};
        const DirectX::XMFLOAT3 eye_pos = {cam->position.x, cam->position.y, cam->position.z};
        const DirectX::XMFLOAT3 player_pos = {cam->look_at_target.x, cam->look_at_target.y, cam->look_at_target.z};
        constexpr DirectX::XMFLOAT3 up = {0.0f, 0.0f, -1.0f};
        XMStoreFloat4x4A(
            &mat_view,
            XMMatrixTranspose(
                DirectX::XMMatrixLookAtLH(XMLoadFloat3(&eye_pos), XMLoadFloat3(&player_pos), XMLoadFloat3(&up))
            )
        );
        if (device->SetVertexShaderConstantF(vertex_shader_view_matrix_offset, reinterpret_cast<const float*>(&mat_view), 4) != D3D_OK) {
            return false;
        }

        DirectX::XMFLOAT4X4A mat_proj{};
        const auto fov = GW::Render::GetFieldOfView();
        const auto aspect_ratio = static_cast<float>(GW::Render::GetViewportWidth()) / static_cast<float>(GW::Render::GetViewportHeight());
        XMStoreFloat4x4A(
            &mat_proj,
            XMMatrixTranspose(
                DirectX::XMMatrixPerspectiveFovLH(fov, aspect_ratio, z_near, z_far)
            )
        );
        if (device->SetVertexShaderConstantF(vertex_shader_proj_matrix_offset, reinterpret_cast<const float*>(&mat_proj), 4) != D3D_OK) {
            return false;
        }
        return true;
    }
} // namespace

int GameWorldCompositor::RegisterDraw(DrawCallback callback)
{
    if (!callback) {
        return 0;
    }
    // Defer the actual hook install to BeginFrame (render time): scanning needs game memory ready.
    const int token = next_token++;
    callbacks.emplace_back(token, std::move(callback));
    return token;
}

void GameWorldCompositor::UnregisterDraw(const int token)
{
    if (token <= 0) {
        return;
    }
    std::erase_if(callbacks, [token](const auto& entry) { return entry.first == token; });
    if (callbacks.empty()) {
        RemoveHook();
    }
}

bool GameWorldCompositor::IsActive()
{
    return compositor_hooked && !compositor_failed;
}

bool GameWorldCompositor::HasFailed()
{
    return compositor_failed;
}

void GameWorldCompositor::BeginFrame()
{
    // Install the hook lazily once something wants to draw; retried each frame until it succeeds or fails.
    if (!callbacks.empty()) {
        EnsureHook();
    }
    drawn_this_frame = false;
}

bool GameWorldCompositor::SetWorldViewProj(IDirect3DDevice9* device, const float z_near, const float z_far)
{
    return device && SetWorldTransform(device, z_near, z_far);
}

void GameWorldCompositor::SetWorldRenderStates(IDirect3DDevice9* device, const bool occlude)
{
    if (!device) {
        return;
    }
    // Fully specify the pipeline: GW's ambient UI state (alpha test/cull/fog/colour-write) would otherwise discard our draw. The caller's state block restores it on exit.
    device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
    device->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
    device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    device->SetRenderState(D3DRS_LIGHTING, FALSE);
    device->SetRenderState(D3DRS_FOGENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    device->SetRenderState(D3DRS_COLORWRITEENABLE,
                           D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
    device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

    if (occlude) {
        // Depth-test against the scene so geometry occludes the overlay; never write depth (must not disturb GW's values).
        device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
        device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
    }
    else {
        device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
    }
}

void GameWorldCompositor::SetDistanceFog(IDirect3DDevice9* device, const float max_distance, const float fog_factor)
{
    if (!device) {
        return;
    }
    const GW::Camera* cam = GW::CameraMgr::GetCamera();
    // Pixel shader constants (all Float4): c0 camera focus, c1 max distance, c2 fog start.
    const float cur_pos_constant[4] = {cam ? cam->look_at_target.x : 0.f, cam ? cam->look_at_target.y : 0.f, cam ? cam->look_at_target.z : 0.f, 0.0f};
    device->SetPixelShaderConstantF(0, cur_pos_constant, 1);
    const float max_dist_constant[4] = {max_distance, 0.0f, 0.0f, 0.0f};
    device->SetPixelShaderConstantF(1, max_dist_constant, 1);
    const float fog_starts_at_constant[4] = {max_distance - max_distance * fog_factor, 0.0f, 0.0f, 0.0f};
    device->SetPixelShaderConstantF(2, fog_starts_at_constant, 1);
}

bool GameWorldCompositor::SetupPipeline(IDirect3DDevice9* device, const bool occlude, const float z_near, const float z_far,
                                        const float max_distance, const float fog_factor)
{
    if (!device) {
        return false;
    }
    if (need_configure_pipeline && !ConfigureProgrammablePipeline(device)) {
        return false;
    }
    if (device->SetVertexShader(vshader) != D3D_OK
        || device->SetPixelShader(pshader) != D3D_OK
        || device->SetVertexDeclaration(vertex_declaration) != D3D_OK
        || !SetWorldTransform(device, z_near, z_far)) {
        return false;
    }
    SetWorldRenderStates(device, occlude);
    SetDistanceFog(device, max_distance, fog_factor);
    return true;
}

IDirect3DVertexShader9* GameWorldCompositor::VertexShader() { return vshader; }
IDirect3DPixelShader9* GameWorldCompositor::PixelShader() { return pshader; }
IDirect3DVertexDeclaration9* GameWorldCompositor::VertexDeclaration() { return vertex_declaration; }

void GameWorldCompositor::Terminate()
{
    RemoveHook();
    callbacks.clear();
    if (vshader) {
        vshader->Release();
        vshader = nullptr;
    }
    if (pshader) {
        pshader->Release();
        pshader = nullptr;
    }
    if (vertex_declaration) {
        vertex_declaration->Release();
        vertex_declaration = nullptr;
    }
    need_configure_pipeline = true;
}
