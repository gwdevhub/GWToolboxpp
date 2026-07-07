#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Skills.h>
#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Skill.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>

#include <Color.h>
#include <ImGuiAddons.h>
#include <Modules/SkillRangeRingsModule.h>
#include <Utils/GameWorldCompositor.h>
#include <Utils/SettingsRegistry.h>
#include <Utils/TerrainDrape.h>
#include <Widgets/Minimap/GameWorldRenderer.h>

namespace {
    constexpr int kSegments = 64;
    constexpr float kMaxRingRadius = 5200.f; // ignore bogus range data past compass-ish sizes
    constexpr uint8_t kTargetNone = 0;        // Skill.target == no_target (flash enchant / stance / self-cast form)

    // Occlusion behind terrain is shared with the "In-game rendering" module
    // via GameWorldRenderer::GetOccludeBehindTerrain(), so it's configured in
    // one place. The incomplete-ring artifact (arcs that drop in/out with the camera) only occurs with occlusion
    // ON - it's an interaction between our depth test and GW's depth buffer - and this shared setting defaults off,
    // so rings draw whole by default.
    float render_max_distance = 7000.f;
    float fog_factor = 0.6f;
    float ring_thickness = 24.f;
    float opacity = 0.7f;
    float z_lift = 5.f; // raise above the floor to avoid z-fighting (GW up is -z)
    bool aoe_at_target = true;
    Color color_aoe = Colors::ARGB(255, 255, 120, 40);
    Color color_earshot = Colors::ARGB(255, 80, 220, 120);
    Color color_effect = Colors::ARGB(255, 190, 100, 255);

    struct RingVertex {
        float x, y, z;
        DWORD color;
    };

    struct RingSpec {
        float radius;
        Color color;
        bool at_target;
    };

    // Rings are rebuilt into `scratch` (absolute world coords) every frame and drawn with DrawPrimitiveUP,
    // so following the character is smooth. They are FLAT (a range is a horizontal distance, drawn at the
    // anchor's foot height) rather than terrain-draped, which keeps the per-frame rebuild trivially cheap
    // (no altitude queries). The per-ring specs only change when the hovered skill or the settings change.
    std::vector<RingSpec> built_specs;
    std::vector<RingVertex> scratch;
    GW::Constants::SkillID built_skill = static_cast<GW::Constants::SkillID>(0);
    bool rings_dirty = false;
    uint32_t debug_skill_id = 0;
    int compositor_token = 0;

    void SpecsForSkill(const GW::Skill& skill, std::vector<RingSpec>& out)
    {
        using enum GW::Constants::SkillType;
        const auto type = skill.type;
        const bool shout_like = type == Shout || type == Chant || type == EchoRefrain;
        const bool spell_like = type == Spell || type == Hex || type == Enchantment || type == Well
                                || type == Signet || type == ItemSpell || type == WeaponSpell;
        // No cast-range ring: the caster already knows their own cast range and it's just clutter. Only the
        // skill's actual area of effect is shown. `targets_other` still decides whether an AoE anchors to
        // the target vs the caster (self/flash skills have Skill.target == no_target -> around the caster).
        const bool targets_other = skill.target != kTargetNone;
        if (shout_like) {
            out.push_back({GW::Constants::Range::Earshot, color_earshot, false});
        }
        if (type == Ritual) {
            out.push_back({GW::Constants::Range::Spirit, color_effect, false}); // what the placed spirit will cover
        }
        // Sub-50 values are spawn offsets (e.g. Shelter's 10), ~5000 means "party-wide/everywhere" - neither is a ring.
        if (skill.aoe_range > 50.f && skill.aoe_range < 4990.f) {
            out.push_back({skill.aoe_range, color_aoe, spell_like && targets_other});
        }
        if (skill.const_effect > 50.f && skill.const_effect < 4990.f) {
            out.push_back({skill.const_effect, color_effect, false});
        }
        // Same radius twice (e.g. a shout whose aoe_range is already earshot) reads as one ring.
        for (size_t i = 0; i < out.size(); ++i) {
            for (size_t j = out.size(); j-- > i + 1;) {
                if (std::fabs(out[i].radius - out[j].radius) < 15.f) out.erase(out.begin() + static_cast<int>(j));
            }
        }
        std::erase_if(out, [](const RingSpec& s) { return s.radius > kMaxRingRadius; });
    }

    DWORD WithOpacity(const Color color)
    {
        const auto a = static_cast<DWORD>(std::clamp(static_cast<float>((color >> IM_COL32_A_SHIFT) & 0xFF) * opacity, 0.f, 255.f));
        return (color & ~(0xFFu << IM_COL32_A_SHIFT)) | (a << IM_COL32_A_SHIFT);
    }

    // Append a ring band for one spec into `out`, draped onto the terrain: each vertex takes the surface
    // height at its own (x,y), preferring the anchor's plane so the ring hugs the surface you're standing on.
    void EmitBand(std::vector<RingVertex>& out, const float cx, const float cy, const uint32_t zplane,
                  const uint32_t n_planes, const float ref_z, const RingSpec& spec)
    {
        const float half = std::max(2.f, ring_thickness) * 0.5f;
        const float r_in = std::max(1.f, spec.radius - half);
        const float r_out = spec.radius + half;
        const DWORD col = WithOpacity(spec.color);
        RingVertex inner[kSegments], outer[kSegments];
        for (int s = 0; s < kSegments; ++s) {
            const float angle = s * (DirectX::XM_2PI / kSegments);
            const float cos_a = std::cos(angle), sin_a = std::sin(angle);
            const float xi = cx + cos_a * r_in, yi = cy + sin_a * r_in;
            const float xo = cx + cos_a * r_out, yo = cy + sin_a * r_out;
            inner[s] = {xi, yi, TerrainDrape::DrapeZ(xi, yi, zplane, n_planes, ref_z) - z_lift, col};
            outer[s] = {xo, yo, TerrainDrape::DrapeZ(xo, yo, zplane, n_planes, ref_z) - z_lift, col};
        }
        for (int s = 0; s < kSegments; ++s) {
            const int s1 = (s + 1) % kSegments;
            out.push_back(inner[s]);
            out.push_back(outer[s]);
            out.push_back(outer[s1]);
            out.push_back(inner[s]);
            out.push_back(outer[s1]);
            out.push_back(inner[s1]);
        }
    }

    // Recompute the per-ring specs for the hovered skill. Only runs on a skill/settings change.
    void BuildSpecs(const GW::Skill& skill)
    {
        built_specs.clear();
        SpecsForSkill(skill, built_specs);
        built_skill = skill.skill_id;
        rings_dirty = false;
    }

    void ResetRings()
    {
        built_specs.clear();
        built_skill = static_cast<GW::Constants::SkillID>(0);
    }
} // namespace

void SkillRangeRingsModule::SetDebugSkill(const uint32_t skill_id)
{
    debug_skill_id = skill_id;
}

void SkillRangeRingsModule::DrawInWorld(IDirect3DDevice9* device)
{
    const GW::Skill* skill = nullptr;
    const GW::AgentLiving* me = nullptr;
    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading) {
        me = GW::Agents::GetControlledCharacter();
        if (me) {
            skill = debug_skill_id
                        ? GW::SkillbarMgr::GetSkillConstantData(static_cast<GW::Constants::SkillID>(debug_skill_id))
                        : GW::SkillbarMgr::GetHoveredSkill();
        }
    }
    if (!skill || !me) {
        if (built_skill != static_cast<GW::Constants::SkillID>(0)) ResetRings();
        return;
    }

    if (rings_dirty || skill->skill_id != built_skill) BuildSpecs(*skill);
    if (built_specs.empty()) return;

    // Rebuild the draped rings at the current anchor positions every frame, so movement is smooth. Each
    // ring anchors to the current target (targeted AoE skills) or the player, draped onto that surface.
    const GW::Agent* target = GW::Agents::GetTarget();
    const bool tgt_valid = aoe_at_target && target && target->agent_id != me->agent_id;
    const auto n_planes = TerrainDrape::PathingPlaneCount();
    scratch.clear();
    for (const auto& spec : built_specs) {
        const GW::Agent* anchor = (spec.at_target && tgt_valid) ? target : static_cast<const GW::Agent*>(me);
        EmitBand(scratch, anchor->pos.x, anchor->pos.y, anchor->pos.zplane, n_planes, anchor->z, spec);
    }
    if (scratch.size() < 3) return;

    IDirect3DStateBlock9* state_block = nullptr;
    if (device->CreateStateBlock(D3DSBT_ALL, &state_block) != D3D_OK) return;
    if (GameWorldCompositor::SetupPipeline(device, GameWorldRenderer::GetOccludeBehindTerrain(), render_max_distance, fog_factor)) {
        constexpr BOOL dotted_off[1] = {FALSE};
        device->SetPixelShaderConstantB(0, dotted_off, 1);
        device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, static_cast<UINT>(scratch.size() / 3), scratch.data(), sizeof(RingVertex));
    }
    state_block->Apply();
    state_block->Release();
}

void SkillRangeRingsModule::RegisterSettings(ToolboxModule* module)
{
    SettingsRegistry::RegisterField(module, "render_max_distance", &render_max_distance);
    SettingsRegistry::RegisterField(module, "fog_factor", &fog_factor);
    SettingsRegistry::RegisterField(module, "ring_thickness", &ring_thickness);
    SettingsRegistry::RegisterField(module, "opacity", &opacity);
    SettingsRegistry::RegisterField(module, "z_lift", &z_lift);
    SettingsRegistry::RegisterField(module, "aoe_at_target", &aoe_at_target);
    SettingsRegistry::RegisterField(module, "color_aoe", &color_aoe);
    SettingsRegistry::RegisterField(module, "color_earshot", &color_earshot);
    SettingsRegistry::RegisterField(module, "color_effect", &color_effect);
}

void SkillRangeRingsModule::Initialize()
{
    ToolboxModule::Initialize();
    RegisterSettings(this);
    if (!compositor_token) compositor_token = GameWorldCompositor::RegisterDraw(&SkillRangeRingsModule::DrawInWorld);
}

void SkillRangeRingsModule::SignalTerminate()
{
    if (compositor_token) {
        GameWorldCompositor::UnregisterDraw(compositor_token);
        compositor_token = 0;
    }
    ResetRings();
}

void SkillRangeRingsModule::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxModule::LoadSettings(doc, legacy);
    rings_dirty = true;
}

void SkillRangeRingsModule::DrawSettingsInternal()
{
    const auto red = ImGui::ColorConvertU32ToFloat4(Colors::Red());
    if (!GameWorldCompositor::IsActive())
        ImGui::TextColored(red, GameWorldCompositor::HasFailed() ? "In-world compositor FAILED to install." : "In-world compositor: not installed yet.");

    ImGui::TextUnformatted("Hover any skill (skillbar, skills window...) to see its ranges on the ground.");
    if (ImGui::Checkbox("Show AoE ring at your current target", &aoe_at_target)) rings_dirty = true;
    ImGui::ShowHelp("For targeted skills with an area effect. Off: always around you.");
    ImGui::TextDisabled("Occlusion behind terrain follows the \"In-game rendering\" module's setting.");
    ImGui::DragFloat("Maximum render distance", &render_max_distance, 25.f, 10.f, 100000.f, "%.0f", ImGuiSliderFlags_AlwaysClamp);
    if (ImGui::DragFloat("Ring thickness", &ring_thickness, 1.f, 4.f, 200.f, "%.0f", ImGuiSliderFlags_AlwaysClamp)) rings_dirty = true;
    if (ImGui::DragFloat("Opacity", &opacity, 0.01f, 0.f, 1.f, "%.2f", ImGuiSliderFlags_AlwaysClamp)) rings_dirty = true;
    if (ImGui::DragFloat("Height lift", &z_lift, 0.5f, 0.f, 200.f, "%.1f", ImGuiSliderFlags_AlwaysClamp)) rings_dirty = true;
    ImGui::Separator();
    if (Colors::DrawSettingHueWheel("AoE radius", &color_aoe)) rings_dirty = true;
    if (Colors::DrawSettingHueWheel("Earshot (shouts, chants)", &color_earshot)) rings_dirty = true;
    if (Colors::DrawSettingHueWheel("Constant effect (spirit range)", &color_effect)) rings_dirty = true;
}
