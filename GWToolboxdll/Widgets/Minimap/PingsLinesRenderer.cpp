#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/Packets/StoC.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <Defines.h>
#include <Utils/GuiUtils.h>
#include <Widgets/Minimap/Minimap.h>
#include <GWCA/Managers/PlayerMgr.h>

namespace {
    bool is_minimap_compass_draw = false;
}

void PingsLinesRenderer::RegisterSettings(ToolboxModule* module)
{
    // SettingColor is layout-compatible with Color; the cast lets the registry persist it as a hex string
    const auto register_color = [module](const char* key, Color* color) {
        SettingsRegistry::RegisterField(module, key, reinterpret_cast<Colors::SettingColor*>(color));
    };
    register_color("color_drawings", &color_drawings);
    register_color("color_pings", &color_pings);
    register_color("color_shadowstep_mark", &marker.color);
    register_color("color_shadowstep_line", &color_shadowstep_line);
    register_color("color_shadowstep_line_maxrange", &color_shadowstep_line_maxrange);
    SettingsRegistry::RegisterField(module, "maxrange_interp_begin", &maxrange_interp_begin);
    SettingsRegistry::RegisterField(module, "maxrange_interp_end", &maxrange_interp_end);
    SettingsRegistry::RegisterField(module, "reduce_ping_spam", &reduce_ping_spam);
}

void PingsLinesRenderer::DrawSettings()
{
    bool changed = false;
    ImGui::SmallConfirmButton("Restore Defaults", "Are you sure?", [&](bool result, void*) {
        if (result) {
            color_drawings = Colors::ARGB(0xFF, 0xFF, 0xFF, 0xFF);
            color_pings = Colors::ARGB(104, 255, 0, 0);
            marker.color = Colors::ARGB(200, 128, 0, 128);
            color_shadowstep_line = Colors::ARGB(48, 128, 0, 128);
            color_shadowstep_line_maxrange = Colors::ARGB(48, 128, 0, 128);
            ping_circle.Invalidate();
            marker.Invalidate();
        }
        });
    changed |= Colors::DrawSettingHueWheel("Drawings", &color_drawings);
    changed |= Colors::DrawSettingHueWheel("Player Pings", &color_pings);
    ImGui::ShowHelp("The alpha level is also used for the game's pings");
    changed |= Colors::DrawSettingHueWheel("Shadow Step Marker", &marker.color);
    changed |= Colors::DrawSettingHueWheel("Shadow Step Line", &color_shadowstep_line);
    changed |= Colors::DrawSettingHueWheel("Shadow Step Line (Max range)", &color_shadowstep_line_maxrange);
    if (ImGui::SliderFloat("Max range start", &maxrange_interp_begin, 0.0f, 1.0f)
        && maxrange_interp_end < maxrange_interp_begin) {
        maxrange_interp_end = maxrange_interp_begin;
    }
    if (ImGui::SliderFloat("Max range end", &maxrange_interp_end, 0.0f, 1.0f)
        && maxrange_interp_begin > maxrange_interp_end) {
        maxrange_interp_begin = maxrange_interp_end;
    }

    if (changed) {
        ping_circle.Invalidate();
        marker.Invalidate();
    }
}

PingsLinesRenderer::PingsLinesRenderer()
    : lastshown(TIMER_INIT()), lastsent(TIMER_INIT()), lastqueued(TIMER_INIT()) {}

void PingsLinesRenderer::P046Callback(const GW::Packet::StoC::AgentPinged* pak)
{
    bool found = false;
    if (reduce_ping_spam) {
        for (Ping* ping : pings) {
            if (ping->GetAgentID() == pak->agent_id) {
                // extend the duration to count for the current ping.
                const clock_t diff = TIMER_DIFF(ping->start);
                ping->duration = 3000 + diff;
                found = true;
                break;
            }
        }
    }
    if (!found) {
        pings.push_front(new AgentPing(pak->agent_id));
    }
}

void PingsLinesRenderer::OnUIMessage(GW::HookStatus*, GW::UI::UIMessage message_id, void* wparam, void*) {
    switch (message_id) {
    case GW::UI::UIMessage::kCompassDraw: {
        const auto packet = (GW::UI::UIPacket::kCompassDraw*)wparam;

        bool new_session;

        if (is_minimap_compass_draw) {
            return;
        }
        if (drawings[packet->player_number].player == packet->player_number) {
            new_session = drawings[packet->player_number].session != packet->session_id;
            drawings[packet->player_number].session = packet->session_id;
        }
        else {
            drawings[packet->player_number].player = packet->player_number;
            drawings[packet->player_number].session = packet->session_id;
            new_session = true;
        }

        if (new_session && packet->number_of_points == 1) {
            pings.push_front(new TerrainPing(
                packet->points[0].x * drawing_scale,
                packet->points[0].y * drawing_scale));
            return;
        }

        if (new_session) {
            for (auto i = 0u; i < packet->number_of_points - 1; i++) {
                DrawingLine l;
                l.x1 = packet->points[i + 0].x * drawing_scale;
                l.y1 = packet->points[i + 0].y * drawing_scale;
                l.x2 = packet->points[i + 1].x * drawing_scale;
                l.y2 = packet->points[i + 1].y * drawing_scale;
                drawings[packet->player_number].lines.push_back(l);
            }
        }
        else {
            if (drawings[packet->player_number].lines.empty()) {
                return;
            }
            for (auto i = 0u; i < packet->number_of_points; i++) {
                DrawingLine l;
                if (i == 0) {
                    l.x1 = drawings[packet->player_number].lines.back().x2;
                    l.y1 = drawings[packet->player_number].lines.back().y2;
                }
                else {
                    l.x1 = packet->points[i - 1].x * drawing_scale;
                    l.y1 = packet->points[i - 1].y * drawing_scale;
                }
                l.x2 = packet->points[i].x * drawing_scale;
                l.y2 = packet->points[i].y * drawing_scale;
                drawings[packet->player_number].lines.push_back(l);
            }
        }
    } break;
    case GW::UI::UIMessage::kCompassPing: {
        const auto packet = (GW::UI::UIPacket::kCompassPing*)wparam;

        pings.push_front(new TerrainPing(
            packet->point.x * drawing_scale,
            packet->point.y * drawing_scale,
            packet->color
        ));
    } break;
    }


}

void PingsLinesRenderer::P153Callback(const GW::Packet::StoC::GenericValueTarget* pak)
{
    if (pak->Value_id == 20
        && pak->caster == GW::Agents::GetControlledCharacterId()
        && pak->value == 928) {
        recall_target = pak->target;
    }
}

void PingsLinesRenderer::Initialize(IDirect3DDevice9* device)
{
    if (initialized) {
        return;
    }

    initialized = true;
    type = D3DPT_LINELIST;

    vertices_max = 0x1000; // support for up to 4096 line segments, should be enough

    vertices = nullptr;

    const HRESULT hr = device->CreateVertexBuffer(sizeof(D3DVertex) * vertices_max, D3DUSAGE_WRITEONLY,
                                                  D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer, nullptr);
    if (FAILED(hr)) {
        printf("Error setting up PingsLinesRenderer vertex buffer: HRESULT: 0x%lX\n", hr);
    }
}

void PingsLinesRenderer::Render(IDirect3DDevice9* device)
{
    Initialize(device);

    DrawPings(device);

    DrawShadowstepMarker(device);

    vertices_count = 0;
    const auto i = DirectX::XMMatrixIdentity();
    device->SetTransform(D3DTS_WORLD, reinterpret_cast<const D3DMATRIX*>(&i));

    if (!HasPendingLines()) {
        return;
    }

    const HRESULT res = buffer->Lock(0, sizeof(D3DVertex) * vertices_max, reinterpret_cast<void**>(&vertices), 0);
    if (FAILED(res)) {
        printf("PingsLinesRenderer Lock() error: HRESULT 0x%lX\n", res);
        return;
    }

    DrawShadowstepLine(device);

    DrawRecallLine(device);

    DrawDrawings(device);

    buffer->Unlock();
    if (vertices_count != 0) {
        device->SetStreamSource(0, buffer, 0, sizeof(D3DVertex));
        device->DrawPrimitive(type, 0, vertices_count / 2);
        vertices_count = 0;
    }
}

bool PingsLinesRenderer::HasPendingLines() const
{
    if ((color_shadowstep_line & IM_COL32_A_MASK) != 0) {
        const GW::Vec2f& shadowstep_location = Minimap::Instance().ShadowstepLocation();
        if (shadowstep_location.x != 0.0f || shadowstep_location.y != 0.0f || recall_target != 0) {
            return true;
        }
    }
    return std::ranges::any_of(drawings, [](const auto& drawing) {
        return drawing.second.player != 0 && !drawing.second.lines.empty();
    });
}

void PingsLinesRenderer::DrawPings(IDirect3DDevice9* device)
{
    for (const Ping* ping : pings) {
        float px = 0.f, py = 0.f, ping_scale = 0.f;
        if (const DWORD agent_id = ping->GetAgentID()) {
            const GW::Agent* agent = GW::Agents::GetAgentByID(agent_id);
            if (agent) {
                px = agent->pos.x;
                py = agent->pos.y;
                ping_scale = 1.0f;
            }
        }
        else {
            px = ping->GetX();
            py = ping->GetY();
            ping_scale = ping->GetScale();
        }
        if (ping_scale == 0) {
            continue;
        }
        if (TIMER_DIFF(ping->start) > ping->duration) {
            continue;
        }

        if (ping->GetColor() != Colors::Empty()) {
            ping_circle.color = (ping->GetColor() & ~IM_COL32_A_MASK) | (color_pings & IM_COL32_A_MASK);
        }
        else {
            ping_circle.color = color_pings;
        }

        DirectX::XMMATRIX scale, world;
        const auto translate = DirectX::XMMatrixTranslation(px, py, 0.0f);

        if (ping->ShowInner()) {
            static IDirect3DTexture9** inner_texture_ptr = nullptr;

            if (inner_texture_ptr == nullptr) {
                inner_texture_ptr = GwDatModule::LoadGreyscaleTextureFromFileId(PING_INNER_FILE_ID);
            }
            
            const auto context = Minimap::GetRenderContext();
            const GW::Agent* me = inner_texture_ptr ? GW::Agents::GetObservingAgent() : nullptr;

            if (me) {
                const float rotation = context.rotation - DirectX::XM_PIDIV2;
                const auto center = me->pos - GW::Rotate(context.translation, rotation) / context.zoom_scale;

                const auto p = GW::Vec2f(px, py);
                const auto delta = p - center;

                const float max_distance = (GW::Constants::Range::Compass - drawing_scale) / context.zoom_scale;
                const float distance_sq = GW::GetSquareDistance(p, center);

                auto inner_translate = translate;

                if (distance_sq > max_distance * max_distance) {
                    const float distance = GW::GetDistance(p, center);
                    const float factor = max_distance / distance;
                    const auto clamped = delta * factor;

                    inner_translate = DirectX::XMMatrixTranslation(
                        center.x + clamped.x,
                        center.y + clamped.y,
                        0.0f
                    );
                }

                scale = DirectX::XMMatrixScaling(drawing_scale * 2, drawing_scale * 2, 1.0f);
                world = scale * inner_translate;
                device->SetTransform(D3DTS_WORLD, reinterpret_cast<const D3DMATRIX*>(&world));

                ping_circle.texture = *inner_texture_ptr;
                ping_circle.Render(device);
            }
        }

        static IDirect3DTexture9** outer_texture_ptr = nullptr;

        if (outer_texture_ptr == nullptr) {
            outer_texture_ptr = GwDatModule::LoadGreyscaleTextureFromFileId(PING_OUTER_FILE_ID);
        }

        if (outer_texture_ptr) {
            int diff = TIMER_DIFF(ping->start);
            const bool first_loop = diff < 1000;
            diff = diff % 1000;
            diff *= first_loop ? 2 : 1;

            scale = DirectX::XMMatrixScaling(diff * ping_scale, diff * ping_scale, 1.0f);
            world = scale * translate;
            device->SetTransform(D3DTS_WORLD, reinterpret_cast<const D3DMATRIX*>(&world));

            ping_circle.texture = *outer_texture_ptr;
            ping_circle.Render(device);
        }
    }
    if (!pings.empty()) {
        const Ping* last = pings.back();
        if (TIMER_DIFF(last->start) > last->duration) {
            delete last;
            pings.pop_back();
        }
    }
}

void PingsLinesRenderer::EnqueueVertex(const float x, const float y, const Color color)
{
    if (vertices_count == vertices_max) {
        return;
    }
    vertices[0].x = x;
    vertices[0].y = y;
    vertices[0].z = 0.0f;
    vertices[0].color = color;
    ++vertices;
    ++vertices_count;
}

void PingsLinesRenderer::DrawDrawings(IDirect3DDevice9*)
{
    for (auto it = drawings.begin(); it != drawings.end(); ++it) {
        if (it->second.player == 0) {
            continue;
        }
        std::deque<DrawingLine>& lines = it->second.lines;

        if (vertices_count < vertices_max - 2) {
            for (const DrawingLine& line : lines) {
                const uint32_t max_alpha = (color_drawings & IM_COL32_A_MASK) >> IM_COL32_A_SHIFT;
                const uint32_t left = static_cast<uint32_t>(drawing_timeout - TIMER_DIFF(line.start));
                // @Robustness:
                // This is not safe, casting time to uint32_t is unsafe.
                if (left > static_cast<uint32_t>(drawing_timeout)) {
                    continue; // This is actually a negative integer i.e. no time left.
                }
                uint32_t alpha = left * max_alpha / 2000;
                if (alpha > max_alpha) {
                    alpha = max_alpha;
                }
                const Color color = color_drawings & 0x00FFFFFF | alpha << IM_COL32_A_SHIFT;
                EnqueueVertex(line.x1, line.y1, color);
                EnqueueVertex(line.x2, line.y2, color);

                if (vertices_count >= vertices_max - 2) {
                    break;
                }
            }
        }

        if (!lines.empty() && TIMER_DIFF(lines.front().start) > drawing_timeout) {
            lines.pop_front();
        }
    }
}

void PingsLinesRenderer::DrawShadowstepMarker(IDirect3DDevice9* device)
{
    if ((marker.color & IM_COL32_A_MASK) == 0) {
        return;
    }
    const GW::Vec2f& shadowstep_location = Minimap::Instance().ShadowstepLocation();
    if (shadowstep_location.x == 0.0f && shadowstep_location.y == 0.0f) {
        return;
    }
    const auto translate = DirectX::XMMatrixTranslation(shadowstep_location.x, shadowstep_location.y, 0.0f);
    const auto scale = DirectX::XMMatrixScaling(100.0f, 100.0f, 1.0f);
    const auto world = scale * translate;
    device->SetTransform(D3DTS_WORLD, reinterpret_cast<const D3DMATRIX*>(&world));
    marker.Render(device);
}

void PingsLinesRenderer::DrawShadowstepLine(IDirect3DDevice9*)
{
    if ((color_shadowstep_line & IM_COL32_A_MASK) == 0) {
        return;
    }
    const GW::Vec2f& shadowstep_location = Minimap::Instance().ShadowstepLocation();
    if (shadowstep_location.x == 0.0f && shadowstep_location.y == 0.0f) {
        return;
    }

    const GW::Agent* player = GW::Agents::GetControlledCharacter();
    if (player == nullptr) {
        return;
    }

    EnqueueVertex(shadowstep_location.x, shadowstep_location.y, color_shadowstep_line);
    EnqueueVertex(player->pos.x, player->pos.y, color_shadowstep_line);
}

void PingsLinesRenderer::DrawRecallLine(IDirect3DDevice9*)
{
    if (recall_target == 0) {
        return;
    }
    if ((color_shadowstep_line & IM_COL32_A_MASK) == 0) {
        return;
    }

    const GW::Buff* recall = GW::Effects::GetPlayerBuffBySkillId(GW::Constants::SkillID::Recall);
    const GW::Agent* player = recall && recall->skill_id != GW::Constants::SkillID::No_Skill ? GW::Agents::GetControlledCharacter() : nullptr;
    const GW::Agent* target = player ? GW::Agents::GetAgentByID(recall_target) : nullptr;
    if (target == nullptr) {
        // This can happen if you recall something that then despawns before you drop recall.
        recall_target = 0;
        return;
    }
    const float distance = GetDistance(target->pos, player->pos);
    const float distance_perc = distance / GW::Constants::Range::Compass;
    Color c;
    if (distance_perc < maxrange_interp_begin) {
        c = color_shadowstep_line;
    }
    else if (distance_perc < maxrange_interp_end && maxrange_interp_end - maxrange_interp_begin > 0) {
        const float t = (distance_perc - maxrange_interp_begin) / (maxrange_interp_end - maxrange_interp_begin);
        c = Colors::Slerp(color_shadowstep_line, color_shadowstep_line_maxrange, t);
    }
    else {
        c = color_shadowstep_line_maxrange;
    }

    EnqueueVertex(target->pos.x, target->pos.y, c);
    EnqueueVertex(player->pos.x, player->pos.y, c);
}


void PingsLinesRenderer::PingCircle::Initialize(IDirect3DDevice9* device)
{
    type = D3DPT_TRIANGLESTRIP;
    count = 2;

    constexpr size_t vertex_count = 4;

    D3DVertexTextured* _vertices = nullptr;

    if (buffer)
    {
        buffer->Release();
        buffer = nullptr;
    }

    device->CreateVertexBuffer(
        sizeof(D3DVertexTextured) * vertex_count,
        0,
        D3DFVF_TEXTUREDVERTEX,
        D3DPOOL_MANAGED,
        &buffer,
        nullptr);

    buffer->Lock(
        0,
        sizeof(D3DVertexTextured) * vertex_count,
        reinterpret_cast<void**>(&_vertices),
        D3DLOCK_DISCARD);

    _vertices[0] = { -1.0f, -1.0f, 0.0f, color, 0.0f, 1.0f };
    _vertices[1] = { -1.0f,  1.0f, 0.0f, color, 0.0f, 0.0f };
    _vertices[2] = {  1.0f, -1.0f, 0.0f, color, 1.0f, 1.0f };
    _vertices[3] = {  1.0f,  1.0f, 0.0f, color, 1.0f, 0.0f };

    buffer->Unlock();

    device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG2);
    device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);

    device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

    device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    device->SetRenderState(D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
    device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
}

void PingsLinesRenderer::PingCircle::Render(IDirect3DDevice9* device)
{
    if (dirty) Invalidate();
    if (!initialized) {
        initialized = true;
        Initialize(device);
    }
    if (!buffer || !count || !texture) return;

    device->SetTexture(0, texture);

    device->SetFVF(D3DFVF_TEXTUREDVERTEX);
    device->SetStreamSource(0, buffer, 0, sizeof(D3DVertexTextured));
    device->DrawPrimitive(type, 0, count);

    device->SetTexture(0, nullptr);
}

void PingsLinesRenderer::Marker::Initialize(IDirect3DDevice9* device)
{
    type = D3DPT_TRIANGLEFAN;
    count = 16; // polycount
    const unsigned int vertex_count = count + 2;
    D3DVertex* _vertices = nullptr;

    if (buffer) {
        buffer->Release();
    }
    device->CreateVertexBuffer(sizeof(D3DVertex) * vertex_count, 0,
                               D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer, nullptr);
    buffer->Lock(0, sizeof(D3DVertex) * vertex_count, reinterpret_cast<void**>(&_vertices),
                 D3DLOCK_DISCARD);

    _vertices[0].x = 0.0f;
    _vertices[0].y = 0.0f;
    _vertices[0].z = 0.0f;
    _vertices[0].color = Colors::Sub(color, Colors::ARGB(50, 0, 0, 0));
    for (size_t i = 1; i < vertex_count; i++) {
        const float angle = (i - 1) * (2 * DirectX::XM_PI / count);
        _vertices[i].x = std::cos(angle);
        _vertices[i].y = std::sin(angle);
        _vertices[i].z = 0.0f;
        _vertices[i].color = color;
    }

    buffer->Unlock();
}

float PingsLinesRenderer::AgentPing::GetX() const
{
    const GW::Agent* agent = GW::Agents::GetAgentByID(id);
    if (agent == nullptr) {
        return 0.0f;
    }
    return agent->pos.x;
}

float PingsLinesRenderer::AgentPing::GetY() const
{
    const GW::Agent* agent = GW::Agents::GetAgentByID(id);
    if (agent == nullptr) {
        return 0.0f;
    }
    return agent->pos.y;
}

float PingsLinesRenderer::AgentPing::GetScale() const
{
    const GW::Agent* agent = GW::Agents::GetAgentByID(id);
    if (agent == nullptr) {
        return 0.0f;
    }
    return 1.0f;
}

bool PingsLinesRenderer::OnMouseDown(const float x, const float y)
{
    mouse_down = true;
    mouse_moved = false;
    mouse_x = x;
    mouse_y = y;
    queue.clear();
    lastsent = TIMER_INIT();
    return true;
}

void PingsLinesRenderer::AddMouseClickPing(const GW::Vec2f pos)
{
    pings.push_front(new ClickPing(pos.x, pos.y));
}

bool PingsLinesRenderer::OnMouseMove(const float x, const float y)
{
    if (!mouse_down) {
        return false;
    }

    const uint32_t my_player_id = GW::PlayerMgr::GetPlayerNumber();

    drawings[my_player_id].player = my_player_id;
    if (!mouse_moved) {
        mouse_moved = true;
        BumpSessionID();
        drawings[my_player_id].session = static_cast<DWORD>(session_id);
    }

    if (TIMER_DIFF(lastshown) > show_interval
        || TIMER_DIFF(lastqueued) > queue_interval
        || TIMER_DIFF(lastsent) > send_interval) {
        lastshown = TIMER_INIT();

        DrawingLine l;
        l.x1 = mouse_x;
        l.y1 = mouse_y;
        l.x2 = mouse_x = x;
        l.y2 = mouse_y = y;
        drawings[my_player_id].lines.push_back(l);

        if (TIMER_DIFF(lastqueued) > queue_interval
            || TIMER_DIFF(lastsent) > send_interval) {
            lastqueued = TIMER_INIT();

            queue.push_back(GW::UI::CompassPoint(ToIntPos(x), ToIntPos(y)));

            if (queue.size() == 7 || TIMER_DIFF(lastsent) > send_interval) {
                lastsent = TIMER_INIT();
                SendQueue();
            }
        }
    }

    return true;
}

bool PingsLinesRenderer::OnMouseUp()
{
    if (!mouse_down) {
        return false;
    }
    mouse_down = false;

    if (mouse_moved) {
        lastsent = TIMER_INIT();
    }
    else {
        BumpSessionID();
        queue.push_back(GW::UI::CompassPoint(ToIntPos(mouse_x), ToIntPos(mouse_y)));
        pings.push_front(new TerrainPing(mouse_x, mouse_y));
    }

    SendQueue();

    return true;
}

void PingsLinesRenderer::SendQueue()
{
    if (queue.size() > 0 && queue.size() < 8) {
        GW::UI::CompassPoint pts[8];
        for (auto i = 0u; i < queue.size(); i++) {
            pts[i] = queue[i];
        }
        DrawOnCompass(static_cast<size_t>(session_id), queue.size(), pts);
        GW::UI::UIPacket::kCompassDraw packet = {
            .player_number = GW::PlayerMgr::GetPlayerNumber(),
            .session_id = static_cast<size_t>(session_id),
            .number_of_points = queue.size(),
            .points = pts
        };
        is_minimap_compass_draw = true;
        GW::UI::SendUIMessage(GW::UI::UIMessage::kCompassDraw, &packet);
        is_minimap_compass_draw = false;
    }

    queue.clear();
}
