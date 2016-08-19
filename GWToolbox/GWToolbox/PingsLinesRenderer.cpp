#include "PingsLinesRenderer.h"

#include <d3dx9math.h>
#include <d3d9.h>

#include <GWCA\GWCA.h>
#include <GWCA\StoCMgr.h>

PingsLinesRenderer::PingsLinesRenderer() : vertices(nullptr) {
	GWCA::StoC().AddGameServerEvent<GWCA::StoC_Pak::P133>(
		[&](GWCA::StoC_Pak::P133* pak) -> bool {

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
			pings.push_front(new TerrainPing(pak->points[0].x, pak->points[0].y));
			return false;
		} 

		if (new_session) {
			for (unsigned int i = 0; i < pak->NumberPts - 1; ++i) {
				DrawingLine l;
				l.x1 = pak->points[i + 0].x;
				l.y1 = pak->points[i + 0].y;
				l.x2 = pak->points[i + 1].x;
				l.y2 = pak->points[i + 1].y;
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
					l.x1 = pak->points[i - 1].x;
					l.y1 = pak->points[i - 1].y;
				}
				l.x2 = pak->points[i].x;
				l.y2 = pak->points[i].y;
				drawings[pak->Player].lines.push_back(l);
			}
		}

		return false;
	});

	GWCA::StoC().AddGameServerEvent<GWCA::StoC_Pak::P041>(
		[&](GWCA::StoC_Pak::P041* pak) -> bool {
		if (!visible_) return false;
		pings.push_front(new AgentPing(pak->agent_id));
		return false;
	});
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

void PingsLinesRenderer::CreatePing(float x, float y) {
	static CtoS_P37* packet = new CtoS_P37();

	short sx = static_cast<short>(std::lroundf(x / 100.0f));
	short sy = static_cast<short>(std::lroundf(y / 100.0f));

	packet->NumberPts = 1;
	packet->session_id = 1;
	packet->points[0].x = sx;
	packet->points[0].y = sy;
	DWORD size = sizeof(CtoS_P37);
	printf("Size 0x%X\n", size);
	GWCA::CtoS().SendPacket<CtoS_P37>(packet);

	pings.push_front(new TerrainPing(sx, sy));
}

void PingsLinesRenderer::Render(IDirect3DDevice9* device) {
	if (!initialized_) {
		initialized_ = true;
		Initialize(device);
	}

	for (Ping* ping : pings) {
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

	auto Enqueue = [&](short x, short y, short c) {
		vertices[0].x = x;
		vertices[0].y = y;
		vertices[0].z = 0.0f;
		vertices[0].color = D3DCOLOR_RGBA(255, 255, 255, c);
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

	D3DXMATRIX scale;
	D3DXMatrixScaling(&scale, 100.0f, 100.0f, 1.0f);
	device->SetTransform(D3DTS_WORLD, &scale);

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

	if (buffer_) buffer_->Release();
	device->CreateVertexBuffer(sizeof(D3DVertex) * vertex_count, 0,
		D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer_, NULL);
	buffer_->Lock(0, sizeof(D3DVertex) * vertex_count,
		(VOID**)&vertices, D3DLOCK_DISCARD);
	
	for (size_t i = 0; i < count_; ++i) {
		float angle = i * (static_cast<float>(2 * M_PI) / count_);
		bool outer = (i % 2 == 0);
		float radius = outer ? 1.0f : 0.8f;
		vertices[i].x = radius * std::cos(angle);
		vertices[i].y = radius * std::sin(angle);
		vertices[i].z = 0.0f;
		vertices[i].color = D3DCOLOR_ARGB(outer ? 150 : 0, 255, 0, 0);
	}
	vertices[count_] = vertices[0];
	vertices[count_ + 1] = vertices[1];

	buffer_->Unlock();
}

float PingsLinesRenderer::AgentPing::GetX() {
	GWCA::GW::Agent* agent = GWCA::Agents().GetAgentByID(id);
	if (agent == nullptr) return 0.0f;
	return agent->X;
}

float PingsLinesRenderer::AgentPing::GetY() {
	GWCA::GW::Agent* agent = GWCA::Agents().GetAgentByID(id);
	if (agent == nullptr) return 0.0f;
	return agent->Y;
}
