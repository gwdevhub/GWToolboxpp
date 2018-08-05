#include "InfoWindow.h"

#include <string>
#include <cmath>

#include <GWCA\Constants\Constants.h>
#include <GWCA\Managers\ItemMgr.h>
#include <GWCA\Managers\StoCMgr.h>
#include <GWCA\Managers\PartyMgr.h>

#include "GWToolbox.h"
#include "GuiUtils.h"

#include <GWCA\Managers\AgentMgr.h>
#include <GWCA\Managers\MapMgr.h>
#include <GWCA\Managers\GameThreadMgr.h>
#include <GWCA\Managers\ChatMgr.h>
#include <GWCA\Managers\EffectMgr.h>

#include <Widgets\TimerWidget.h>
#include <Widgets\HealthWidget.h>
#include <Widgets\DistanceWidget.h>
#include <Widgets\BondsWidget.h>
#include <Widgets\PartyDamage.h>
#include <Widgets\Minimap\Minimap.h>
#include <Widgets\ClockWidget.h>
#include <Widgets\AlcoholWidget.h>
#include <Windows\NotepadWindow.h>
#include <Modules\Resources.h>

void InfoWindow::Initialize() {
	ToolboxWindow::Initialize();

	GW::Agents::SetupLastDialogHook();

	Resources::Instance().LoadTextureAsync(&button_texture, Resources::GetPath(L"img/icons", L"info.png"), IDB_Icon_Info);
	GW::StoC::AddCallback<GW::Packet::StoC::MessageCore>(
		[this](GW::Packet::StoC::MessageCore *pak) {
		if (pak->message[0] == 0x7BFF
			&& pak->message[1] == 0xC9C4
			&& pak->message[2] == 0xAeAA
			&& pak->message[3] == 0x1B9B
			&& pak->message[4] == 0x107) { // 0x107 is the "start string" marker
			
			// Prepare the name
			const int offset = 5;
			int i = 0;
			wchar_t buf[128];
			while (i < 256 && pak->message[i + offset] != 0x1 && pak->message[i + offset] != 0) {
				buf[i] = (pak->message[offset + i]);
				++i;
			}
			buf[i] = '\0';

			// get all the data
			GW::PartyInfo* info = GW::PartyMgr::GetPartyInfo();
			if (info == nullptr) return false;
			GW::PlayerPartyMemberArray partymembers = info->players;
			if (!partymembers.valid()) return false;
			GW::PlayerArray players = GW::Agents::GetPlayerArray();
			if (!players.valid()) return false;

			// set the right index in party
			for (unsigned i = 0; i < partymembers.size(); ++i) {
				if (i >= status.size()) continue;
				if (partymembers[i].loginnumber >= players.size()) continue;
				if (wcscmp(players[partymembers[i].loginnumber].Name, buf) == 0) {
					status[i] = Resigned;
					timestamp[i] = GW::Map::GetInstanceTime();
				}
			}
		}
		return false;
	});
	GW::StoC::AddCallback<GW::Packet::StoC::InstanceLoadFile>(
		[this](GW::Packet::StoC::InstanceLoadFile *packet) -> bool {
		mapfile = packet->map_fileID;
		for (unsigned i = 0; i < status.size(); ++i) {
			status[i] = NotYetConnected;
			timestamp[i] = 0;
		}
		return false;
	});
}

void InfoWindow::Draw(IDirect3DDevice9* pDevice) {
	if (!visible) return;
	ImGui::SetNextWindowPosCenter(ImGuiSetCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiSetCond_FirstUseEver);
	if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
		if (show_widgets) {
			ImGui::Checkbox("Timer", &TimerWidget::Instance().visible);
			ImGui::ShowHelp("Time the instance has been active");
			ImGui::SameLine(ImGui::GetWindowContentRegionWidth() / 2);
			ImGui::Checkbox("Minimap", &Minimap::Instance().visible);
			ImGui::ShowHelp("An alternative to the default compass");
			ImGui::Checkbox("Bonds", &BondsWidget::Instance().visible);
			ImGui::ShowHelp("Show the bonds maintained by you.\nOnly works on human players");
			ImGui::SameLine(ImGui::GetWindowContentRegionWidth() / 2);
			ImGui::Checkbox("Damage", &PartyDamage::Instance().visible);
			ImGui::ShowHelp("Show the damage done by each player in your party.\nOnly works on the damage done within your radar range.");
			ImGui::Checkbox("Health", &HealthWidget::Instance().visible);
			ImGui::ShowHelp("Displays the health of the target.\nMax health is only computed and refreshed when you directly damage or heal your target");
			ImGui::SameLine(ImGui::GetWindowContentRegionWidth() / 2);
			ImGui::Checkbox("Distance", &DistanceWidget::Instance().visible);
			ImGui::ShowHelp("Displays the distance to your target.\n1010 = Earshot / Aggro\n1248 = Cast range\n2500 = Spirit range\n5000 = Radar range");
			ImGui::Checkbox("Clock", &ClockWidget::Instance().visible);
			ImGui::ShowHelp("Displays the system time (hour : minutes)");
			ImGui::SameLine(ImGui::GetWindowContentRegionWidth() / 2);
			ImGui::Checkbox("Notepad", &NotePadWindow::Instance().visible);
			ImGui::ShowHelp("A simple in-game text editor");
			ImGui::Checkbox("Alcohol", &AlcoholWidget::Instance().visible);
			ImGui::ShowHelp("Shows a countdown timer for alcohol");
		}

		if (show_open_chest) {
			if (ImGui::Button("Open Xunlai Chest", ImVec2(-1.0f, 0))) {
				GW::GameThread::Enqueue([]() {
					GW::Items::OpenXunlaiWindow();
				});
			}
		}

		if (show_player && ImGui::CollapsingHeader("Player")) {
			static char x_buf[32] = "";
			static char y_buf[32] = "";
			static char s_buf[32] = "";
			static char agentid_buf[32] = "";
			static char modelid_buf[32] = "";
			GW::Agent* player = GW::Agents::GetPlayer();
			if (player) {
				snprintf(x_buf, 32, "%.2f", player->pos.x);
				snprintf(y_buf, 32, "%.2f", player->pos.y);
				float s = sqrtf(player->MoveX * player->MoveX + player->MoveY * player->MoveY);
				snprintf(s_buf, 32, "%.3f", s / 288.0f);
				snprintf(agentid_buf, 32, "%d", player->Id);
				snprintf(modelid_buf, 32, "%d", player->PlayerNumber);
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
            if (ImGui::TreeNode("Effects##player")) {
                GW::EffectArray effects = GW::Effects::GetPlayerEffectArray();
                if (effects.valid()) {
                    for (DWORD i = 0; i < effects.size(); ++i) {
                        ImGui::Text("id: %d", effects[i].SkillId);
                        long time = effects[i].GetTimeRemaining();
                        if (time > 0) {
                            ImGui::SameLine();
                            ImGui::Text(" duration: %d", time / 1000);
                        }
                    }
                }
                ImGui::TreePop();
            }

		}
		if (show_target && ImGui::CollapsingHeader("Target")) {
			static char x_buf[32] = "";
			static char y_buf[32] = "";
			static char s_buf[32] = "";
			static char agentid_buf[32] = "";
			static char modelid_buf[32] = "";
			GW::Agent* target = GW::Agents::GetTarget();
			if (target) {
				snprintf(x_buf, 32, "%.2f", target->pos.x);
				snprintf(y_buf, 32, "%.2f", target->pos.y);
				float s = sqrtf(target->MoveX * target->MoveX + target->MoveY * target->MoveY);
				snprintf(s_buf, 32, "%.3f", s / 288.0f);
				snprintf(agentid_buf, 32, "%d", target->Id);
				snprintf(modelid_buf, 32, "%d", target->PlayerNumber);
			} else {
				snprintf(x_buf, 32, "-");
				snprintf(y_buf, 32, "-");
				snprintf(s_buf, 32, "-");
				snprintf(agentid_buf, 32, "-");
				snprintf(modelid_buf, 32, "-");
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
			if (ImGui::TreeNode("Advanced##target")) {
				ImGui::PushItemWidth(ImGui::GetContentRegionAvailWidth() / 2);
				if (target) {
					ImGui::LabelText("Addr", "%p", target);
					ImGui::LabelText("Id", "%d", target->Id);
					ImGui::LabelText("Z", "%f", target->Z);
					ImGui::LabelText("Width", "%f", target->Width1);
					ImGui::LabelText("Height", "%f", target->Height1);
					ImGui::LabelText("Rotation", "%f", target->Rotation_angle);
					ImGui::LabelText("NameProperties", "0x%X", target->NameProperties);
					ImGui::LabelText("X", "%f", target->pos.x);
					ImGui::LabelText("Y", "%f", target->pos.y);
					ImGui::LabelText("plane", "%d", target->plane);
					ImGui::LabelText("Type", "0x%X", target->Type);
					ImGui::LabelText("Owner", "%d", target->Owner);
					ImGui::LabelText("ItemId", "%d", target->ItemID);
					ImGui::LabelText("ExtraType", "%d", target->ExtraType);
					ImGui::LabelText("AS of Weapon", "%f", target->WeaponAttackSpeed);
					ImGui::LabelText("AS modifier", "%f", target->AttackSpeedModifier);
					ImGui::LabelText("PlayerNumber", "%d", target->PlayerNumber);
					ImGui::LabelText("Primary Prof", "%d", target->Primary);
					ImGui::LabelText("Secondary Prof", "%d", target->Secondary);
					ImGui::LabelText("Level", "%d", target->Level);
					ImGui::LabelText("TeamId", "%d", target->TeamId);
					ImGui::LabelText("Effects", "0x%X", target->Effects);
					ImGui::LabelText("ModelState", "0x%X", target->ModelState);
					ImGui::LabelText("typeMap", "0x%X", target->TypeMap);
					ImGui::LabelText("LoginNumber", "%d", target->LoginNumber);
					ImGui::LabelText("Allegiance", "%d", target->Allegiance);
					ImGui::LabelText("WeaponType", "%d", target->WeaponType);
					ImGui::LabelText("Skill", "%d", target->Skill);
				}
				ImGui::PopItemWidth();
				ImGui::TreePop();
			}
		}
		if (show_map && ImGui::CollapsingHeader("Map")) {
			static char id_buf[32] = "";
			char* type = "";
			static char file_buf[32] = "";
			snprintf(id_buf, 32, "%d", GW::Map::GetMapID());
			switch (GW::Map::GetInstanceType()) {
			case GW::Constants::InstanceType::Outpost: type = "Outpost\0\0\0"; break;
			case GW::Constants::InstanceType::Explorable: type = "Explorable"; break;
			case GW::Constants::InstanceType::Loading: type = "Loading\0\0\0"; break;
			}
			snprintf(file_buf, 32, "%d", mapfile);
			ImGui::PushItemWidth(-80.0f);
			ImGui::InputText("Map ID", id_buf, 32, ImGuiInputTextFlags_ReadOnly);
			ImGui::ShowHelp("Map ID is unique for each area");
			ImGui::InputText("Map Type", type, 11, ImGuiInputTextFlags_ReadOnly);
			ImGui::InputText("Map file", file_buf, 32, ImGuiInputTextFlags_ReadOnly);
			ImGui::ShowHelp("Map file is unique for each pathing map (e.g. used by minimap).\nMany different maps use the same map file");
			ImGui::PopItemWidth();
		}
		if (show_dialog && ImGui::CollapsingHeader("Dialog")) {
			static char id_buf[32] = "";
			snprintf(id_buf, 32, "0x%X", GW::Agents::GetLastDialogId());
			ImGui::PushItemWidth(-80.0f);
			ImGui::InputText("Last Dialog", id_buf, 32, ImGuiInputTextFlags_ReadOnly);
			ImGui::PopItemWidth();
		}
		if (show_item && ImGui::CollapsingHeader("Items")) {
			ImGui::Text("First item in inventory");
			static char modelid[32] = "";
			strcpy_s(modelid, "-");
			GW::Bag** bags = GW::Items::GetBagArray();
			if (bags) {
				GW::Bag* bag1 = bags[1];
				if (bag1) {
					GW::ItemArray items = bag1->Items;
					if (items.valid()) {
						GW::Item* item = items[0];
						if (item) {
							snprintf(modelid, 32, "%d", item->ModelId);
						}
					}
				}
			}
			ImGui::PushItemWidth(-80.0f);
			ImGui::InputText("ModelID", modelid, 32, ImGuiInputTextFlags_ReadOnly);
			//ImGui::InputText("ItemID", itemid, 32, ImGuiInputTextFlags_ReadOnly);
			ImGui::PopItemWidth();
		}
		if (show_quest && ImGui::CollapsingHeader("Quest")) {
			GW::QuestLog qlog = GW::GameContext::instance()->world->questlog;
			DWORD qid = GW::GameContext::instance()->world->activequestid;
			if (qid && qlog.valid()) {
				for (unsigned int i = 0; i < qlog.size(); ++i) {
					GW::Quest q = qlog[i];
					if (q.questid == qid) {
						ImGui::Text("ID: 0x%X", q.questid);
						ImGui::Text("Marker: (%.0f, %.0f)", q.marker.x, q.marker.y);
						break;
					}
				}
			}
		}
		if (show_mobcount && ImGui::CollapsingHeader("Enemy count")) {
			const float sqr_soul_range = 1400.0f * 1400.0f;
			int soul_count = 0;
			int cast_count = 0;
			int spirit_count = 0;
			int compass_count = 0;
			GW::AgentArray agents = GW::Agents::GetAgentArray();
			GW::Agent* player = GW::Agents::GetPlayer();
			if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading
				&& agents.valid()
				&& player != nullptr) {

				for (unsigned int i = 0; i < agents.size(); ++i) {
					GW::Agent* agent = agents[i];
					if (agent == nullptr) continue; // ignore nothings
					if (agent->Allegiance != 0x3) continue; // ignore non-hostiles
					if (agent->GetIsDead()) continue; // ignore dead 
					float sqrd = GW::Agents::GetSqrDistance(player->pos, agent->pos);
					if (agent->PlayerNumber == GW::Constants::ModelID::DoA::SoulTormentor
						|| agent->PlayerNumber == GW::Constants::ModelID::DoA::VeilSoulTormentor) {
						if (GW::Map::GetMapID() == GW::Constants::MapID::Domain_of_Anguish
							&& sqrd < sqr_soul_range) {
							++soul_count;
						}
					}
					if (sqrd < GW::Constants::SqrRange::Spellcast) ++cast_count;
					if (sqrd < GW::Constants::SqrRange::Spirit) ++spirit_count;
					++compass_count;
				}
			}

			if (GW::Map::GetMapID() == GW::Constants::MapID::Domain_of_Anguish) {
				ImGui::Text("%d Soul Tormentors", soul_count);
				ImGui::ShowHelp("Only in Domain of Anguish, within 1400 range");
			}
			ImGui::Text("%d foes in casting range", cast_count);
			ImGui::Text("%d foes in spirit range", spirit_count);
			ImGui::Text("%d foes in compass range", compass_count);
		}
		if (show_resignlog && ImGui::CollapsingHeader("Resign log")) {
			DrawResignlog();
		}
	}
	ImGui::End();
}

void InfoWindow::Update(float delta) {
	if (show_resignlog
		&& GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading
		&& GW::PartyMgr::GetPartyInfo()) {

		GW::PlayerPartyMemberArray partymembers = GW::PartyMgr::GetPartyInfo()->players;
		if (partymembers.valid()) {
			if (partymembers.size() != status.size()) {
				status.resize(partymembers.size(), Unknown);
				timestamp.resize(partymembers.size(), 0);
			}
		}
		for (unsigned i = 0; i < partymembers.size(); ++i) {
			GW::PlayerPartyMember& partymember = partymembers[i];
			if (partymember.connected()) {
				if (status[i] == NotYetConnected || status[i] == Unknown) {
					status[i] = Connected;
					timestamp[i] = GW::Map::GetInstanceTime();
				}
			} else {
				if (status[i] == Connected || status[i] == Resigned) {
					status[i] = Left;
					timestamp[i] = GW::Map::GetInstanceTime();
				}
			}
		}
	}
}

void InfoWindow::DrawResignlog() {
	if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) return;
	GW::PartyInfo* info = GW::PartyMgr::GetPartyInfo();
	if (info == nullptr) return;
	GW::PlayerPartyMemberArray partymembers = info->players;
	if (!partymembers.valid()) return;
	GW::PlayerArray players = GW::Agents::GetPlayerArray();
	if (!players.valid()) return;
	for (unsigned i = 0; i < partymembers.size(); ++i) {
		GW::PlayerPartyMember& partymember = partymembers[i];
		if (partymember.loginnumber >= players.size()) continue;
		GW::Player& player = players[partymember.loginnumber];
		const char* status_str = [](Status status) -> const char* {
			switch (status) {
			case InfoWindow::Unknown: return "Unknown";
			case InfoWindow::NotYetConnected: return "Not connected";
			case InfoWindow::Connected: return "Connected";
			case InfoWindow::Resigned: return "Resigned";
			case InfoWindow::Left: return "Left";
			default: return "";
			}
		}(status[i]);
		ImGui::PushID(i);
		if (ImGui::Button("Send")) {
			// Todo: wording probably needs improvement
			char buf[256];
			snprintf(buf, 256, "%d. %S - %s", i + 1, player.Name,
				(status[i] == Connected 
					&& GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable) 
				? "Connected (not resigned)" : status_str);
			GW::Chat::SendChat('#', buf);
		}
		ImGui::SameLine();
		ImGui::Text("%d. %S - %s", i + 1, player.Name, status_str);
		if (status[i] != Unknown) {
			ImGui::SameLine();
			ImGui::TextDisabled("[%d:%02d:%02d.%03d]",
				(timestamp[i] / (60 * 60 * 1000)),
				(timestamp[i] / (60 * 1000)) % 60,
				(timestamp[i] / (1000)) % 60,
				(timestamp[i]) % 1000);
		}
		ImGui::PopID();
	}
}

void InfoWindow::DrawSettingInternal() {
	ImGui::Checkbox("Show widget toggles", &show_widgets);
	ImGui::Checkbox("Show 'open xunlai chest' button", &show_open_chest);
	ImGui::Checkbox("Show Player", &show_player);
	ImGui::Checkbox("Show target", &show_target);
	ImGui::Checkbox("Show map", &show_map);
	ImGui::Checkbox("Show dialog", &show_dialog);
	ImGui::Checkbox("Show item", &show_item);
	ImGui::Checkbox("Show Quest", &show_quest);
	ImGui::Checkbox("Show enemy count", &show_mobcount);
	ImGui::Checkbox("Show resign log", &show_resignlog);
}

void InfoWindow::LoadSettings(CSimpleIni* ini) {
	ToolboxWindow::LoadSettings(ini);
	show_menubutton = ini->GetBoolValue(Name(), VAR_NAME(show_menubutton), true);

	show_widgets = ini->GetBoolValue(Name(), VAR_NAME(show_widgets), true);
	show_open_chest = ini->GetBoolValue(Name(), VAR_NAME(show_open_chest), true);
	show_player = ini->GetBoolValue(Name(), VAR_NAME(show_player), true);
	show_target = ini->GetBoolValue(Name(), VAR_NAME(show_target), true);
	show_map = ini->GetBoolValue(Name(), VAR_NAME(show_map), true);
	show_dialog = ini->GetBoolValue(Name(), VAR_NAME(show_dialog), true);
	show_item = ini->GetBoolValue(Name(), VAR_NAME(show_item), true);
	show_quest = ini->GetBoolValue(Name(), VAR_NAME(show_quest), true);
	show_mobcount = ini->GetBoolValue(Name(), VAR_NAME(show_enemycount), true);
	show_resignlog = ini->GetBoolValue(Name(), VAR_NAME(show_resignlog), true);
}

void InfoWindow::SaveSettings(CSimpleIni* ini) {
	ToolboxWindow::SaveSettings(ini);
	ini->SetBoolValue(Name(), VAR_NAME(show_widgets), show_widgets);
	ini->SetBoolValue(Name(), VAR_NAME(show_open_chest), show_open_chest);
	ini->SetBoolValue(Name(), VAR_NAME(show_player), show_player);
	ini->SetBoolValue(Name(), VAR_NAME(show_target), show_target);
	ini->SetBoolValue(Name(), VAR_NAME(show_map), show_map);
	ini->SetBoolValue(Name(), VAR_NAME(show_dialog), show_dialog);
	ini->SetBoolValue(Name(), VAR_NAME(show_item), show_item);
	ini->SetBoolValue(Name(), VAR_NAME(show_quest), show_quest);
	ini->SetBoolValue(Name(), VAR_NAME(show_enemycount), show_mobcount);
	ini->SetBoolValue(Name(), VAR_NAME(show_resignlog), show_resignlog);
}
