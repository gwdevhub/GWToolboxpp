#include "ToolboxSettings.h"

#include <fstream>

#include <GWCA\Managers\AgentMgr.h>
#include <GWCA\Managers\MapMgr.h>

#include <Defines.h>
#include "GuiUtils.h"
#include <GWToolbox.h>
#include <Modules\Resources.h>
#include <Modules\Updater.h>

#include <Windows\MainWindow.h>
#include <Windows\PconsWindow.h>
#include <Windows\HotkeysWindow.h>
#include <Windows\BuildsWindow.h>
#include <Windows\TravelWindow.h>
#include <Windows\DialogsWindow.h>
#include <Windows\InfoWindow.h>
#include <Windows\MaterialsWindow.h>
#include <Windows\SettingsWindow.h>
#include <Windows\NotePadWindow.h>

#include <Widgets\TimerWidget.h>
#include <Widgets\HealthWidget.h>
#include <Widgets\DistanceWidget.h>
#include <Widgets\Minimap\Minimap.h>
#include <Widgets\PartyDamage.h>
#include <Widgets\BondsWidget.h>
#include <Widgets\ClockWidget.h>
#include <Widgets\VanquishWidget.h>
#include <Widgets\AlcoholWidget.h>

bool ToolboxSettings::move_all = false;
bool ToolboxSettings::clamp_window_positions = false;

void ToolboxSettings::InitializeModules() {
	SettingsWindow::Instance().sep_windows = GWToolbox::Instance().GetModules().size();

	MainWindow::Instance().Initialize();
	if (use_pcons) PconsWindow::Instance().Initialize();
	if (use_hotkeys) HotkeysWindow::Instance().Initialize();
	if (use_builds) BuildsWindow::Instance().Initialize();
	if (use_travel) TravelWindow::Instance().Initialize();
	if (use_dialogs) DialogsWindow::Instance().Initialize();
	if (use_info) InfoWindow::Instance().Initialize();
	if (use_materials) MaterialsWindow::Instance().Initialize();
	if (use_notepad) NotePadWindow::Instance().Initialize();

	SettingsWindow::Instance().Initialize();

	SettingsWindow::Instance().sep_widgets = GWToolbox::Instance().GetModules().size();

	if (use_timer) TimerWidget::Instance().Initialize();
	if (use_health)  HealthWidget::Instance().Initialize();
	if (use_distance) DistanceWidget::Instance().Initialize();
	if (use_minimap) Minimap::Instance().Initialize();
	if (use_damage) PartyDamage::Instance().Initialize();
	if (use_bonds) BondsWidget::Instance().Initialize();
	if (use_clock) ClockWidget::Instance().Initialize();
	if (use_vanquish) VanquishWidget::Instance().Initialize();
	if (use_alcohol) AlcoholWidget::Instance().Initialize();
}

void ToolboxSettings::DrawSettingInternal() {
	Updater::Instance().DrawSettingInternal();

	ImGui::Separator();
	DrawFreezeSetting();
	ImGui::Checkbox("Save Location Data", &save_location_data);
	ImGui::ShowHelp("Toolbox will save your location every second in a file in Settings Folder.");
	ImGui::Checkbox("Keep windows on screen", &clamp_window_positions);
	ImGui::ShowHelp("Windows will not move off-screen.\n"
		"This might also move windows on bottom and right side\n"
		"of the screen slightly towards the center during rezone.\n"
		"Might also cause windows to move when minimizing.");

	ImGui::Separator();
	ImGui::Text("Enable the following features:");
	ImGui::TextDisabled("Unticking will completely disable a feature from initializing and running. Requires Toolbox restart.");
	ImGui::Text("Windows:");
	ImGui::Checkbox("Pcons", &use_pcons);
	ImGui::Checkbox("Hotkeys", &use_hotkeys);
	ImGui::Checkbox("Builds", &use_builds);
	ImGui::Checkbox("Travel", &use_travel);
	ImGui::Checkbox("Dialogs", &use_dialogs);
	ImGui::Checkbox("Info", &use_info);
	ImGui::Checkbox("Materials", &use_materials);
	ImGui::Checkbox("Notepad", &use_notepad);
	ImGui::Text("Widgets:");
	ImGui::Checkbox("Timer", &use_timer);
	ImGui::Checkbox("Health", &use_health);
	ImGui::Checkbox("Distance", &use_distance);
	ImGui::Checkbox("Minimap", &use_minimap);
	ImGui::Checkbox("Damage", &use_damage);
	ImGui::Checkbox("Bonds", &use_bonds);
	ImGui::Checkbox("Clock", &use_clock);
	ImGui::Checkbox("Vanquish counter", &use_vanquish);
	ImGui::Checkbox("Alcohol monitor", &use_alcohol);
}

void ToolboxSettings::DrawFreezeSetting() {
	ImGui::Checkbox("Unlock Move All", &move_all);
	ImGui::ShowHelp("Will allow movement and resize of all widgets and windows");
}

void ToolboxSettings::LoadSettings(CSimpleIni* ini) {
	ToolboxModule::LoadSettings(ini);
	move_all = false;
	clamp_window_positions = ini->GetBoolValue(Name(), "clamp_window_positions", false);
	use_pcons = ini->GetBoolValue(Name(), "use_pcons", true);
	use_hotkeys = ini->GetBoolValue(Name(), "use_hotkeys", true);
	use_builds = ini->GetBoolValue(Name(), "use_builds", true);
	use_travel = ini->GetBoolValue(Name(), "use_travel", true);
	use_dialogs = ini->GetBoolValue(Name(), "use_dialogs", true);
	use_info = ini->GetBoolValue(Name(), "use_info", true);
	use_materials = ini->GetBoolValue(Name(), "use_materials", true);
	use_timer = ini->GetBoolValue(Name(), "use_timer", true);
	use_health = ini->GetBoolValue(Name(), "use_health", true);
	use_distance = ini->GetBoolValue(Name(), "use_distance", true);
	use_minimap = ini->GetBoolValue(Name(), "use_minimap", true);
	use_damage = ini->GetBoolValue(Name(), "use_damage", true);
	use_bonds = ini->GetBoolValue(Name(), "use_bonds", true);
	use_clock = ini->GetBoolValue(Name(), "use_clock", true);
	use_notepad = ini->GetBoolValue(Name(), "use_notepad", true);
	use_vanquish = ini->GetBoolValue(Name(), "use_vanquish", true);
	use_alcohol = ini->GetBoolValue(Name(), "use_alcohol", true);
}

void ToolboxSettings::SaveSettings(CSimpleIni* ini) {
	ToolboxModule::SaveSettings(ini);
	ini->SetBoolValue(Name(), "clamp_window_positions", clamp_window_positions);
	if (location_file.is_open()) location_file.close();
	ini->SetBoolValue(Name(), "use_pcons", use_pcons);
	ini->SetBoolValue(Name(), "use_hotkeys", use_hotkeys);
	ini->SetBoolValue(Name(), "use_builds", use_builds);
	ini->SetBoolValue(Name(), "use_travel", use_travel);
	ini->SetBoolValue(Name(), "use_dialogs", use_dialogs);
	ini->SetBoolValue(Name(), "use_info", use_info);
	ini->SetBoolValue(Name(), "use_materials", use_materials);
	ini->SetBoolValue(Name(), "use_timer", use_timer);
	ini->SetBoolValue(Name(), "use_health", use_health);
	ini->SetBoolValue(Name(), "use_distance", use_distance);
	ini->SetBoolValue(Name(), "use_minimap", use_minimap);
	ini->SetBoolValue(Name(), "use_damage", use_damage);
	ini->SetBoolValue(Name(), "use_bonds", use_bonds);
	ini->SetBoolValue(Name(), "use_clock", use_clock);
	ini->SetBoolValue(Name(), "use_notepad", use_notepad);
	ini->SetBoolValue(Name(), "use_vanquish", use_vanquish);
	ini->SetBoolValue(Name(), "use_alcohol", use_alcohol);
}

void ToolboxSettings::Update() {
	// save location data
	if (save_location_data && TIMER_DIFF(location_timer) > 1000) {
		location_timer = TIMER_INIT();
		if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable
			&& GW::Agents::GetPlayer() != nullptr
			&& GW::Map::GetInstanceTime() > 3000) {
			GW::Constants::MapID current = GW::Map::GetMapID();
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
				GW::Agent* me = GW::Agents::GetPlayer();
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
				location_file.open(Resources::GetPath(filename, "location logs").c_str());
			}

			GW::Agent* me = GW::Agents::GetPlayer();
			if (location_file.is_open() && me != nullptr) {
				location_file << "Time=" << GW::Map::GetInstanceTime();
				location_file << " X=" << me->pos.x;
				location_file << " Y=" << me->pos.y;
				location_file << "\n";
			}
		} else {
			location_current_map = GW::Constants::MapID::None;
			location_file.close();
		}
	}
}

