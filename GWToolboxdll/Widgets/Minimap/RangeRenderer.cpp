#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>

#include <Widgets/Minimap/RangeRenderer.h>

void RangeRenderer::LoadSettings(CSimpleIni* ini, const char* section) {
    color_range_hos = Colors::Load(ini, section, "color_range_hos", 0xFF881188);
    color_range_aggro = Colors::Load(ini, section, "color_range_aggro", 0xFF994444);
    color_range_cast = Colors::Load(ini, section, "color_range_cast", 0xFF117777);
    color_range_spirit = Colors::Load(ini, section, "color_range_spirit", 0xFF337733);
    color_range_compass = Colors::Load(ini, section, "color_range_compass", 0xFF666611);
    line_thickness = ini->GetLongValue(section, "range_line_thickness", 1);
    Invalidate();
}
void RangeRenderer::SaveSettings(CSimpleIni* ini, const char* section) const {
    Colors::Save(ini, section, "color_range_hos", color_range_hos);
    Colors::Save(ini, section, "color_range_aggro", color_range_aggro);
    Colors::Save(ini, section, "color_range_cast", color_range_cast);
    Colors::Save(ini, section, "color_range_spirit", color_range_spirit);
    Colors::Save(ini, section, "color_range_compass", color_range_compass);
    ini->SetLongValue(section, "range_line_thickness", line_thickness);
}
void RangeRenderer::DrawSettings() {
    bool changed = false;
    if (ImGui::SmallButton("Restore Defaults")) {
        changed = true;
        color_range_hos = 0xFF881188;
        color_range_aggro = 0xFF994444;
        color_range_cast = 0xFF117777;
        color_range_spirit = 0xFF337733;
        color_range_compass = 0xFF666611;
        line_thickness = 1;
    }
    changed |= Colors::DrawSettingHueWheel("HoS range", &color_range_hos);
    changed |= Colors::DrawSettingHueWheel("Aggro range", &color_range_aggro);
    changed |= Colors::DrawSettingHueWheel("Cast range", &color_range_cast);
    changed |= Colors::DrawSettingHueWheel("Spirit range", &color_range_spirit);
    changed |= Colors::DrawSettingHueWheel("Compass range", &color_range_compass);
    changed |= ImGui::DragInt("Line thickness", &line_thickness, 0.1f, 1, 30, "%d");
    if (changed) Invalidate();
}

void RangeRenderer::CreateCircle(D3DVertex *vertices, float radius, DWORD color) const
{
    for (size_t i = 0; i < circle_vertices - 1; ++i) {
        const auto angle = i * (2 * static_cast<float>(M_PI) / circle_vertices);
        for (auto j = 0; j < line_thickness; j++) {
            vertices[i + j * circle_vertices].x = (radius - j) * std::cos(angle);
            vertices[i + j * circle_vertices].y = (radius - j) * std::sin(angle);
            vertices[i + j * circle_vertices].z = 0.0f;
            vertices[i + j * circle_vertices].color = color;
        }
    }
    for (auto j = 0; j < line_thickness; j++) {
        vertices[circle_vertices * (j + 1) - 1] = vertices[circle_vertices * j];
    }
}

void RangeRenderer::Initialize(IDirect3DDevice9* device) {
    count = circle_vertices * num_circles * line_thickness;
    type = D3DPT_LINESTRIP;

    checkforhos_ = true;
    havehos_ = false;

    D3DVertex* vertices = nullptr;
    const unsigned int vertex_count = count + 6;

    device->CreateVertexBuffer(sizeof(D3DVertex) * vertex_count, D3DUSAGE_WRITEONLY,
        D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer, nullptr);
    buffer->Lock(0, sizeof(D3DVertex) * vertex_count,
        reinterpret_cast<void**>(&vertices), D3DLOCK_DISCARD);

    auto radius = GW::Constants::Range::Compass;
    CreateCircle(vertices, radius, color_range_compass);
    vertices += circle_vertices * line_thickness;

    radius = GW::Constants::Range::Spirit;
    CreateCircle(vertices, radius, color_range_spirit);
    vertices += circle_vertices * line_thickness;

    radius = GW::Constants::Range::Spellcast;
    CreateCircle(vertices, radius, color_range_cast);
    vertices += circle_vertices * line_thickness;

    radius = GW::Constants::Range::Earshot;
    CreateCircle(vertices, radius, color_range_aggro);
    vertices += circle_vertices * line_thickness;

    radius = 360.0f;
    CreateCircle(vertices, radius, color_range_hos);
    vertices += circle_vertices * line_thickness;

    for (auto i = 0; i < 6; ++i) {
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

void RangeRenderer::Render(IDirect3DDevice9* device) {
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
    for (size_t i = 0; i < (num_circles - 1) * line_thickness; ++i) { // do not draw HoS circles yet
        device->DrawPrimitive(type, circle_vertices * i, circle_points);
    }

    if (havehos_) {
        for (auto i = 0; i < line_thickness ; ++i) { // draw HoS circles
            device->DrawPrimitive(type, circle_vertices * ((num_circles - 1) * line_thickness + i), circle_points);
        }

        GW::AgentLiving* me = GW::Agents::GetPlayerAsAgentLiving();
        GW::AgentLiving* tgt = GW::Agents::GetTargetAsAgentLiving();

        if (!draw_center_
            && me != nullptr
            && tgt != nullptr
            && me != tgt
            && !me->GetIsDead()
            && !tgt->GetIsDead()
            && GW::GetSquareDistance(tgt->pos, me->pos) < GW::Constants::SqrRange::Spellcast) {
            
            GW::Vec2f v = me->pos - tgt->pos;
            float angle = std::atan2(v.y, v.x);

            D3DXMATRIX oldworld, rotate, newworld;
            device->GetTransform(D3DTS_WORLD, &oldworld);
            D3DXMatrixRotationZ(&rotate, angle);
            newworld = rotate * oldworld;
            device->SetTransform(D3DTS_WORLD, &newworld);
            device->DrawPrimitive(type, circle_vertices * num_circles * line_thickness, 1);
            device->SetTransform(D3DTS_WORLD, &oldworld);
        }
    }

    if (draw_center_) {
        device->DrawPrimitive(type, circle_vertices * num_circles * line_thickness + 2, 1);
        device->DrawPrimitive(type, circle_vertices * num_circles * line_thickness + 4, 1);
    }
}

bool RangeRenderer::HaveHos() {
    GW::Skillbar *skillbar = GW::SkillbarMgr::GetPlayerSkillbar();
    if (!skillbar || !skillbar->IsValid()) {
        checkforhos_ = true;
        return false;
    }

    for (auto skill : skillbar->skills) {
        const auto id = static_cast<GW::Constants::SkillID>(skill.skill_id);
        if (id == GW::Constants::SkillID::Heart_of_Shadow) return true;
        if (id == GW::Constants::SkillID::Vipers_Defense) return true;
    }
    return false;
}
