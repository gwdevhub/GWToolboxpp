#include "stdafx.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/Packets/StoC.h>

#include <GWCA/GameEntities/Party.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/AgentMgr.h>

#include <Defines.h>
#include "GuiUtils.h"
#include <GWToolbox.h>
#include <Modules/Resources.h>
#include <Modules/Updater.h>

#include <Windows/MainWindow.h>
#include <Windows/PconsWindow.h>
#include <Windows/HotkeysWindow.h>
#include <Windows/BuildsWindow.h>
#include <Windows/HeroBuildsWindow.h>
#include <Windows/TravelWindow.h>
#include <Windows/DialogsWindow.h>
#include <Windows/InfoWindow.h>
#include <Windows/MaterialsWindow.h>
#include <Windows/SettingsWindow.h>
#include <Windows/NotePadWindow.h>
#include <Windows/TradeWindow.h>
#include <Windows/ObjectiveTimerWindow.h>

#include <Widgets/TimerWidget.h>
#include <Widgets/HealthWidget.h>
#include <Widgets/DistanceWidget.h>
#include <Widgets/Minimap/Minimap.h>
#include <Widgets/PartyDamage.h>
#include <Widgets/BondsWidget.h>
#include <Widgets/ClockWidget.h>
#include <Widgets/VanquishWidget.h>
#include <Widgets/AlcoholWidget.h>

#include "ToolboxSettings.h"

bool ToolboxSettings::move_all = false;

void ToolboxSettings::InitializeModules() {
	SettingsWindow::Instance().sep_windows = GWToolbox::Instance().GetModules().size();

	MainWindow::Instance().Initialize();
	if (use_pcons) PconsWindow::Instance().Initialize();
	if (use_hotkeys) HotkeysWindow::Instance().Initialize();
	if (use_builds) BuildsWindow::Instance().Initialize();
	if (use_herobuilds) HeroBuildsWindow::Instance().Initialize();
	if (use_travel) TravelWindow::Instance().Initialize();
	if (use_dialogs) DialogsWindow::Instance().Initialize();
	if (use_info) InfoWindow::Instance().Initialize();
	if (use_materials) MaterialsWindow::Instance().Initialize();
	if (use_trade) TradeWindow::Instance().Initialize();
	if (use_notepad) NotePadWindow::Instance().Initialize();
	if (use_objectivetimer) ObjectiveTimerWindow::Instance().Initialize();

	SettingsWindow::Instance().Initialize();

	SettingsWindow::Instance().sep_widgets = GWToolbox::Instance().GetModules().size();

	if (use_timer) TimerWidget::Instance().Initialize();
	if (use_health) HealthWidget::Instance().Initialize();
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

	ImGui::Separator();
	ImGui::PushID("global_enable");
	ImGui::Text("Enable the following features:");
	ImGui::TextDisabled("Unticking will completely disable a feature from initializing and running. Requires Toolbox restart.");
	ImGui::Checkbox("Pcons", &use_pcons);
	ImGui::SameLine(ImGui::GetWindowWidth() / 2);
	ImGui::Checkbox("Hotkeys", &use_hotkeys);
	ImGui::Checkbox("Builds", &use_builds);
	ImGui::SameLine(ImGui::GetWindowWidth() / 2);
	ImGui::Checkbox("Hero Builds", &use_herobuilds);
	ImGui::Checkbox("Travel", &use_travel);
	ImGui::SameLine(ImGui::GetWindowWidth() / 2);
	ImGui::Checkbox("Dialogs", &use_dialogs);
	ImGui::Checkbox("Info", &use_info);
	ImGui::SameLine(ImGui::GetWindowWidth() / 2);
	ImGui::Checkbox("Materials", &use_materials);
	ImGui::Checkbox("Notepad", &use_notepad);
	ImGui::SameLine(ImGui::GetWindowWidth() / 2);
	ImGui::Checkbox("Objective Timer", &use_objectivetimer);
	ImGui::Checkbox("Timer", &use_timer);
	ImGui::Checkbox("Health", &use_health);
	ImGui::SameLine(ImGui::GetWindowWidth() / 2);
	ImGui::Checkbox("Distance", &use_distance);
	ImGui::Checkbox("Minimap", &use_minimap);
	ImGui::SameLine(ImGui::GetWindowWidth() / 2);
	ImGui::Checkbox("Damage", &use_damage);
	ImGui::Checkbox("Bonds", &use_bonds);
	ImGui::SameLine(ImGui::GetWindowWidth() / 2);
	ImGui::Checkbox("Clock", &use_clock);
	ImGui::Checkbox("Vanquish counter", &use_vanquish);
	ImGui::SameLine(ImGui::GetWindowWidth() / 2);
	ImGui::Checkbox("Alcohol", &use_alcohol);
	ImGui::Checkbox("Trade", &use_trade);
	ImGui::PopID();

	ImGui::PushID("menubuttons");
	ImGui::Separator();
	ImGui::Text("Show the following in the main window:");
	bool odd = true;
	auto ui = GWToolbox::Instance().GetUIElements();
	for (unsigned int i = 0; i < ui.size(); ++i) {
		auto window = ui[i];
		if (window == &Updater::Instance()) continue;
		if (window == &MainWindow::Instance()) continue;

		ImGui::Checkbox(window ->Name(), &window->show_menubutton);

		if (i < ui.size() - 1) {
			if (odd) ImGui::SameLine(ImGui::GetWindowWidth() / 2);
			odd = !odd; // cannot use i%2 because we skip some elements
		}
	}
	ImGui::PopID();
}

void ToolboxSettings::DrawFreezeSetting() {
    ImGui::Checkbox("Unlock Move All", &move_all);
    ImGui::ShowHelp("Will allow movement and resize of all widgets and windows");
}

void ToolboxSettings::LoadSettings(CSimpleIni* ini) {
	ToolboxModule::LoadSettings(ini);
	move_all = false;
	use_pcons = ini->GetBoolValue(Name(), VAR_NAME(use_pcons), true);
	use_hotkeys = ini->GetBoolValue(Name(), VAR_NAME(use_hotkeys), true);
	use_builds = ini->GetBoolValue(Name(), VAR_NAME(use_builds), true);
	use_herobuilds = ini->GetBoolValue(Name(), VAR_NAME(use_herobuilds), true);
	use_travel = ini->GetBoolValue(Name(), VAR_NAME(use_travel), true);
	use_dialogs = ini->GetBoolValue(Name(), VAR_NAME(use_dialogs), true);
	use_info = ini->GetBoolValue(Name(), VAR_NAME(use_info), true);
	use_materials = ini->GetBoolValue(Name(), VAR_NAME(use_materials), true);
	use_timer = ini->GetBoolValue(Name(), VAR_NAME(use_timer), true);
	use_health = ini->GetBoolValue(Name(), VAR_NAME(use_health), true);
	use_distance = ini->GetBoolValue(Name(), VAR_NAME(use_distance), true);
	use_minimap = ini->GetBoolValue(Name(), VAR_NAME(use_minimap), true);
	use_damage = ini->GetBoolValue(Name(), VAR_NAME(use_damage), true);
	use_bonds = ini->GetBoolValue(Name(), VAR_NAME(use_bonds), true);
	use_clock = ini->GetBoolValue(Name(), VAR_NAME(use_clock), true);
	use_notepad = ini->GetBoolValue(Name(), VAR_NAME(use_notepad), true);
	use_vanquish = ini->GetBoolValue(Name(), VAR_NAME(use_vanquish), true);
	use_alcohol = ini->GetBoolValue(Name(), VAR_NAME(use_alcohol), true);
	use_trade = ini->GetBoolValue(Name(), VAR_NAME(use_trade), true);
    use_objectivetimer = ini->GetBoolValue(Name(), VAR_NAME(use_instancetimer), true);
	save_location_data = ini->GetBoolValue(Name(), VAR_NAME(save_location_data), false);
}

void ToolboxSettings::SaveSettings(CSimpleIni* ini) {
	ToolboxModule::SaveSettings(ini);
	if (location_file.is_open()) location_file.close();
	ini->SetBoolValue(Name(), VAR_NAME(use_pcons), use_pcons);
	ini->SetBoolValue(Name(), VAR_NAME(use_hotkeys), use_hotkeys);
	ini->SetBoolValue(Name(), VAR_NAME(use_builds), use_builds);
	ini->SetBoolValue(Name(), VAR_NAME(use_herobuilds), use_herobuilds);
	ini->SetBoolValue(Name(), VAR_NAME(use_travel), use_travel);
	ini->SetBoolValue(Name(), VAR_NAME(use_dialogs), use_dialogs);
	ini->SetBoolValue(Name(), VAR_NAME(use_info), use_info);
	ini->SetBoolValue(Name(), VAR_NAME(use_materials), use_materials);
	ini->SetBoolValue(Name(), VAR_NAME(use_timer), use_timer);
	ini->SetBoolValue(Name(), VAR_NAME(use_health), use_health);
	ini->SetBoolValue(Name(), VAR_NAME(use_distance), use_distance);
	ini->SetBoolValue(Name(), VAR_NAME(use_minimap), use_minimap);
	ini->SetBoolValue(Name(), VAR_NAME(use_damage), use_damage);
	ini->SetBoolValue(Name(), VAR_NAME(use_bonds), use_bonds);
	ini->SetBoolValue(Name(), VAR_NAME(use_clock), use_clock);
	ini->SetBoolValue(Name(), VAR_NAME(use_notepad), use_notepad);
	ini->SetBoolValue(Name(), VAR_NAME(use_vanquish), use_vanquish);
	ini->SetBoolValue(Name(), VAR_NAME(use_alcohol), use_alcohol);
	ini->SetBoolValue(Name(), VAR_NAME(use_trade), use_trade);
	ini->SetBoolValue(Name(), VAR_NAME(use_objectivetimer), use_objectivetimer);
	ini->SetBoolValue(Name(), VAR_NAME(save_location_data), save_location_data);
}

void ToolboxSettings::Update(float delta) {
    ImGui::GetStyle().WindowBorderSize = (move_all ? 1.0f : 0.0f);

	// save location data
	if (save_location_data && TIMER_DIFF(location_timer) > 1000) {
		location_timer = TIMER_INIT();
		if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable
			&& GW::Agents::GetPlayer() != nullptr
			&& GW::Map::GetInstanceTime() > 3000) {
			GW::Constants::MapID current = GW::Map::GetMapID();
			if (location_current_map != current) {
				location_current_map = current;

				std::wstring map_string;
				switch (current) {
				case GW::Constants::MapID::Domain_of_Anguish:
					map_string = L"DoA";
					break;
				case GW::Constants::MapID::Urgozs_Warren:
					map_string = L"Urgoz";
					break;
				case GW::Constants::MapID::The_Deep:
					map_string = L"Deep";
					break;
				case GW::Constants::MapID::The_Underworld:
					map_string = L"UW";
					break;
				case GW::Constants::MapID::The_Fissure_of_Woe:
					map_string = L"FoW";
					break;
				default:
					map_string = std::wstring(L"Map-") + std::to_wstring(static_cast<long>(current));
				}

				std::wstring prof_string = L"";
				GW::Agent* me = GW::Agents::GetPlayer();
				if (me) {
					prof_string += L" - ";
					prof_string += GW::Constants::GetWProfessionAcronym(
						static_cast<GW::Constants::Profession>(me->primary));
					prof_string += L"-";
					prof_string += GW::Constants::GetWProfessionAcronym(
						static_cast<GW::Constants::Profession>(me->secondary));
				}

				SYSTEMTIME localtime;
				GetLocalTime(&localtime);
				std::wstring filename = std::to_wstring(localtime.wYear)
					+ L"-" + std::to_wstring(localtime.wMonth)
					+ L"-" + std::to_wstring(localtime.wDay)
					+ L" - " + std::to_wstring(localtime.wHour)
					+ L"-" + std::to_wstring(localtime.wMinute)
					+ L"-" + std::to_wstring(localtime.wSecond)
					+ L" - " + map_string + prof_string + L".log";

				if (location_file && location_file.is_open()) {
					location_file.close();
				}
				const std::wstring path = Resources::GetPath(L"location logs", filename);
				location_file.open(path);
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

