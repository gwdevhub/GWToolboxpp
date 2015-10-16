#include "AgentRenderer.h"

#include <GWCA\APIMain.h>

using namespace GWAPI;

void AgentRenderer::Initialize(IDirect3DDevice9* device) {
	type_ = D3DPT_TRIANGLELIST;
	triangles_max = 0x200;
	vertices_max = triangles_max * 3;

	vertices = nullptr;
	device->CreateVertexBuffer(sizeof(Vertex) * vertices_max, 0,
		D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer_, NULL);
	buffer_->Lock(0, sizeof(Vertex) * vertices_max, (VOID**)&vertices, D3DLOCK_NOOVERWRITE);
}

void AgentRenderer::Render(IDirect3DDevice9* device) {
	if (!initialized_) {
		Initialize(device);
		initialized_ = true;
	}

	triangle_count = 0;

	GWAPIMgr* api = GWAPIMgr::instance();
	GW::Agent* me = api->Agents()->GetPlayer();
	if (me == nullptr) return;
	GW::AgentArray agents = api->Agents()->GetAgentArray();
	if (!agents.valid()) return;	

	// target
	GW::Agent* target = api->Agents()->GetTarget();
	if (target != nullptr) {
		QueueTriangle(device, target->X, target->Y, target->Rotation, 200, 0xFFFFFFFF);
	}

	// all agents
	for (size_t i = 0; i < agents.size(); ++i) {
		GW::Agent* agent = agents[i];
		if (agent == nullptr) continue;

		DWORD color;
		switch (agent->Allegiance) {
		case 0x100: color = D3DCOLOR_XRGB(0, 200, 0); break;
		case 0x300: color = D3DCOLOR_XRGB(200, 0, 0); break;
		default:	color = D3DCOLOR_XRGB(0, 150, 0); break;
		}
		if (agent->Id == GWAPIMgr::instance()->Agents()->GetPlayerId()) {
			color = D3DCOLOR_XRGB(200, 50, 255);
		}
		if (agent->GetIsDead()) color = D3DCOLOR_XRGB(20, 20, 20);

		if (agent->TypeMap == 0x40000) {
			QueueQuad(device, agent->X, agent->Y, 75, color);
		} else {
			switch (agent->Type) {
			case 0xDB:
				QueueTriangle(device, agent->X, agent->Y, agent->Rotation, 150, color);
				break;
			case 0x200:
				QueueQuad(device, agent->X, agent->Y, 150, D3DCOLOR_XRGB(0, 0, 200));
				break;
			case 0x400:
				QueueQuad(device, agent->X, agent->Y, 100, D3DCOLOR_XRGB(0, 0, 200));
			default:
				break;
			}
		}

		if (triangle_count >= triangles_max + 1) Flush(device);
	}

	Flush(device);
}

void AgentRenderer::QueueTriangle(IDirect3DDevice9* device,
	float x, float y, float rotation, float size, DWORD color) {
	for (int i = 0; i < 3; ++i) {
		vertices[i].z = 1.0f;
		vertices[i].color = color;
	}

	vertices[0].x = x + size * std::cos(rotation);
	vertices[0].y = y + size * std::sin(rotation);

	vertices[1].x = x + size * std::cos(rotation + (float)M_PI * 2 / 3);
	vertices[1].y = y + size * std::sin(rotation + (float)M_PI * 2 / 3);

	vertices[2].x = x + size * std::cos(rotation - (float)M_PI * 2 / 3);
	vertices[2].y = y + size * std::sin(rotation - (float)M_PI * 2 / 3);

	vertices += 3;
	triangle_count += 1;
}

void AgentRenderer::QueueQuad(IDirect3DDevice9* device,
	float x, float y, float size, DWORD color) {

	for (int i = 0; i < 6; ++i) {
		vertices[i].z = 1.0f;
		vertices[i].color = color;
	}

	vertices[0].x = x - size;
	vertices[0].y = y + size;
	vertices[1].x = x + size;
	vertices[1].y = y + size;
	vertices[2].x = x - size;
	vertices[2].y = y - size;
	vertices[3].x = x - size;
	vertices[3].y = y - size;
	vertices[4].x = x + size;
	vertices[4].y = y + size;
	vertices[5].x = x + size;
	vertices[5].y = y - size;

	vertices += 6;
	triangle_count += 2;
}

void AgentRenderer::Flush(IDirect3DDevice9* device) {
	buffer_->Unlock();
	device->SetStreamSource(0, buffer_, 0, sizeof(Vertex));
	device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, triangle_count);
	buffer_->Lock(0, sizeof(Vertex) * vertices_max, (VOID**)&vertices, D3DLOCK_NOOVERWRITE);
	triangle_count = 0;
}
