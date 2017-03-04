#include "InfoPanel.h"

#include <string>
#include <cmath>

#include <GWCA\GWCA.h>
#include <GWCA\Managers\ItemMgr.h>
#include <GWCA\Managers\StoCMgr.h>
#include <GWCA\Constants\Constants.h>

#include "GWToolbox.h"
#include "GuiUtils.h"

#include <Windows\TimerWindow.h>
#include <Windows\HealthWindow.h>
#include <Windows\DistanceWindow.h>
#include <Windows\BondsWindow.h>
#include <Windows\PartyDamage.h>
#include <Windows\Minimap\Minimap.h>
#include <Windows\ClockWindow.h>
#include <Windows\NotepadWindow.h>
#include <OtherModules\Resources.h>

void InfoPanel::Initialize() {
	ToolboxPanel::Initialize();
	Resources::Instance().LoadTextureAsync(&texture, "info.png", "img");
	GW::StoC().AddGameServerEvent<GW::Packet::StoC::P081>(
		[this](GW::Packet::StoC::P081* pak) {
		if (pak->message[0] == 0x7BFF
			&& pak->message[1] == 0xC9C4
			&& pak->message[2] == 0xAeAA
			&& pak->message[3] == 0x1B9B
			&& pak->message[4] == 0x107) { // 0x107 is the "start string" marker
			
			const int offset = 5;
			int i = 0;
			char buf[256];
			while (i < 256 && pak->message[i + offset] != 0x1 && pak->message[i + offset] != 0) {
				buf[i] = (char)(pak->message[i + offset]);
				++i;
			}
			buf[i] = '\0';
			resigned.push_back(std::string(buf));
		}
		return false;
	});
	GW::StoC().AddGameServerEvent<GW::Packet::StoC::P391_InstanceLoadFile>(
		[this](GW::Packet::StoC::P391_InstanceLoadFile* packet) -> bool {
		mapfile = packet->map_fileID;
		resigned.clear();
		return false;
	});
}

void InfoPanel::Draw(IDirect3DDevice9* pDevice) {
	if (!visible) return;
	ImGui::SetNextWindowPosCenter(ImGuiSetCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiSetCond_FirstUseEver);
	if (ImGui::Begin(Name(), &visible)) {
		ImGui::Checkbox("Timer", &TimerWindow::Instance().visible);
		ImGui::ShowHelp("Time the instance has been active");
		ImGui::SameLine(ImGui::GetWindowContentRegionWidth() / 2);
		ImGui::Checkbox("Minimap", &Minimap::Instance().visible);
		ImGui::ShowHelp("An alternative to the default compass");
		ImGui::Checkbox("Bonds", &BondsWindow::Instance().visible);
		ImGui::ShowHelp("Show the bonds maintained by you.\nOnly works on human players");
		ImGui::SameLine(ImGui::GetWindowContentRegionWidth() / 2);
		ImGui::Checkbox("Damage", &PartyDamage::Instance().visible);
		ImGui::ShowHelp("Show the damage done by each player in your party.\nOnly works on the damage done within your radar range.");
		ImGui::Checkbox("Health", &HealthWindow::Instance().visible);
		ImGui::ShowHelp("Displays the health of the target.\nMax health is only computed and refreshed when you directly damage or heal your target");
		ImGui::SameLine(ImGui::GetWindowContentRegionWidth() / 2);
		ImGui::Checkbox("Distance", &DistanceWindow::Instance().visible);
		ImGui::ShowHelp("Displays the distance to your target.\n1010 = Earshot / Aggro\n1248 = Cast range\n2500 = Spirit range\n5000 = Radar range");
		ImGui::Checkbox("Clock", &ClockWindow::Instance().visible);
		ImGui::ShowHelp("Displays the system time (hour : minutes)");
		ImGui::SameLine(ImGui::GetWindowContentRegionWidth() / 2);
		ImGui::Checkbox("Notepad", &NotePadWindow::Instance().visible);
		ImGui::ShowHelp("A simple in-game text editor");

		if (ImGui::Button("Open Xunlai Chest", ImVec2(-1.0f, 0))) {
			GW::Items().OpenXunlaiWindow();
		}
		if (ImGui::CollapsingHeader("Player")) {
			static char x_buf[32] = "";
			static char y_buf[32] = "";
			static char s_buf[32] = "";
			static char agentid_buf[32] = "";
			static char modelid_buf[32] = "";
			GW::Agent* player = GW::Agents().GetPlayer();
			if (player) {
				sprintf_s(x_buf, "%.2f", player->X);
				sprintf_s(y_buf, "%.2f", player->Y);
				float s = sqrtf(player->MoveX * player->MoveX + player->MoveY * player->MoveY);
				sprintf_s(s_buf, "%.3f", s / 288.0f);
				sprintf_s(agentid_buf, "%d", player->Id);
				sprintf_s(modelid_buf, "%d", player->PlayerNumber);
			}
			ImGui::PushItemWidth(-80.0f);
			ImGui::InputText("X pos##player", x_buf, 32, ImGuiInputTextFlags_ReadOnly);
			ImGui::InputText("Y pos##player", y_buf, 32, ImGuiInputTextFlags_ReadOnly);
			ImGui::InputText("Speed##player", s_buf, 32, ImGuiInputTextFlags_ReadOnly);
			ImGui::InputText("Agent ID##player", agentid_buf, 32, ImGuiInputTextFlags_ReadOnly);
			ImGui::ShowHelp("Agent ID is unique for each agent in the instance,\nIt's generated on spawn and will change in different instances.");
			ImGui::InputText("Player ID##player", modelid_buf, 32, ImGuiInputTextFlags_ReadOnly);
			ImGui::ShowHelp("Player ID is unique for each human player in the instance.");
			ImGui::PopItemWidth();
		}
		if (ImGui::CollapsingHeader("Target")) {
			static char x_buf[32] = "";
			static char y_buf[32] = "";
			static char s_buf[32] = "";
			static char agentid_buf[32] = "";
			static char modelid_buf[32] = "";
			GW::Agent* target = GW::Agents().GetTarget();
			if (target) {
				sprintf_s(x_buf, "%.2f", target->X);
				sprintf_s(y_buf, "%.2f", target->Y);
				float s = sqrtf(target->MoveX * target->MoveX + target->MoveY * target->MoveY);
				sprintf_s(s_buf, "%.3f", s / 288.0f);
				sprintf_s(agentid_buf, "%d", target->Id);
				sprintf_s(modelid_buf, "%d", target->PlayerNumber);
			} else {
				sprintf_s(x_buf, "-");
				sprintf_s(y_buf, "-");
				sprintf_s(s_buf, "-");
				sprintf_s(agentid_buf, "-");
				sprintf_s(modelid_buf, "-");
			}
			ImGui::PushItemWidth(-80.0f);
			ImGui::InputText("X pos##target", x_buf, 32, ImGuiInputTextFlags_ReadOnly);
			ImGui::InputText("Y pos##target", y_buf, 32, ImGuiInputTextFlags_ReadOnly);
			ImGui::InputText("Speed##target", s_buf, 32, ImGuiInputTextFlags_ReadOnly);
			ImGui::InputText("Agent ID##target", agentid_buf, 32, ImGuiInputTextFlags_ReadOnly);
			ImGui::ShowHelp("Agent ID is unique for each agent in the instance,\nIt's generated on spawn and will change in different instances.");
			ImGui::InputText("Model ID##target", modelid_buf, 32, ImGuiInputTextFlags_ReadOnly);
			ImGui::ShowHelp("Model ID is unique for each kind of agent.\nIt is static and shared by the same agents.\nWhen targeting players, this is Player ID instead, unique for each player in the instance.\nFor the purpose of targeting hotkeys and commands, use this value");
			ImGui::PopItemWidth();
		}
		if (ImGui::CollapsingHeader("Map")) {
			static char id_buf[32] = "";
			char* type = "";
			static char file_buf[32] = "";
			sprintf_s(id_buf, "%d", GW::Map().GetMapID());
			switch (GW::Map().GetInstanceType()) {
			case GW::Constants::InstanceType::Outpost: type = "Outpost\0\0\0"; break;
			case GW::Constants::InstanceType::Explorable: type = "Explorable"; break;
			case GW::Constants::InstanceType::Loading: type = "Loading\0\0\0"; break;
			}
			sprintf_s(file_buf, "%d", mapfile);
			ImGui::PushItemWidth(-80.0f);
			ImGui::InputText("Map ID", id_buf, 32, ImGuiInputTextFlags_ReadOnly);
			ImGui::ShowHelp("Map ID is unique for each area");
			ImGui::InputText("Map Type", type, 11);
			ImGui::InputText("Map file", file_buf, 32, ImGuiInputTextFlags_ReadOnly);
			ImGui::ShowHelp("Map file is unique for each pathing map (e.g. used by minimap).\nMany different maps use the same map file");
			ImGui::PopItemWidth();
		}
		if (ImGui::CollapsingHeader("Dialog")) {
			static char id_buf[32] = "";
			sprintf_s(id_buf, "0x%X", GW::Agents().GetLastDialogId());
			ImGui::PushItemWidth(-80.0f);
			ImGui::InputText("Last Dialog", id_buf, 32, ImGuiInputTextFlags_ReadOnly);
			ImGui::PopItemWidth();
		}
		if (ImGui::CollapsingHeader("Items")) {
			ImGui::Text("First item in inventory");
			static char modelid[32] = "";
			//static char itemid[32] = "";
			strcpy_s(modelid, "-");
			//strcpy_s(itemid, "-");
			GW::Bag** bags = GW::Items().GetBagArray();
			if (bags) {
				GW::Bag* bag1 = bags[1];
				if (bag1) {
					GW::ItemArray items = bag1->Items;
					if (items.valid()) {
						GW::Item* item = items[0];
						if (item) {
							sprintf_s(modelid, "%d", item->ModelId);
							//sprintf_s(itemid, "%d", item->ItemId);
						}
					}
				}
			}
			ImGui::PushItemWidth(-80.0f);
			ImGui::InputText("ModelID", modelid, 32, ImGuiInputTextFlags_ReadOnly);
			//ImGui::InputText("ItemID", itemid, 32, ImGuiInputTextFlags_ReadOnly);
			ImGui::PopItemWidth();
		}
		if (ImGui::CollapsingHeader("Mob count")) {
			int cast_count = 0;
			int spirit_count = 0;
			int compass_count = 0;
			GW::AgentArray agents = GW::Agents().GetAgentArray();
			GW::Agent* player = GW::Agents().GetPlayer();
			if (GW::Map().GetInstanceType() != GW::Constants::InstanceType::Loading
				&& agents.valid()
				&& player != nullptr) {

				for (unsigned int i = 0; i < agents.size(); ++i) {
					GW::Agent* agent = agents[i];
					if (agent == nullptr) continue; // ignore nothings
					if (agent->Allegiance != 0x3) continue; // ignore non-hostiles
					if (agent->GetIsDead()) continue; // ignore dead 
					float sqrd = GW::Agents().GetSqrDistance(player->pos, agent->pos);
					if (sqrd < GW::Constants::SqrRange::Spellcast) ++cast_count;
					if (sqrd < GW::Constants::SqrRange::Spirit) ++spirit_count;
					++compass_count;
				}
			}

			ImGui::Text("%d in casting range", cast_count);
			ImGui::Text("%d in spirit range", spirit_count);
			ImGui::Text("%d in compass range", compass_count);
		}
		if (ImGui::CollapsingHeader("Resigns")) {
			if (resigned.empty()) {
				ImGui::Text("<none>");
			} else {
				for (std::string& s : resigned) {
					ImGui::Text(s.c_str());
				}
			}
		}
	}
	ImGui::End();
}
