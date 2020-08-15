#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>

#include <Widgets/Minimap/RangeRenderer.h>
#include <Widgets/Minimap/PingsLinesRenderer.h>

#include "Minimap.h"

void RangeRenderer::LoadSettings(CSimpleIni *ini, const char *section)
{
    color_range_hos = Colors::Load(ini, section, "color_range_hos", 0xFF881188);
    color_range_aggro = Colors::Load(ini, section, "color_range_aggro", 0xFF994444);
    color_range_cast = Colors::Load(ini, section, "color_range_cast", 0xFF117777);
    color_range_spirit = Colors::Load(ini, section, "color_range_spirit", 0xFF337733);
    color_range_compass = Colors::Load(ini, section, "color_range_compass", 0xFF666611);
    color_range_chain_aggro = Colors::Load(ini, section, "color_range_chain_aggro", 0x00994444);
    color_range_res_aggro = Colors::Load(ini, section, "color_range_res_aggro", 0x64D6D6D6);
    color_range_shadowstep_aggro = Colors::Load(ini, section, "color_range_shadowstep_aggro", Colors::ARGB(200, 128, 0, 128));
    line_thickness = static_cast<float>(ini->GetDoubleValue(section, "range_line_thickness", 1.f));
    Invalidate();
}
void RangeRenderer::SaveSettings(CSimpleIni *ini, const char *section) const
{
    Colors::Save(ini, section, "color_range_hos", color_range_hos);
    Colors::Save(ini, section, "color_range_aggro", color_range_aggro);
    Colors::Save(ini, section, "color_range_cast", color_range_cast);
    Colors::Save(ini, section, "color_range_spirit", color_range_spirit);
    Colors::Save(ini, section, "color_range_compass", color_range_compass);
    ini->SetDoubleValue(section, "range_line_thickness", static_cast<double>(line_thickness));
    Colors::Save(ini, section, "color_range_chain_aggro", color_range_chain_aggro);
    Colors::Save(ini, section, "color_range_res_aggro", color_range_res_aggro);
    Colors::Save(ini, section, "color_range_shadowstep_aggro", color_range_shadowstep_aggro);
}
void RangeRenderer::DrawSettings()
{
    bool changed = false;
    if (ImGui::SmallButton("Restore Defaults")) {
        changed = true;
        color_range_hos = 0xFF881188;
        color_range_aggro = 0xFF994444;
        color_range_cast = 0xFF117777;
        color_range_spirit = 0xFF337733;
        color_range_compass = 0xFF666611;
        color_range_chain_aggro = 0x00994444;
        color_range_res_aggro = 0x64D6D6D6;
        color_range_shadowstep_aggro = Colors::ARGB(200, 128, 0, 128);
        line_thickness = 1.f;
    }
    changed |= Colors::DrawSettingHueWheel("HoS range", &color_range_hos);
    changed |= Colors::DrawSettingHueWheel("Aggro range", &color_range_aggro);
    changed |= Colors::DrawSettingHueWheel("Cast range", &color_range_cast);
    changed |= Colors::DrawSettingHueWheel("Spirit range", &color_range_spirit);
    changed |= Colors::DrawSettingHueWheel("Compass range", &color_range_compass);
    changed |= Colors::DrawSettingHueWheel("Chain Aggro range", &color_range_chain_aggro);
    changed |= Colors::DrawSettingHueWheel("Res Aggro range", &color_range_res_aggro);
    changed |= Colors::DrawSettingHueWheel("Shadow Step range", &color_range_shadowstep_aggro);
    changed |= ImGui::DragFloat("Line thickness", &line_thickness, 0.1f, 1.f, 10.f, "%.1f");
    if (changed)
        Invalidate();
}

size_t RangeRenderer::CreateCircle(D3DVertex *vertices, float radius, DWORD color) const
{
    const auto scale = Minimap::Instance().GetGwinchScale();
    const auto xdiff = static_cast<float>(line_thickness) / scale.x;
    const auto ydiff = static_cast<float>(line_thickness) / scale.y;
    size_t circle_vertices = 0;
    for (circle_vertices = 0; circle_vertices < circle_points - 1; circle_vertices += 2) {
        const auto angle = circle_vertices * (2 * static_cast<float>(M_PI) / circle_triangles);
        vertices[circle_vertices].x = radius * std::cosf(angle);
        vertices[circle_vertices].y = radius * std::sinf(angle);
        vertices[circle_vertices + 1].x = (radius - xdiff) * std::cosf(angle);
        vertices[circle_vertices + 1].y = (radius - ydiff) * std::sinf(angle);
        vertices[circle_vertices].z = vertices[circle_vertices + 1].z = 0.0f;
        vertices[circle_vertices].color = vertices[circle_vertices + 1].color = color;
    }
    return circle_vertices;
}

void RangeRenderer::Initialize(IDirect3DDevice9 *device)
{
    count = circle_points * num_circles;
    type = D3DPT_TRIANGLESTRIP;

    checkforhos_ = true;
    havehos_ = false;

    D3DVertex *vertices = nullptr;
    size_t vertex_count = count + 6;

    device->CreateVertexBuffer(sizeof(D3DVertex) * vertex_count, D3DUSAGE_WRITEONLY, D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer, nullptr);
    buffer->Lock(0, sizeof(D3DVertex) * vertex_count, reinterpret_cast<void **>(&vertices), D3DLOCK_DISCARD);
    ASSERT(vertices != nullptr);

    const D3DVertex *vertices_max = vertices + vertex_count;

    // Compass range
    float radius = GW::Constants::Range::Compass;
    vertices += CreateCircle(vertices, radius, color_range_compass);
    ASSERT(vertices < vertices_max);

    // Spirit range
    radius = GW::Constants::Range::Spirit;
    vertices += CreateCircle(vertices, radius, color_range_spirit);
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
    for (size_t i = 0; i < 6; ++i) {
        vertices[i].z = 0.0f;
        vertices[i].color = color_range_hos;
    }
    vertices[0].x = 260.0f;
    vertices[0].y = 0.0f;
    vertices[1].x = 460.0f;
    vertices[1].y = 0.0f;

    vertices[2].x = -150.0f;
    vertices[2].y = 0.0f;
    vertices[3].x = 150.0f;
    vertices[3].y = 0.0f;
    vertices[4].x = 0.0f;
    vertices[4].y = -150.0f;
    vertices[5].x = 0.0f;
    vertices[5].y = 150.0f;

    buffer->Unlock();
}

void RangeRenderer::Render(IDirect3DDevice9 *device)
{
    if (!initialized) {
        initialized = true;
        Initialize(device);
    }

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

    size_t render_index = 0;

    // Draw first 4 ranges always (compass, spirit, cast, aggro)
    while (render_index < 4) {
        device->DrawPrimitive(type, circle_points * render_index, circle_triangles);
        render_index++;
    }

    // Draw Hos range
    if (havehos_)
        device->DrawPrimitive(type, circle_points * render_index, circle_triangles);
    render_index++;

    // Draw either aggro range or res range
    const GW::AgentLiving *target = GW::Agents::GetTargetAsAgentLiving();
    if (target && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable) {
        Color *color = nullptr;
        size_t target_circle_render_index = 0;
        if (target->allegiance == 0x1 && target->GetIsDead()) {
            color = &color_range_res_aggro;
            target_circle_render_index = 1;
        } else if (target->allegiance == 0x3) {
            color = &color_range_chain_aggro;
        }
        
        if (color != nullptr && (*color & IM_COL32_A_MASK) != 0) {
            D3DXMATRIX translate, oldworld;
            device->GetTransform(D3DTS_WORLD, &oldworld);
            D3DXMatrixTranslation(&translate, target->x, target->y, 0.0f);
            device->SetTransform(D3DTS_WORLD, &translate);
            device->DrawPrimitive(type, circle_points * (render_index + target_circle_render_index), circle_triangles);
            device->SetTransform(D3DTS_WORLD, &oldworld);
        }
    }
    render_index++;
    render_index++;

    // Draw shadowstep range i.e. shadowwalk aggro
    const GW::Vec2f &shadowstep_location = Minimap::Instance().ShadowstepLocation();
    if ((color_range_shadowstep_aggro & IM_COL32_A_MASK) != 0 && (shadowstep_location.x != 0.f || shadowstep_location.y != 0.f)) {
        D3DXMATRIX translate, oldworld;
        device->GetTransform(D3DTS_WORLD, &oldworld);
        D3DXMatrixTranslation(&translate, shadowstep_location.x, shadowstep_location.y, 0.0f);
        device->SetTransform(D3DTS_WORLD, &translate);
        device->DrawPrimitive(type, circle_points * render_index, circle_triangles);
        device->SetTransform(D3DTS_WORLD, &oldworld);
    }
    render_index++;

    // Draw hos line
    if (havehos_) {
        GW::AgentLiving *me = GW::Agents::GetPlayerAsAgentLiving();
        GW::AgentLiving *tgt = GW::Agents::GetTargetAsAgentLiving();

        if (!draw_center_ && me != nullptr && tgt != nullptr && me != tgt && !me->GetIsDead() && !tgt->GetIsDead() && GW::GetSquareDistance(tgt->pos, me->pos) < GW::Constants::SqrRange::Spellcast) {
            GW::Vec2f v = me->pos - tgt->pos;
            const float angle = std::atan2(v.y, v.x);

            D3DXMATRIX oldworld, rotate, newworld;
            device->GetTransform(D3DTS_WORLD, &oldworld);
            D3DXMatrixRotationZ(&rotate, angle);
            newworld = rotate * oldworld;
            device->SetTransform(D3DTS_WORLD, &newworld);
            device->DrawPrimitive(D3DPT_LINESTRIP, circle_points * num_circles, 1);
            device->SetTransform(D3DTS_WORLD, &oldworld);
        }
    }

    if (draw_center_) {
        device->DrawPrimitive(D3DPT_LINESTRIP, circle_points * num_circles + 2, 1);
        device->DrawPrimitive(D3DPT_LINESTRIP, circle_points * num_circles + 4, 1);
    }
}

bool RangeRenderer::HaveHos()
{
    GW::Skillbar *skillbar = GW::SkillbarMgr::GetPlayerSkillbar();
    if (!skillbar || !skillbar->IsValid()) {
        checkforhos_ = true;
        return false;
    }

    for (const auto& skill : skillbar->skills) {
        const auto id = static_cast<GW::Constants::SkillID>(skill.skill_id);
        if (id == GW::Constants::SkillID::Heart_of_Shadow)
            return true;
        if (id == GW::Constants::SkillID::Vipers_Defense)
            return true;
    }
    return false;
}
