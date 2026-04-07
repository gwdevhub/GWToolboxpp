#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Skills.h>
#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>

#include <Widgets/Minimap/RangeRenderer.h>
#include <Widgets/Minimap/Minimap.h>

#include <ImGuiAddons.h>

void RangeRenderer::LoadSettings(const ToolboxIni* ini, const char* section)
{
    LoadDefaults();
    color_range_hos = Colors::Load(ini, section, "color_range_hos", color_range_hos);
    color_range_aggro = Colors::Load(ini, section, "color_range_aggro", color_range_aggro);
    color_range_cast = Colors::Load(ini, section, "color_range_cast", color_range_cast);
    color_range_spirit = Colors::Load(ini, section, "color_range_spirit", color_range_spirit);
    color_range_spirit_extended = Colors::Load(ini, section, "color_range_spirit_extended", color_range_spirit_extended);
    color_range_compass = Colors::Load(ini, section, "color_range_compass", color_range_compass);
    color_range_chain_aggro = Colors::Load(ini, section, "color_range_chain_aggro", color_range_chain_aggro);
    color_range_res_aggro = Colors::Load(ini, section, "color_range_res_aggro", color_range_res_aggro);
    color_range_shadowstep_aggro = Colors::Load(ini, section, "color_range_shadowstep_aggro", color_range_shadowstep_aggro);
    line_thickness = static_cast<float>(ini->GetDoubleValue(section, "range_line_thickness", line_thickness));
    Invalidate();
}

void RangeRenderer::LoadDefaults()
{
    color_range_hos = 0xFF881188;
    color_range_aggro = 0xFF994444;
    color_range_cast = 0xFF117777;
    color_range_spirit = color_range_spirit_extended = 0xFF337733;
    color_range_compass = 0xFF666611;
    color_range_chain_aggro = 0x00994444;
    color_range_res_aggro = 0x64D6D6D6;
    color_range_shadowstep_aggro = Colors::ARGB(200, 128, 0, 128);
    line_thickness = 1.f;
    Invalidate();
}

void RangeRenderer::SaveSettings(ToolboxIni* ini, const char* section) const
{
    Colors::Save(ini, section, "color_range_hos", color_range_hos);
    Colors::Save(ini, section, "color_range_aggro", color_range_aggro);
    Colors::Save(ini, section, "color_range_cast", color_range_cast);
    Colors::Save(ini, section, "color_range_spirit", color_range_spirit);
    Colors::Save(ini, section, "color_range_spirit_extended", color_range_spirit_extended);
    Colors::Save(ini, section, "color_range_compass", color_range_compass);
    ini->SetDoubleValue(section, "range_line_thickness", line_thickness);
    Colors::Save(ini, section, "color_range_chain_aggro", color_range_chain_aggro);
    Colors::Save(ini, section, "color_range_res_aggro", color_range_res_aggro);
    Colors::Save(ini, section, "color_range_shadowstep_aggro", color_range_shadowstep_aggro);
}

void RangeRenderer::DrawSettings()
{
    bool changed = false;
    ImGui::SmallConfirmButton("Restore Defaults", "Are you sure?", [&](bool result, void*) {
        if (result) {
            LoadDefaults();
            Invalidate();
        }
    });
    changed |= Colors::DrawSettingHueWheel("HoS range", &color_range_hos);
    changed |= Colors::DrawSettingHueWheel("Aggro range", &color_range_aggro);
    changed |= Colors::DrawSettingHueWheel("Cast range", &color_range_cast);
    changed |= Colors::DrawSettingHueWheel("Spirit range", &color_range_spirit);
    changed |= Colors::DrawSettingHueWheel("Extended Spirit range", &color_range_spirit_extended);
    changed |= Colors::DrawSettingHueWheel("Compass range", &color_range_compass);
    changed |= Colors::DrawSettingHueWheel("Chain Aggro range", &color_range_chain_aggro);
    changed |= Colors::DrawSettingHueWheel("Res Aggro range", &color_range_res_aggro);
    changed |= Colors::DrawSettingHueWheel("Shadow Step range", &color_range_shadowstep_aggro);
    changed |= ImGui::DragFloat("Line thickness", &line_thickness, 0.1f, 1.f, 10.f, "%.1f");
    if (changed) {
        Invalidate();
    }
}

void RangeRenderer::Initialize(IDirect3DDevice9* device)
{
    type = D3DPT_TRIANGLESTRIP;
    checkforhos_ = true;
    havehos_ = false;

    clear();
    vertices.reserve(circle_points * num_circles + 6);

    auto CreateCircle =
        [&](const float radius, const DWORD color) {
            const auto xdiff = static_cast<float>(line_thickness) / gwinches_per_pixel;
            const auto ydiff = static_cast<float>(line_thickness) / gwinches_per_pixel;
            for (auto i = 0; i <= circle_triangles; i += 2) {
                const auto angle = i / static_cast<float>(circle_triangles) * DirectX::XM_2PI;
                vertices.push_back({radius * cos(angle), radius * sin(angle), 0.f, color});
                vertices.push_back({(radius - xdiff) * cos(angle), (radius - ydiff) * sin(angle), 0.f, color});
            }
        };

    CreateCircle(GW::Constants::Range::Compass, color_range_compass);
    CreateCircle(GW::Constants::Range::Spirit, color_range_spirit);
    CreateCircle(GW::Constants::Range::SpiritExtended, color_range_spirit_extended);
    CreateCircle(GW::Constants::Range::Spellcast, color_range_cast);
    CreateCircle(GW::Constants::Range::Earshot, color_range_aggro);
    CreateCircle(360.f, color_range_hos);
    CreateCircle(700.f, color_range_chain_aggro);
    CreateCircle(GW::Constants::Range::Earshot, color_range_res_aggro);
    CreateCircle(GW::Constants::Range::Earshot, color_range_shadowstep_aggro);

    // HoS line
    vertices.push_back({260.f, 0.f, 0.f, color_range_hos});
    vertices.push_back({460.f, 0.f, 0.f, color_range_hos});
    vertices.push_back({-150.f, 0.f, 0.f, color_range_hos});
    vertices.push_back({150.f, 0.f, 0.f, color_range_hos});
    vertices.push_back({0.f, -150.f, 0.f, color_range_hos});
    vertices.push_back({0.f, 150.f, 0.f, color_range_hos});

    D3DVertexBuffer::Initialize(device);
}

void RangeRenderer::Render(IDirect3DDevice9* device, float _gwinches_per_pixel)
{
    if (_gwinches_per_pixel != gwinches_per_pixel) {
        Invalidate();
        gwinches_per_pixel = _gwinches_per_pixel;
    }

    if (!initialized) {
        initialized = true;
        Initialize(device);
    }
    if (!buffer) return;

    switch (GW::Map::GetInstanceType()) {
        case GW::Constants::InstanceType::Explorable:
            if (checkforhos_) {
                checkforhos_ = false;
                havehos_ = HaveHos();
            }
            break;
        case GW::Constants::InstanceType::Outpost:
            checkforhos_ = true;
            havehos_ = HaveHos();
            break;
        case GW::Constants::InstanceType::Loading:
            havehos_ = false;
            checkforhos_ = true;
            break;
        default:
            break;
    }

    device->SetFVF(D3DFVF_CUSTOMVERTEX);
    device->SetStreamSource(0, buffer, 0, sizeof(D3DVertex));

    const auto DrawCircle = [&](size_t& offset) {
        device->DrawPrimitive(type, offset, circle_triangles);
        offset += circle_points;
    };
    const auto DrawCircleAt = [&](size_t& offset, float x, float y) {
        D3DMATRIX oldworld;
        device->GetTransform(D3DTS_WORLD, &oldworld);
        const auto translate = DirectX::XMMatrixTranslation(x, y, 0.f);
        device->SetTransform(D3DTS_WORLD, reinterpret_cast<const D3DMATRIX*>(&translate));
        device->DrawPrimitive(type, offset, circle_triangles);
        device->SetTransform(D3DTS_WORLD, &oldworld);
        offset += circle_points;
    };

    size_t offset = 0;

    DrawCircle(offset); // Compass
    DrawCircle(offset); // Spirit
    DrawCircle(offset); // Spirit Extended
    DrawCircle(offset); // Cast
    DrawCircle(offset); // Aggro

    // HoS range
    if (havehos_) device->DrawPrimitive(type, offset, circle_triangles);
    offset += circle_points;

    const GW::AgentLiving* target = GW::Agents::GetTargetAsAgentLiving();

    // Chain aggro (on target)
    if ((color_range_chain_aggro & IM_COL32_A_MASK) && target && target->allegiance == GW::Constants::Allegiance::Enemy) {
        DrawCircleAt(offset, target->x, target->y);
    }
    else {
        offset += circle_points;
    }

    // Res aggro (on dead ally)
    if ((color_range_res_aggro & IM_COL32_A_MASK) && target && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable && target->allegiance == GW::Constants::Allegiance::Ally_NonAttackable && target->GetIsDead()) {
        DrawCircleAt(offset, target->x, target->y);
    }
    else {
        offset += circle_points;
    }

    // Shadowstep aggro
    const GW::Vec2f& shadowstep_location = Minimap::Instance().ShadowstepLocation();
    if ((color_range_shadowstep_aggro & IM_COL32_A_MASK) && (shadowstep_location.x != 0.f || shadowstep_location.y != 0.f)) {
        DrawCircleAt(offset, shadowstep_location.x, shadowstep_location.y);
    }
    else {
        offset += circle_points;
    }

    // HoS line
    if (havehos_) {
        const auto me = target ? GW::Agents::GetControlledCharacter() : nullptr;
        if (me && me != target && !me->GetIsDead() && !target->GetIsDead() && GetSquareDistance(target->pos, me->pos) < GW::Constants::SqrRange::Spellcast) {
            GW::Vec2f v = me->pos - target->pos;
            D3DMATRIX oldworld;
            device->GetTransform(D3DTS_WORLD, &oldworld);
            const auto rotate = DirectX::XMMatrixRotationZ(std::atan2(v.y, v.x));
            const auto newworld = rotate * XMLoadFloat4x4(reinterpret_cast<const DirectX::XMFLOAT4X4*>(&oldworld));
            device->SetTransform(D3DTS_WORLD, reinterpret_cast<const D3DMATRIX*>(&newworld));
            device->DrawPrimitive(D3DPT_LINESTRIP, offset, 1);
            device->SetTransform(D3DTS_WORLD, &oldworld);
        }
    }
    offset += 2;

    if (draw_center_) {
        device->DrawPrimitive(D3DPT_LINESTRIP, offset, 1);
        offset += 2;
        device->DrawPrimitive(D3DPT_LINESTRIP, offset, 1);
    }
}

bool RangeRenderer::HaveHos()
{
    GW::Skillbar* skillbar = GW::SkillbarMgr::GetPlayerSkillbar();
    if (!skillbar || !skillbar->IsValid()) {
        checkforhos_ = true;
        return false;
    }

    for (const auto& skill : skillbar->skills) {
        const auto id = static_cast<GW::Constants::SkillID>(skill.skill_id);
        if (id == GW::Constants::SkillID::Heart_of_Shadow) {
            return true;
        }
        if (id == GW::Constants::SkillID::Vipers_Defense) {
            return true;
        }
    }
    return false;
}
