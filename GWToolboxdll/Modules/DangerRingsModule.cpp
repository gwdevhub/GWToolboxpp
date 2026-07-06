#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/Managers/MapMgr.h>

#include <Color.h>
#include <ImGuiAddons.h>
#include <Modules/DangerRingsModule.h>
#include <Timer.h>
#include <Utils/AoeEffects.h>
#include <Utils/GameWorldCompositor.h>
#include <Utils/SettingsRegistry.h>
#include <Utils/TerrainDrape.h>
#include <Widgets/Minimap/GameWorldRenderer.h>

namespace {
    constexpr int kSegments = 48;
    constexpr int kMaxBuildsPerFrame = 4; // draping QueryAltitude budget: ~200 queries per ring build

    // Occlusion (whether the rings hide behind terrain, and the depth-projection planes it needs) is shared with
    // the "In-game rendering" module via GameWorldRenderer::GetOccludeBehindTerrain()/GetDepthZNear()/GetDepthZFar()
    // so it's configured in one place. It defaults off there: with occlusion on, the ring and the terrain it hugs
    // land at nearly-equal depths, and the depth-test tie is resolved by the compositor's reconstructed projection
    // vs GW's real one - a mismatch that grows non-linearly with distance, so arcs drop in/out as the camera moves.
    float render_max_distance = 5000.f;
    float fog_factor = 1.0f;
    float ring_thickness = 40.f;
    float rim_opacity = 0.9f;
    float fill_opacity = 0.15f;
    float z_lift = 5.f; // raise above the floor to avoid z-fighting (GW up is -z)
    float pulse_speed = 1.2f;

    struct RingVertex {
        float x, y, z;
        DWORD color;
    };

    struct RingMesh {
        float built_range = 0.f;
        std::vector<RingVertex> verts; // baked with per-band base alpha; scaled by the envelope per frame
    };

    std::unordered_map<uint64_t, RingMesh> meshes;
    std::vector<AoeEffects::ActiveEffect> effects_snapshot;
    std::vector<RingVertex> scratch;
    bool meshes_dirty = false;
    int compositor_token = 0;

    void BuildRingMesh(const AoeEffects::ActiveEffect& effect, RingMesh& mesh, const uint32_t n_planes)
    {
        mesh.built_range = effect.range;
        mesh.verts.clear();

        const float center_z = TerrainDrape::SurfaceZ(effect.pos.x, effect.pos.y, effect.zplane, n_planes);

        const float rim_in = std::max(0.f, effect.range - ring_thickness);
        const float radii[] = {0.f, rim_in * 0.5f, rim_in, effect.range};
        const float band_opacity[] = {fill_opacity, fill_opacity, rim_opacity};
        constexpr int n_radii = static_cast<int>(std::size(radii));

        RingVertex points[n_radii][kSegments];
        for (int r = 0; r < n_radii; ++r) {
            for (int s = 0; s < kSegments; ++s) {
                const float angle = s * (DirectX::XM_2PI / kSegments);
                const float x = effect.pos.x + std::cos(angle) * radii[r];
                const float y = effect.pos.y + std::sin(angle) * radii[r];
                const float z = radii[r] > 0.f ? TerrainDrape::DrapeZ(x, y, effect.zplane, n_planes, center_z) : center_z;
                points[r][s] = {x, y, z - z_lift, 0};
            }
        }

        const Color base = effect.color; // resolved by allegiance in AoeEffects (enemy red / ally green)
        const auto band_color = [&](const int band) {
            const auto base_a = static_cast<float>((base >> IM_COL32_A_SHIFT) & 0xFF);
            const auto a = static_cast<DWORD>(std::clamp(base_a * band_opacity[band], 0.f, 255.f));
            return (base & ~(0xFFu << IM_COL32_A_SHIFT)) | (a << IM_COL32_A_SHIFT);
        };

        mesh.verts.reserve(static_cast<size_t>(n_radii - 1) * kSegments * 6);
        for (int band = 0; band < n_radii - 1; ++band) {
            const DWORD col = band_color(band);
            for (int s = 0; s < kSegments; ++s) {
                const int s1 = (s + 1) % kSegments;
                RingVertex a = points[band][s], b = points[band + 1][s], c = points[band + 1][s1], d = points[band][s1];
                a.color = b.color = c.color = d.color = col;
                mesh.verts.push_back(a);
                mesh.verts.push_back(b);
                mesh.verts.push_back(c);
                mesh.verts.push_back(a);
                mesh.verts.push_back(c);
                mesh.verts.push_back(d);
            }
        }
    }

    // Fade in fast, fade out at expiry, and pulse gently in between.
    float AlphaEnvelope(const AoeEffects::ActiveEffect& effect, const float t_seconds)
    {
        const auto elapsed = static_cast<float>(TIMER_DIFF(effect.start));
        const float remaining = static_cast<float>(effect.duration) - elapsed;
        float env = std::clamp(elapsed / 200.f, 0.f, 1.f) * std::clamp(remaining / 400.f, 0.f, 1.f);
        if (pulse_speed > 0.f) {
            env *= 0.8f + 0.2f * std::sin(t_seconds * pulse_speed * DirectX::XM_2PI);
        }
        return env;
    }
} // namespace

void DangerRingsModule::DrawInWorld(IDirect3DDevice9* device)
{
    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable) {
        meshes.clear();
        return;
    }
    AoeEffects::GetActiveEffects(effects_snapshot);
    if (effects_snapshot.empty()) {
        meshes.clear();
        return;
    }
    if (meshes_dirty) {
        meshes.clear();
        meshes_dirty = false;
    }

    const auto n_planes = TerrainDrape::PathingPlaneCount();

    std::erase_if(meshes, [](const auto& entry) {
        return std::ranges::none_of(effects_snapshot, [&entry](const auto& e) { return e.uid == entry.first; });
    });

    const float t_seconds = static_cast<float>(GetTickCount64() % 3600000) / 1000.f;
    scratch.clear();
    int builds = 0;
    for (const auto& effect : effects_snapshot) {
        if ((effect.color >> IM_COL32_A_SHIFT) == 0) continue; // this side is hidden (colour alpha 0)
        auto& mesh = meshes[effect.uid];
        if (mesh.verts.empty() || mesh.built_range != effect.range) {
            if (!n_planes || builds >= kMaxBuildsPerFrame) continue; // no altitude data yet / budget spent; retry next frame
            BuildRingMesh(effect, mesh, n_planes);
            ++builds;
        }
        const float env = AlphaEnvelope(effect, t_seconds);
        if (env <= 0.f) continue;
        for (const auto& v : mesh.verts) {
            RingVertex out = v;
            const auto a = static_cast<DWORD>(static_cast<float>((v.color >> IM_COL32_A_SHIFT) & 0xFF) * env);
            out.color = (v.color & ~(0xFFu << IM_COL32_A_SHIFT)) | (a << IM_COL32_A_SHIFT);
            scratch.push_back(out);
        }
    }
    if (scratch.empty()) return;

    IDirect3DStateBlock9* state_block = nullptr;
    if (device->CreateStateBlock(D3DSBT_ALL, &state_block) != D3D_OK) return;
    if (GameWorldCompositor::SetupPipeline(device, GameWorldRenderer::GetOccludeBehindTerrain(),
                                           GameWorldRenderer::GetDepthZNear(), GameWorldRenderer::GetDepthZFar(),
                                           render_max_distance, fog_factor)) {
        constexpr BOOL dotted_off[1] = {FALSE};
        device->SetPixelShaderConstantB(0, dotted_off, 1);
        device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, static_cast<UINT>(scratch.size() / 3), scratch.data(), sizeof(RingVertex));
    }
    state_block->Apply();
    state_block->Release();
}

void DangerRingsModule::RegisterSettings(ToolboxModule* module)
{
    SettingsRegistry::RegisterField(module, "render_max_distance", &render_max_distance);
    SettingsRegistry::RegisterField(module, "fog_factor", &fog_factor);
    SettingsRegistry::RegisterField(module, "ring_thickness", &ring_thickness);
    SettingsRegistry::RegisterField(module, "rim_opacity", &rim_opacity);
    SettingsRegistry::RegisterField(module, "fill_opacity", &fill_opacity);
    SettingsRegistry::RegisterField(module, "z_lift", &z_lift);
    SettingsRegistry::RegisterField(module, "pulse_speed", &pulse_speed);
}

void DangerRingsModule::Initialize()
{
    ToolboxModule::Initialize();
    RegisterSettings(this);
    AoeEffects::Initialize();
    if (!compositor_token) compositor_token = GameWorldCompositor::RegisterDraw(&DangerRingsModule::DrawInWorld);
}

void DangerRingsModule::SignalTerminate()
{
    if (compositor_token) {
        GameWorldCompositor::UnregisterDraw(compositor_token);
        compositor_token = 0;
    }
    meshes.clear();
}

void DangerRingsModule::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxModule::LoadSettings(doc, legacy);
    meshes_dirty = true;
}

void DangerRingsModule::DrawSettingsInternal()
{
    const auto red = ImGui::ColorConvertU32ToFloat4(Colors::Red());
    if (!GameWorldCompositor::IsActive())
        ImGui::TextColored(red, GameWorldCompositor::HasFailed() ? "In-world compositor FAILED to install." : "In-world compositor: not installed yet.");

    ImGui::TextDisabled("Occlusion behind terrain follows the \"In-game rendering\" module's setting.");
    ImGui::DragFloat("Maximum render distance", &render_max_distance, 5.f, 10.f, 100000.f, "%.0f", ImGuiSliderFlags_AlwaysClamp);
    if (ImGui::DragFloat("Ring thickness", &ring_thickness, 1.f, 5.f, 500.f, "%.0f", ImGuiSliderFlags_AlwaysClamp)) meshes_dirty = true;
    if (ImGui::DragFloat("Rim opacity", &rim_opacity, 0.01f, 0.f, 1.f, "%.2f", ImGuiSliderFlags_AlwaysClamp)) meshes_dirty = true;
    if (ImGui::DragFloat("Fill opacity", &fill_opacity, 0.01f, 0.f, 1.f, "%.2f", ImGuiSliderFlags_AlwaysClamp)) meshes_dirty = true;
    if (ImGui::DragFloat("Height lift", &z_lift, 0.5f, 0.f, 200.f, "%.1f", ImGuiSliderFlags_AlwaysClamp)) meshes_dirty = true;
    ImGui::DragFloat("Pulse speed", &pulse_speed, 0.05f, 0.f, 5.f, "%.2f Hz", ImGuiSliderFlags_AlwaysClamp);
    ImGui::ShowHelp("0 = no pulsing.");

    ImGui::Separator();
    if (AoeEffects::DrawColorSettings()) meshes_dirty = true;
}
