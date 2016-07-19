#include "AgentRenderer.h"

#include <GWCA\GWCA.h>
#include <GWCA\AgentMgr.h>

using namespace GWCA;

AgentRenderer::AgentRenderer() :
	vertices(nullptr),
	triangle_count(0),
	triangles_max(0),
	vertices_max(0) {

	shapes[Tear].AddVertex(1.8f, 0, -50);		// A
	shapes[Tear].AddVertex(0.7f, 0.7f, -50);	// B
	shapes[Tear].AddVertex(0.0f, 0.0f, 50);		// O
	shapes[Tear].AddVertex(0.7f, 0.7f, -50);	// B
	shapes[Tear].AddVertex(0.0f, 1.0f, -50);	// C
	shapes[Tear].AddVertex(0.0f, 0.0f, 50);		// O
	shapes[Tear].AddVertex(0.0f, 1.0f, -50);	// C
	shapes[Tear].AddVertex(-0.7f, 0.7f, -50);	// D
	shapes[Tear].AddVertex(0.0f, 0.0f, 50);		// O
	shapes[Tear].AddVertex(-0.7f, 0.7f, -50);	// D
	shapes[Tear].AddVertex(-1.0f, 0.0f, -50);	// E
	shapes[Tear].AddVertex(0.0f, 0.0f, 50);		// O
	shapes[Tear].AddVertex(-1.0f, 0.0f, -50);	// E
	shapes[Tear].AddVertex(-0.7f, -0.7f, -50);	// F
	shapes[Tear].AddVertex(0.0f, 0.0f, 50);		// O
	shapes[Tear].AddVertex(-0.7f, -0.7f, -50);	// F
	shapes[Tear].AddVertex(0.0f, -1.0f, -50);	// G
	shapes[Tear].AddVertex(0.0f, 0.0f, 50);		// O
	shapes[Tear].AddVertex(0.0f, -1.0f, -50);	// G
	shapes[Tear].AddVertex(0.7f, -0.7f, -50);	// H
	shapes[Tear].AddVertex(0.0f, 0.0f, 50);		// O
	shapes[Tear].AddVertex(0.7f, -0.7f, -50);	// H
	shapes[Tear].AddVertex(1.8f, 0.0f, -50);	// A
	shapes[Tear].AddVertex(0.0f, 0.0f, 50);		// O

	shapes[Circle].AddVertex(1.0f, 0, -50);		// A
	shapes[Circle].AddVertex(0.7f, 0.7f, -50);	// B
	shapes[Circle].AddVertex(0.0f, 0.0f, 50);	// O
	shapes[Circle].AddVertex(0.7f, 0.7f, -50);	// B
	shapes[Circle].AddVertex(0.0f, 1.0f, -50);	// C
	shapes[Circle].AddVertex(0.0f, 0.0f, 50);	// O
	shapes[Circle].AddVertex(0.0f, 1.0f, -50);	// C
	shapes[Circle].AddVertex(-0.7f, 0.7f, -50);	// D
	shapes[Circle].AddVertex(0.0f, 0.0f, 50);	// O
	shapes[Circle].AddVertex(-0.7f, 0.7f, -50);	// D
	shapes[Circle].AddVertex(-1.0f, 0.0f, -50);	// E
	shapes[Circle].AddVertex(0.0f, 0.0f, 50);	// O
	shapes[Circle].AddVertex(-1.0f, 0.0f, -50);	// E
	shapes[Circle].AddVertex(-0.7f, -0.7f, -50);// F
	shapes[Circle].AddVertex(0.0f, 0.0f, 50);	// O
	shapes[Circle].AddVertex(-0.7f, -0.7f, -50);// F
	shapes[Circle].AddVertex(0.0f, -1.0f, -50);	// G
	shapes[Circle].AddVertex(0.0f, 0.0f, 50);	// O
	shapes[Circle].AddVertex(0.0f, -1.0f, -50);	// G
	shapes[Circle].AddVertex(0.7f, -0.7f, -50);	// H
	shapes[Circle].AddVertex(0.0f, 0.0f, 50);	// O
	shapes[Circle].AddVertex(0.7f, -0.7f, -50);	// H
	shapes[Circle].AddVertex(1.0f, 0.0f, -50);	// A
	shapes[Circle].AddVertex(0.0f, 0.0f, 50);	// O

	shapes[Quad].AddVertex(1.0f, -1.0f, -50);
	shapes[Quad].AddVertex(1.0f, 1.0f, -50);
	shapes[Quad].AddVertex(0.0f, 0.0f, 50);
	shapes[Quad].AddVertex(1.0f, 1.0f, -50);
	shapes[Quad].AddVertex(-1.0f, 1.0f, -50);
	shapes[Quad].AddVertex(0.0f, 0.0f, 50);
	shapes[Quad].AddVertex(-1.0f, 1.0f, -50);
	shapes[Quad].AddVertex(-1.0f, -1.0f, -50);
	shapes[Quad].AddVertex(0.0f, 0.0f, 50);
	shapes[Quad].AddVertex(-1.0f, -1.0f, -50);
	shapes[Quad].AddVertex(1.0f, -1.0f, -50);
	shapes[Quad].AddVertex(0.0f, 0.0f, 50);

	max_shape_tris = shapes[Tear].vertices.size() / 3;
}


void AgentRenderer::Initialize(IDirect3DDevice9* device) {
	type_ = D3DPT_TRIANGLELIST;
	triangles_max = 0x200;
	vertices_max = triangles_max * 3;

	vertices = nullptr;
	device->CreateVertexBuffer(sizeof(D3DVertex) * vertices_max, 0,
		D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer_, NULL);
	buffer_->Lock(0, sizeof(D3DVertex) * vertices_max, (VOID**)&vertices, D3DLOCK_NOOVERWRITE);
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

		Enqueue(agent);

		CheckFlush(device);
	}

	GW::Agent* target = GWCA::Agents().GetTarget();
	if (target) Enqueue(target);

	CheckFlush(device);

	GW::Agent* player = GWCA::Agents().GetPlayer();
	if (player && player->Id != Agents().GetTargetId()) Enqueue(player);

	Flush(device);
}

void AgentRenderer::Enqueue(GW::Agent* agent) {
	if (agent->GetIsLivingType() && agent->IsNPC()) {
		auto npcs = GWCA::Agents().GetNPCArray();
		if (!npcs.valid()) return;

		GW::NPC& npc = npcs[agent->PlayerNumber];
		if ((npc.npcflags & 0x10000) > 0) return;
	}

	Color color = GetColor(agent);
	float size = GetSize(agent);
	Shape_e shape = GetShape(agent);

	if (GWCA::Agents().GetTargetId() == agent->Id) {
		Enqueue(shape, agent, size + 50.0f, Color(255, 255, 0));
	}
	Enqueue(shape, agent, size, color);
}

AgentRenderer::Color AgentRenderer::GetColor(GW::Agent* agent) const {
	if (agent->Id == GWCA::Agents().GetPlayerId()) return Color(240, 0, 240);
	if (agent->GetIsSignpostType()) return Color(0, 0, 200);
	if (agent->GetIsItemType()) return Color(0, 0, 240);

	switch (agent->Allegiance) {
	case 0x2: // neutral
		return Color(0, 0, 220); 

	case 0x3: // hostile
		if (agent->HP > 0.9f) return Color(240, 0, 0);
		else if (agent->HP > 0.0f) return Color(170, 0, 0);
		else return Color(100, 50, 0, 0);

	case 0x1: // ally
	case 0x4: // spirit / pet
	case 0x5: // minion
	case 0x6: // npc / minipet
		if (agent->GetIsDead()) return Color(100, 0, 50, 0);
		switch (agent->Allegiance) {
		case 0x1: return Color(0, 240, 0); // ally
		case 0x6: return Color(0, 180, 0); // npc / minipet
		case 0x4: return Color(0, 140, 0); // spirit / pet
		case 0x5: return Color(0, 80, 0); // minion
		default: return Color(0, 0, 0);
		}

	default:
		printf("unknown allegiance %d\n", agent->Allegiance);
		return Color(0, 255, 255);
	}
}

float AgentRenderer::GetSize(GW::Agent* agent) const {
	if (agent->Id == GWCA::Agents().GetPlayerId()) return 100.0f;
	if (agent->GetIsSignpostType()) return 50.0f;
	if (agent->GetIsItemType()) return 25.0f;
	if (agent->GetHasBossGlow()) return 125.0f;

	switch (agent->Allegiance) {
	case 0x1: // ally
	case 0x2: // neutral
	case 0x4: // spirit / pet
	case 0x6: // npc / minipet
		return 75.0f;

	case 0x5: // minion
		return 50.0f;

	case 0x3: // hostile
		switch (agent->PlayerNumber) {
		case GwConstants::ModelID::StygianLordNecro:
		case GwConstants::ModelID::StygianLordMesmer:
		case GwConstants::ModelID::StygianLordEle:
		case GwConstants::ModelID::StygianLordMonk:
		case GwConstants::ModelID::StygianLordDerv:
		case GwConstants::ModelID::StygianLordRanger:
		case GwConstants::ModelID::BlackBeastOfArgh:
			// TODO add more
			return 125.0f;

		default:
			return 75.0f;
		}
		break;

	default:
		return 75.0f;
	}
}

AgentRenderer::Shape_e AgentRenderer::GetShape(GW::Agent* agent) const {
	if (agent->GetIsSignpostType()) return Quad;
	if (agent->GetIsItemType()) return Quad;

	if (agent->LoginNumber > 0) return Tear;	// players
	if (!agent->GetIsLivingType()) return Quad; // shouldn't happen but just in case
	
	auto npcs = GWCA::Agents().GetNPCArray();
	if (!npcs.valid()) return Tear;

	GW::NPC& npc = npcs[agent->PlayerNumber];
	switch (npc.modelfileid) {
	case 0x22A34: // nature rituals
	case 0x2D0E4: // defensive binding rituals
	case 0x2963E: // dummies
		return Circle;
	
	default: 
		return Tear;
	}
}

void AgentRenderer::Enqueue(Shape_e shape, GW::Agent* agent, float size, Color color) {
	Vector2f translate(agent->X, agent->Y);
	unsigned int i;
	for (i = 0; i < shapes[shape].vertices.size(); ++i) {
		Vector2f v = shapes[shape].vertices[i].Rotated(agent->Rotation_cos,
			agent->Rotation_sin) * size + translate;
		Color c = color + shapes[shape].colors[i];
		c.Clamp();
		vertices[i].z = 0.0f;
		vertices[i].color = c.GetDXColor();
		vertices[i].x = v.x;
		vertices[i].y = v.y;
	}
	vertices += shapes[shape].vertices.size();
	triangle_count += shapes[shape].vertices.size() / 3;
}

void AgentRenderer::CheckFlush(IDirect3DDevice9* device) {
	if (triangle_count > triangles_max - max_shape_tris) Flush(device);
}

void AgentRenderer::Flush(IDirect3DDevice9* device) {
	if (triangle_count != 0) {
		buffer_->Unlock();
		device->SetStreamSource(0, buffer_, 0, sizeof(D3DVertex));
		device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, triangle_count);
		buffer_->Lock(0, sizeof(D3DVertex) * vertices_max, (VOID**)&vertices, D3DLOCK_NOOVERWRITE);
		triangle_count = 0;
	}
}

void AgentRenderer::Shape_t::AddVertex(float x, float y, int color) {
	vertices.push_back(Vector2f(x, y));
	colors.push_back(color);
}
