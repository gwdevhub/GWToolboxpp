#include "InfoPanel.h"

#include <string>
#include <cmath>

#include <GWCA\GWCA.h>
#include <GWCA\Managers\ItemMgr.h>
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

InfoPanel::InfoPanel() {
	Resources::Instance()->LoadTextureAsync(&texture, "info.png", "img");
}

void InfoPanel::Draw(IDirect3DDevice9* pDevice) {
	ImGui::SetNextWindowPosCenter(ImGuiSetCond_FirstUseEver);
	ImGui::Begin(Name(), &visible);
	ImGui::Checkbox("Timer", &GWToolbox::Instance().timer_window->visible);
	ImGui::ShowHelp("Time the instance has been active");
	ImGui::SameLine(ImGui::GetWindowContentRegionWidth() / 2);
	ImGui::Checkbox("Minimap", &GWToolbox::Instance().minimap->visible);
	ImGui::ShowHelp("An alternative to the default compass");
	ImGui::Checkbox("Bonds", &GWToolbox::Instance().bonds_window->visible);
	ImGui::ShowHelp("Show the bonds maintained by you.\nOnly works on human players");
	ImGui::SameLine(ImGui::GetWindowContentRegionWidth() / 2);
	ImGui::Checkbox("Damage", &GWToolbox::Instance().party_damage->visible);
	ImGui::ShowHelp("Show the damage done by each player in your party.\nOnly works on the damage done within your radar range.");
	ImGui::Checkbox("Health", &GWToolbox::Instance().health_window->visible);
	ImGui::ShowHelp("Displays the health of the target.\nMax health is only computed and refreshed when you directly damage or heal your target");
	ImGui::SameLine(ImGui::GetWindowContentRegionWidth() / 2);
	ImGui::Checkbox("Distance", &GWToolbox::Instance().distance_window->visible);
	ImGui::ShowHelp("Displays the distance to your target.\n1010 = Earshot / Aggro\n1248 = Cast range\n2500 = Spirit range\n5000 = Radar range");
	ImGui::Checkbox("Clock", &GWToolbox::Instance().clock_window->visible);
	ImGui::ShowHelp("Displays the system time (hour : minutes)");
	ImGui::SameLine(ImGui::GetWindowContentRegionWidth() / 2);
	ImGui::Checkbox("Notepad", &GWToolbox::Instance().notepad_window->visible);
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
		sprintf_s(file_buf, "%d", GWToolbox::Instance().minimap->mapfile);
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
		ImGui::InputText("First item ModelID", modelid, 32, ImGuiInputTextFlags_ReadOnly);
		//ImGui::InputText("First item ItemID", itemid, 32, ImGuiInputTextFlags_ReadOnly);
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
	// todo
	// if (ImGui::CollapsingHeader("Resigns")) {}
	ImGui::End();
}
