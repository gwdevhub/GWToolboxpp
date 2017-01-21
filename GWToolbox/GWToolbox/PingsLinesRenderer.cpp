#include "PingsLinesRenderer.h"

#include <d3dx9math.h>
#include <d3d9.h>

#include <GWCA\GWCA.h>
#include <GWCA\GameEntities\Agent.h>
#include <GWCA\Managers\StoCMgr.h>
#include <GWCA\Packets\CtoS.h>

#include "MinimapUtils.h"

PingsLinesRenderer::PingsLinesRenderer() : 
	vertices(nullptr),
	visible_(false) {

	color_drawings = MinimapUtils::IniReadColor(L"color_drawings", L"0x00FFFFFF");

	GW::StoC().AddGameServerEvent<GW::Packet::StoC::P133>(
		[&](GW::Packet::StoC::P133* pak) -> bool {

		if (!visible_) return false;

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
				pak->points[0].x * 100.0f, 
				pak->points[0].y * 100.0f));
			return false;
		} 

		if (new_session) {
			for (unsigned int i = 0; i < pak->NumberPts - 1; ++i) {
				DrawingLine l;
				l.x1 = pak->points[i + 0].x * 100.0f;
				l.y1 = pak->points[i + 0].y * 100.0f;
				l.x2 = pak->points[i + 1].x * 100.0f;
				l.y2 = pak->points[i + 1].y * 100.0f;
				drawings[pak->Player].lines.push_back(l);
			}
		} else {
			if (drawings[pak->Player].lines.empty()) return false;
			for (unsigned int i = 0; i < pak->NumberPts; ++i) {
				DrawingLine l;
				if (i == 0) {
					l.x1 = drawings[pak->Player].lines.back().x2;
					l.y1 = drawings[pak->Player].lines.back().y2;
				} else {
					l.x1 = pak->points[i - 1].x * 100.0f;
					l.y1 = pak->points[i - 1].y * 100.0f;
				}
				l.x2 = pak->points[i].x * 100.0f;
				l.y2 = pak->points[i].y * 100.0f;
				drawings[pak->Player].lines.push_back(l);
			}
		}

		return false;
	});

	GW::StoC().AddGameServerEvent<GW::Packet::StoC::P041>(
		[&](GW::Packet::StoC::P041* pak) -> bool {
		if (!visible_) return false;
		pings.push_front(new AgentPing(pak->agent_id));
		return false;
	});

	mouse_down = false;
	mouse_moved = false;
	mouse_x = 0;
	mouse_y = 0;
	session_id = 0;
	lastshown = TBTimer::init();
	lastsent = TBTimer::init();
	lastqueued = TBTimer::init();
}

void PingsLinesRenderer::Initialize(IDirect3DDevice9* device) {
	type_ = D3DPT_LINELIST;

	vertices_max = 0x1000; // support for up to 2048 line segments, should be enough

	vertices = nullptr;

	HRESULT hr = device->CreateVertexBuffer(sizeof(D3DVertex) * vertices_max, 0,
		D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer_, NULL);
	if (FAILED(hr)) {
		printf("Error setting up PingsLinesRenderer vertex buffer: %d\n", hr);
	}
}

void PingsLinesRenderer::Render(IDirect3DDevice9* device) {
	if (!initialized_) {
		initialized_ = true;
		Initialize(device);
	}

	for (Ping* ping : pings) {
		if (ping->GetScale() == 0) continue;

		D3DXMATRIX translate, scale, world;
		D3DXMatrixTranslation(&translate, ping->GetX(), ping->GetY(), 0.0f);
		D3DXMatrixScaling(&scale, 100.0f, 100.0f, 1.0f);
		world = scale * translate;
		device->SetTransform(D3DTS_WORLD, &world);
		ping_circle.Render(device);

		int diff = TBTimer::diff(ping->start);
		bool first_loop = diff < 1000;
		diff = diff % 1000;
		diff *= first_loop ? 2 : 1;

		D3DXMatrixScaling(&scale, diff * ping->GetScale(), diff * ping->GetScale(), 1.0f);
		world = scale * translate;
		device->SetTransform(D3DTS_WORLD, &world);
		ping_circle.Render(device);
	}
	if (!pings.empty() && TBTimer::diff(pings.back()->start) > 3000) {
		delete pings.back();
		pings.pop_back();
	}

	vertices_count = 0;
	HRESULT res = buffer_->Lock(0, sizeof(D3DVertex) * vertices_max, (VOID**)&vertices, D3DLOCK_DISCARD);
	if (FAILED(res)) printf("AgentRenderer Lock() error: %d\n", res);

	auto Enqueue = [&](float x, float y, short c) {
		vertices[0].x = x;
		vertices[0].y = y;
		vertices[0].z = 0.0f;
		vertices[0].color = (D3DCOLOR_ARGB(c, 0, 0, 0) | color_drawings);
		++vertices;
		++vertices_count;
	};

	for (auto it = drawings.begin(); it != drawings.end(); ++it) {
		if (it->second.player == 0) continue;
		std::deque<DrawingLine>& lines = it->second.lines;

		if (vertices_count < vertices_max - 2) {
			for (const DrawingLine& line : lines) {
				int left = 5000 - TBTimer::diff(line.start);
				int alpha = left * 255 / 2000;
				if (alpha < 0) alpha = 0;
				if (alpha > 255) alpha = 255;
				Enqueue(line.x1, line.y1, alpha);
				Enqueue(line.x2, line.y2, alpha);

				if (vertices_count >= vertices_max - 2) break;
			}
		}

		if (!lines.empty() && TBTimer::diff(lines.front().start) > 5000) {
			lines.pop_front();
		}
	}

	D3DXMATRIX i;
	D3DXMatrixIdentity(&i);
	//D3DXMatrixScaling(&scale, 100.0f, 100.0f, 1.0f);
	device->SetTransform(D3DTS_WORLD, &i);

	buffer_->Unlock();
	if (vertices_count != 0) {
		device->SetStreamSource(0, buffer_, 0, sizeof(D3DVertex));
		device->DrawPrimitive(type_, 0, vertices_count / 2);
		vertices_count = 0;
	}
}

void PingsLinesRenderer::PingCircle::Initialize(IDirect3DDevice9* device) {
	type_ = D3DPT_TRIANGLESTRIP;
	count_ = 48; // polycount
	vertex_count = count_ + 2;
	vertices = nullptr;

	DWORD color = MinimapUtils::IniReadColor(L"color_pings", L"0x00FF0000");


	if (buffer_) buffer_->Release();
	device->CreateVertexBuffer(sizeof(D3DVertex) * vertex_count, 0,
		D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer_, NULL);
	buffer_->Lock(0, sizeof(D3DVertex) * vertex_count,
		(VOID**)&vertices, D3DLOCK_DISCARD);
	
	for (size_t i = 0; i < count_; ++i) {
		float angle = i * (2 * fPI / count_);
		bool outer = (i % 2 == 0);
		float radius = outer ? 1.0f : 0.8f;
		vertices[i].x = radius * std::cos(angle);
		vertices[i].y = radius * std::sin(angle);
		vertices[i].z = 0.0f;
		vertices[i].color = (D3DCOLOR_ARGB(outer ? 150 : 0, 0, 0, 0) | color);
	}
	vertices[count_] = vertices[0];
	vertices[count_ + 1] = vertices[1];

	buffer_->Unlock();
}

float PingsLinesRenderer::AgentPing::GetX() {
	GW::Agent* agent = GW::Agents().GetAgentByID(id);
	if (agent == nullptr) return 0.0f;
	return agent->X;
}

float PingsLinesRenderer::AgentPing::GetY() {
	GW::Agent* agent = GW::Agents().GetAgentByID(id);
	if (agent == nullptr) return 0.0f;
	return agent->Y;
}

float PingsLinesRenderer::AgentPing::GetScale() {
	GW::Agent* agent = GW::Agents().GetAgentByID(id);
	if (agent == nullptr) return 0.0f;
	return 1.0f;
}

bool PingsLinesRenderer::OnMouseDown(float x, float y) {
	mouse_down = true;
	mouse_moved = false;
	mouse_x = x;
	mouse_y = y;
	queue.clear();
	lastsent = TBTimer::init();
	return true;
}

bool PingsLinesRenderer::OnMouseMove(float x, float y) {
	if (!mouse_down) return false;

	GW::Agent* me = GW::Agents().GetPlayer();
	if (me == nullptr) return false;


	drawings[me->PlayerNumber].player = me->PlayerNumber;
	if (!mouse_moved) { // first time
		mouse_moved = true;
		BumpSessionID();
		drawings[me->PlayerNumber].session = session_id;
	}

	if (TBTimer::diff(lastshown) > show_interval
		|| TBTimer::diff(lastqueued) > queue_interval
		|| TBTimer::diff(lastsent) > send_interval) {
		lastshown = TBTimer::init();

		DrawingLine l;
		l.x1 = mouse_x;
		l.y1 = mouse_y;
		l.x2 = mouse_x = x;
		l.y2 = mouse_y = y;
		drawings[me->PlayerNumber].lines.push_back(l);

		if (TBTimer::diff(lastqueued) > queue_interval
			|| TBTimer::diff(lastsent) > send_interval) {
			lastqueued = TBTimer::init();

			queue.push_back(ShortPos(ToShortPos(x), ToShortPos(y)));

			if (queue.size() == 7 || TBTimer::diff(lastsent) > send_interval) {
				lastsent = TBTimer::init();
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
		lastsent = TBTimer::init();
	} else {
		BumpSessionID();
		GW::Agent* me = GW::Agents().GetPlayer();
		queue.push_back(ShortPos(ToShortPos(mouse_x), ToShortPos(mouse_y)));
		pings.push_front(new TerrainPing(mouse_x, mouse_y));
	}

	SendQueue();

	return true;
}

void PingsLinesRenderer::SendQueue() {
	static GW::Packet::P037* packet = new GW::Packet::P037();

	//printf("sending %d pos [%d]\n", queue.size(), session_id);

	if (queue.size() > 0 && queue.size() < 8) {

		packet->NumberPts = queue.size();
		packet->session_id = session_id;
		for (unsigned int i = 0; i < queue.size(); ++i) {
			packet->points[i].x = queue[i].x;
			packet->points[i].y = queue[i].y;
		}

		GW::CtoS().SendPacket<GW::Packet::P037>(packet);
	}

	queue.clear();
}
