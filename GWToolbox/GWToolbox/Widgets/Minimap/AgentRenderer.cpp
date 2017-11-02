#include "AgentRenderer.h"

#include <GWCA\Managers\AgentMgr.h>
#include <GWCA\Managers\MapMgr.h>

#include "GuiUtils.h"
#include <Defines.h>

void AgentRenderer::LoadSettings(CSimpleIni* ini, const char* section) {
	color_agent_modifier = Colors::Load(ini, section, VAR_NAME(color_agent_modifier), 0x001E1E1E);
	color_eoe = Colors::Load(ini, section, VAR_NAME(color_eoe), 0x3200FF00);
	color_qz = Colors::Load(ini, section, VAR_NAME(color_qz), 0x320000FF);
	color_target = Colors::Load(ini, section, VAR_NAME(color_target), 0xFFFFFF00);
	color_player = Colors::Load(ini, section, VAR_NAME(color_player), 0xFFFF8000);
	color_player_dead = Colors::Load(ini, section, VAR_NAME(color_player_dead), 0x64FF8000);
	color_signpost = Colors::Load(ini, section, VAR_NAME(color_signpost), 0xFF0000C8);
	color_item = Colors::Load(ini, section, VAR_NAME(color_item), 0xFF0000F0);
	color_hostile = Colors::Load(ini, section, VAR_NAME(color_hostile), 0xFFF00000);
	color_hostile_damaged = Colors::Load(ini, section, VAR_NAME(color_hostile_damaged), 0xFF800000);
	color_hostile_dead = Colors::Load(ini, section, VAR_NAME(color_hostile_dead), 0xFF320000);
	color_neutral = Colors::Load(ini, section, VAR_NAME(color_neutral), 0xFF0000DC);
	color_ally = Colors::Load(ini, section, VAR_NAME(color_ally), 0xFF00B300);
	color_ally_npc = Colors::Load(ini, section, VAR_NAME(color_ally_npc), 0xFF99FF99);
	color_ally_spirit = Colors::Load(ini, section, VAR_NAME(color_ally_spirit), 0xFF608000);
	color_ally_minion = Colors::Load(ini, section, VAR_NAME(color_ally_minion), 0xFF008060);
	color_ally_dead = Colors::Load(ini, section, VAR_NAME(color_ally_dead), 0x64006400);

	size_default = (float)ini->GetDoubleValue(section, VAR_NAME(size_default), 75.0);
	size_player = (float)ini->GetDoubleValue(section, VAR_NAME(size_player), 100.0);
	size_signpost = (float)ini->GetDoubleValue(section, VAR_NAME(size_signpost), 50.0);
	size_item = (float)ini->GetDoubleValue(section, VAR_NAME(size_item), 25.0);
	size_boss = (float)ini->GetDoubleValue(section, VAR_NAME(size_boss), 125.0);
	size_minion = (float)ini->GetDoubleValue(section, VAR_NAME(size_minion), 50.0);

	Invalidate();
}

void AgentRenderer::SaveSettings(CSimpleIni* ini, const char* section) const {
	Colors::Save(ini, section, VAR_NAME(color_agent_modifier), color_agent_modifier);
	Colors::Save(ini, section, "color_eoe", color_eoe);
	Colors::Save(ini, section, "color_qz", color_qz);
	Colors::Save(ini, section, "color_target", color_target);
	Colors::Save(ini, section, "color_player", color_player);
	Colors::Save(ini, section, "color_player_dead", color_player_dead);
	Colors::Save(ini, section, "color_signpost", color_signpost);
	Colors::Save(ini, section, "color_item", color_item);
	Colors::Save(ini, section, "color_hostile", color_hostile);
	Colors::Save(ini, section, "color_hostile_damaged", color_hostile_damaged);
	Colors::Save(ini, section, "color_hostile_dead", color_hostile_dead);
	Colors::Save(ini, section, "color_neutral", color_neutral);
	Colors::Save(ini, section, "color_ally", color_ally);
	Colors::Save(ini, section, "color_ally_npc", color_ally_npc);
	Colors::Save(ini, section, "color_ally_spirit", color_ally_spirit);
	Colors::Save(ini, section, "color_ally_minion", color_ally_minion);
	Colors::Save(ini, section, "color_ally_dead", color_ally_dead);

	ini->SetDoubleValue(section, "size_default", size_default);
	ini->SetDoubleValue(section, "size_player", size_player);
	ini->SetDoubleValue(section, "size_signpost", size_signpost);
	ini->SetDoubleValue(section, "size_item", size_item);
	ini->SetDoubleValue(section, "size_boss", size_boss);
	ini->SetDoubleValue(section, "size_minion", size_minion);
}

void AgentRenderer::DrawSettings() {
	if (ImGui::SmallButton("Restore Defaults")) {
		color_agent_modifier = 0x001E1E1E;
		color_eoe = 0x3200FF00;
		color_qz = 0x320000FF;
		color_target = 0xFFFFFF00;
		color_player = 0xFFFF8000;
		color_player_dead = 0x64FF8000;
		color_signpost = 0xFF0000C8;
		color_item = 0xFF0000F0;
		color_hostile = 0xFFF00000;
		color_hostile_damaged = 0xFF800000;
		color_hostile_dead = 0xFF320000;
		color_neutral = 0xFF0000DC;
		color_ally = 0xFF00B300;
		color_ally_npc = 0xFF99FF99;
		color_ally_spirit = 0xFF608000;
		color_ally_minion = 0xFF008060;
		color_ally_dead = 0x64006400;
		size_default = 75.0f;
		size_player = 100.0f;
		size_signpost = 50.0f;
		size_item = 25.0f;
		size_boss = 125.0f;
		size_minion = 50.0f;
	}
	Colors::DrawSetting("EoE", &color_eoe);
	ImGui::ShowHelp("This is the color at the edge, the color in the middle is the same, with alpha-50");
	Colors::DrawSetting("QZ", &color_qz);
	ImGui::ShowHelp("This is the color at the edge, the color in the middle is the same, with alpha-50");
	Colors::DrawSetting("Target", &color_target);
	Colors::DrawSetting("Player (alive)", &color_player);
	Colors::DrawSetting("Player (dead)", &color_player_dead);
	Colors::DrawSetting("Signpost", &color_signpost);
	Colors::DrawSetting("Item", &color_item);
	Colors::DrawSetting("Hostile (>90%%)", &color_hostile);
	Colors::DrawSetting("Hostile (<90%%)", &color_hostile_damaged);
	Colors::DrawSetting("Hostile (dead)", &color_hostile_dead);
	Colors::DrawSetting("Neutral", &color_neutral);
	Colors::DrawSetting("Ally (player)", &color_ally);
	Colors::DrawSetting("Ally (NPC)", &color_ally_npc);
	Colors::DrawSetting("Ally (spirit)", &color_ally_spirit);
	Colors::DrawSetting("Ally (minion)", &color_ally_minion);
	Colors::DrawSetting("Ally (dead)", &color_ally_dead);
	Colors::DrawSetting("Agent modifier", &color_agent_modifier);
	ImGui::ShowHelp("Each agent has this value removed on the border and added at the center\nZero makes agents have solid color, while a high number makes them appear more shaded.");

	ImGui::DragFloat("Default Size", &size_default, 1.0f, 1.0f, 0.0f, "%.0f");
	ImGui::DragFloat("Player Size", &size_player, 1.0f, 1.0f, 0.0f, "%.0f");
	ImGui::DragFloat("Signpost Size", &size_signpost, 1.0f, 1.0f, 0.0f, "%.0f");
	ImGui::DragFloat("Item Size", &size_item, 1.0f, 1.0f, 0.0f, "%.0f");
	ImGui::DragFloat("Boss Size", &size_boss, 1.0f, 1.0f, 0.0f, "%.0f");
	ImGui::DragFloat("Minion Size", &size_minion, 1.0f, 1.0f, 0.0f, "%.0f");
}

AgentRenderer::AgentRenderer() : vertices(nullptr) {
	shapes[Tear].AddVertex(1.8f, 0, Dark);		// A
	shapes[Tear].AddVertex(0.7f, 0.7f, Dark);	// B
	shapes[Tear].AddVertex(0.0f, 0.0f, Light);	// O
	shapes[Tear].AddVertex(0.7f, 0.7f, Dark);	// B
	shapes[Tear].AddVertex(0.0f, 1.0f, Dark);	// C
	shapes[Tear].AddVertex(0.0f, 0.0f, Light);	// O
	shapes[Tear].AddVertex(0.0f, 1.0f, Dark);	// C
	shapes[Tear].AddVertex(-0.7f, 0.7f, Dark);	// D
	shapes[Tear].AddVertex(0.0f, 0.0f, Light);	// O
	shapes[Tear].AddVertex(-0.7f, 0.7f, Dark);	// D
	shapes[Tear].AddVertex(-1.0f, 0.0f, Dark);	// E
	shapes[Tear].AddVertex(0.0f, 0.0f, Light);	// O
	shapes[Tear].AddVertex(-1.0f, 0.0f, Dark);	// E
	shapes[Tear].AddVertex(-0.7f, -0.7f, Dark);	// F
	shapes[Tear].AddVertex(0.0f, 0.0f, Light);	// O
	shapes[Tear].AddVertex(-0.7f, -0.7f, Dark);	// F
	shapes[Tear].AddVertex(0.0f, -1.0f, Dark);	// G
	shapes[Tear].AddVertex(0.0f, 0.0f, Light);	// O
	shapes[Tear].AddVertex(0.0f, -1.0f, Dark);	// G
	shapes[Tear].AddVertex(0.7f, -0.7f, Dark);	// H
	shapes[Tear].AddVertex(0.0f, 0.0f, Light);	// O
	shapes[Tear].AddVertex(0.7f, -0.7f, Dark);	// H
	shapes[Tear].AddVertex(1.8f, 0.0f, Dark);	// A
	shapes[Tear].AddVertex(0.0f, 0.0f, Light);	// O

	int num_triangles = 8;
	float PI = static_cast<float>(M_PI);
	for (int i = 0; i < num_triangles; ++i) {
		float angle1 = 2 * (i + 0) * PI / num_triangles;
		float angle2 = 2 * (i + 1) * PI / num_triangles;
		shapes[Circle].AddVertex(std::cos(angle1), std::sin(angle1), Dark);
		shapes[Circle].AddVertex(std::cos(angle2), std::sin(angle2), Dark);
		shapes[Circle].AddVertex(0.0f, 0.0f, Light);
	}

	num_triangles = 32;
	for (int i = 0; i < num_triangles; ++i) {
		float angle1 = 2 * (i + 0) * PI / num_triangles;
		float angle2 = 2 * (i + 1) * PI / num_triangles;
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

	max_shape_verts = 0;
	for (int shape = 0; shape < shape_size; ++shape) {
		if (max_shape_verts < shapes[shape].vertices.size()) {
			max_shape_verts = shapes[shape].vertices.size();
		}
	}
}

void AgentRenderer::Shape_t::AddVertex(float x, float y, AgentRenderer::Color_Modifier mod) {
	vertices.push_back(Shape_Vertex(x, y, mod));
}


void AgentRenderer::Initialize(IDirect3DDevice9* device) {
	type = D3DPT_TRIANGLELIST;
	vertices_max = max_shape_verts * 0x200; // support for up to 512 agents, should be enough
	vertices = nullptr;
	HRESULT hr = device->CreateVertexBuffer(sizeof(D3DVertex) * vertices_max, 0,
		D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer, NULL);
	if (FAILED(hr)) printf("AgentRenderer initialize error: %d\n", hr);
}

void AgentRenderer::Render(IDirect3DDevice9* device) {
	if (!initialized) {
		Initialize(device);
		initialized = true;
	}

	HRESULT res = buffer->Lock(0, sizeof(D3DVertex) * vertices_max, (VOID**)&vertices, D3DLOCK_DISCARD);
	if (FAILED(res)) printf("AgentRenderer Lock() error: %d\n", res);

	vertices_count = 0;

	// get stuff
	GW::AgentArray agents = GW::Agents::GetAgentArray();
	if (!agents.valid()) return;

	GW::NPCArray npcs = GW::Agents::GetNPCArray();
	if (!npcs.valid()) return;

	GW::Agent* player = GW::Agents::GetPlayer();
	GW::Agent* target = GW::Agents::GetTarget();

	// 1. eoes
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
	// 2. non-player agents
	for (size_t i = 0; i < agents.size(); ++i) {
		GW::Agent* agent = agents[i];
		if (agent == nullptr) continue;
		if (agent->PlayerNumber <= 12) continue;
		if (agent->GetIsGadgetType()
			&& GW::Map::GetMapID() == GW::Constants::MapID::Domain_of_Anguish
			&& agent->ExtraType == 7602) continue;
		if (agent->GetIsCharacterType()
			&& agent->IsNPC()
			&& agent->PlayerNumber < npcs.size()
			&& (npcs[agent->PlayerNumber].NpcFlags & 0x10000) > 0) continue;
		if (target == agent) continue; // will draw target at the end

		Enqueue(agent);

		if (vertices_count >= vertices_max - 16 * max_shape_verts) break;
	}
	// 3. target if it's a non-player
	if (target && target->PlayerNumber > 12) {
		Enqueue(target);
	}

	// 4. players
	for (size_t i = 0; i < agents.size(); ++i) {
		GW::Agent* agent = agents[i];
		if (agent == nullptr) continue;
		if (agent->PlayerNumber > 12) continue;
		if (agent == player) continue; // will draw player at the end
		if (agent == target) continue; // will draw target at the end

		Enqueue(agent);

		if (vertices_count >= vertices_max - 4 * max_shape_verts) break;
	}

	// 5. target if it's a player
	if (target && target != player && target->PlayerNumber <= 12) {
		Enqueue(target);
	}

	// 6. player
	if (player) {
		Enqueue(player);
	}

	buffer->Unlock();

	if (vertices_count != 0) {
		device->SetStreamSource(0, buffer, 0, sizeof(D3DVertex));
		device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, vertices_count / 3);
		vertices_count = 0;
	}
}

void AgentRenderer::Enqueue(GW::Agent* agent) {
	Color color = GetColor(agent);
	float size = GetSize(agent);
	Shape_e shape = GetShape(agent);

	if (GW::Agents::GetTargetId() == agent->Id) {
		Enqueue(shape, agent, size + 50.0f, color_target);
	}

	Enqueue(shape, agent, size, color);
}

Color AgentRenderer::GetColor(GW::Agent* agent) const {
	if (agent->Id == GW::Agents::GetPlayerId()) {
		if (agent->GetIsDead()) return color_player_dead;
		else return color_player;
	}

	if (agent->GetIsGadgetType()) return color_signpost;
	if (agent->GetIsItemType()) return color_item;

	// don't draw dead spirits
	auto npcs = GW::Agents::GetNPCArray();
	if (agent->GetIsDead() && npcs.valid() && agent->PlayerNumber < npcs.size()) {
		GW::NPC& npc = npcs[agent->PlayerNumber];
		switch (npc.ModelFileID) {
		case 0x22A34: // nature rituals
		case 0x2D0E4: // defensive binding rituals
		case 0x2D07E: // offensive binding rituals
			return IM_COL32(0, 0, 0, 0);
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
	case 0x1: return color_ally; // ally
	case 0x6: return color_ally_npc; // npc / minipet
	case 0x4: return color_ally_spirit; // spirit / pet
	case 0x5: return color_ally_minion; // minion
	default: break;
	}

	return IM_COL32(0, 0, 0, 0);
}

float AgentRenderer::GetSize(GW::Agent* agent) const {
	if (agent->Id == GW::Agents::GetPlayerId()) return size_player;
	if (agent->GetIsGadgetType()) return size_signpost;
	if (agent->GetIsItemType()) return size_item;
	if (agent->GetHasBossGlow()) return size_boss;

	switch (agent->Allegiance) {
	case 0x1: // ally
	case 0x2: // neutral
	case 0x4: // spirit / pet
	case 0x6: // npc / minipet
		return size_default;

	case 0x5: // minion
		return size_minion;

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
		case GW::Constants::ModelID::UW::FourHorseman:
		case GW::Constants::ModelID::UW::Slayer:

		case GW::Constants::ModelID::FoW::ShardWolf:
		case GW::Constants::ModelID::FoW::SeedOfCorruption:

		case GW::Constants::ModelID::SoO::Brigand:
		case GW::Constants::ModelID::SoO::Fendi:
		case GW::Constants::ModelID::SoO::Fendi_soul:
			return size_boss;

		default:
			return size_default;
		}
		break;

	default:
		return size_default;
	}
}

AgentRenderer::Shape_e AgentRenderer::GetShape(GW::Agent* agent) const {
	if (agent->GetIsGadgetType()) return Quad;
	if (agent->GetIsItemType()) return Quad;

	if (agent->LoginNumber > 0) return Tear;	// players
	if (!agent->GetIsCharacterType()) return Quad; // shouldn't happen but just in case

	auto npcs = GW::Agents::GetNPCArray();
	if (npcs.valid() && agent->PlayerNumber < npcs.size()) {
		GW::NPC& npc = npcs[agent->PlayerNumber];
		switch (npc.ModelFileID) {
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

void AgentRenderer::Enqueue(Shape_e shape, GW::Agent* agent, float size, Color color) {
	if ((color & IM_COL32_A_MASK) == 0) return;

	unsigned int i;
	for (i = 0; i < shapes[shape].vertices.size(); ++i) {
		const Shape_Vertex& vert = shapes[shape].vertices[i];
		GW::Vector2f pos = vert.Rotated(agent->Rotation_cos, agent->Rotation_sin) * size + agent->pos;
		Color vcolor = color;
		switch (vert.modifier) {
		case Dark: vcolor = Colors::Sub(color, color_agent_modifier); break;
		case Light: vcolor = Colors::Add(color, color_agent_modifier); break;
		case CircleCenter: vcolor = Colors::Sub(color, IM_COL32(0, 0, 0, 50)); break;
		case None: break;
		}
		vertices[i].z = 0.0f;
		vertices[i].color = vcolor;
		vertices[i].x = pos.x;
		vertices[i].y = pos.y;
	}
	vertices += shapes[shape].vertices.size();
	vertices_count += shapes[shape].vertices.size();
}
