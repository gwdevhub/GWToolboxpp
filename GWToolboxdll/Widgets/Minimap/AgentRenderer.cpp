#include "stdafx.h"

#include <GWCA/Context/MapContext.h>
#include <GWCA/Constants/Maps.h>
#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Pathing.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <Timer.h>
#include <Widgets/Minimap/AgentRenderer.h>

AgentRenderer::AgentRenderer()
{
    last_check = TIMER_INIT();
    shapes[Tear].AddVertex(1.8f, 0, Dark);
    shapes[Tear].AddVertex(0.7f, 0.7f, Dark);
    shapes[Tear].AddVertex(0.0f, 0.0f, Light);
    shapes[Tear].AddVertex(0.7f, 0.7f, Dark);
    shapes[Tear].AddVertex(0.0f, 1.0f, Dark);
    shapes[Tear].AddVertex(0.0f, 0.0f, Light);
    shapes[Tear].AddVertex(0.0f, 1.0f, Dark);
    shapes[Tear].AddVertex(-0.7f, 0.7f, Dark);
    shapes[Tear].AddVertex(0.0f, 0.0f, Light);
    shapes[Tear].AddVertex(-0.7f, 0.7f, Dark);
    shapes[Tear].AddVertex(-1.0f, 0.0f, Dark);
    shapes[Tear].AddVertex(0.0f, 0.0f, Light);
    shapes[Tear].AddVertex(-1.0f, 0.0f, Dark);
    shapes[Tear].AddVertex(-0.7f, -0.7f, Dark);
    shapes[Tear].AddVertex(0.0f, 0.0f, Light);
    shapes[Tear].AddVertex(-0.7f, -0.7f, Dark);
    shapes[Tear].AddVertex(0.0f, -1.0f, Dark);
    shapes[Tear].AddVertex(0.0f, 0.0f, Light);
    shapes[Tear].AddVertex(0.0f, -1.0f, Dark);
    shapes[Tear].AddVertex(0.7f, -0.7f, Dark);
    shapes[Tear].AddVertex(0.0f, 0.0f, Light);
    shapes[Tear].AddVertex(0.7f, -0.7f, Dark);
    shapes[Tear].AddVertex(1.8f, 0.0f, Dark);
    shapes[Tear].AddVertex(0.0f, 0.0f, Light);

    constexpr auto pi = DirectX::XM_PI;
    for (int i = 0; i < num_triangles; ++i) {
        const auto angle1 = 2 * (i + 0) * pi / num_triangles;
        const auto angle2 = 2 * (i + 1) * pi / num_triangles;
        shapes[Circle].AddVertex(std::cos(angle1), std::sin(angle1), Dark);
        shapes[Circle].AddVertex(std::cos(angle2), std::sin(angle2), Dark);
        shapes[Circle].AddVertex(0.0f, 0.0f, Light);
        shapes[BigCircle].AddVertex(std::cos(angle1), std::sin(angle1), None);
        shapes[BigCircle].AddVertex(std::cos(angle2), std::sin(angle2), None);
        shapes[BigCircle].AddVertex(0.0f, 0.0f, CircleCenter);
    }

    shapes[Quad].AddVertex(1.0f, -1.0f, Dark);
    shapes[Quad].AddVertex(1.0f, 1.0f, Dark);
    shapes[Quad].AddVertex(0.0f, 0.0f, Light);
    shapes[Quad].AddVertex(1.0f, 1.0f, Dark);
    shapes[Quad].AddVertex(-1.0f, 1.0f, Dark);
    shapes[Quad].AddVertex(0.0f, 0.0f, Light);
    shapes[Quad].AddVertex(-1.0f, 1.0f, Dark);
    shapes[Quad].AddVertex(-1.0f, -1.0f, Dark);
    shapes[Quad].AddVertex(0.0f, 0.0f, Light);
    shapes[Quad].AddVertex(-1.0f, -1.0f, Dark);
    shapes[Quad].AddVertex(1.0f, -1.0f, Dark);
    shapes[Quad].AddVertex(0.0f, 0.0f, Light);

    constexpr size_t star_ntriangles = 16;
    constexpr auto star_size_small = 1.f;
    constexpr auto star_size_big = 1.5f;
    for (unsigned int i = 0; i < star_ntriangles; ++i) {
        const auto angle1 = 2 * (i + 0) * pi / star_ntriangles;
        const auto angle2 = 2 * (i + 1) * pi / star_ntriangles;
        const auto size1 = (i + 0) % 2 == 0 ? star_size_small : star_size_big;
        const auto size2 = (i + 1) % 2 == 0 ? star_size_small : star_size_big;
        shapes[Star].AddVertex(std::cos(angle1) * size1, std::sin(angle1) * size1, None);
        shapes[Star].AddVertex(std::cos(angle2) * size2, std::sin(angle2) * size2, None);
        shapes[Star].AddVertex(0.0f, 0.0f, CircleCenter);
    }
}

void AgentRenderer::Shape_t::AddVertex(const float x, const float y, const Color_Modifier mod)
{
    vertices.push_back(Shape_Vertex(x, y, mod));
}

void AgentRenderer::Initialize(IDirect3DDevice9* device)
{
    type = D3DPT_TRIANGLELIST;
    D3DVertexBuffer::Initialize(device);
}

void AgentRenderer::Terminate()
{
    D3DVertexBuffer::Terminate();
    AgentAppearanceWindow::ResetAppearanceCache();
}

void AgentRenderer::Render(IDirect3DDevice9* device)
{
    const auto now = TIMER_INIT();
    if (now - last_check > 33) {
        last_check = now;
        clear();

        if (AgentAppearanceWindow::show_props_on_minimap) {
            const auto& props = GW::GetMapContext()->props->propArray;
            for (size_t i = 0; i < props.size(); ++i) {
                Enqueue(AgentAppearanceWindow::default_shape, props[i], AgentAppearanceWindow::GetBaseSize(), AgentAppearanceWindow::color_default);
            }
        }
        auto* agents = GW::Agents::GetAgentArray();
        if (!agents) return;

        const auto* player = GW::Agents::GetControlledCharacter();
        const auto* target = GW::Agents::GetTarget();
        if (target) {
            AgentAppearanceWindow::auto_target_id = 0;
        }
        else if (AgentAppearanceWindow::auto_target_id) {
            const auto* target_agent = GW::Agents::GetAgentByID(AgentAppearanceWindow::auto_target_id);
            target = target_agent ? target_agent->GetAsAgentLiving() : nullptr;
        }
        static std::vector<std::pair<const GW::Agent*, const AppearanceRule*>> custom_agents_to_draw;
        static std::vector<const GW::AgentLiving*> marked_targets_to_draw;
        static std::vector<const GW::AgentLiving*> players_to_draw;
        static std::vector<const GW::AgentLiving*> dead_agents_to_draw;
        static std::vector<const GW::Agent*> other_agents_to_draw;
        custom_agents_to_draw.clear();
        marked_targets_to_draw.clear();
        players_to_draw.clear();
        dead_agents_to_draw.clear();
        other_agents_to_draw.clear();
        target_drawn = false;

        const auto add_custom_agent = [](const GW::Agent* agent) {
            const auto* rules = AgentAppearanceWindow::GetAppearanceRules(agent);
            if (!rules) return false;
            custom_agents_to_draw.emplace_back(agent, rules->front());
            return true;
        };
        for (const auto* agent : *agents) {
            if (!agent || agent == player || agent == target) continue;
            if (agent->GetIsGadgetType()) {
                const auto* gadget = agent->GetAsAgentGadget();
                if (GW::Map::GetMapID() == GW::Constants::MapID::Domain_of_Anguish && gadget->extra_type == 7602) continue;
                if (add_custom_agent(gadget)) continue;
            }
            else if (agent->GetIsLivingType()) {
                const auto* living = agent->GetAsAgentLiving();
                if (!AgentAppearanceWindow::show_hidden_npcs && !GW::Agents::GetAgentMatchesFlags(living)) continue;
                if (AgentAppearanceWindow::IsMarked(living->agent_id)) {
                    marked_targets_to_draw.push_back(living);
                    continue;
                }
                if (living->IsPlayer() && living != GW::Agents::GetObservingAgent()) {
                    players_to_draw.push_back(living);
                    continue;
                }
                if (living->GetIsDead()) {
                    dead_agents_to_draw.push_back(living);
                    continue;
                }
                if (add_custom_agent(living)) continue;
            }
            else if (agent->GetIsItemType() && add_custom_agent(agent)) {
                continue;
            }
            other_agents_to_draw.push_back(agent);
        }
        for (const auto* agent : dead_agents_to_draw) Enqueue(agent);
        for (const auto* agent : other_agents_to_draw) Enqueue(agent);
        std::ranges::sort(custom_agents_to_draw, [](const auto& a, const auto& b) { return a.second->index > b.second->index; });
        for (const auto& entry : custom_agents_to_draw) Enqueue(entry.first);
        for (const auto* agent : marked_targets_to_draw) {
            if (agent->GetIsAlive()) Enqueue(agent);
        }
        if (target && (!target->GetAsAgentLiving() || !target->GetAsAgentLiving()->IsPlayer())) Enqueue(target);
        for (const auto* agent : players_to_draw) Enqueue(agent);
        if (target && target != player && target->GetAsAgentLiving() && target->GetAsAgentLiving()->IsPlayer()) Enqueue(target);
        if (player) Enqueue(player);
    }
    D3DVertexBuffer::Render(device);
}

void AgentRenderer::Enqueue(const GW::Agent* agent)
{
    auto color = Color{0};
    auto size = 0.f;
    auto shape = Shape_None;
    auto border_color = Color{0};
    auto border_thickness = 0.f;
    AgentAppearanceWindow::GetAgentAppearance(agent, &shape, &color, &border_color, &border_thickness, nullptr, &size);
    Enqueue(shape, agent, size, color, border_color, border_thickness);
}

void AgentRenderer::Enqueue(const Shape_e shape, const GW::Agent* agent, const float size, const Color color,
    const Color border_color, const float border_thickness)
{
    const auto alpha = color >> IM_COL32_A_SHIFT & 0xFFu;
    if (!alpha) return;
    const RenderPosition pos = {agent->rotation_cos, agent->rotation_sin, agent->pos};
    if (shape != BigCircle) {
        const auto is_target = AgentAppearanceWindow::auto_target_id == agent->agent_id || GW::Agents::GetTargetId() == agent->agent_id;
        if (is_target && target_drawn) return;
        if (border_thickness > 0.f) {
            Enqueue(shape, pos, size + border_thickness, border_color);
        }
        if (is_target) target_drawn = true;
    }
    Enqueue(shape, pos, size, color, AgentAppearanceWindow::color_agent_modifier);
}

void AgentRenderer::Enqueue(const Shape_e shape, const GW::MapProp* agent, const float size, const Color color)
{
    const RenderPosition pos = {agent->rotation_cos, agent->rotation_sin, {agent->position.x, agent->position.y}};
    Enqueue(shape, pos, size, color);
}

void AgentRenderer::Enqueue(const Shape_e shape, const RenderPosition& pos, const float size, const Color color, const Color modifier)
{
    if (shape == Shape_None || (color & IM_COL32_A_MASK) == 0) return;
    const auto& shape_verts = shapes[shape].vertices;
    vertices.reserve(vertices.size() + shape_verts.size());
    const auto c_dark = Colors::Sub(color, modifier);
    const auto c_light = Colors::Add(color, modifier);
    const auto c_center = Colors::Sub(color, IM_COL32(0, 0, 0, 50));
    for (const auto& vert : shape_verts) {
        GW::Vec2f calc_pos;
        calc_pos.x = ((vert.x * pos.rotation_cos) - (vert.y * pos.rotation_sin)) * size + pos.position.x;
        calc_pos.y = ((vert.x * pos.rotation_sin) + (vert.y * pos.rotation_cos)) * size + pos.position.y;
        auto c = color;
        switch (vert.modifier) {
            case Dark:
                c = c_dark;
                break;
            case Light:
                c = c_light;
                break;
            case CircleCenter:
                c = c_center;
                break;
            default:
                break;
        }
        vertices.push_back({calc_pos.x, calc_pos.y, c});
    }
}
