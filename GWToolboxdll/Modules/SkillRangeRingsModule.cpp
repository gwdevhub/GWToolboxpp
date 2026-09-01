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
#include <D3DContainers.h>
#include <ImGuiAddons.h>
#include <Modules/SkillRangeRingsModule.h>
#include <Utils/GameWorldCompositor.h>
#include <Utils/PropSurfaceIndex.h>
#include <Utils/SettingsRegistry.h>
#include <Utils/TerrainDrape.h>

namespace {
    constexpr int kMinSegments = 64;
    constexpr int kMaxSegments = 512;
    constexpr float kSampleSpacing = 25.f;
    constexpr float kMaxRingRadius = 5200.f; // 忽略超出罗盘范围的虚假范围数据
    constexpr uint8_t kTargetNone = 0;        // Skill.target == no_target（瞬发附魔/姿态/自我施放形态）

    constexpr GW::Constants::SkillID kStaleAoeRange[] = {
        GW::Constants::SkillID::Double_Dragon,
    };

    float render_max_distance = 7000.f;
    float fog_factor = 0.6f;
    float ring_thickness = 24.f;
    float opacity = 0.7f;
    float z_lift = 5.f; // 抬高以避免 Z 冲突（GW 的向上方向为 -z）
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

    std::vector<RingSpec> built_specs;
    std::vector<RingVertex> scratch;
    GW::Constants::SkillID built_skill = static_cast<GW::Constants::SkillID>(0);
    bool rings_dirty = false;
    uint32_t debug_skill_id = 0;
    int compositor_token = 0;

    // `scratch` 上次拖拽的锚点状态；超过 kAnchorMoveEpsilon 的变化触发重建。
    constexpr float kAnchorMoveEpsilon = 1.f;
    float built_me_x = 0.f, built_me_y = 0.f;
    uint32_t built_me_zplane = 0, built_target_id = 0, built_target_zplane = 0;
    float built_target_x = 0.f, built_target_y = 0.f;
    bool built_tgt_valid = false;
    // 异步道具表面烘焙完成时 SurfaceZ 输出会变化，因此也要在该转换时重新拖拽。
    bool built_prop_ready = false;

    int RingSegments(const float radius)
    {
        const auto circumference = std::max(radius, ring_thickness) * DirectX::XM_2PI;
        return std::clamp(static_cast<int>(std::ceil(circumference / kSampleSpacing)), kMinSegments, kMaxSegments);
    }

    void SpecsForSkill(const GW::Skill& skill, std::vector<RingSpec>& out)
    {
        using enum GW::Constants::SkillType;
        const auto type = skill.type;
        const bool shout_like = type == Shout || type == Chant || type == EchoRefrain;
        const bool spell_like = type == Spell || type == Hex || type == Enchantment || type == Well
                                || type == Signet || type == ItemSpell || type == WeaponSpell;
        const bool targets_other = skill.target != kTargetNone;
        if (shout_like) {
            out.push_back({GW::Constants::Range::Earshot, color_earshot, false});
        }
        if (type == Ritual) {
            out.push_back({GW::Constants::Range::Spirit, color_effect, false}); // 放置的灵将覆盖的范围
        }
        // 小于 50 的值为生成偏移（例如 Shelter 的 10），约 5000 表示“全队/全域”——两者都不是环。
        const bool stale_aoe = std::ranges::contains(kStaleAoeRange, skill.skill_id);
        const float aoe_range = stale_aoe ? skill.const_effect : skill.aoe_range;
        if (aoe_range > 50.f && aoe_range < 4990.f) {
            out.push_back({aoe_range, color_aoe, spell_like && targets_other});
        }
        if (!stale_aoe && skill.const_effect > 50.f && skill.const_effect < 4990.f) {
            out.push_back({skill.const_effect, color_effect, false});
        }
        // 相同半径出现两次（例如某个战吼的 aoe_range 已经是 earshot）则合并为一个环。
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

    // 将一个环带追加到 `out` 中，拖拽到可见表面：每个顶点在其自己的 (x,y) 处获取表面高度，
    // 优先使用锚点的平面，使环紧贴您所站立的表面。
    void EmitBand(std::vector<RingVertex>& out, const float cx, const float cy, const uint32_t zplane,
                  const uint32_t n_planes, const float ref_z, const RingSpec& spec)
    {
        const float half = std::max(2.f, ring_thickness) * 0.5f;
        const float r_in = std::max(1.f, spec.radius - half);
        const float r_out = spec.radius + half;
        const DWORD col = WithOpacity(spec.color);
        const int segments = RingSegments(spec.radius);
        std::vector<RingVertex> inner(segments), outer(segments);
        for (int s = 0; s < segments; ++s) {
            const float angle = s * (DirectX::XM_2PI / segments);
            const float cos_a = std::cos(angle), sin_a = std::sin(angle);
            const float xi = cx + cos_a * r_in, yi = cy + sin_a * r_in;
            const float xo = cx + cos_a * r_out, yo = cy + sin_a * r_out;
            const float zi = TerrainDrape::SurfaceZ(xi, yi, zplane, n_planes);
            const float zo = TerrainDrape::SurfaceZ(xo, yo, zplane, n_planes);
            inner[s] = {xi, yi, (zi ? zi : ref_z) - z_lift, col};
            outer[s] = {xo, yo, (zo ? zo : ref_z) - z_lift, col};
        }
        for (int s = 0; s < segments; ++s) {
            const int s1 = (s + 1) % segments;
            out.push_back(inner[s]);
            out.push_back(outer[s]);
            out.push_back(outer[s1]);
            out.push_back(inner[s]);
            out.push_back(outer[s1]);
            out.push_back(inner[s1]);
        }
    }

    // 为悬停技能重新计算每个环的规格。仅在技能/设置变化时运行。
    void BuildSpecs(const GW::Skill& skill)
    {
        built_specs.clear();
        SpecsForSkill(skill, built_specs);
        built_skill = skill.skill_id;
        rings_dirty = false;
        scratch.clear();
    }

    void ResetRings()
    {
        built_specs.clear();
        scratch.clear();
        built_skill = static_cast<GW::Constants::SkillID>(0);
    }
} // namespace

void SkillRangeRingsModule::SetDebugSkill(const uint32_t skill_id)
{
    debug_skill_id = skill_id;
}

size_t SkillRangeRingsModule::DebugSpecs(const uint32_t skill_id, char* buf, const size_t len)
{
    if (!buf || !len) return 0;
    buf[0] = 0;
    const auto skill = GW::SkillbarMgr::GetSkillConstantData(static_cast<GW::Constants::SkillID>(skill_id));
    if (!skill) return 0;
    std::vector<RingSpec> specs;
    SpecsForSkill(*skill, specs);
    size_t off = 0;
    for (const auto& s : specs) {
        const char* palette = s.color == color_aoe ? "aoe" : s.color == color_earshot ? "earshot" : "effect";
        const auto wrote = snprintf(buf + off, len - off, "%s%.0f:%s%s", off ? "," : "", s.radius, palette, s.at_target ? "@target" : "");
        if (wrote < 0 || static_cast<size_t>(wrote) >= len - off) break;
        off += wrote;
    }
    return specs.size();
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

    // 仅当锚点实际移动时重新拖拽；否则重用缓存的几何体。每个环锚定到当前目标（目标 AoE 技能）或玩家，拖拽到该表面。
    const GW::Agent* target = GW::Agents::GetTarget();
    const bool tgt_valid = aoe_at_target && target && target->agent_id != me->agent_id;
    const uint32_t target_id = tgt_valid ? target->agent_id : 0;
    const bool prop_ready = PropSurface::Enabled() && PropSurface::Ready();
    const auto moved = [](const float ax, const float ay, const float bx, const float by) {
        return std::fabs(ax - bx) > kAnchorMoveEpsilon || std::fabs(ay - by) > kAnchorMoveEpsilon;
    };
    const bool rebuild = scratch.empty() || prop_ready != built_prop_ready
                         || tgt_valid != built_tgt_valid || target_id != built_target_id
                         || me->pos.zplane != built_me_zplane || moved(me->pos.x, me->pos.y, built_me_x, built_me_y)
                         || (tgt_valid && (target->pos.zplane != built_target_zplane
                                           || moved(target->pos.x, target->pos.y, built_target_x, built_target_y)));
    if (rebuild) {
        const auto n_planes = TerrainDrape::PathingPlaneCount();
        scratch.clear();
        for (const auto& spec : built_specs) {
            const GW::Agent* anchor = (spec.at_target && tgt_valid) ? target : static_cast<const GW::Agent*>(me);
            const float ref_z = TerrainDrape::SurfaceZ(anchor->pos.x, anchor->pos.y, anchor->pos.zplane, n_planes);
            EmitBand(scratch, anchor->pos.x, anchor->pos.y, anchor->pos.zplane, n_planes, ref_z ? ref_z : anchor->z, spec);
        }
        built_me_x = me->pos.x, built_me_y = me->pos.y, built_me_zplane = me->pos.zplane;
        built_target_id = target_id, built_tgt_valid = tgt_valid;
        built_target_x = tgt_valid ? target->pos.x : 0.f, built_target_y = tgt_valid ? target->pos.y : 0.f;
        built_target_zplane = tgt_valid ? target->pos.zplane : 0;
        built_prop_ready = prop_ready;
    }
    if (scratch.size() < 3) return;

    const D3DStateGuard state_guard(device);
    // Static depth keeps walls/props occluding overlays; agents draw later in GW's pass.
    if (GameWorldCompositor::SetupPipeline(device, true, render_max_distance, fog_factor)) {
        constexpr BOOL dotted_off[1] = {FALSE};
        device->SetPixelShaderConstantB(0, dotted_off, 1);
        device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, static_cast<UINT>(scratch.size() / 3), scratch.data(), sizeof(RingVertex));
    }
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
        ImGui::TextColored(red, GameWorldCompositor::HasFailed() ? "世界内合成器安装失败。" : "世界内合成器：尚未安装。");

    ImGui::TextUnformatted("悬停任意技能（技能栏、技能窗口…）即可在地面看到其范围。");
    if (ImGui::Checkbox("在当前目标位置显示 AoE 环", &aoe_at_target)) rings_dirty = true;
    ImGui::ShowHelp("对于有区域效果的目标技能。关闭：始终围绕你自己。");
    ImGui::TextDisabled("地形后的遮挡遵循“游戏内渲染”模块的设置。");
    ImGui::DragFloat("最大渲染距离", &render_max_distance, 25.f, 10.f, 100000.f, "%.0f", ImGuiSliderFlags_AlwaysClamp);
    if (ImGui::DragFloat("环厚度", &ring_thickness, 1.f, 4.f, 200.f, "%.0f", ImGuiSliderFlags_AlwaysClamp)) rings_dirty = true;
    if (ImGui::DragFloat("不透明度", &opacity, 0.01f, 0.f, 1.f, "%.2f", ImGuiSliderFlags_AlwaysClamp)) rings_dirty = true;
    if (ImGui::DragFloat("高度抬升", &z_lift, 0.5f, 0.f, 200.f, "%.1f", ImGuiSliderFlags_AlwaysClamp)) rings_dirty = true;
    ImGui::Separator();
    if (Colors::DrawSettingHueWheel("AoE 半径", &color_aoe)) rings_dirty = true;
    if (Colors::DrawSettingHueWheel("听觉范围（战吼、赞歌）", &color_earshot)) rings_dirty = true;
    if (Colors::DrawSettingHueWheel("恒定效果（灵范围）", &color_effect)) rings_dirty = true;
}
