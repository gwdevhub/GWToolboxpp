#include "AgentRenderer.h"

#include <GWCA\GWCA.h>
#include <GWCA\AgentMgr.h>

using namespace GWCA;

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

	GW::AgentArray agents = GWCA::Agents().GetAgentArray();
	if (!agents.valid()) return;	

	// all agents
	for (size_t i = 0; i < agents.size(); ++i) {
		GW::Agent* agent = agents[i];
		if (agent == nullptr) continue;
		if (agent->Id == GWCA::Agents().GetPlayerId()) continue; // will draw player at the end
		if (agent->Id == GWCA::Agents().GetTargetId()) continue; // will draw target at the end

		QueueAgent(device, agent);

		CheckFlush(device);
	}

	GW::Agent* target = GWCA::Agents().GetTarget();
	if (target) QueueAgent(device, target);

	CheckFlush(device);

	GW::Agent* player = GWCA::Agents().GetPlayer();
	if (player) QueueAgent(device, player);

	Flush(device);
}

void AgentRenderer::QueueAgent(IDirect3DDevice9* device, GW::Agent* agent) {
	bool is_target = GWCA::Agents().GetTargetId() == agent->Id;
	bool is_player = GWCA::Agents().GetPlayerId() == agent->Id;

	DWORD color;
	if (is_player) {
		color = D3DCOLOR_XRGB(200, 50, 255);
	} else if (agent->GetIsDead()) {
		color = D3DCOLOR_ARGB(150, 20, 20, 20);
	} else {
		switch (agent->Type) {
		case 0x200:
		case 0x400:
			color = D3DCOLOR_XRGB(0, 0, 200);
			break;
		default:
			switch (agent->Allegiance) {
			case 0x100: color = D3DCOLOR_XRGB(0, 200, 0); break;
			case 0x300: color = D3DCOLOR_XRGB(200, 0, 0); break;
			default:	color = D3DCOLOR_XRGB(0, 150, 0); break;
			}
			break;
		}
	}

	float size = 150.0f;
	switch (agent->TypeMap) {
	case 0x40000: size = 100.0f; break; // spirit
	case 0xC00:	size = 200.0f; break;  // boss
	default:
		switch (agent->Type) {
		case 0xDB: // players, npcs
		case 0x200: // signposts
			size = 75.0f; 
			break;
		case 0x400: // item
			size = 75.0f;
			break;
		default:
			printf("found no size for agent, id %d\n", agent->Id);
			break;
		}
		break;
	}

	enum Shape { Tri, Quad };
	Shape shape;
	if (agent->TypeMap == 0x40000) { // spirit
		QueueQuad(device, agent->X, agent->Y, 75, color);
		shape = Quad;
	} else {
		switch (agent->Type) {
		case 0xDB: shape = Tri; break; // players, npcs
		case 0x200: shape = Quad; break; // signposts, chests, objects
		case 0x400: shape = Quad; break; // item on the ground
		default:
			printf("Found no shape for agent, id %d\n", agent->Id);
			break;
		}
	}

	switch (shape) {
	case Tri:
		if (is_target) QueueTriangle(device, agent->X, agent->Y, agent->Rotation, size + 50, 0xFFFFFFFF);
		QueueTriangle(device, agent->X, agent->Y, agent->Rotation, size, color);
		break;
	case Quad:
		if (is_target) QueueQuad(device, agent->X, agent->Y, size + 50, 0xFFFFFFFF);
		QueueQuad(device, agent->X, agent->Y, size, color);
		break;
	}
}

void AgentRenderer::QueueTriangle(IDirect3DDevice9* device,
	float x, float y, float rotation, float size, DWORD color) {
	for (int i = 0; i < 3; ++i) {
		vertices[i].z = 1.0f;
		vertices[i].color = color;
	}

	vertices[0].x = x + size * 1.3f * std::cos(rotation);
	vertices[0].y = y + size * 1.3f * std::sin(rotation);

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
