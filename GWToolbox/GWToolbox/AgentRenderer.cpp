#include "AgentRenderer.h"

#include <GWCA\GWCA.h>
#include <GWCA\Managers\AgentMgr.h>

#include "Config.h"
#include "MinimapUtils.h"

AgentRenderer::AgentRenderer() : vertices(nullptr) {

	MinimapUtils::Color modifier = MinimapUtils::IniReadColor2(L"color_agent_modifier", L"0x001E1E1E");
	color_eoe = MinimapUtils::IniReadColor2(L"color_eoe", L"0x3200FF00");
	color_qz = MinimapUtils::IniReadColor2(L"color_qz", L"0x320000FF");
	color_target = MinimapUtils::IniReadColor2(L"color_target", L"0xFFFFFF00");
	color_player = MinimapUtils::IniReadColor2(L"color_player", L"0xFFFF8000");
	color_player_dead = MinimapUtils::IniReadColor2(L"color_player_dead", L"0x64FF8000");
	color_signpost = MinimapUtils::IniReadColor2(L"color_signpost", L"0xFF0000C8");
	color_item = MinimapUtils::IniReadColor2(L"color_item", L"0xFF0000F0");
	color_hostile = MinimapUtils::IniReadColor2(L"color_hostile", L"0xFFF00000");
	color_hostile_damaged = MinimapUtils::IniReadColor2(L"color_hostile_damaged", L"0xFF800000");
	color_hostile_dead = MinimapUtils::IniReadColor2(L"color_hostile_dead", L"0xFF320000");
	color_neutral = MinimapUtils::IniReadColor2(L"color_neutral", L"0xFF0000DC");
	color_ally_party = MinimapUtils::IniReadColor2(L"color_ally", L"0xFF00B300");
	color_ally_npc = MinimapUtils::IniReadColor2(L"color_ally_npc", L"0xFF99FF99");
	color_ally_spirit = MinimapUtils::IniReadColor2(L"color_ally_spirit", L"0xFF608000");
	color_ally_minion = MinimapUtils::IniReadColor2(L"color_ally_minion", L"0xFF008060");
	color_ally_dead = MinimapUtils::IniReadColor2(L"color_ally_dead", L"0x64006400");

	MinimapUtils::Color light = modifier;
	MinimapUtils::Color dark(0, -light.r, -light.g, -light.b);

	shapes[Tear].AddVertex(1.8f, 0, dark);		// A
	shapes[Tear].AddVertex(0.7f, 0.7f, dark);	// B
	shapes[Tear].AddVertex(0.0f, 0.0f, light);		// O
	shapes[Tear].AddVertex(0.7f, 0.7f, dark);	// B
	shapes[Tear].AddVertex(0.0f, 1.0f, dark);	// C
	shapes[Tear].AddVertex(0.0f, 0.0f, light);		// O
	shapes[Tear].AddVertex(0.0f, 1.0f, dark);	// C
	shapes[Tear].AddVertex(-0.7f, 0.7f, dark);	// D
	shapes[Tear].AddVertex(0.0f, 0.0f, light);		// O
	shapes[Tear].AddVertex(-0.7f, 0.7f, dark);	// D
	shapes[Tear].AddVertex(-1.0f, 0.0f, dark);	// E
	shapes[Tear].AddVertex(0.0f, 0.0f, light);		// O
	shapes[Tear].AddVertex(-1.0f, 0.0f, dark);	// E
	shapes[Tear].AddVertex(-0.7f, -0.7f, dark);	// F
	shapes[Tear].AddVertex(0.0f, 0.0f, light);		// O
	shapes[Tear].AddVertex(-0.7f, -0.7f, dark);	// F
	shapes[Tear].AddVertex(0.0f, -1.0f, dark);	// G
	shapes[Tear].AddVertex(0.0f, 0.0f, light);		// O
	shapes[Tear].AddVertex(0.0f, -1.0f, dark);	// G
	shapes[Tear].AddVertex(0.7f, -0.7f, dark);	// H
	shapes[Tear].AddVertex(0.0f, 0.0f, light);		// O
	shapes[Tear].AddVertex(0.7f, -0.7f, dark);	// H
	shapes[Tear].AddVertex(1.8f, 0.0f, dark);	// A
	shapes[Tear].AddVertex(0.0f, 0.0f, light);		// O

	int num_triangles = 8;
	float PI = static_cast<float>(M_PI);
	for (int i = 0; i < num_triangles; ++i) {
		float angle1 = 2 * (i + 0) * PI / num_triangles;
		float angle2 = 2 * (i + 1) * PI / num_triangles;
		shapes[Circle].AddVertex(std::cos(angle1), std::sin(angle1), dark);
		shapes[Circle].AddVertex(std::cos(angle2), std::sin(angle2), dark);
		shapes[Circle].AddVertex(0.0f, 0.0f, light);
	}

	num_triangles = 32;
	for (int i = 0; i < num_triangles; ++i) {
		float angle1 = 2 * (i + 0) * PI / num_triangles;
		float angle2 = 2 * (i + 1) * PI / num_triangles;
		shapes[BigCircle].AddVertex(std::cos(angle1), std::sin(angle1), MinimapUtils::Color(0, 0, 0, 0));
		shapes[BigCircle].AddVertex(std::cos(angle2), std::sin(angle2), MinimapUtils::Color(0, 0, 0, 0));
		shapes[BigCircle].AddVertex(0.0f, 0.0f, MinimapUtils::Color(-50, 0, 0, 0));
	}

	shapes[Quad].AddVertex(1.0f, -1.0f, dark);
	shapes[Quad].AddVertex(1.0f, 1.0f, dark);
	shapes[Quad].AddVertex(0.0f, 0.0f, light);
	shapes[Quad].AddVertex(1.0f, 1.0f, dark);
	shapes[Quad].AddVertex(-1.0f, 1.0f, dark);
	shapes[Quad].AddVertex(0.0f, 0.0f, light);
	shapes[Quad].AddVertex(-1.0f, 1.0f, dark);
	shapes[Quad].AddVertex(-1.0f, -1.0f, dark);
	shapes[Quad].AddVertex(0.0f, 0.0f, light);
	shapes[Quad].AddVertex(-1.0f, -1.0f, dark);
	shapes[Quad].AddVertex(1.0f, -1.0f, dark);
	shapes[Quad].AddVertex(0.0f, 0.0f, light);

	max_shape_verts = 0;
	for (int shape = 0; shape < shape_size; ++shape) {
		if (max_shape_verts < shapes[shape].vertices.size()) {
			max_shape_verts = shapes[shape].vertices.size();
		}
	}
}


void AgentRenderer::Initialize(IDirect3DDevice9* device) {
	type_ = D3DPT_TRIANGLELIST;

	vertices_max = max_shape_verts * 0x200; // support for up to 512 agents, should be enough

	vertices = nullptr;

	HRESULT hr = device->CreateVertexBuffer(sizeof(D3DVertex) * vertices_max, 0,
		D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer_, NULL);
	if (FAILED(hr)) printf("Error: %ls\n", hr);
}

void AgentRenderer::Render(IDirect3DDevice9* device) {
	if (!initialized_) {
		Initialize(device);
		initialized_ = true;
	}

	HRESULT res = buffer_->Lock(0, sizeof(D3DVertex) * vertices_max, (VOID**)&vertices, D3DLOCK_DISCARD);
	if (FAILED(res)) printf("AgentRenderer Lock() error: %d\n", res);

	vertices_count = 0;

	// get stuff
	GW::AgentArray agents = GW::Agents().GetAgentArray();
	if (!agents.valid()) return;

	GW::NPCArray npcs = GW::Agents().GetNPCArray();
	if (!npcs.valid()) return;

	DWORD player_id = GW::Agents().GetPlayerId();
	DWORD target_id = GW::Agents().GetTargetId();

	// eoes
	for (size_t i = 0; i < agents.size(); ++i) {
		GW::Agent* agent = agents[i];
		if (agent == nullptr) continue;
		if (agent->GetIsDead()) continue;
		switch (agent->PlayerNumber) {
		case GW::Constants::ModelID::EoE:
			Enqueue(BigCircle, agent, GW::Constants::Range::Spirit, color_eoe);
			break;
		case GW::Constants::ModelID::QZ:
			Enqueue(BigCircle, agent, GW::Constants::Range::Spirit, color_qz);
			break;
		default:
			break;
		}
	}
	// non-player agents
	for (size_t i = 0; i < agents.size(); ++i) {
		GW::Agent* agent = agents[i];
		if (agent == nullptr) continue;
		if (agent->PlayerNumber <= 12) continue;
		if (agent->PlayerNumber == 33280) continue; // signposts in foundry
		if (agent->GetIsLivingType()
			&& agent->IsNPC()
			&& agent->PlayerNumber < npcs.size()
			&& (npcs[agent->PlayerNumber].npcflags & 0x10000) > 0) continue;
		if (agent->Id == target_id) continue; // will draw target at the end

		Enqueue(agent);

		if (vertices_count >= vertices_max - 16 * max_shape_verts) break;
	}
	// players
	for (size_t i = 0; i < agents.size(); ++i) {
		GW::Agent* agent = agents[i];
		if (agent == nullptr) continue;
		if (agent->PlayerNumber > 12) continue;
		if (agent->Id == target_id) continue; // will draw player at the end
		if (agent->Id == target_id) continue; // will draw target at the end

		Enqueue(agent);

		if (vertices_count >= vertices_max - 4 * max_shape_verts) break;
	}

	GW::Agent* target = agents[target_id];
	if (target != nullptr) Enqueue(target);

	GW::Agent* player = agents[player_id];
	if (player != nullptr && player->Id != target_id) Enqueue(player);

	buffer_->Unlock();

	if (vertices_count != 0) {
		device->SetStreamSource(0, buffer_, 0, sizeof(D3DVertex));
		device->DrawPrimitive(type_, 0, vertices_count / 3);
		vertices_count = 0;
	}
}

void AgentRenderer::Enqueue(GW::Agent* agent) {
	MinimapUtils::Color color = GetColor(agent);
	float size = GetSize(agent);
	Shape_e shape = GetShape(agent);

	if (GW::Agents().GetTargetId() == agent->Id) {
		Enqueue(shape, agent, size + 50.0f, color_target);
	}

	Enqueue(shape, agent, size, color);
}

MinimapUtils::Color AgentRenderer::GetColor(GW::Agent* agent) const {
	if (agent->Id == GW::Agents().GetPlayerId()) {
		if (agent->GetIsDead()) return color_player_dead;
		else return color_player;
	}

	if (agent->GetIsSignpostType()) return color_signpost;
	if (agent->GetIsItemType()) return color_item;

	// don't draw dead spirits
	auto npcs = GW::Agents().GetNPCArray();
	if (agent->GetIsDead() && npcs.valid() && agent->PlayerNumber < npcs.size()) {
		GW::NPC& npc = npcs[agent->PlayerNumber];
		switch (npc.modelfileid) {
		case 0x22A34: // nature rituals
		case 0x2D0E4: // defensive binding rituals
		case 0x2D07E: // offensive binding rituals
			return MinimapUtils::Color(0, 0, 0, 0);;
		default:
			break;
		}
	}

	// hostiles
	if (agent->Allegiance == 0x3) {
		if (agent->HP > 0.9f) return color_hostile;
		if (agent->HP > 0.0f) return color_hostile_damaged;
		return color_hostile_dead;
	}

	// neutrals
	if (agent->Allegiance == 0x2) return color_neutral;

	// friendly
	if (agent->GetIsDead()) return color_ally_dead;
	switch (agent->Allegiance) {
	case 0x1: return color_ally_party; // ally
	case 0x6: return color_ally_npc; // npc / minipet
	case 0x4: return color_ally_spirit; // spirit / pet
	case 0x5: return color_ally_minion; // minion
	default: break;
	}

	return MinimapUtils::Color(0, 0, 0);
}

float AgentRenderer::GetSize(GW::Agent* agent) const {
	if (agent->Id == GW::Agents().GetPlayerId()) return 100.0f;
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
		case GW::Constants::ModelID::DoA::StygianLordNecro:
		case GW::Constants::ModelID::DoA::StygianLordMesmer:
		case GW::Constants::ModelID::DoA::StygianLordEle:
		case GW::Constants::ModelID::DoA::StygianLordMonk:
		case GW::Constants::ModelID::DoA::StygianLordDerv:
		case GW::Constants::ModelID::DoA::StygianLordRanger:
		case GW::Constants::ModelID::DoA::BlackBeastOfArgh:
		case GW::Constants::ModelID::DoA::SmotheringTendril:
		case GW::Constants::ModelID::DoA::LordJadoth:
		
		case GW::Constants::ModelID::UW::KeeperOfSouls:

		case GW::Constants::ModelID::FoW::ShardWolf:

		case GW::Constants::ModelID::SoO::Brigand:
		case GW::Constants::ModelID::SoO::Fendi:
		case GW::Constants::ModelID::SoO::Fendi_soul:
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

	auto npcs = GW::Agents().GetNPCArray();
	if (npcs.valid() && agent->PlayerNumber < npcs.size()) {
		GW::NPC& npc = npcs[agent->PlayerNumber];
		switch (npc.modelfileid) {
		case 0x22A34: // nature rituals
		case 0x2D0E4: // defensive binding rituals
		case 0x2963E: // dummies
			return Circle;
		default:
			break;
		}
	}

	return Tear;
}

void AgentRenderer::Enqueue(Shape_e shape, GW::Agent* agent, float size, MinimapUtils::Color color) {
	if (color.a == 0) return;

	GW::Vector2f translate(agent->X, agent->Y);
	unsigned int i;
	for (i = 0; i < shapes[shape].vertices.size(); ++i) {
		GW::Vector2f v = shapes[shape].vertices[i].Rotated(agent->Rotation_cos,
			agent->Rotation_sin) * size + translate;
		MinimapUtils::Color c = color + shapes[shape].colors[i];
		c.Clamp();
		vertices[i].z = 0.0f;
		vertices[i].color = c.GetDXColor();
		vertices[i].x = v.x;
		vertices[i].y = v.y;
	}
	vertices += shapes[shape].vertices.size();
	vertices_count += shapes[shape].vertices.size();
}

void AgentRenderer::Shape_t::AddVertex(float x, float y, MinimapUtils::Color color) {
	vertices.push_back(GW::Vector2f(x, y));
	colors.push_back(color);
}
