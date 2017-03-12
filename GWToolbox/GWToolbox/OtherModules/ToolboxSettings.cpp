#include "ToolboxSettings.h"

#include <GWCA\GWCA.h>
#include <GWCA\Managers\AgentMgr.h>

#include "GuiUtils.h"
#include "GWToolbox.h"

bool ToolboxSettings::move_all = false;
bool ToolboxSettings::clamp_window_positions = false;

void ToolboxSettings::DrawSettingInternal() {
	DrawFreezeSetting();
	ImGui::Checkbox("Save Location Data", &save_location_data);
	ImGui::ShowHelp("Toolbox will save your location every second in an file in Settings Folder.");
	ImGui::Checkbox("Keep windows on screen", &clamp_window_positions);
	ImGui::ShowHelp("Windows will not move off-screen.\nThis might also move windows on bottom and right side\nof the screen slightly towards the center during rezone.");
}

void ToolboxSettings::DrawFreezeSetting() {
	ImGui::Checkbox("Unlock Move All", &move_all);
	ImGui::ShowHelp("Will allow movement and resize of all widgets and windows");
}

void ToolboxSettings::LoadSettings(CSimpleIni* ini) {
	ToolboxModule::LoadSettings(ini);
	move_all = false;
	clamp_window_positions = ini->GetBoolValue(Name(), "clamp_window_positions", false);
}

void ToolboxSettings::SaveSettings(CSimpleIni* ini) {
	ToolboxModule::SaveSettings(ini);
	ini->SetBoolValue(Name(), "clamp_window_positions", clamp_window_positions);
	if (location_file.is_open()) location_file.close();
}

void ToolboxSettings::Update() {
	// save location data
	if (save_location_data && TIMER_DIFF(location_timer) > 1000) {
		location_timer = TIMER_INIT();
		if (GW::Map().GetInstanceType() == GW::Constants::InstanceType::Explorable
			&& GW::Agents().GetPlayer() != nullptr
			&& GW::Map().GetInstanceTime() > 3000) {
			GW::Constants::MapID current = GW::Map().GetMapID();
			if (location_current_map != current) {
				location_current_map = current;

				std::string map_string;
				switch (current) {
				case GW::Constants::MapID::Domain_of_Anguish:
					map_string = "DoA";
					break;
				case GW::Constants::MapID::Urgozs_Warren:
					map_string = "Urgoz";
					break;
				case GW::Constants::MapID::The_Deep:
					map_string = "Deep";
					break;
				case GW::Constants::MapID::The_Underworld:
					map_string = "UW";
					break;
				case GW::Constants::MapID::The_Fissure_of_Woe:
					map_string = "FoW";
					break;
				default:
					map_string = std::string("Map-") + std::to_string(static_cast<long>(current));
				}

				std::string prof_string = "";
				GW::Agent* me = GW::Agents().GetPlayer();
				if (me) {
					prof_string += " - ";
					prof_string += GW::Constants::GetProfessionAcronym(
						static_cast<GW::Constants::Profession>(me->Primary));
					prof_string += "-";
					prof_string += GW::Constants::GetProfessionAcronym(
						static_cast<GW::Constants::Profession>(me->Secondary));
				}

				SYSTEMTIME localtime;
				GetLocalTime(&localtime);
				std::string filename = std::to_string(localtime.wYear)
					+ "-" + std::to_string(localtime.wMonth)
					+ "-" + std::to_string(localtime.wDay)
					+ " - " + std::to_string(localtime.wHour)
					+ "-" + std::to_string(localtime.wMinute)
					+ "-" + std::to_string(localtime.wSecond)
					+ " - " + map_string + prof_string + ".log";

				if (location_file && location_file.is_open()) {
					location_file.close();
				}
				location_file.open(GuiUtils::getSubPath(filename, "location logs").c_str());
			}

			GW::Agent* me = GW::Agents().GetPlayer();
			if (location_file.is_open() && me != nullptr) {
				location_file << "Time=" << GW::Map().GetInstanceTime();
				location_file << " X=" << me->X;
				location_file << " Y=" << me->Y;
				location_file << "\n";
			}
		} else {
			location_current_map = GW::Constants::MapID::None;
			location_file.close();
		}
	}
}
