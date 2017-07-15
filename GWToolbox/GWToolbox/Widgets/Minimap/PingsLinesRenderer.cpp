#include "PingsLinesRenderer.h"

#include <d3dx9math.h>
#include <d3d9.h>

#include <GWCA\Packets\CtoS.h>
#include <GWCA\Managers\AgentMgr.h>
#include <GWCA\Managers\StoCMgr.h>
#include <GWCA\Managers\EffectMgr.h>
#include <GWCA\Managers\CtoSMgr.h>

#include "GuiUtils.h"

void PingsLinesRenderer::LoadSettings(CSimpleIni* ini, const char* section) {
	color_drawings = Colors::Load(ini, section, "color_drawings", Colors::ARGB(0xFF, 0xFF, 0xFF, 0xFF));
	if ((color_drawings & IM_COL32_A_MASK) == 0) color_drawings |= Colors::ARGB(255, 0, 0, 0);
	ping_circle.color = Colors::Load(ini, section, "color_pings", Colors::ARGB(128, 255, 0, 0));
	if ((ping_circle.color & IM_COL32_A_MASK) == 0) ping_circle.color |= Colors::ARGB(128, 0, 0, 0);
	marker.color = Colors::Load(ini, section, "color_shadowstep_mark", Colors::ARGB(200, 128, 0, 128));
	color_shadowstep_line = Colors::Load(ini, section, "color_shadowstep_line", Colors::ARGB(155, 128, 0, 128));
	color_shadowstep_line_maxrange = Colors::Load(ini, section, "color_shadowstep_line_maxrange", Colors::ARGB(255, 255, 0, 128));
	maxrange_interp_begin = (float)ini->GetDoubleValue(section, "maxrange_interp_begin", 0.85);
	maxrange_interp_end = (float)ini->GetDoubleValue(section, "maxrange_interp_end", 0.95);
	Invalidate();
}
void PingsLinesRenderer::SaveSettings(CSimpleIni* ini, const char* section) const {
	Colors::Save(ini, section, "color_drawings", color_drawings);
	Colors::Save(ini, section, "color_pings", ping_circle.color);
	Colors::Save(ini, section, "color_shadowstep_mark", marker.color);
	Colors::Save(ini, section, "color_shadowstep_line", color_shadowstep_line);
	Colors::Save(ini, section, "color_shadowstep_line_maxrange", color_shadowstep_line_maxrange);
	ini->SetDoubleValue(section, "maxrange_interp_begin", maxrange_interp_begin);
	ini->SetDoubleValue(section, "maxrange_interp_end", maxrange_interp_end);
}
void PingsLinesRenderer::DrawSettings() {
	if (ImGui::SmallButton("Restore Defaults")) {
		color_drawings = Colors::ARGB(0xFF, 0xFF, 0xFF, 0xFF);
		ping_circle.color = Colors::ARGB(128, 255, 0, 0);
		marker.color = Colors::ARGB(200, 128, 0, 128);
		color_shadowstep_line = Colors::ARGB(48, 128, 0, 128);
		color_shadowstep_line_maxrange = Colors::ARGB(48, 128, 0, 128);
		ping_circle.Invalidate();
		marker.Invalidate();
	}
	Colors::DrawSetting("Drawings", &color_drawings);
	if (Colors::DrawSetting("Pings", &ping_circle.color)) {
		ping_circle.Invalidate();
	}
	if (Colors::DrawSetting("Shadowstep Marker", &marker.color)) {
		marker.Invalidate();
	}
	Colors::DrawSetting("Shadowstep Line", &color_shadowstep_line);
	Colors::DrawSetting("Shadowstep Line (Max range)", &color_shadowstep_line_maxrange);
	if (ImGui::SliderFloat("Max range start", &maxrange_interp_begin, 0.0f, 1.0f)
		&& maxrange_interp_end < maxrange_interp_begin) {
		maxrange_interp_end = maxrange_interp_begin;
	}
	if (ImGui::SliderFloat("Max range end", &maxrange_interp_end, 0.0f, 1.0f)
		&& maxrange_interp_begin > maxrange_interp_end) {
		maxrange_interp_begin = maxrange_interp_end;
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

	shadowstep_location = GW::Vector2f(0, 0);
}

void PingsLinesRenderer::P041Callback(GW::Packet::StoC::P041* pak) {
	pings.push_front(new AgentPing(pak->agent_id));
}

void PingsLinesRenderer::P133Callback(GW::Packet::StoC::P133* pak) {
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

void PingsLinesRenderer::P148Callback(GW::Packet::StoC::P148* pak) {
	if (pak->Value_id == 20
		&& pak->caster == GW::Agents::GetPlayerId()
		&& pak->value == 928) {
		recall_target = pak->target;
	}
};
void PingsLinesRenderer::P216Callback(GW::Packet::StoC::P216* pak) {
	if (pak->agent_id == GW::Agents::GetPlayerId()) {
		if (pak->skill_id == (DWORD)GW::Constants::SkillID::Shadow_of_Haste
			|| pak->skill_id == (DWORD)GW::Constants::SkillID::Shadow_Walk) {
			shadowstep_location = GW::Agents::GetPlayer()->pos;
		}
	}
};

void PingsLinesRenderer::Initialize(IDirect3DDevice9* device) {
	type = D3DPT_LINELIST;

	vertices_max = 0x1000; // support for up to 4096 line segments, should be enough

	vertices = nullptr;

	HRESULT hr = device->CreateVertexBuffer(sizeof(D3DVertex) * vertices_max, 0,
		D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer, NULL);
	if (FAILED(hr)) {
		printf("Error setting up PingsLinesRenderer vertex buffer: %d\n", hr);
	}
}

void PingsLinesRenderer::Render(IDirect3DDevice9* device) {
	if (!initialized) {
		initialized = true;
		Initialize(device);
	}

	DrawPings(device);

	DrawShadowstepMarker(device);

	vertices_count = 0;
	HRESULT res = buffer->Lock(0, sizeof(D3DVertex) * vertices_max, (VOID**)&vertices, D3DLOCK_DISCARD);
	if (FAILED(res)) printf("PingsLinesRenderer Lock() error: %d\n", res);

	DrawShadowstepLine(device);

	DrawRecallLine(device);

	DrawDrawings(device);

	D3DXMATRIX i;
	D3DXMatrixIdentity(&i);
	device->SetTransform(D3DTS_WORLD, &i);

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

		D3DXMATRIX translate, scale, world;
		D3DXMatrixTranslation(&translate, ping->GetX(), ping->GetY(), 0.0f);
		D3DXMatrixScaling(&scale, drawing_scale, drawing_scale, 1.0f);
		world = scale * translate;
		device->SetTransform(D3DTS_WORLD, &world);
		ping_circle.Render(device);

		int diff = TIMER_DIFF(ping->start);
		bool first_loop = diff < 1000;
		diff = diff % 1000;
		diff *= first_loop ? 2 : 1;

		D3DXMatrixScaling(&scale, diff * ping->GetScale(), diff * ping->GetScale(), 1.0f);
		world = scale * translate;
		device->SetTransform(D3DTS_WORLD, &world);
		ping_circle.Render(device);
	}
	if (!pings.empty() && TIMER_DIFF(pings.back()->start) > 3000) {
		delete pings.back();
		pings.pop_back();
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
	for (auto it = drawings.begin(); it != drawings.end(); ++it) {
		if (it->second.player == 0) continue;
		std::deque<DrawingLine>& lines = it->second.lines;

		if (vertices_count < vertices_max - 2) {
			for (const DrawingLine& line : lines) {
				int max_alpha = (color_drawings & IM_COL32_A_MASK) >> IM_COL32_A_SHIFT;
				int left = 5000 - TIMER_DIFF(line.start);
				int alpha = left * max_alpha / 2000;
				if (alpha < 0) alpha = 0;
				if (alpha > max_alpha) alpha = max_alpha;
				Color color = (color_drawings & 0x00FFFFFF) | (alpha << IM_COL32_A_SHIFT);
				EnqueueVertex(line.x1, line.y1, color);
				EnqueueVertex(line.x2, line.y2, color);

				if (vertices_count >= vertices_max - 2) break;
			}
		}

		if (!lines.empty() && TIMER_DIFF(lines.front().start) > 5000) {
			lines.pop_front();
		}
	}
}

void PingsLinesRenderer::DrawShadowstepMarker(IDirect3DDevice9* device) {
	if (shadowstep_location.x == 0.0f && shadowstep_location.y == 0.0f) return;
	if ((marker.color & IM_COL32_A_MASK) == 0) return;

	GW::EffectArray effects = GW::Effects::GetPlayerEffectArray();
	if (!effects.valid()) {
		shadowstep_location = GW::Vector2f();
		return;
	}

	bool found = false;
	for (unsigned int i = 0; i < effects.size(); ++i) {
		if (effects[i].SkillId == (DWORD)GW::Constants::SkillID::Shadow_of_Haste
			|| effects[i].SkillId == (DWORD)GW::Constants::SkillID::Shadow_Walk) {
			found = true;
		}
	}
	if (!found) {
		shadowstep_location = GW::Vector2f();
		return;
	}

	D3DXMATRIX translate, scale, world;
	D3DXMatrixTranslation(&translate, shadowstep_location.x, shadowstep_location.y, 0.0f);
	D3DXMatrixScaling(&scale, 100.0f, 100.0f, 1.0f);
	world = scale * translate;
	device->SetTransform(D3DTS_WORLD, &world);
	marker.Render(device);
}

void PingsLinesRenderer::DrawShadowstepLine(IDirect3DDevice9* device) {
	// this is after the previous func, so don't check for effect
	if (shadowstep_location.x == 0.0f && shadowstep_location.y == 0.0f) return;
	if ((color_shadowstep_line & IM_COL32_A_MASK) == 0) return;

	GW::Agent* player = GW::Agents::GetPlayer();
	if (player == nullptr) return;

	EnqueueVertex(shadowstep_location.x, shadowstep_location.y, color_shadowstep_line);
	EnqueueVertex(player->pos.x, player->pos.y, color_shadowstep_line);
}

void PingsLinesRenderer::DrawRecallLine(IDirect3DDevice9* device) {
	if (recall_target == 0) return;
	if ((color_shadowstep_line & IM_COL32_A_MASK) == 0) return;

	GW::Buff recall = GW::Effects::GetPlayerBuffBySkillId(GW::Constants::SkillID::Recall);
	if (recall.SkillId == 0) {
		recall_target = 0;
		return;
	}
	GW::Agent* player = GW::Agents::GetPlayer();
	if (player == nullptr) return;

	GW::AgentArray agents = GW::Agents::GetAgentArray();
	if (!agents.valid()) return;
	if (recall_target > agents.size()) {
		recall_target = 0;
		return;
	}
	GW::Agent* target = agents[recall_target];
	static GW::GamePos targetpos(0.0f, 0.0f);
	if (target) { targetpos = target->pos; }

	float distance = GW::Agents::GetDistance(targetpos, player->pos);
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

	EnqueueVertex(targetpos.x, targetpos.y, c);
	EnqueueVertex(player->pos.x, player->pos.y, c);
}


void PingsLinesRenderer::PingCircle::Initialize(IDirect3DDevice9* device) {
	type = D3DPT_TRIANGLESTRIP;
	count = 48; // polycount
	unsigned int vertex_count = count + 2;
	D3DVertex* vertices = nullptr;

	if (buffer) buffer->Release();
	device->CreateVertexBuffer(sizeof(D3DVertex) * vertex_count, 0,
		D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer, NULL);
	buffer->Lock(0, sizeof(D3DVertex) * vertex_count,
		(VOID**)&vertices, D3DLOCK_DISCARD);
	
	const float PI = 3.1415927f;
	for (size_t i = 0; i < count; ++i) {
		float angle = i * (2 * PI / count);
		bool outer = (i % 2 == 0);
		float radius = outer ? 1.0f : 0.8f;
		vertices[i].x = radius * std::cos(angle);
		vertices[i].y = radius * std::sin(angle);
		vertices[i].z = 0.0f;
		vertices[i].color = (outer ? color : Colors::Sub(color, 0xFF000000));
	}
	vertices[count] = vertices[0];
	vertices[count + 1] = vertices[1];

	buffer->Unlock();
}
void PingsLinesRenderer::Marker::Initialize(IDirect3DDevice9* device) {
	type = D3DPT_TRIANGLEFAN;
	count = 16; // polycount
	unsigned int vertex_count = count + 2;
	D3DVertex* vertices = nullptr;

	if (buffer) buffer->Release();
	device->CreateVertexBuffer(sizeof(D3DVertex) * vertex_count, 0,
		D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer, NULL);
	buffer->Lock(0, sizeof(D3DVertex) * vertex_count,
		(VOID**)&vertices, D3DLOCK_DISCARD);

	const float PI = 3.1415927f;
	vertices[0].x = 0.0f;
	vertices[0].y = 0.0f;
	vertices[0].z = 0.0f;
	vertices[0].color = Colors::Sub(color, Colors::ARGB(50, 0, 0, 0));
	for (size_t i = 1; i < vertex_count; ++i) {
		float angle = (i-1) * (2 * PI / count);
		vertices[i].x = std::cos(angle);
		vertices[i].y = std::sin(angle);
		vertices[i].z = 0.0f;
		vertices[i].color = color;
	}

	buffer->Unlock();
}

float PingsLinesRenderer::AgentPing::GetX() {
	GW::Agent* agent = GW::Agents::GetAgentByID(id);
	if (agent == nullptr) return 0.0f;
	return agent->pos.x;
}

float PingsLinesRenderer::AgentPing::GetY() {
	GW::Agent* agent = GW::Agents::GetAgentByID(id);
	if (agent == nullptr) return 0.0f;
	return agent->pos.y;
}

float PingsLinesRenderer::AgentPing::GetScale() {
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

bool PingsLinesRenderer::OnMouseMove(float x, float y) {
	if (!mouse_down) return false;

	GW::Agent* me = GW::Agents::GetPlayer();
	if (me == nullptr) return false;


	drawings[me->PlayerNumber].player = me->PlayerNumber;
	if (!mouse_moved) { // first time
		mouse_moved = true;
		BumpSessionID();
		drawings[me->PlayerNumber].session = session_id;
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
		drawings[me->PlayerNumber].lines.push_back(l);

		if (TIMER_DIFF(lastqueued) > queue_interval
			|| TIMER_DIFF(lastsent) > send_interval) {
			lastqueued = TIMER_INIT();

			queue.push_back(ShortPos(ToShortPos(x), ToShortPos(y)));

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
		GW::Agent* me = GW::Agents::GetPlayer();
		queue.push_back(ShortPos(ToShortPos(mouse_x), ToShortPos(mouse_y)));
		pings.push_front(new TerrainPing(mouse_x, mouse_y));
	}

	SendQueue();

	return true;
}

void PingsLinesRenderer::SendQueue() {
	static GW::Packet::CtoS::P037 packet = GW::Packet::CtoS::P037();

	//printf("sending %d pos [%d]\n", queue.size(), session_id);

	if (queue.size() > 0 && queue.size() < 8) {

		packet.NumberPts = queue.size();
		packet.session_id = session_id;
		for (unsigned int i = 0; i < queue.size(); ++i) {
			packet.points[i].x = queue[i].x;
			packet.points[i].y = queue[i].y;
		}

		GW::CtoS::SendPacket(&packet);
	}

	queue.clear();
}
