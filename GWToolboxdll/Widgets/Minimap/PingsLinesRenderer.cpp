#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/Packets/StoC.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/EffectMgr.h>

#include <Utils/GuiUtils.h>
#include <Widgets/Minimap/Minimap.h>

void PingsLinesRenderer::LoadSettings(ToolboxIni* ini, const char* section) {
    color_drawings = Colors::Load(ini, section, "color_drawings", Colors::ARGB(0xFF, 0xFF, 0xFF, 0xFF));
    if ((color_drawings & IM_COL32_A_MASK) == 0) color_drawings |= Colors::ARGB(255, 0, 0, 0);
    ping_circle.color = Colors::Load(ini, section, "color_pings", Colors::ARGB(128, 255, 0, 0));
    if ((ping_circle.color & IM_COL32_A_MASK) == 0) ping_circle.color |= Colors::ARGB(128, 0, 0, 0);
    marker.color = Colors::Load(ini, section, "color_shadowstep_mark", Colors::ARGB(200, 128, 0, 128));
    color_shadowstep_line = Colors::Load(ini, section, VAR_NAME(color_shadowstep_line), color_shadowstep_line);
    color_shadowstep_line_maxrange = Colors::Load(ini, section, VAR_NAME(color_shadowstep_line_maxrange), color_shadowstep_line_maxrange);
    maxrange_interp_begin = static_cast<float>(ini->GetDoubleValue(section, VAR_NAME(maxrange_interp_begin), maxrange_interp_begin));
    maxrange_interp_end = static_cast<float>(ini->GetDoubleValue(section, VAR_NAME(maxrange_interp_end), maxrange_interp_end));
    reduce_ping_spam = ini->GetBoolValue(section, VAR_NAME(reduce_ping_spam), reduce_ping_spam);
    Invalidate();
}
void PingsLinesRenderer::SaveSettings(ToolboxIni* ini, const char* section) const {
    Colors::Save(ini, section, "color_drawings", color_drawings);
    Colors::Save(ini, section, "color_pings", ping_circle.color);
    Colors::Save(ini, section, "color_shadowstep_mark", marker.color);
    Colors::Save(ini, section, "color_shadowstep_line", color_shadowstep_line);
    Colors::Save(ini, section, "color_shadowstep_line_maxrange", color_shadowstep_line_maxrange);
    ini->SetDoubleValue(section, "maxrange_interp_begin", maxrange_interp_begin);
    ini->SetDoubleValue(section, "maxrange_interp_end", maxrange_interp_end);
    ini->SetBoolValue(section, VAR_NAME(reduce_ping_spam), reduce_ping_spam);
}
void PingsLinesRenderer::DrawSettings() {
    bool changed = false;
    bool confirm = false;
    if (ImGui::SmallConfirmButton("Restore Defaults", &confirm)) {
        color_drawings = Colors::ARGB(0xFF, 0xFF, 0xFF, 0xFF);
        ping_circle.color = Colors::ARGB(128, 255, 0, 0);
        marker.color = Colors::ARGB(200, 128, 0, 128);
        color_shadowstep_line = Colors::ARGB(48, 128, 0, 128);
        color_shadowstep_line_maxrange = Colors::ARGB(48, 128, 0, 128);
        changed = true;
    }
    changed |= Colors::DrawSettingHueWheel("Drawings", &color_drawings);
    changed |= Colors::DrawSettingHueWheel("Pings", &ping_circle.color);
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

PingsLinesRenderer::PingsLinesRenderer() : vertices(nullptr) {
    mouse_down = false;
    mouse_moved = false;
    mouse_x = 0;
    mouse_y = 0;
    session_id = 0;
    lastshown = TIMER_INIT();
    lastsent = TIMER_INIT();
    lastqueued = TIMER_INIT();
}

void PingsLinesRenderer::P046Callback(GW::Packet::StoC::AgentPinged *pak) {
    bool found = false;
    if (reduce_ping_spam) {
        for (Ping* ping : pings) {
            if (ping->GetAgentID() == pak->agent_id) {
                // extend the duration to count for the current ping.
                clock_t diff = TIMER_DIFF(ping->start);
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

void PingsLinesRenderer::P138Callback(GW::Packet::StoC::CompassEvent *pak) {
    bool new_session;
    if (drawings[pak->Player].player == pak->Player) {
        new_session = drawings[pak->Player].session != pak->SessionID;
        drawings[pak->Player].session = pak->SessionID;
    } else {
        drawings[pak->Player].player = pak->Player;
        drawings[pak->Player].session = pak->SessionID;
        new_session = true;
    }

    if (new_session && pak->NumberPts == 1) {
        pings.push_front(new TerrainPing(
            pak->points[0].x * drawing_scale,
pak->points[0].y * drawing_scale));
return;
    }

    if (new_session) {
        for (unsigned int i = 0; i < pak->NumberPts - 1; ++i) {
            DrawingLine l;
            l.x1 = pak->points[i + 0].x * drawing_scale;
            l.y1 = pak->points[i + 0].y * drawing_scale;
            l.x2 = pak->points[i + 1].x * drawing_scale;
            l.y2 = pak->points[i + 1].y * drawing_scale;
            drawings[pak->Player].lines.push_back(l);
        }
    } else {
        if (drawings[pak->Player].lines.empty()) return;
        for (unsigned int i = 0; i < pak->NumberPts; ++i) {
            DrawingLine l;
            if (i == 0) {
                l.x1 = drawings[pak->Player].lines.back().x2;
                l.y1 = drawings[pak->Player].lines.back().y2;
            } else {
                l.x1 = pak->points[i - 1].x * drawing_scale;
                l.y1 = pak->points[i - 1].y * drawing_scale;
            }
            l.x2 = pak->points[i].x * drawing_scale;
            l.y2 = pak->points[i].y * drawing_scale;
            drawings[pak->Player].lines.push_back(l);
        }
    }
}

void PingsLinesRenderer::P153Callback(GW::Packet::StoC::GenericValueTarget *pak) {
    if (pak->Value_id == 20
        && pak->caster == GW::Agents::GetPlayerId()
        && pak->value == 928) {
        recall_target = pak->target;
    }
};

void PingsLinesRenderer::Initialize(IDirect3DDevice9* device) {
    if (initialized)
        return;
    initialized = true;
    type = D3DPT_LINELIST;

    vertices_max = 0x1000; // support for up to 4096 line segments, should be enough

    vertices = nullptr;

    HRESULT hr = device->CreateVertexBuffer(sizeof(D3DVertex) * vertices_max, 0,
        D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer, nullptr);
    if (FAILED(hr)) {
        printf("Error setting up PingsLinesRenderer vertex buffer: HRESULT: 0x%lX\n", hr);
    }
}

void PingsLinesRenderer::Render(IDirect3DDevice9* device) {
    Initialize(device);

    DrawPings(device);

    DrawShadowstepMarker(device);

    vertices_count = 0;
    HRESULT res = buffer->Lock(0, sizeof(D3DVertex) * vertices_max, (VOID**)&vertices, D3DLOCK_DISCARD);
    if (FAILED(res)) printf("PingsLinesRenderer Lock() error: HRESULT 0x%lX\n", res);

    DrawShadowstepLine(device);

    DrawRecallLine(device);

    DrawDrawings(device);

    const auto i = DirectX::XMMatrixIdentity();
    device->SetTransform(D3DTS_WORLD, reinterpret_cast<const D3DMATRIX*>(&i));

    buffer->Unlock();
    if (vertices_count != 0) {
        device->SetStreamSource(0, buffer, 0, sizeof(D3DVertex));
        device->DrawPrimitive(type, 0, vertices_count / 2);
        vertices_count = 0;
    }
}

void PingsLinesRenderer::DrawPings(IDirect3DDevice9* device) {
    for (Ping* ping : pings) {
        if (ping->GetScale() == 0) continue;
        if (TIMER_DIFF(ping->start) > ping->duration) continue;

        DirectX::XMMATRIX scale, world;
        const auto translate = DirectX::XMMatrixTranslation(ping->GetX(), ping->GetY(), 0.0f);

        if (ping->ShowInner()) {
            scale = DirectX::XMMatrixScaling(drawing_scale, drawing_scale, 1.0f);
            world = scale * translate;
            device->SetTransform(D3DTS_WORLD, reinterpret_cast<const D3DMATRIX*>(&world));
            ping_circle.Render(device);
        }

        int diff = TIMER_DIFF(ping->start);
        bool first_loop = diff < 1000;
        diff = diff % 1000;
        diff *= first_loop ? 2 : 1;

        scale = DirectX::XMMatrixScaling(diff * ping->GetScale(), diff * ping->GetScale(), 1.0f);
        world = scale * translate;
        device->SetTransform(D3DTS_WORLD, reinterpret_cast<const D3DMATRIX*>(&world));
        ping_circle.Render(device);
    }
    if (!pings.empty()) {
        const Ping* last = pings.back();
        if (TIMER_DIFF(last->start) > last->duration) {
            delete last;
            pings.pop_back();
        }
    }
}

void PingsLinesRenderer::EnqueueVertex(float x, float y, Color color) {
    if (vertices_count == vertices_max) return;
    vertices[0].x = x;
    vertices[0].y = y;
    vertices[0].z = 0.0f;
    vertices[0].color = color;
    ++vertices;
    ++vertices_count;
}
void PingsLinesRenderer::DrawDrawings(IDirect3DDevice9* device) {
    UNREFERENCED_PARAMETER(device);
    for (auto it = drawings.begin(); it != drawings.end(); ++it) {
        if (it->second.player == 0) continue;
        std::deque<DrawingLine>& lines = it->second.lines;

        if (vertices_count < vertices_max - 2) {
            for (const DrawingLine& line : lines) {
                uint32_t max_alpha = (color_drawings & IM_COL32_A_MASK) >> IM_COL32_A_SHIFT;
                uint32_t left = static_cast<uint32_t>(drawing_timeout - TIMER_DIFF(line.start));
                // @Robustness:
                // This is not safe, casting time to uint32_t is unsafe.
                if (left > static_cast<uint32_t>(drawing_timeout))
                    continue; // This is actually a negative integer i.e. no time left.
                uint32_t alpha = left * max_alpha / 2000;
                if (alpha > max_alpha) alpha = max_alpha;
                Color color = (color_drawings & 0x00FFFFFF) | (alpha << IM_COL32_A_SHIFT);
                EnqueueVertex(line.x1, line.y1, color);
                EnqueueVertex(line.x2, line.y2, color);

                if (vertices_count >= vertices_max - 2) break;
            }
        }

        if (!lines.empty() && TIMER_DIFF(lines.front().start) > drawing_timeout) {
            lines.pop_front();
        }
    }
}

void PingsLinesRenderer::DrawShadowstepMarker(IDirect3DDevice9* device) {
    if ((marker.color & IM_COL32_A_MASK) == 0)
        return;
    const GW::Vec2f &shadowstep_location = Minimap::Instance().ShadowstepLocation();
    if (shadowstep_location.x == 0.0f && shadowstep_location.y == 0.0f)
        return;
    const auto translate = DirectX::XMMatrixTranslation(shadowstep_location.x, shadowstep_location.y, 0.0f);
    const auto scale = DirectX::XMMatrixScaling(100.0f, 100.0f, 1.0f);
    const auto world = scale * translate;
    device->SetTransform(D3DTS_WORLD, reinterpret_cast<const D3DMATRIX*>(&world));
    marker.Render(device);
}

void PingsLinesRenderer::DrawShadowstepLine(IDirect3DDevice9* device) {
    UNREFERENCED_PARAMETER(device);
    if ((color_shadowstep_line & IM_COL32_A_MASK) == 0)
        return;
    const GW::Vec2f &shadowstep_location = Minimap::Instance().ShadowstepLocation();
    if (shadowstep_location.x == 0.0f && shadowstep_location.y == 0.0f)
        return;

    GW::Agent* player = GW::Agents::GetPlayer();
    if (player == nullptr) return;

    EnqueueVertex(shadowstep_location.x, shadowstep_location.y, color_shadowstep_line);
    EnqueueVertex(player->pos.x, player->pos.y, color_shadowstep_line);
}

void PingsLinesRenderer::DrawRecallLine(IDirect3DDevice9* device) {
    UNREFERENCED_PARAMETER(device);
    if (recall_target == 0) return;
    if ((color_shadowstep_line & IM_COL32_A_MASK) == 0) return;

    GW::Buff *recall = GW::Effects::GetPlayerBuffBySkillId(GW::Constants::SkillID::Recall);
    GW::Agent* player = recall && recall->skill_id != GW::Constants::SkillID::No_Skill ? GW::Agents::GetPlayer() : nullptr;
    GW::Agent* target = player ? GW::Agents::GetAgentByID(recall_target) : nullptr;
    if (target == nullptr) {
        // This can happen if you recall something that then despawns before you drop recall.
        recall_target = 0;
        return;
    }
    float distance = GW::GetDistance(target->pos, player->pos);
    float distance_perc = distance / GW::Constants::Range::Compass;
    Color c;
    if (distance_perc < maxrange_interp_begin) {
        c = color_shadowstep_line;
    } else if (distance_perc < maxrange_interp_end && (maxrange_interp_end - maxrange_interp_begin > 0)) {
        float t = (distance_perc - maxrange_interp_begin) / (maxrange_interp_end - maxrange_interp_begin);
        c = Colors::Slerp(color_shadowstep_line, color_shadowstep_line_maxrange, t);
    } else {
        c = color_shadowstep_line_maxrange;
    }

    EnqueueVertex(target->pos.x, target->pos.y, c);
    EnqueueVertex(player->pos.x, player->pos.y, c);
}


void PingsLinesRenderer::PingCircle::Initialize(IDirect3DDevice9* device) {
    type = D3DPT_TRIANGLESTRIP;
    count = 96; // polycount
    const auto vertex_count = count + 2;
    D3DVertex* _vertices = nullptr;

    if (buffer) buffer->Release();
    device->CreateVertexBuffer(sizeof(D3DVertex) * vertex_count, 0,
        D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer, nullptr);
    buffer->Lock(0, sizeof(D3DVertex) * vertex_count, reinterpret_cast<void**>(&_vertices),
                 D3DLOCK_DISCARD);

    const float PI = 3.1415927f;
    for (size_t i = 0; i < count; ++i) {
        float angle = i * (2 * PI / count);
        bool outer = (i % 2 == 0);
        float radius = outer ? 1.0f : 0.8f;
        _vertices[i].x = radius * std::cos(angle);
        _vertices[i].y = radius * std::sin(angle);
        _vertices[i].z = 0.0f;
        _vertices[i].color = (outer ? color : Colors::Sub(color, 0xFF000000));
    }
    _vertices[count] = _vertices[0];
    _vertices[count + 1] = _vertices[1];

    buffer->Unlock();
}
void PingsLinesRenderer::Marker::Initialize(IDirect3DDevice9* device) {
    type = D3DPT_TRIANGLEFAN;
    count = 16; // polycount
    unsigned int vertex_count = count + 2;
    D3DVertex *_vertices = nullptr;

    if (buffer) buffer->Release();
    device->CreateVertexBuffer(sizeof(D3DVertex) * vertex_count, 0,
        D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer, nullptr);
    buffer->Lock(0, sizeof(D3DVertex) * vertex_count, reinterpret_cast<void**>(&_vertices),
                 D3DLOCK_DISCARD);

    const float PI = 3.1415927f;
    _vertices[0].x = 0.0f;
    _vertices[0].y = 0.0f;
    _vertices[0].z = 0.0f;
    _vertices[0].color = Colors::Sub(color, Colors::ARGB(50, 0, 0, 0));
    for (size_t i = 1; i < vertex_count; ++i) {
        float angle = (i-1) * (2 * PI / count);
        _vertices[i].x = std::cos(angle);
        _vertices[i].y = std::sin(angle);
        _vertices[i].z = 0.0f;
        _vertices[i].color = color;
    }

    buffer->Unlock();
}

float PingsLinesRenderer::AgentPing::GetX() const {
    GW::Agent* agent = GW::Agents::GetAgentByID(id);
    if (agent == nullptr) return 0.0f;
    return agent->pos.x;
}

float PingsLinesRenderer::AgentPing::GetY() const {
    GW::Agent* agent = GW::Agents::GetAgentByID(id);
    if (agent == nullptr) return 0.0f;
    return agent->pos.y;
}

float PingsLinesRenderer::AgentPing::GetScale() const {
    GW::Agent* agent = GW::Agents::GetAgentByID(id);
    if (agent == nullptr) return 0.0f;
    return 1.0f;
}

bool PingsLinesRenderer::OnMouseDown(float x, float y) {
    mouse_down = true;
    mouse_moved = false;
    mouse_x = x;
    mouse_y = y;
    queue.clear();
    lastsent = TIMER_INIT();
    return true;
}

void PingsLinesRenderer::AddMouseClickPing(GW::Vec2f pos) {
    pings.push_front(new ClickPing(pos.x, pos.y));
}

bool PingsLinesRenderer::OnMouseMove(float x, float y) {
    if (!mouse_down) return false;

    GW::AgentLiving* me = GW::Agents::GetPlayerAsAgentLiving();
    if (me == nullptr) return false;


    drawings[me->player_number].player = me->player_number;
    if (!mouse_moved) { // first time
        mouse_moved = true;
        BumpSessionID();
        drawings[me->player_number].session = static_cast<DWORD>(session_id);
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
        drawings[me->player_number].lines.push_back(l);

        if (TIMER_DIFF(lastqueued) > queue_interval
            || TIMER_DIFF(lastsent) > send_interval) {
            lastqueued = TIMER_INIT();

            queue.push_back(GW::UI::CompassPoint(ToShortPos(x), ToShortPos(y)));

            if (queue.size() == 7 || TIMER_DIFF(lastsent) > send_interval) {
                lastsent = TIMER_INIT();
                SendQueue();
            }
        }
    }

    return true;
}

bool PingsLinesRenderer::OnMouseUp() {
    if (!mouse_down) return false;
    mouse_down = false;

    if (mouse_moved) {
        lastsent = TIMER_INIT();
    } else {
        BumpSessionID();
        queue.push_back(GW::UI::CompassPoint(ToShortPos(mouse_x), ToShortPos(mouse_y)));
        pings.push_front(new TerrainPing(mouse_x, mouse_y));
    }

    SendQueue();

    return true;
}

void PingsLinesRenderer::SendQueue() {
    if (queue.size() > 0 && queue.size() < 8) {
        GW::UI::CompassPoint pts[8];
        for (unsigned int i = 0; i < queue.size(); ++i) {
            pts[i] = queue[i];
        }
        GW::UI::DrawOnCompass(static_cast<size_t>(session_id), queue.size(), pts);
    }

    queue.clear();
}
