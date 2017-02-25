#include "ToolboxSettings.h"

#include <GWCA\GWCA.h>
#include <GWCA\Managers\AgentMgr.h>

#include "GuiUtils.h"

void ToolboxSettings::DrawSettings() {
	if (ImGui::CollapsingHeader(Name())) {
		ImGui::Checkbox("Freeze Widgets", &freeze_widgets);
		GuiUtils::ShowHelp("Widgets such as timer, health, minimap will not move");
		ImGui::Checkbox("Save Location Data", &save_location_data);
	}
}

void ToolboxSettings::LoadSettings(CSimpleIni* ini) {
	freeze_widgets = ini->GetBoolValue("main_window", "freeze_widgets", false);
}

void ToolboxSettings::SaveSettings(CSimpleIni* ini) {
	ini->SetBoolValue("main_window", "freeze_widgets", freeze_widgets);

	//if (location_file.is_open()) location_file.close();
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
