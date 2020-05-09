#include "stdafx.h"
#include "AgentRenderer.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/GameEntities/NPC.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/MapMgr.h>

#include "GuiUtils.h"
#include <Defines.h>

#include <Modules/Resources.h>

#define AGENTCOLOR_INIFILENAME L"AgentColors.ini"

namespace {
	static unsigned int GetAgentProfession(const GW::AgentLiving* agent) {
		if (!agent) return 0;
		if (agent->primary) return agent->primary;
		GW::NPC* npc = GW::Agents::GetNPCByID(agent->player_number);
		if (!npc) return 0;
		return npc->primary;
	}
}

unsigned int AgentRenderer::CustomAgent::cur_ui_id = 0;

void AgentRenderer::LoadSettings(CSimpleIni* ini, const char* section) {
	color_agent_modifier = Colors::Load(ini, section, VAR_NAME(color_agent_modifier), 0x001E1E1E);
	color_agent_damaged_modifier = Colors::Load(ini, section, VAR_NAME(color_agent_lowhp_modifier), 0x00505050);
	color_eoe = Colors::Load(ini, section, VAR_NAME(color_eoe), 0x3200FF00);
	color_qz = Colors::Load(ini, section, VAR_NAME(color_qz), 0x320000FF);
	color_winnowing = Colors::Load(ini, section, VAR_NAME(color_winnowing), 0x3200FFFF);
	color_target = Colors::Load(ini, section, VAR_NAME(color_target), 0xFFFFFF00);
	color_player = Colors::Load(ini, section, VAR_NAME(color_player), 0xFFFF8000);
	color_player_dead = Colors::Load(ini, section, VAR_NAME(color_player_dead), 0x64FF8000);
	color_signpost = Colors::Load(ini, section, VAR_NAME(color_signpost), 0xFF0000C8);
	color_item = Colors::Load(ini, section, VAR_NAME(color_item), 0xFF0000F0);
	color_hostile = Colors::Load(ini, section, VAR_NAME(color_hostile), 0xFFF00000);
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

	show_hidden_npcs = ini->GetBoolValue(section, VAR_NAME(show_hidden_npcs), show_hidden_npcs);
	boss_colors = ini->GetBoolValue(section, VAR_NAME(boss_colors), boss_colors);

	LoadAgentColors();

	Invalidate();
}

void AgentRenderer::LoadAgentColors() {
	if (agentcolorinifile == nullptr) agentcolorinifile = new CSimpleIni();
	agentcolorinifile->LoadFile(Resources::GetPath(AGENTCOLOR_INIFILENAME).c_str());

	custom_agents.clear();
	custom_agents_map.clear();

	CSimpleIni::TNamesDepend entries;
	agentcolorinifile->GetAllSections(entries);

	for (const CSimpleIni::Entry& entry : entries) {
		// we know that all sections are agent colors, don't even check the section names
		CustomAgent* customAgent = new CustomAgent(agentcolorinifile, entry.pItem);
		customAgent->index = custom_agents.size();
		custom_agents.push_back(customAgent);
	}
	BuildCustomAgentsMap();
	agentcolors_changed = false;
}

void AgentRenderer::SaveSettings(CSimpleIni* ini, const char* section) const {
	Colors::Save(ini, section, VAR_NAME(color_agent_modifier), color_agent_modifier);
	Colors::Save(ini, section, VAR_NAME(color_agent_damaged_modifier), color_agent_damaged_modifier);
	Colors::Save(ini, section, VAR_NAME(color_eoe), color_eoe);
	Colors::Save(ini, section, VAR_NAME(color_qz), color_qz);
	Colors::Save(ini, section, VAR_NAME(color_winnowing), color_winnowing);
	Colors::Save(ini, section, VAR_NAME(color_target), color_target);
	Colors::Save(ini, section, VAR_NAME(color_player), color_player);
	Colors::Save(ini, section, VAR_NAME(color_player_dead), color_player_dead);
	Colors::Save(ini, section, VAR_NAME(color_signpost), color_signpost);
	Colors::Save(ini, section, VAR_NAME(color_item), color_item);
	Colors::Save(ini, section, VAR_NAME(color_hostile), color_hostile);
	Colors::Save(ini, section, VAR_NAME(color_hostile_dead), color_hostile_dead);
	Colors::Save(ini, section, VAR_NAME(color_neutral), color_neutral);
	Colors::Save(ini, section, VAR_NAME(color_ally), color_ally);
	Colors::Save(ini, section, VAR_NAME(color_ally_npc), color_ally_npc);
	Colors::Save(ini, section, VAR_NAME(color_ally_spirit), color_ally_spirit);
	Colors::Save(ini, section, VAR_NAME(color_ally_minion), color_ally_minion);
	Colors::Save(ini, section, VAR_NAME(color_ally_dead), color_ally_dead);

	ini->SetDoubleValue(section, VAR_NAME(size_default), size_default);
	ini->SetDoubleValue(section, VAR_NAME(size_player), size_player);
	ini->SetDoubleValue(section, VAR_NAME(size_signpost), size_signpost);
	ini->SetDoubleValue(section, VAR_NAME(size_item), size_item);
	ini->SetDoubleValue(section, VAR_NAME(size_boss), size_boss);
	ini->SetDoubleValue(section, VAR_NAME(size_minion), size_minion);

	ini->SetBoolValue(section, VAR_NAME(show_hidden_npcs), show_hidden_npcs);
	ini->SetBoolValue(section, VAR_NAME(boss_colors), boss_colors);

	SaveAgentColors();
}

void AgentRenderer::SaveAgentColors() const {
	if (agentcolors_changed && agentcolorinifile != nullptr) {
		// clear colors from ini
		agentcolorinifile->Reset();

		// then save again
		char buf[256];
		for (unsigned int i = 0; i < custom_agents.size(); ++i) {
			snprintf(buf, 256, "customagent%03d", i);
			custom_agents[i]->SaveSettings(agentcolorinifile, buf);
		}
		agentcolorinifile->SaveFile(Resources::GetPath(AGENTCOLOR_INIFILENAME).c_str());
	}
}

void AgentRenderer::DrawSettings() {
	if (ImGui::TreeNode("Agent Colors")) {
		Colors::DrawSettingHueWheel("EoE", &color_eoe);
		ImGui::ShowHelp("This is the color at the edge, the color in the middle is the same, with alpha-50");
		Colors::DrawSettingHueWheel("QZ", &color_qz);
		ImGui::ShowHelp("This is the color at the edge, the color in the middle is the same, with alpha-50");
		Colors::DrawSettingHueWheel("Winnowing", &color_winnowing, 0);
		ImGui::ShowHelp("This is the color at the edge, the color in the middle is the same, with alpha-50");
		Colors::DrawSettingHueWheel("Target", &color_target);
		Colors::DrawSettingHueWheel("Player (alive)", &color_player);
		Colors::DrawSettingHueWheel("Player (dead)", &color_player_dead);
		Colors::DrawSettingHueWheel("Signpost", &color_signpost);
		Colors::DrawSettingHueWheel("Item", &color_item);
		Colors::DrawSettingHueWheel("Hostile (>90%%)", &color_hostile);
		Colors::DrawSettingHueWheel("Hostile (dead)", &color_hostile_dead);
		Colors::DrawSettingHueWheel("Neutral", &color_neutral);
		Colors::DrawSettingHueWheel("Ally (player)", &color_ally);
		Colors::DrawSettingHueWheel("Ally (NPC)", &color_ally_npc);
		Colors::DrawSettingHueWheel("Ally (spirit)", &color_ally_spirit);
		Colors::DrawSettingHueWheel("Ally (minion)", &color_ally_minion);
		Colors::DrawSettingHueWheel("Ally (dead)", &color_ally_dead);
		Colors::DrawSettingHueWheel("Agent modifier", &color_agent_modifier);
		ImGui::ShowHelp("Each agent has this value removed on the border and added at the center\nZero makes agents have solid color, while a high number makes them appear more shaded.");
		Colors::DrawSettingHueWheel("Agent damaged modifier", &color_agent_damaged_modifier);
		ImGui::ShowHelp("Each hostile agent has this value subtracted from it when under 90% HP.");
		if (ImGui::SmallButton("Restore Defaults")) {
			ImGui::OpenPopup("Restore Defaults?");
		}
		if (ImGui::BeginPopupModal("Restore Defaults?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Text("Are you sure?\nThis will reset all agent colors to the default values.\nThis operation cannot be undone.\n\n");
			if (ImGui::Button("OK", ImVec2(120, 0))) {
				color_agent_modifier = 0x001E1E1E;
				color_agent_damaged_modifier = 0x00505050;
				color_eoe = 0x3200FF00;
				color_qz = 0x320000FF;
				color_winnowing = 0x3200FFFF;
				color_target = 0xFFFFFF00;
				color_player = 0xFFFF8000;
				color_player_dead = 0x64FF8000;
				color_signpost = 0xFF0000C8;
				color_item = 0xFF0000F0;
				color_hostile = 0xFFF00000;
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
				boss_colors = true;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120, 0))) {
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Agent Sizes")) {
		ImGui::DragFloat("Default Size", &size_default, 1.0f, 1.0f, 0.0f, "%.0f");
		ImGui::DragFloat("Player Size", &size_player, 1.0f, 1.0f, 0.0f, "%.0f");
		ImGui::DragFloat("Signpost Size", &size_signpost, 1.0f, 1.0f, 0.0f, "%.0f");
		ImGui::DragFloat("Item Size", &size_item, 1.0f, 1.0f, 0.0f, "%.0f");
		ImGui::DragFloat("Boss Size", &size_boss, 1.0f, 1.0f, 0.0f, "%.0f");
		ImGui::DragFloat("Minion Size", &size_minion, 1.0f, 1.0f, 0.0f, "%.0f");
		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Custom Agents")) {
		bool changed = false;
		for (unsigned i = 0; i < custom_agents.size(); ++i) {

			CustomAgent* custom = custom_agents[i];
			if (!custom) continue;
			DWORD modelId = custom->modelId;

			ImGui::PushID(custom->ui_id);

			CustomAgent::Operation op = CustomAgent::Operation::None;
			if (custom->DrawSettings(op)) changed = true;

			ImGui::PopID();

			switch (op) {
			case CustomAgent::Operation::None:
				break;
			case CustomAgent::Operation::MoveUp:
				if (i > 0) std::swap(custom_agents[i], custom_agents[i - 1]);
				break;
			case CustomAgent::Operation::MoveDown:
				if (i < custom_agents.size() - 1) {
					std::swap(custom_agents[i], custom_agents[i + 1]);
					// render the moved one and increase i
					++i;
					ImGui::PushID(custom_agents[i]->ui_id);
					CustomAgent::Operation op2 = CustomAgent::Operation::None;
					custom_agents[i]->DrawSettings(op2);
					ImGui::PopID();
				}
				break;
			case CustomAgent::Operation::Delete:
				custom_agents.erase(custom_agents.begin() + i);
				delete custom;
				--i;
				break;
			case CustomAgent::Operation::ModelIdChange: {
				changed = true;
				break;
			}
			default:
				break;
			}

			switch (op) {
			case AgentRenderer::CustomAgent::Operation::MoveUp:
			case AgentRenderer::CustomAgent::Operation::MoveDown:
			case AgentRenderer::CustomAgent::Operation::Delete: 
				for (size_t j = 0; j < custom_agents.size(); ++j) {
					custom_agents[j]->index = j;
				}
				changed = true;
			default: break;
			}
		}
		if (changed) {
			agentcolors_changed = true;
			BuildCustomAgentsMap();
		}
		if (ImGui::Button("Add Agent Custom Color")) {
			custom_agents.push_back(new CustomAgent(0, color_hostile, "<name>"));
			custom_agents.back()->index = custom_agents.size() - 1;
			agentcolors_changed = true;
		}
		ImGui::TreePop();
	}
}

AgentRenderer::~AgentRenderer() {
	for (CustomAgent* ca : custom_agents) {
		if (ca) delete ca;
	}
	custom_agents.clear();
	custom_agents_map.clear();
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

	GW::AgentLiving* player = GW::Agents::GetPlayerAsAgentLiving();
	GW::AgentLiving* target = GW::Agents::GetTargetAsAgentLiving();

	// 1. eoes
	for (size_t i = 0; i < agents.size(); ++i) {
		if (!agents[i]) continue;
		GW::AgentLiving* agent = agents[i]->GetAsAgentLiving();
		if (agent == nullptr) continue;
		if (agent->GetIsDead()) continue;
		switch (agent->player_number) {
		case GW::Constants::ModelID::EoE:
			Enqueue(BigCircle, agent, GW::Constants::Range::Spirit, color_eoe);
			break;
		case GW::Constants::ModelID::QZ:
			Enqueue(BigCircle, agent, GW::Constants::Range::Spirit, color_qz);
			break;
		case GW::Constants::ModelID::Winnowing:
			Enqueue(BigCircle, agent, GW::Constants::Range::Spirit, color_winnowing);
			break;
		default:
			break;
		}
	}
	// 2. non-player agents
	static std::vector<std::pair<const GW::Agent*, const CustomAgent*>> custom_agents_to_draw;
	custom_agents_to_draw.clear();

	// some helper lambads
	auto AddCustomAgentsToDraw = [this](const GW::Agent* agent) -> bool {
		const GW::AgentLiving* living = agent->GetAsAgentLiving();
		if (!living) return false;
		const auto it = custom_agents_map.find(living->player_number);
		bool found_custom_agent = false;
		if (it != custom_agents_map.end()) {
			for (const CustomAgent* ca : it->second) {
				if (!ca->active) continue;
				if (ca->mapId > 0 && ca->mapId != (DWORD)GW::Map::GetMapID()) continue;
				custom_agents_to_draw.push_back({ agent, ca });
				found_custom_agent = true;
			}
		}
		return found_custom_agent;
	};
	auto SortCustomAgentsToDraw = []() -> void {
		std::sort(custom_agents_to_draw.begin(), custom_agents_to_draw.end(), 
			[&](const std::pair<const GW::Agent*, const CustomAgent*>& pA,
			const std::pair<const GW::Agent*, const CustomAgent*>& pB) -> bool {
			return pA.second->index > pB.second->index;
		});
	};

	for (size_t i = 0; i < agents.size(); ++i) {
		if (!agents[i]) continue;
		if (target == agents[i]) continue; // will draw target at the end
		if (agents[i]->GetIsGadgetType()) {
			GW::AgentGadget* agent = agents[i]->GetAsAgentGadget();
			if(GW::Map::GetMapID() == GW::Constants::MapID::Domain_of_Anguish && agent->extra_type == 7602) continue;
		}
		else if (agents[i]->GetIsLivingType()) {
			GW::AgentLiving* agent = agents[i]->GetAsAgentLiving();
			if (agent->player_number <= 12) continue;
			if (!show_hidden_npcs
				&& agent->IsNPC()
				&& agent->player_number < npcs.size()
				&& (npcs[agent->player_number].npc_flags & 0x10000) > 0) continue;
		}
		if (AddCustomAgentsToDraw(agents[i])) {
			// found a custom agent to draw, we'll draw them later
		} else {
			Enqueue(agents[i]);
		}
		if (vertices_count >= vertices_max - 16 * max_shape_verts) break;
	}
	// 3. custom colored models
	SortCustomAgentsToDraw();
	for (const auto pair : custom_agents_to_draw) {
		Enqueue(pair.first, pair.second);
		if (vertices_count >= vertices_max - 16 * max_shape_verts) break;
	}
	custom_agents_to_draw.clear();

	// 4. target if it's a non-player
	if (target && target->player_number > 12) {
		if (AddCustomAgentsToDraw(target)) {
			SortCustomAgentsToDraw();
			for (const auto pair : custom_agents_to_draw) {
				Enqueue(pair.first, pair.second);
				if (vertices_count >= vertices_max - 16 * max_shape_verts) break;
			}
			custom_agents_to_draw.clear();
		} else {
			Enqueue(target);
		}
	}

	// note: we don't support custom agents for players

	// 5. players
	for (size_t i = 0; i < agents.size(); ++i) {
		if (!agents[i]) continue;
		GW::AgentLiving* agent = agents[i]->GetAsAgentLiving();
		if (agent == nullptr) continue;
		if (agent->player_number > 12) continue;
		if (agent == player) continue; // will draw player at the end
		if (agent == target) continue; // will draw target at the end

		Enqueue(agent);

		if (vertices_count >= vertices_max - 4 * max_shape_verts) break;
	}

	// 6. target if it's a player
	if (target && target != player && target->player_number <= 12) {
		Enqueue(target);
	}

	// 7. player
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

void AgentRenderer::Enqueue(const GW::Agent* agent, const CustomAgent* ca) {
	Color color = GetColor(agent, ca);
	float size = GetSize(agent, ca);
	Shape_e shape = GetShape(agent, ca);

	if (GW::Agents::GetTargetId() == agent->agent_id && shape != BigCircle) {
		Enqueue(shape, agent, size + 50.0f, color_target);
	}

	Enqueue(shape, agent, size, color);
}

Color AgentRenderer::GetColor(const GW::Agent* agent, const CustomAgent* ca) const {
	if (agent->agent_id == GW::Agents::GetPlayerId()) {
		if (agent->GetAsAgentLiving()->GetIsDead()) return color_player_dead;
		else return color_player;
	}

	if (agent->GetIsGadgetType()) return color_signpost;
	if (agent->GetIsItemType()) return color_item;
	if (!agent->GetIsLivingType()) return color_item;

	const GW::AgentLiving* living = agent->GetAsAgentLiving();

	// don't draw dead spirits
	auto npcs = GW::Agents::GetNPCArray();
	if (living->GetIsDead() && npcs.valid() && living->player_number < npcs.size()) {
		GW::NPC& npc = npcs[living->player_number];
		switch (npc.model_file_id) {
		case 0x22A34: // nature rituals
		case 0x2D0E4: // defensive binding rituals
		case 0x2D07E: // offensive binding rituals
			return IM_COL32(0, 0, 0, 0);
		default:
			break;
		}
	}

	if (ca && ca->color_active) {
		if (living->allegiance == 0x3 && living->hp > 0.0f && living->hp <= 0.9f) {
			return Colors::Sub(ca->color, color_agent_damaged_modifier);
		}
		if (living->hp > 0.0f) return ca->color;
	}
	// hostiles
	if (living->allegiance == 0x3) {
		if(living->hp <= 0.0f) return color_hostile_dead;
		const Color* c = &color_hostile;
		if (boss_colors && living->GetHasBossGlow()) {
			unsigned int prof = GetAgentProfession(living);
			if (prof) c = &profession_colors[prof];
		}
		if (living->hp > 0.9f) return *c;
		return Colors::Sub(*c, color_agent_damaged_modifier);
	}

	// neutrals
	if (living->allegiance == 0x2) return color_neutral;

	// friendly
	if (living->GetIsDead()) return color_ally_dead;
	switch (living->allegiance) {
	case 0x1: return color_ally; // ally
	case 0x6: return color_ally_npc; // npc / minipet
	case 0x4: return color_ally_spirit; // spirit / pet
	case 0x5: return color_ally_minion; // minion
	default: break;
	}

	return IM_COL32(0, 0, 0, 0);
}

float AgentRenderer::GetSize(const GW::Agent* agent, const CustomAgent* ca) const {
	if (agent->agent_id == GW::Agents::GetPlayerId()) return size_player;
	if (agent->GetIsGadgetType()) return size_signpost;
	if (agent->GetIsItemType()) return size_item;
	if (!agent->GetIsLivingType()) return size_item;

	const GW::AgentLiving* living = agent->GetAsAgentLiving();
	if (ca && ca->size_active && ca->size >= 0) return ca->size;

	if (living->GetHasBossGlow()) return size_boss;

	switch (living->allegiance) {
	case 0x1: // ally
	case 0x2: // neutral
	case 0x4: // spirit / pet
	case 0x6: // npc / minipet
		return size_default;

	case 0x5: // minion
		return size_minion;

	case 0x3: // hostile
		switch (living->player_number) {
		case GW::Constants::ModelID::Rotscale:

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
		case GW::Constants::ModelID::UW::TerrorwebQueen:
		case GW::Constants::ModelID::UW::Dhuum:

		case GW::Constants::ModelID::FoW::ShardWolf:
		case GW::Constants::ModelID::FoW::SeedOfCorruption:
		case GW::Constants::ModelID::FoW::LordKhobay:
		case GW::Constants::ModelID::FoW::DragonLich:

		case GW::Constants::ModelID::Deep::Kanaxai:
		case GW::Constants::ModelID::Deep::KanaxaiAspect:
		case GW::Constants::ModelID::Urgoz::Urgoz:

		case GW::Constants::ModelID::EotnDungeons::DiscOfChaos:
		case GW::Constants::ModelID::EotnDungeons::PlagueOfDestruction:
		case GW::Constants::ModelID::EotnDungeons::ZhimMonns:
		case GW::Constants::ModelID::EotnDungeons::Khabuus:
		case GW::Constants::ModelID::EotnDungeons::DuncanTheBlack:
		case GW::Constants::ModelID::EotnDungeons::JusticiarThommis:
		case GW::Constants::ModelID::EotnDungeons::RandStormweaver:
		case GW::Constants::ModelID::EotnDungeons::Selvetarm:
		case GW::Constants::ModelID::EotnDungeons::Forgewright:
		case GW::Constants::ModelID::EotnDungeons::HavokSoulwail:
		case GW::Constants::ModelID::EotnDungeons::RragarManeater3:
		case GW::Constants::ModelID::EotnDungeons::RragarManeater12:
		case GW::Constants::ModelID::EotnDungeons::Arachni:
		case GW::Constants::ModelID::EotnDungeons::Hidesplitter:
		case GW::Constants::ModelID::EotnDungeons::PrismaticOoze:
		case GW::Constants::ModelID::EotnDungeons::IlsundurLordofFire:
		case GW::Constants::ModelID::EotnDungeons::EldritchEttin:
		case GW::Constants::ModelID::EotnDungeons::TPSRegulartorGolem:
		case GW::Constants::ModelID::EotnDungeons::MalfunctioningEnduringGolem:
		case GW::Constants::ModelID::EotnDungeons::CyndrTheMountainHeart:
		case GW::Constants::ModelID::EotnDungeons::InfernalSiegeWurm:
		case GW::Constants::ModelID::EotnDungeons::Frostmaw:
		case GW::Constants::ModelID::EotnDungeons::RemnantOfAntiquities:
		case GW::Constants::ModelID::EotnDungeons::MurakaiLadyOfTheNight:
		case GW::Constants::ModelID::EotnDungeons::ZoldarkTheUnholy:
		case GW::Constants::ModelID::EotnDungeons::Brigand:
		case GW::Constants::ModelID::EotnDungeons::FendiNin:
		case GW::Constants::ModelID::EotnDungeons::SoulOfFendiNin:
		case GW::Constants::ModelID::EotnDungeons::KeymasterOfMurakai:
		case GW::Constants::ModelID::EotnDungeons::AngrySnowman:

		case GW::Constants::ModelID::BonusMissionPack::WarAshenskull:
		case GW::Constants::ModelID::BonusMissionPack::RoxAshreign:
		case GW::Constants::ModelID::BonusMissionPack::AnrakTindershot:
		case GW::Constants::ModelID::BonusMissionPack::DettMortash:
		case GW::Constants::ModelID::BonusMissionPack::AkinCinderspire:
		case GW::Constants::ModelID::BonusMissionPack::TwangSootpaws:
		case GW::Constants::ModelID::BonusMissionPack::MagisEmberglow:
		case GW::Constants::ModelID::BonusMissionPack::MerciaTheSmug:
		case GW::Constants::ModelID::BonusMissionPack::OptimusCaliph:
		case GW::Constants::ModelID::BonusMissionPack::LazarusTheDire:
		case GW::Constants::ModelID::BonusMissionPack::AdmiralJakman:
		case GW::Constants::ModelID::BonusMissionPack::PalawaJoko:
		case GW::Constants::ModelID::BonusMissionPack::YuriTheHand:
		case GW::Constants::ModelID::BonusMissionPack::MasterRiyo:
		case GW::Constants::ModelID::BonusMissionPack::CaptainSunpu:
		case GW::Constants::ModelID::BonusMissionPack::MinisterWona:
			return size_boss;

		default:
			return size_default;
		}
		break;

	default:
		return size_default;
	}
}

AgentRenderer::Shape_e AgentRenderer::GetShape(const GW::Agent* agent, const CustomAgent* ca) const {
	if (agent->GetIsGadgetType()) return Quad;
	if (agent->GetIsItemType()) return Quad;
	if (!agent->GetIsLivingType()) return Quad; // shouldn't happen but just in case

	const GW::AgentLiving* living = agent->GetAsAgentLiving();
	if (living->login_number > 0) return Tear;	// players

	if (ca && ca->shape_active) {
		return ca->shape;
	}

	auto npcs = GW::Agents::GetNPCArray();
	if (npcs.valid() && living->player_number < npcs.size()) {
		GW::NPC& npc = npcs[living->player_number];
		switch (npc.model_file_id) {
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

void AgentRenderer::Enqueue(Shape_e shape, const GW::Agent* agent, float size, Color color) {
	if ((color & IM_COL32_A_MASK) == 0) return;

	unsigned int i;
	for (i = 0; i < shapes[shape].vertices.size(); ++i) {
		const Shape_Vertex& vert = shapes[shape].vertices[i];
		GW::Vec2f pos = (GW::Rotate(vert, agent->rotation_cos, agent->rotation_sin) * size) + agent->pos;
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

void AgentRenderer::BuildCustomAgentsMap() {
	custom_agents_map.clear();
	for (const CustomAgent* ca : custom_agents) {
		const auto it = custom_agents_map.find(ca->modelId);
		if (it == custom_agents_map.end()) {
			custom_agents_map[ca->modelId] = std::vector<const CustomAgent*>();
		}
		custom_agents_map[ca->modelId].push_back(ca);
	}
}

AgentRenderer::CustomAgent::CustomAgent(CSimpleIni* ini, const char* section)
	: ui_id(++cur_ui_id) {

	active = ini->GetBoolValue(section, VAR_NAME(active), active);
	GuiUtils::StrCopy(name, ini->GetValue(section, VAR_NAME(name), ""), sizeof(name));
	modelId = ini->GetLongValue(section, VAR_NAME(modelId), modelId);
	mapId = ini->GetLongValue(section, VAR_NAME(mapId), mapId);

	color = Colors::Load(ini, section, VAR_NAME(color), color);
	int s = ini->GetLongValue(section, VAR_NAME(shape), shape);
	if (s >= 1 && s <= 4) {
		// this is a small hack because we used to have shape=0 -> default, now we just cast to Shape_e.
		// but shape=1 on file is still tear (which is Shape_e::Tear == 0).
		shape = (Shape_e)(s - 1);
	}
	size = (float)ini->GetDoubleValue(section, VAR_NAME(size), size);

	color_active = ini->GetBoolValue(section, VAR_NAME(color_active), color_active);
	shape_active = ini->GetBoolValue(section, VAR_NAME(shape_active), shape_active);
	size_active = ini->GetBoolValue(section, VAR_NAME(size_active), size_active);
}

AgentRenderer::CustomAgent::CustomAgent(DWORD _modelId, Color _color, const char* _name)
	: ui_id(++cur_ui_id) {

	modelId = _modelId;
	color = _color;
	GuiUtils::StrCopy(name, _name, sizeof(name));
	active = true;
}

void AgentRenderer::CustomAgent::SaveSettings(CSimpleIni* ini, const char* section) const {
	ini->SetBoolValue(section, VAR_NAME(active), active);
	ini->SetValue(section, VAR_NAME(name), name);
	ini->SetLongValue(section, VAR_NAME(modelId), modelId);
	ini->SetLongValue(section, VAR_NAME(mapId), mapId);

	Colors::Save(ini, section, VAR_NAME(color), color);
	ini->SetLongValue(section, VAR_NAME(shape), shape + 1);
	ini->SetDoubleValue(section, VAR_NAME(size), size);

	ini->SetBoolValue(section, VAR_NAME(color_active), color_active);
	ini->SetBoolValue(section, VAR_NAME(shape_active), shape_active);
	ini->SetBoolValue(section, VAR_NAME(size_active), size_active);
}

bool AgentRenderer::CustomAgent::DrawHeader() {
	ImGui::SameLine(0, 18);
	bool changed = ImGui::Checkbox("##visible", &active);
	ImGui::SameLine();
	int color_i4[4];
	Colors::ConvertU32ToInt4(color, color_i4);
	ImGui::ColorButton("", ImColor(color_i4[1], color_i4[2], color_i4[3]));
	ImGui::SameLine();
	ImGui::Text(name);
	return changed;
}
bool AgentRenderer::CustomAgent::DrawSettings(AgentRenderer::CustomAgent::Operation& op) {
	bool changed = false;

	if (ImGui::TreeNode("##params")) {
		ImGui::PushID(ui_id);

		changed |= DrawHeader();

		if (ImGui::Checkbox("##visible2", &active)) changed = true;
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("If this custom agent is active");
		ImGui::SameLine();
		float x = ImGui::GetCursorPosX();
		if (ImGui::InputText("Name", name, 128)) changed = true;
		ImGui::ShowHelp("A name to help you remember what this is. Optional.");
		ImGui::SetCursorPosX(x);
		if (ImGui::InputInt("Model ID", (int*)(&modelId))) {
			op = Operation::ModelIdChange;
			changed = true;
		}
		ImGui::ShowHelp("The Agent to which this custom attributes will be applied. Required.");
		ImGui::SetCursorPosX(x);
		if (ImGui::InputInt("Map ID", (int*)&mapId)) changed = true;
		ImGui::ShowHelp("The map where it will be applied. Optional. Leave 0 for any map");

		ImGui::Spacing();

		if (ImGui::Checkbox("##color_active", &color_active)) changed = true;
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("If unchecked, the default color will be used");
		ImGui::SameLine();
		if (Colors::DrawSettingHueWheel("", &color, 0)) changed = true;
		ImGui::ShowHelp("The custom color for this agent.");

		if (ImGui::Checkbox("##size_active", &size_active)) changed = true;
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("If unchecked, the default size will be used");
		ImGui::SameLine();
		if (ImGui::DragFloat("Size", &size, 1.0f, 0.0f, 200.0f)) changed = true;
		ImGui::ShowHelp("The size for this agent.");

		if (ImGui::Checkbox("##shape_active", &shape_active)) changed = true;
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("If unchecked, the default shape will be used");
		ImGui::SameLine();
		static const char* items[] = { "Tear", "Circle", "Square", "Big Circle" };
		if (ImGui::Combo("Shape", (int*)&shape, items, 4)) {
			changed = true;
		}
		ImGui::ShowHelp("The shape of this agent.");

		ImGui::Spacing();

		// === Move and delete buttons ===
		float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
		float width = (ImGui::CalcItemWidth() - spacing * 2) / 3;
		if (ImGui::Button("Move Up", ImVec2(width, 0))) {
			op = Operation::MoveUp;
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Move the color up in the list");
		ImGui::SameLine(0, spacing);
		if (ImGui::Button("Move Down", ImVec2(width, 0))) {
			op = Operation::MoveDown;
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Move the color down in the list");
		ImGui::SameLine(0, spacing);
		if (ImGui::Button("Delete", ImVec2(width, 0))) {
			ImGui::OpenPopup("Delete Color?");
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Delete the color");

		if (ImGui::BeginPopupModal("Delete Color?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Text("Are you sure?\nThis operation cannot be undone\n\n");
			if (ImGui::Button("OK", ImVec2(120, 0))) {
				op = Operation::Delete;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120, 0))) {
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		ImGui::TreePop();
		ImGui::PopID();
	} else {
		changed |= DrawHeader();
	}
	return changed;
}
