#include "SettingsPanel.h"

#include <Windows.h>

#include <GWCA\GWCA.h>

#include "Defines.h"

#include "Config.h"
#include "GuiUtils.h"

#include "GWToolbox.h"
#include "MainWindow.h"
#include "SettingManager.h"


SettingsPanel::SettingsPanel() {
	//SettingsPanel::SettingsPanel(OSHGui::Control* parent) : ToolboxPanel(parent),
	//	save_location_data(false, MainWindow::IniSection(), "save_location") {
	//
	//	save_location_data.SetText("Save location data");
	//	save_location_data.SetTooltip("When enabled, toolbox will save character location every second in a file in settings folder / location logs");
	//}
	save_location_data = false;
}


void SettingsPanel::Update() {
	// save location data
	if (save_location_data && TBTimer::diff(location_timer_) > 1000) {
		location_timer_ = TBTimer::init();
		if (GW::Map().GetInstanceType() == GW::Constants::InstanceType::Explorable
			&& GW::Agents().GetPlayer() != nullptr
			&& GW::Map().GetInstanceTime() > 3000) {
			GW::Constants::MapID current = GW::Map().GetMapID();
			if (location_current_map_ != current) {
				location_current_map_ = current;

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

				if (location_file_ && location_file_.is_open()) {
					location_file_.close();
				}
				location_file_.open(GuiUtils::getSubPath(filename, "location logs").c_str());
			}

			GW::Agent* me = GW::Agents().GetPlayer();
			if (location_file_.is_open() && me != nullptr) {
				location_file_ << "Time=" << GW::Map().GetInstanceTime();
				location_file_ << " X=" << me->X;
				location_file_ << " Y=" << me->Y;
				location_file_ << "\n";
			}
		} else {
			location_current_map_ = GW::Constants::MapID::None;
			location_file_.close();
		}
	}
}

void SettingsPanel::Draw(IDirect3DDevice9* pDevice) {
	//ImGui::SetNextWindowSize(ImVec2(280, 285));
	ImGui::Begin(Name());
	ImGui::Text("GWToolbox++ version");
	ImGui::SameLine();
	ImGui::Text(GWTOOLBOX_VERSION);

	ImGui::Text("by Has and KAOS");

	if (ImGui::Button("Open Settings Folder")) {
		CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
		ShellExecute(NULL, "open", GuiUtils::getSettingsFolder().c_str(), NULL, NULL, SW_SHOWNORMAL);
	}

	if (ImGui::Button("Open GWToolbox++ Website")) {
		CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
		ShellExecute(NULL, "open", GWTOOLBOX_WEBSITE, NULL, NULL, SW_SHOWNORMAL);
	}

	if (ImGui::CollapsingHeader("General")) {
		GWToolbox::instance().other_settings->DrawSettings();
	}
	if (ImGui::CollapsingHeader("Toolbox++ General")) {
		ImGui::Checkbox("Save Location Data", &save_location_data);
	}
	if (ImGui::CollapsingHeader("Main Window")) {
		GWToolbox::instance().main_window->DrawSettings();
	}
	if (ImGui::CollapsingHeader("Minimap")) {

	}
	if (ImGui::CollapsingHeader("Chat Filter")) {
		GWToolbox::instance().chat_filter->DrawSettings();
	}
	if (ImGui::CollapsingHeader("Theme")) {
		
	}

	ImGui::End();
}
