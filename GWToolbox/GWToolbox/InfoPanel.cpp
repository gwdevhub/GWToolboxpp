#include "InfoPanel.h"

#include <string>
#include <cmath>

#include <GWCA\GWCA.h>
#include <GWCA\Managers\ItemMgr.h>
#include <GWCA\Constants\Constants.h>

#include "GWToolbox.h"
#include "Config.h"
#include "GuiUtils.h"

#include "HealthWindow.h"
#include "DistanceWindow.h"
#include "BondsWindow.h"
#include "PartyDamage.h"
#include "Minimap.h"

void InfoPanel::Draw(IDirect3DDevice9* pDevice) {
	ImGui::Begin(Name());
	ImGui::Checkbox("Timer", &GWToolbox::instance().timer_window->visible);
	GuiUtils::ShowHelp("Time the instance has been active");
	ImGui::SameLine(ImGui::GetWindowContentRegionWidth() / 2);
	static bool tmp = true;
	ImGui::Checkbox("Minimap", &tmp); // &GWToolbox::instance().minimap->visible);
	GuiUtils::ShowHelp("An alternative to the default compass");
	ImGui::Checkbox("Bonds", &tmp); // &GWToolbox::instance().bonds_window->isVisible_);
	GuiUtils::ShowHelp("Show the bonds maintained by you.\nOnly works on human players");
	ImGui::SameLine(ImGui::GetWindowContentRegionWidth() / 2);
	ImGui::Checkbox("Damage", &tmp);
	GuiUtils::ShowHelp("Show the damage done by each player in your party.\nOnly works on the damage done within your radar range.");
	ImGui::Checkbox("Health", &GWToolbox::instance().health_window->visible);
	GuiUtils::ShowHelp("Displays the health of the target.\nMax health is only computed and refreshed when you directly damage or heal your target");
	ImGui::SameLine(ImGui::GetWindowContentRegionWidth() / 2);
	ImGui::Checkbox("Distance", &GWToolbox::instance().distance_window->visible);
	GuiUtils::ShowHelp("Displays the distance to your target.\n1010 = Earshot / Aggro\n1248 = Cast range\n2500 = Spirit range\n5000 = Radar range");
	if (ImGui::Button("Open Xunlai Chest", ImVec2(-1.0f, 0))) {
		GW::Items().OpenXunlaiWindow();
	}
	if (ImGui::TreeNode("Player")) {
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
		ImGui::InputText("X pos", x_buf, 32, ImGuiInputTextFlags_ReadOnly);
		ImGui::InputText("Y pos", y_buf, 32, ImGuiInputTextFlags_ReadOnly);
		ImGui::InputText("Speed", s_buf, 32, ImGuiInputTextFlags_ReadOnly);
		ImGui::InputText("Agent ID", agentid_buf, 32, ImGuiInputTextFlags_ReadOnly);
		GuiUtils::ShowHelp("Agent ID is unique for each agent in the instance,\nIt's generated on spawn and will change in different instances.");
		ImGui::InputText("Player ID", modelid_buf, 32, ImGuiInputTextFlags_ReadOnly);
		GuiUtils::ShowHelp("Player ID is unique for each human player in the instance.");
		ImGui::PopItemWidth();
		ImGui::TreePop();
	}
	if (ImGui::TreeNode("Target")) {
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
		ImGui::InputText("X pos", x_buf, 32, ImGuiInputTextFlags_ReadOnly);
		ImGui::InputText("Y pos", y_buf, 32, ImGuiInputTextFlags_ReadOnly);
		ImGui::InputText("Speed", s_buf, 32, ImGuiInputTextFlags_ReadOnly);
		ImGui::InputText("Agent ID", agentid_buf, 32, ImGuiInputTextFlags_ReadOnly);
		GuiUtils::ShowHelp("Agent ID is unique for each agent in the instance,\nIt's generated on spawn and will change in different instances.");
		ImGui::InputText("Model ID", modelid_buf, 32, ImGuiInputTextFlags_ReadOnly);
		GuiUtils::ShowHelp("Model ID is unique for each kind of agent.\nIt is static and shared by the same agents.\nWhen targeting players, this is Player ID instead, unique for each player in the instance.\nFor the purpose of targeting hotkeys and commands, use this value");
		ImGui::PopItemWidth();
		ImGui::TreePop();
	}
	if (ImGui::TreeNode("Map")) {
		static char id_buf[32] = "";
		char* type = "";
		static char file_buf[32] = "";
		sprintf_s(id_buf, "%d", GW::Map().GetMapID());
		switch (GW::Map().GetInstanceType()) {
		case GW::Constants::InstanceType::Outpost: type = "Outpost\0\0\0"; break;
		case GW::Constants::InstanceType::Explorable: type = "Explorable"; break;
		case GW::Constants::InstanceType::Loading: type = "Loading\0\0\0"; break;
		}
		sprintf_s(file_buf, "%d", GWToolbox::instance().minimap->mapfile);
		ImGui::PushItemWidth(-80.0f);
		ImGui::InputText("Map ID", id_buf, 32, ImGuiInputTextFlags_ReadOnly);
		GuiUtils::ShowHelp("Map ID is unique for each area");
		ImGui::InputText("Map Type", type, 11);
		ImGui::InputText("Map file", file_buf, 32, ImGuiInputTextFlags_ReadOnly);
		GuiUtils::ShowHelp("Map file is unique for each pathing map (e.g. used by minimap).\nMany different maps use the same map file");
		ImGui::PopItemWidth();
		ImGui::TreePop();
	}
	if (ImGui::TreeNode("Dialog")) {
		static char id_buf[32] = "";
		sprintf_s(id_buf, "0x%X", GW::Agents().GetLastDialogId());
		ImGui::PushItemWidth(-80.0f);
		ImGui::InputText("Last Dialog", id_buf, 32, ImGuiInputTextFlags_ReadOnly);
		ImGui::PopItemWidth();
		ImGui::TreePop();
	}
	if (ImGui::TreeNode("Items")) {
		static char id_buf[32] = "";
		sprintf_s(id_buf, "-");
		GW::Bag** bags = GW::Items().GetBagArray();
		if (bags) {
			GW::Bag* bag1 = bags[1];
			if (bag1) {
				GW::ItemArray items = bag1->Items;
				if (items.valid()) {
					GW::Item* item = items[0];
					if (item) {
						sprintf_s(id_buf, "%d", item->ModelId);
					}
				}
			}
		}
		ImGui::PushItemWidth(-80.0f);
		ImGui::InputText("First item ID", id_buf, 32, ImGuiInputTextFlags_ReadOnly);
		ImGui::PopItemWidth();
		ImGui::TreePop();
	}
	ImGui::End();
}
