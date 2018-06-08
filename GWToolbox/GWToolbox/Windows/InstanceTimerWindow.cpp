#include "InstanceTimerWindow.h"

#include <GWCA\Constants\QuestIDs.h>
#include <GWCA\Constants\Constants.h>

#include <GWCA\Managers\AgentMgr.h>
#include <GWCA\Managers\MapMgr.h>
#include <GWCA\Managers\ChatMgr.h>
#include <GWCA\Managers\StoCMgr.h>

#include "GuiUtils.h"
#include "GWToolbox.h"

#include <Modules\Resources.h>

void InstanceTimerWindow::Initialize() {
	ToolboxWindow::Initialize();
}

void InstanceTimerWindow::Draw(IDirect3DDevice9* pDevice) {
	if (!visible) return;

	ImGui::SetNextWindowPosCenter(ImGuiSetCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiSetCond_FirstUseEver);
	if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
		
		//DoA Timer
		if (show_doatimer && ImGui::CollapsingHeader("DoA Timer")) {
			//All Areas			
			for (int i = 0; i < 4; ++i) {
				snprintf(doatime[i], 32, "%02d:%02d:%02d", DOAD[i] / (60 * 60), (DOAD[i] / 60) % 60, DOAD[i] % 60);
				if (ImGui::Button(DoAArea[i], ImVec2((ImGui::GetWindowContentRegionWidth() / 2), 0))) {
					snprintf(buf, 256, "%s: %02d:%02d:%02d", DoAArea[i], DOAD[i] / (60 * 60), (DOAD[i] / 60) % 60, DOAD[i] % 60);
					GW::Chat::SendChat('#', buf);
				}
				ImGui::PushItemWidth((ImGui::GetWindowContentRegionWidth()) / 2 - ImGui::GetStyle().ItemInnerSpacing.x);
				ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
				ImGui::InputText("", doatime[i], 32, ImGuiInputTextFlags_ReadOnly);
			}
			//Send All Button
			if (ImGui::Button("Send All", ImVec2((ImGui::GetWindowContentRegionWidth()), 0))) {
				for (int i = 0; i < 4; ++i) {
					snprintf(buf, 256, "%s: %02d:%02d:%02d", DoAArea[i], DOAD[i] / (60 * 60), (DOAD[i] / 60) % 60, DOAD[i] % 60);
					GW::Chat::SendChat('#', buf);
				}
			}
		}

		//UW Timer
		if (show_uwtimer && ImGui::CollapsingHeader("Underworld Timer")) {
			
			ImGui::SetCursorPosX(ImGui::GetWindowContentRegionWidth() / 3 + 15.0f);
			ImGui::Text("Taken");
			ImGui::SameLine(2 * ImGui::GetWindowContentRegionWidth() / 3 + 15.0f);
			ImGui::Text("Done");

			//All Areas
			for (int i = 0; i < 10; ++i) {
				snprintf(UWTtime[i], 32, "%02d:%02d:%02d", UWT[i] / (60 * 60), (UWT[i] / 60) % 60, UWT[i] % 60);
				snprintf(UWDtime[i], 32, "%02d:%02d:%02d", UWD[i] / (60 * 60), (UWD[i] / 60) % 60, UWD[i] % 60);
				if (ImGui::Button(UWArea[i], ImVec2((ImGui::GetWindowContentRegionWidth() / 3), 0))) {
					snprintf(buf, 256, "[%s] ~ Taken: %02d:%02d:%02d ~ Done: %02d:%02d:%02d", UWArea[i], UWT[i] / (60 * 60), (UWT[i] / 60) % 60, UWT[i] % 60, UWD[i] / (60 * 60), (UWD[i] / 60) % 60, UWD[i] % 60);
					GW::Chat::SendChat('#', buf);
				}
				ImGui::PushItemWidth((ImGui::GetWindowContentRegionWidth() - ImGui::GetStyle().ItemInnerSpacing.x * 2) / 3);
				ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
				ImGui::InputText("", UWTtime[i], 32, ImGuiInputTextFlags_ReadOnly);
				ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
				ImGui::InputText("", UWDtime[i], 32, ImGuiInputTextFlags_ReadOnly);
			}
			ImGui::Separator();
			
			//Dhuum
			ImGui::SetCursorPosX(ImGui::GetWindowContentRegionWidth() / 3 + 15.0f);
			ImGui::Text("Started");
			ImGui::SameLine(2 * ImGui::GetWindowContentRegionWidth() / 3 + 15.0f);
			ImGui::Text("Done");

			snprintf(DhuumStarttime, 32, "%02d:%02d:%02d", DhuumS / (60 * 60), (DhuumS / 60) % 60, DhuumS % 60);
			snprintf(DhuumDonetime, 32, "%02d:%02d:%02d", DhuumD / (60 * 60), (DhuumD / 60) % 60, DhuumD % 60);
			if (ImGui::Button("Dhuum", ImVec2((ImGui::GetWindowContentRegionWidth() / 3), 0))) {
				snprintf(buf, 256, "[Dhuum] ~ Started: %02d:%02d:%02d ~ Done: %02d:%02d:%02d", DhuumS / (60 * 60), (DhuumS / 60) % 60, DhuumS % 60, DhuumD / (60 * 60), (DhuumD / 60) % 60, DhuumD % 60);
				GW::Chat::SendChat('#', buf);
			}
			ImGui::PushItemWidth((ImGui::GetWindowContentRegionWidth() - ImGui::GetStyle().ItemInnerSpacing.x * 2) / 3);
			ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
			ImGui::InputText("", DhuumStarttime, 9, ImGuiInputTextFlags_ReadOnly);
			ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
			ImGui::InputText("", DhuumDonetime, 9, ImGuiInputTextFlags_ReadOnly);

		}

		//FOW Timer
		if (show_uwtimer && ImGui::CollapsingHeader("Fissure of Woe Timer")) {

			ImGui::SetCursorPosX(ImGui::GetWindowContentRegionWidth() / 3 + 15.0f);
			ImGui::Text("Taken");
			ImGui::SameLine(2 * ImGui::GetWindowContentRegionWidth() / 3 + 15.0f);
			ImGui::Text("Done");

			//All Areas
			for (int i = 0; i < 11; ++i) {
				snprintf(FOWTtime[i], 32, "%02d:%02d:%02d", FOWT[i] / (60 * 60), (FOWT[i] / 60) % 60, FOWT[i] % 60);
				snprintf(FOWDtime[i], 32, "%02d:%02d:%02d", FOWD[i] / (60 * 60), (FOWD[i] / 60) % 60, FOWD[i] % 60);
				if (ImGui::Button(FOWArea[i], ImVec2((ImGui::GetWindowContentRegionWidth() / 3), 0))) {
					snprintf(buf, 256, "[%s] ~ Taken: %02d:%02d:%02d ~ Done: %02d:%02d:%02d", FOWArea[i], FOWT[i] / (60 * 60), (FOWT[i] / 60) % 60, FOWT[i] % 60, FOWD[i] / (60 * 60), (FOWD[i] / 60) % 60, FOWD[i] % 60);
					GW::Chat::SendChat('#', buf);
				}
				ImGui::PushItemWidth((ImGui::GetWindowContentRegionWidth() - ImGui::GetStyle().ItemInnerSpacing.x * 2) / 3);
				ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
				ImGui::InputText("", FOWTtime[i], 32, ImGuiInputTextFlags_ReadOnly);
				ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
				ImGui::InputText("", FOWDtime[i], 32, ImGuiInputTextFlags_ReadOnly);
			}
		}
	
	}
	ImGui::End();

	//DoA-Calculations
	if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable
		&& GW::Map::GetMapID() == GW::Constants::MapID::Domain_of_Anguish) {
		if (doareset) {
			for (int i = 0; i < 4; ++i) {
				DOAD[i] = 0;
			}
			doareset = false;
		}
		GW::StoC::AddCallback<GW::Packet::StoC::P432>(
			[&](GW::Packet::StoC::P432* packet) -> bool {
			if ((packet->message[0]) == 0x8101) {
				for (int i = 0; i < 4; ++i) {
					if (DOAD[i] == 0) {
						if ((packet->message[1]) == DOAIDs[i]) {
							DOAD[i] = GW::Map::GetInstanceTime() / 1000;
						}
					}
				}
			}
			return false;
		});
	}
	else {
		doareset = true;
	}

	//Underworld-Calculations
	if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable
		&& GW::Map::GetMapID() == GW::Constants::MapID::The_Underworld) {
		if (uwreset) {
			for (int i = 0; i < 10; ++i) {
				UWT[i] = 0;
				UWD[i] = 0;
			}
			DhuumS = 0;
			DhuumD = 0;
			uwreset = false;
		}
		//Quest Taken
		GW::StoC::AddCallback<GW::Packet::StoC::P190>(
			[&](GW::Packet::StoC::P190* packet) -> bool {
			for (int i = 0; i < 10; ++i) {
				if (UWT[i] == 0) {
					if (packet->quest_id == UWIDs[i]) {
						UWT[i] = GW::Map::GetInstanceTime() / 1000;
					}
				}
			}
			return false;
		});
		//Quest Done
		GW::StoC::AddCallback<GW::Packet::StoC::P189>(
			[&](GW::Packet::StoC::P189* packet) -> bool {
			for (int i = 0; i < 10; ++i) {
				if (UWD[i] == 0) {
					if (packet->quest_id == UWIDs[i]) {
						UWD[i] = GW::Map::GetInstanceTime() / 1000;
					}
				}
			}			
			return false;
		});
		//Dhuum Started
		GW::AgentArray agents = GW::Agents::GetAgentArray();
		if (!agents.valid()) return;
		for (unsigned int i = 0; i < agents.size(); ++i) {
			GW::Agent* agent = agents[i];
			if (agent == nullptr) continue; //Skip shit
			if (DhuumS == 0) {
				if (agent->PlayerNumber == 2342 && agent->Allegiance == 0x3) {
					DhuumS = GW::Map::GetInstanceTime() / 1000;
					break;
				}
			}
			
		}

		//Dhuum Finished
		GW::StoC::AddCallback<GW::Packet::StoC::P189>(
			[&](GW::Packet::StoC::P189* packet) -> bool {
			if (DhuumD == 0) {
				if (packet->quest_id == 157) {
					DhuumD = GW::Map::GetInstanceTime() / 1000;
				}
			}
			return false;
		});

	}
	else {
		uwreset = true;
	}

	//FOW-Calculations
	if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable
		&& GW::Map::GetMapID() == GW::Constants::MapID::The_Fissure_of_Woe) {
		if (fowreset) {
			for (int i = 0; i < 11; ++i) {
				FOWT[i] = 0;
				FOWD[i] = 0;
			}
			fowreset = false;
		}
		//Quest Taken
		GW::StoC::AddCallback<GW::Packet::StoC::P190>(
			[&](GW::Packet::StoC::P190* packet) -> bool {
			for (int i = 0; i < 11; ++i) {
				if (FOWT[i] == 0) {
					if (packet->quest_id == FOWIDs[i]) {
						FOWT[i] = GW::Map::GetInstanceTime() / 1000;
					}
				}
			}
			return false;
		});
		//Quest Done
		GW::StoC::AddCallback<GW::Packet::StoC::P189>(
			[&](GW::Packet::StoC::P189* packet) -> bool {
			for (int i = 0; i < 11; ++i) {
				if (FOWD[i] == 0) {
					if (packet->quest_id == FOWIDs[i]) {
						FOWD[i] = GW::Map::GetInstanceTime() / 1000;
					}
				}
			}
			return false;
		});

	}
	else {
		fowreset = true;
	}

}

void InstanceTimerWindow::DrawSettingInternal() {
}

void InstanceTimerWindow::LoadSettings(CSimpleIni* ini) {
	ToolboxWindow::LoadSettings(ini);
	show_menubutton = ini->GetBoolValue(Name(), VAR_NAME(show_menubutton), true);

	show_uwtimer = ini->GetBoolValue(Name(), VAR_NAME(show_uwtimer), true);
	show_doatimer = ini->GetBoolValue(Name(), VAR_NAME(show_doatimer), true);
}

void InstanceTimerWindow::SaveSettings(CSimpleIni* ini) {
	ToolboxWindow::SaveSettings(ini);

	ini->SetBoolValue(Name(), VAR_NAME(show_uwtimer), show_uwtimer);
	ini->SetBoolValue(Name(), VAR_NAME(show_doatimer), show_doatimer);
}
