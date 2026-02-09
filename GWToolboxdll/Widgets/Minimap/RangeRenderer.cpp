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

size_t RangeRenderer::CreateCircle(D3DVertex* vertices, const float radius, const DWORD color) const
{
    const auto scale = Minimap::GetGwinchScale();
    const auto xdiff = static_cast<float>(line_thickness) / scale.x;
    const auto ydiff = static_cast<float>(line_thickness) / scale.y;
    for (auto i = 0; i <= circle_triangles; i += 2) {
        const auto angle = i / static_cast<float>(circle_triangles) * DirectX::XM_2PI;
        vertices[i].x = radius * cos(angle);
        vertices[i].y = radius * sin(angle);
        vertices[i + 1].x = (radius - xdiff) * cos(angle);
        vertices[i + 1].y = (radius - ydiff) * sin(angle);
        vertices[i].z = vertices[i + 1].z = 0.0f;
        vertices[i].color = vertices[i + 1].color = color;
    }
    return circle_points;
}

void RangeRenderer::Initialize(IDirect3DDevice9* device)
{
    if (initialized) {
        return;
    }
    initialized = true;
    count = circle_points * num_circles;
    type = D3DPT_TRIANGLESTRIP;

    checkforhos_ = true;
    havehos_ = false;

    D3DVertex* vertices = nullptr;
    const size_t vertex_count = count + 6;

    device->CreateVertexBuffer(sizeof(D3DVertex) * vertex_count, D3DUSAGE_WRITEONLY, D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer, nullptr);
    buffer->Lock(0, sizeof(D3DVertex) * vertex_count, reinterpret_cast<void**>(&vertices), D3DLOCK_DISCARD);
    ASSERT(vertices != nullptr);

    const D3DVertex* vertices_max = vertices + vertex_count;

    // Compass range
    float radius = GW::Constants::Range::Compass;
    vertices += CreateCircle(vertices, radius, color_range_compass);
    ASSERT(vertices < vertices_max);

    // Spirit range
    radius = GW::Constants::Range::Spirit;
    vertices += CreateCircle(vertices, radius, color_range_spirit);
    ASSERT(vertices < vertices_max);

    // Spirit extended range
    radius = GW::Constants::Range::SpiritExtended;
    vertices += CreateCircle(vertices, radius, color_range_spirit_extended);
    ASSERT(vertices < vertices_max);

    // Spellcast range
    radius = GW::Constants::Range::Spellcast;
    vertices += CreateCircle(vertices, radius, color_range_cast);
    ASSERT(vertices < vertices_max);

    // Aggro range
    radius = GW::Constants::Range::Earshot;
    vertices += CreateCircle(vertices, radius, color_range_aggro);
    ASSERT(vertices < vertices_max);

    // HoS range
    radius = 360.0f;
    vertices += CreateCircle(vertices, radius, color_range_hos);
    ASSERT(vertices < vertices_max);

    // Chain aggro range
    radius = 700.f;
    vertices += CreateCircle(vertices, radius, color_range_chain_aggro);
    ASSERT(vertices < vertices_max);

    // Res aggro range
    radius = GW::Constants::Range::Earshot;
    vertices += CreateCircle(vertices, radius, color_range_res_aggro);
    ASSERT(vertices < vertices_max);

    // Shadowstep location aggro range
    radius = GW::Constants::Range::Earshot;
    vertices += CreateCircle(vertices, radius, color_range_shadowstep_aggro);
    ASSERT(vertices < vertices_max);

    // HoS line
    ASSERT(vertices + 5 < vertices_max);
    vertices[0] = {260.f, 0.f, 0.f, color_range_hos};
    vertices[1] = {460.f, 0.f, 0.f, color_range_hos};
    vertices[2] = {-150.f, 0.f, 0.f, color_range_hos};
    vertices[3] = {150, 0.f, 0.f, color_range_hos};
    vertices[4] = {0, -150.f, 0.f, color_range_hos};
    vertices[5] = {0.f, 150.f, 0.f, color_range_hos};

    buffer->Unlock();
}

void RangeRenderer::Render(IDirect3DDevice9* device)
{
    Initialize(device);

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

    size_t vertices_offset = 0;

    // Compass range
    device->DrawPrimitive(type, vertices_offset, circle_triangles);
    vertices_offset += circle_points;

    // Spirit range
    device->DrawPrimitive(type, vertices_offset, circle_triangles);
    vertices_offset += circle_points;

    // Spirit Extended range
    device->DrawPrimitive(type, vertices_offset, circle_triangles);
    vertices_offset += circle_points;

    // Cast range
    device->DrawPrimitive(type, vertices_offset, circle_triangles);
    vertices_offset += circle_points;

    // Aggro range
    device->DrawPrimitive(type, vertices_offset, circle_triangles);
    vertices_offset += circle_points;

    // Draw Hos range
    if (havehos_) {
        device->DrawPrimitive(type, vertices_offset, circle_triangles);
    }
    vertices_offset += circle_points;

    const GW::AgentLiving* target = GW::Agents::GetTargetAsAgentLiving();

    // Aggro range
    if ((color_range_chain_aggro & IM_COL32_A_MASK) != 0
        && target
        && target->allegiance == GW::Constants::Allegiance::Enemy) {
        D3DMATRIX oldworld;
        device->GetTransform(D3DTS_WORLD, &oldworld);
        const auto translate = DirectX::XMMatrixTranslation(target->x, target->y, 0.0f);
        device->SetTransform(D3DTS_WORLD, reinterpret_cast<const D3DMATRIX*>(&translate));
        device->DrawPrimitive(type, vertices_offset, circle_triangles);
        device->SetTransform(D3DTS_WORLD, &oldworld);
    }
    vertices_offset += circle_points;

    // Res aggro range
    if ((color_range_res_aggro & IM_COL32_A_MASK) != 0
        && target
        && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable
        && target->allegiance == GW::Constants::Allegiance::Ally_NonAttackable
        && target->GetIsDead()) {
        D3DMATRIX oldworld;
        device->GetTransform(D3DTS_WORLD, &oldworld);
        const auto translate = DirectX::XMMatrixTranslation(target->x, target->y, 0.0f);
        device->SetTransform(D3DTS_WORLD, reinterpret_cast<const D3DMATRIX*>(&translate));
        device->DrawPrimitive(type, vertices_offset, circle_triangles);
        device->SetTransform(D3DTS_WORLD, &oldworld);
    }
    vertices_offset += circle_points;

    // Draw shadowstep range i.e. shadowwalk aggro
    const GW::Vec2f& shadowstep_location = Minimap::Instance().ShadowstepLocation();
    if ((color_range_shadowstep_aggro & IM_COL32_A_MASK) != 0
        && (shadowstep_location.x != 0.f || shadowstep_location.y != 0.f)) {
        D3DMATRIX oldworld;
        device->GetTransform(D3DTS_WORLD, &oldworld);
        const auto translate = DirectX::XMMatrixTranslation(shadowstep_location.x, shadowstep_location.y, 0.0f);
        device->SetTransform(D3DTS_WORLD, reinterpret_cast<const D3DMATRIX*>(&translate));
        device->DrawPrimitive(type, vertices_offset, circle_triangles);
        device->SetTransform(D3DTS_WORLD, &oldworld);
    }
    vertices_offset += circle_points;

    // Draw hos line
    if (havehos_) {
        const auto me = target ? GW::Agents::GetControlledCharacter() : nullptr;

        if (me && me != target && !me->GetIsDead() && !target->GetIsDead() && GetSquareDistance(target->pos, me->pos) < GW::Constants::SqrRange::Spellcast) {
            GW::Vec2f v = me->pos - target->pos;
            const float angle = std::atan2(v.y, v.x);

            D3DMATRIX oldworld;
            device->GetTransform(D3DTS_WORLD, &oldworld);
            const auto rotate = DirectX::XMMatrixRotationZ(angle);
            const auto newworld = rotate * XMLoadFloat4x4(reinterpret_cast<const DirectX::XMFLOAT4X4*>(&oldworld));
            device->SetTransform(D3DTS_WORLD, reinterpret_cast<const D3DMATRIX*>(&newworld));
            device->DrawPrimitive(D3DPT_LINESTRIP, vertices_offset, 1);
            device->SetTransform(D3DTS_WORLD, &oldworld);
        }
    }
    vertices_offset += 2;

    if (draw_center_) {
        device->DrawPrimitive(D3DPT_LINESTRIP, vertices_offset, 1);
        vertices_offset += 2;
        device->DrawPrimitive(D3DPT_LINESTRIP, vertices_offset, 1);
        vertices_offset += 2;
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
