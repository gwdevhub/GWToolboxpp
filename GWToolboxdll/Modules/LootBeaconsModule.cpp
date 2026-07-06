#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Item.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/MapMgr.h>

#include <Color.h>
#include <ImGuiAddons.h>
#include <Modules/LootBeaconsModule.h>
#include <Modules/PriceCheckerModule.h>
#include <Utils/GameWorldCompositor.h>
#include <Utils/SettingsRegistry.h>
#include <Utils/TerrainDrape.h>
#include <Utils/ToolboxUtils.h>
#include <Widgets/Minimap/GameWorldRenderer.h>

namespace {
    constexpr int kRingSegments = 32;
    constexpr float kRingBandWidth = 12.f;
    constexpr int kMaxBuildsPerFrame = 8;   // draping QueryAltitude budget: ~70 queries per beacon build
    constexpr uint32_t kScanIntervalMs = 250; // item agents don't move; classification only needs a coarse tick

    // Occlusion behind terrain (and its depth-projection planes) is shared with the "In-game rendering" module
    // via GameWorldRenderer::GetOccludeBehindTerrain()/GetDepthZNear()/GetDepthZFar() so it's set in one place.
    float render_max_distance = 20000.f;
    float fog_factor = 0.5f;
    float beam_height = 225.f;
    float beam_width = 35.f;
    float beam_opacity = 0.5f;
    float ring_radius = 45.f;
    float ring_opacity = 0.8f;
    float z_lift = 5.f; // raise above the floor to avoid z-fighting (GW up is -z)
    float pulse_speed = 0.8f;
    bool show_reserved_for_others = false;

    bool enable_value_beacons = true;
    int value_threshold = 1000; // gold, against the Kamadan trader price
    Color value_color = Colors::ARGB(255, 255, 140, 0);

    struct RarityBeacon {
        const char* label;
        bool enabled;
        Color color;
    };
    RarityBeacon rarity_white = {"White items", false, Colors::ARGB(255, 255, 255, 255)};
    RarityBeacon rarity_blue = {"Blue items", false, Colors::ARGB(255, 80, 160, 255)};
    RarityBeacon rarity_purple = {"Purple items", true, Colors::ARGB(255, 180, 80, 250)};
    RarityBeacon rarity_gold = {"Gold items", true, Colors::ARGB(255, 255, 210, 60)};
    RarityBeacon rarity_green = {"Green items", true, Colors::ARGB(255, 40, 220, 40)};

    struct BeaconVertex {
        float x, y, z;
        DWORD color;
    };

    struct Beacon {
        GW::Vec2f pos;
        uint32_t zplane = 0;
        Color color = 0;
        bool draw = false;
        bool dimmed = false;
        bool built = false;
        uint32_t seen = 0;
        std::vector<BeaconVertex> verts; // baked with base alpha; scaled by the pulse envelope per frame
    };

    std::unordered_map<uint32_t, Beacon> beacons;
    std::vector<BeaconVertex> scratch;
    uint32_t scan_counter = 0;
    uint64_t last_scan_tick = 0;
    bool beacons_dirty = false;
    int compositor_token = 0;

    DWORD WithAlpha(const Color color, const float alpha_factor)
    {
        const auto base_a = static_cast<float>((color >> IM_COL32_A_SHIFT) & 0xFF);
        const auto a = static_cast<DWORD>(std::clamp(base_a * alpha_factor, 0.f, 255.f));
        return (color & ~(0xFFu << IM_COL32_A_SHIFT)) | (a << IM_COL32_A_SHIFT);
    }

    void Classify(const GW::AgentItem& agent_item, const GW::Item& item, const uint32_t my_agent_id, Beacon& beacon)
    {
        const bool mine = !agent_item.owner || agent_item.owner == my_agent_id;
        beacon.dimmed = !mine;
        Color color = 0;
        bool draw = false;
        if (mine || show_reserved_for_others) {
            if (enable_value_beacons && value_threshold > 0 && PriceCheckerModule::GetPriceByItem(&item) >= static_cast<uint32_t>(value_threshold)) {
                color = value_color;
                draw = true;
            }
            else {
                const RarityBeacon* by_rarity = nullptr;
                switch (GW::Items::GetRarity(&item)) {
                    case GW::Constants::Rarity::White: by_rarity = &rarity_white; break;
                    case GW::Constants::Rarity::Blue: by_rarity = &rarity_blue; break;
                    case GW::Constants::Rarity::Purple: by_rarity = &rarity_purple; break;
                    case GW::Constants::Rarity::Gold: by_rarity = &rarity_gold; break;
                    case GW::Constants::Rarity::Green: by_rarity = &rarity_green; break;
                    default: break;
                }
                if (by_rarity && by_rarity->enabled) {
                    color = by_rarity->color;
                    draw = true;
                }
            }
        }
        if (draw && color != beacon.color) beacon.built = false;
        beacon.color = color;
        beacon.draw = draw;
    }

    void ScanItems()
    {
        ++scan_counter;
        const auto* agents = GW::Agents::GetAgentArray();
        if (!agents) {
            beacons.clear();
            return;
        }
        const auto my_agent_id = GW::Agents::GetControlledCharacterId();
        for (const auto* agent : *agents) {
            const auto* agent_item = agent ? agent->GetAsAgentItem() : nullptr;
            if (!agent_item) continue;
            const auto* item = GW::Items::GetItemById(agent_item->item_id);
            if (!item) continue;
            auto& beacon = beacons[agent_item->agent_id];
            beacon.seen = scan_counter;
            beacon.pos = {agent_item->pos.x, agent_item->pos.y};
            beacon.zplane = agent_item->pos.zplane;
            Classify(*agent_item, *item, my_agent_id, beacon);
        }
        std::erase_if(beacons, [](const auto& entry) { return entry.second.seen != scan_counter; });
    }

    bool BuildBeacon(Beacon& beacon, const uint32_t n_planes)
    {
        float base_z = TerrainDrape::SurfaceZ(beacon.pos.x, beacon.pos.y, beacon.zplane, n_planes);
        if (base_z == 0.f) return false; // no altitude data yet; retry next frame
        base_z -= z_lift;

        beacon.verts.clear();
        beacon.verts.reserve(kRingSegments * 6 + 12);

        const DWORD ring_col = WithAlpha(beacon.color, ring_opacity);
        const float r_out = std::max(1.f, ring_radius);
        const float r_in = std::max(0.f, r_out - kRingBandWidth);
        BeaconVertex inner[kRingSegments], outer[kRingSegments];
        for (int s = 0; s < kRingSegments; ++s) {
            const float angle = s * (DirectX::XM_2PI / kRingSegments);
            const float cos_a = std::cos(angle), sin_a = std::sin(angle);
            const float xi = beacon.pos.x + cos_a * r_in, yi = beacon.pos.y + sin_a * r_in;
            const float xo = beacon.pos.x + cos_a * r_out, yo = beacon.pos.y + sin_a * r_out;
            inner[s] = {xi, yi, TerrainDrape::DrapeZ(xi, yi, beacon.zplane, n_planes, base_z) - z_lift, ring_col};
            outer[s] = {xo, yo, TerrainDrape::DrapeZ(xo, yo, beacon.zplane, n_planes, base_z) - z_lift, ring_col};
        }
        for (int s = 0; s < kRingSegments; ++s) {
            const int s1 = (s + 1) % kRingSegments;
            beacon.verts.push_back(inner[s]);
            beacon.verts.push_back(outer[s]);
            beacon.verts.push_back(outer[s1]);
            beacon.verts.push_back(inner[s]);
            beacon.verts.push_back(outer[s1]);
            beacon.verts.push_back(inner[s1]);
        }

        // Two crossed vertical quads read as a pillar from any camera angle without per-frame billboarding.
        const DWORD bottom_col = WithAlpha(beacon.color, beam_opacity);
        const DWORD top_col = WithAlpha(beacon.color, 0.f);
        const float half = std::max(1.f, beam_width) * 0.5f;
        const float z_top = base_z - std::max(1.f, beam_height);
        const auto quad = [&](const float x0, const float y0, const float x1, const float y1) {
            const BeaconVertex b0 = {x0, y0, base_z, bottom_col}, b1 = {x1, y1, base_z, bottom_col};
            const BeaconVertex t0 = {x0, y0, z_top, top_col}, t1 = {x1, y1, z_top, top_col};
            beacon.verts.push_back(b0);
            beacon.verts.push_back(b1);
            beacon.verts.push_back(t1);
            beacon.verts.push_back(b0);
            beacon.verts.push_back(t1);
            beacon.verts.push_back(t0);
        };
        quad(beacon.pos.x - half, beacon.pos.y, beacon.pos.x + half, beacon.pos.y);
        quad(beacon.pos.x, beacon.pos.y - half, beacon.pos.x, beacon.pos.y + half);

        beacon.built = true;
        return true;
    }
} // namespace

void LootBeaconsModule::DrawInWorld(IDirect3DDevice9* device)
{
    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable) {
        beacons.clear();
        return;
    }
    if (beacons_dirty) {
        for (auto& [id, beacon] : beacons) beacon.built = false;
        beacons_dirty = false;
    }

    const auto now = GetTickCount64();
    if (now - last_scan_tick >= kScanIntervalMs) {
        last_scan_tick = now;
        ScanItems();
    }
    if (beacons.empty()) return;

    const auto n_planes = TerrainDrape::PathingPlaneCount();
    const float t_seconds = static_cast<float>(now % 3600000) / 1000.f;

    scratch.clear();
    int builds = 0;
    for (auto& [agent_id, beacon] : beacons) {
        if (!beacon.draw) continue;
        if (!beacon.built) {
            if (!n_planes || builds >= kMaxBuildsPerFrame) continue;
            ++builds;
            if (!BuildBeacon(beacon, n_planes)) continue;
        }
        float env = 1.f;
        if (pulse_speed > 0.f) {
            const float phase = static_cast<float>(agent_id % 628) / 100.f;
            env = 0.75f + 0.25f * std::sin(t_seconds * pulse_speed * DirectX::XM_2PI + phase);
        }
        if (beacon.dimmed) env *= 0.4f;
        for (const auto& v : beacon.verts) {
            BeaconVertex out = v;
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
        device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, static_cast<UINT>(scratch.size() / 3), scratch.data(), sizeof(BeaconVertex));
    }
    state_block->Apply();
    state_block->Release();
}

void LootBeaconsModule::RegisterSettings(ToolboxModule* module)
{
    SettingsRegistry::RegisterField(module, "render_max_distance", &render_max_distance);
    SettingsRegistry::RegisterField(module, "fog_factor", &fog_factor);
    SettingsRegistry::RegisterField(module, "beam_height", &beam_height);
    SettingsRegistry::RegisterField(module, "beam_width", &beam_width);
    SettingsRegistry::RegisterField(module, "beam_opacity", &beam_opacity);
    SettingsRegistry::RegisterField(module, "ring_radius", &ring_radius);
    SettingsRegistry::RegisterField(module, "ring_opacity", &ring_opacity);
    SettingsRegistry::RegisterField(module, "z_lift", &z_lift);
    SettingsRegistry::RegisterField(module, "pulse_speed", &pulse_speed);
    SettingsRegistry::RegisterField(module, "show_reserved_for_others", &show_reserved_for_others);
    SettingsRegistry::RegisterField(module, "enable_value_beacons", &enable_value_beacons);
    SettingsRegistry::RegisterField(module, "value_threshold", &value_threshold);
    SettingsRegistry::RegisterField(module, "value_color", &value_color);
    SettingsRegistry::RegisterField(module, "white_enabled", &rarity_white.enabled);
    SettingsRegistry::RegisterField(module, "white_color", &rarity_white.color);
    SettingsRegistry::RegisterField(module, "blue_enabled", &rarity_blue.enabled);
    SettingsRegistry::RegisterField(module, "blue_color", &rarity_blue.color);
    SettingsRegistry::RegisterField(module, "purple_enabled", &rarity_purple.enabled);
    SettingsRegistry::RegisterField(module, "purple_color", &rarity_purple.color);
    SettingsRegistry::RegisterField(module, "gold_enabled", &rarity_gold.enabled);
    SettingsRegistry::RegisterField(module, "gold_color", &rarity_gold.color);
    SettingsRegistry::RegisterField(module, "green_enabled", &rarity_green.enabled);
    SettingsRegistry::RegisterField(module, "green_color", &rarity_green.color);
}

void LootBeaconsModule::Initialize()
{
    ToolboxModule::Initialize();
    RegisterSettings(this);
    if (!compositor_token) compositor_token = GameWorldCompositor::RegisterDraw(&LootBeaconsModule::DrawInWorld);
}

void LootBeaconsModule::SignalTerminate()
{
    if (compositor_token) {
        GameWorldCompositor::UnregisterDraw(compositor_token);
        compositor_token = 0;
    }
    beacons.clear();
}

void LootBeaconsModule::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxModule::LoadSettings(doc, legacy);
    beacons_dirty = true;
}

void LootBeaconsModule::DrawSettingsInternal()
{
    const auto red = ImGui::ColorConvertU32ToFloat4(Colors::Red());
    if (!GameWorldCompositor::IsActive())
        ImGui::TextColored(red, GameWorldCompositor::HasFailed() ? "In-world compositor FAILED to install." : "In-world compositor: not installed yet.");

    ImGui::Checkbox("Beacon on valuable items", &enable_value_beacons);
    ImGui::ShowHelp("Any drop whose Kamadan trader price meets the threshold gets a beacon,\nregardless of rarity - catches ectos, gemstones, dyes and other white-rarity valuables.");
    if (enable_value_beacons) {
        Colors::DrawSettingHueWheel("##value_color", &value_color);
        ImGui::DragInt("Value threshold (gold)", &value_threshold, 50.f, 0, 1000000);
    }
    ImGui::Separator();
    ImGui::TextUnformatted("Beacon by rarity");
    for (auto* rarity : {&rarity_gold, &rarity_green, &rarity_purple, &rarity_blue, &rarity_white}) {
        ImGui::PushID(rarity->label);
        if (ImGui::Checkbox(rarity->label, &rarity->enabled)) beacons_dirty = true;
        ImGui::SameLine(180.f);
        Colors::DrawSettingHueWheel("##color", &rarity->color);
        ImGui::PopID();
    }
    if (ImGui::Checkbox("Show beacons on items reserved for other party members", &show_reserved_for_others)) beacons_dirty = true;
    ImGui::ShowHelp("Drawn dimmed. Off: only unreserved drops and drops assigned to you.");

    ImGui::Separator();
    ImGui::TextDisabled("Occlusion behind terrain follows the \"In-game rendering\" module's setting.");
    ImGui::DragFloat("Maximum render distance", &render_max_distance, 25.f, 10.f, 100000.f, "%.0f", ImGuiSliderFlags_AlwaysClamp);
    if (ImGui::DragFloat("Beam height", &beam_height, 5.f, 50.f, 5000.f, "%.0f", ImGuiSliderFlags_AlwaysClamp)) beacons_dirty = true;
    if (ImGui::DragFloat("Beam width", &beam_width, 1.f, 5.f, 500.f, "%.0f", ImGuiSliderFlags_AlwaysClamp)) beacons_dirty = true;
    if (ImGui::DragFloat("Beam opacity", &beam_opacity, 0.01f, 0.f, 1.f, "%.2f", ImGuiSliderFlags_AlwaysClamp)) beacons_dirty = true;
    if (ImGui::DragFloat("Ring radius", &ring_radius, 1.f, 5.f, 500.f, "%.0f", ImGuiSliderFlags_AlwaysClamp)) beacons_dirty = true;
    if (ImGui::DragFloat("Ring opacity", &ring_opacity, 0.01f, 0.f, 1.f, "%.2f", ImGuiSliderFlags_AlwaysClamp)) beacons_dirty = true;
    if (ImGui::DragFloat("Height lift", &z_lift, 0.5f, 0.f, 200.f, "%.1f", ImGuiSliderFlags_AlwaysClamp)) beacons_dirty = true;
    ImGui::DragFloat("Pulse speed", &pulse_speed, 0.05f, 0.f, 5.f, "%.2f Hz", ImGuiSliderFlags_AlwaysClamp);
    ImGui::ShowHelp("0 = no pulsing.");
}
