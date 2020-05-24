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

#include <Modules/Updater.h>
#include <Modules/Resources.h>
#include <Modules/ChatFilter.h>
#include <Modules/ChatCommands.h>
#include <Modules/GameSettings.h>
#include <Modules/DiscordModule.h>
#include <Modules/TwitchModule.h>
#include <Modules/PartyWindowModule.h>
#include <Modules/ZrawDeepModule.h>
#include <Modules/AprilFools.h>
#include <Modules/InventoryManager.h>

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
#include <Windows/FactionLeaderboardWindow.h>
#include <Windows/DailyQuests.h>
#include <Windows/PacketLoggerWindow.h>
#include <Windows/DoorMonitorWindow.h>
#include <Windows/SkillListingWindow.h>
#include <Windows/StringDecoderWindow.h>
#include <Windows/FriendListWindow.h>

#include <Widgets/TimerWidget.h>
#include <Widgets/HealthWidget.h>
#include <Widgets/DistanceWidget.h>
#include <Widgets/Minimap/Minimap.h>
#include <Widgets/PartyDamage.h>
#include <Widgets/BondsWidget.h>
#include <Widgets/ClockWidget.h>
#include <Widgets/VanquishWidget.h>
#include <Widgets/AlcoholWidget.h>
#include <Widgets/ServerInfoWidget.h>


#include "ToolboxSettings.h"
//#define _FUN


bool ToolboxSettings::move_all = false;

void ToolboxSettings::LoadModules(CSimpleIni* ini) {
	SettingsWindow::Instance().sep_modules = optional_modules.size();
	optional_modules.push_back(&Updater::Instance());


	if (use_chatcommand) optional_modules.push_back(&ChatCommands::Instance());
	if (use_chatfilter) optional_modules.push_back(&ChatFilter::Instance());
	optional_modules.push_back(&GameSettings::Instance());
	optional_modules.push_back(&InventoryManager::Instance());
	if (use_partywindowmodule) optional_modules.push_back(&PartyWindowModule::Instance());
	optional_modules.push_back(&ZrawDeepModule::Instance());

	if (use_discord) optional_modules.push_back(&DiscordModule::Instance());
	if (use_twitch) optional_modules.push_back(&TwitchModule::Instance());

	SettingsWindow::Instance().sep_windows = optional_modules.size();
	if (use_pcons) optional_modules.push_back(&PconsWindow::Instance());
	if (use_hotkeys) optional_modules.push_back(&HotkeysWindow::Instance());
	if (use_builds) optional_modules.push_back(&BuildsWindow::Instance());
	if (use_herobuilds) optional_modules.push_back(&HeroBuildsWindow::Instance());
	if (use_travel) optional_modules.push_back(&TravelWindow::Instance());
	if (use_dialogs) optional_modules.push_back(&DialogsWindow::Instance());
	if (use_info) optional_modules.push_back(&InfoWindow::Instance());
	if (use_materials) optional_modules.push_back(&MaterialsWindow::Instance());
	if (use_trade) optional_modules.push_back(&TradeWindow::Instance());
	if (use_notepad) optional_modules.push_back(&NotePadWindow::Instance());
	if (use_objectivetimer) optional_modules.push_back(&ObjectiveTimerWindow::Instance());
	if (use_factionleaderboard) optional_modules.push_back(&FactionLeaderboardWindow::Instance());
	if (use_daily_quests) optional_modules.push_back(&DailyQuests::Instance());
	if (use_friendlist) optional_modules.push_back(&FriendListWindow::Instance());
#ifdef _DEBUG
	optional_modules.push_back(&PacketLoggerWindow::Instance());
	optional_modules.push_back(&StringDecoderWindow::Instance());
	optional_modules.push_back(&DoorMonitorWindow::Instance());
	optional_modules.push_back(&SkillListingWindow::Instance());
#endif
	std::sort(
        optional_modules.begin() + static_cast<int>(SettingsWindow::Instance().sep_windows),
        optional_modules.end(),
        [](const ToolboxModule* lhs, const ToolboxModule* rhs) {
		    return std::string(lhs->SettingsName()).compare(rhs->SettingsName()) < 0u;
		});

	SettingsWindow::Instance().sep_widgets = optional_modules.size();
	optional_modules.push_back(&SettingsWindow::Instance());
	if (use_timer) optional_modules.push_back(&TimerWidget::Instance());
	if (use_health) optional_modules.push_back(&HealthWidget::Instance());
	if (use_distance) optional_modules.push_back(&DistanceWidget::Instance());
	if (use_minimap) optional_modules.push_back(&Minimap::Instance());
	if (use_damage) optional_modules.push_back(&PartyDamage::Instance());
	if (use_bonds) optional_modules.push_back(&BondsWidget::Instance());
	if (use_clock) optional_modules.push_back(&ClockWidget::Instance());
	if (use_vanquish) optional_modules.push_back(&VanquishWidget::Instance());
	if (use_alcohol) optional_modules.push_back(&AlcoholWidget::Instance());

	std::sort(
        optional_modules.begin() + static_cast<int>(SettingsWindow::Instance().sep_widgets),
        optional_modules.end(),
        [](const ToolboxModule* lhs, const ToolboxModule* rhs) {
		    return std::string(lhs->SettingsName()).compare(rhs->SettingsName()) < 0u;
		});

	// Only read settings of non-core modules
	for (ToolboxModule* module : optional_modules) {
        module->Initialize();
        module->LoadSettings(ini);
	}
	AprilFools::Instance().Initialize();
	AprilFools::Instance().LoadSettings(ini);
}

void ToolboxSettings::DrawSettingInternal() {
	Updater::Instance().DrawSettingInternal();

	ImGui::Separator();
	DrawFreezeSetting();
	ImGui::Checkbox("Save Location Data", &save_location_data);
	ImGui::ShowHelp("Toolbox will save your location every second in a file in Settings Folder.");
	
	const size_t cols = (size_t)floor(ImGui::GetWindowWidth() / (170.0f * ImGui::GetIO().FontGlobalScale));

	ImGui::Separator();
	ImGui::PushID("global_enable");
	ImGui::Text("Enable the following features:");
	ImGui::TextDisabled("Unticking will completely disable a feature from initializing and running. Requires Toolbox restart.");
	static std::vector<std::pair<const char*, bool*>> features{
		{"Alcohol",&use_alcohol},
		{"Bonds",&use_bonds},
		{"Builds",&use_builds},
		{"Clock",&use_clock},
		{"Daily Quests",&use_daily_quests},
		{"Damage",&use_damage},
		{"Dialogs",&use_dialogs},
		{"Discord",&use_discord},
		{"Distance",&use_distance},
		{"Health",&use_health},
		{"Hotkeys",&use_hotkeys},
		{"Friend List",&use_friendlist},
		{"Hero Builds",&use_herobuilds},
		{"Info",&use_info},
		{"Materials",&use_materials},
		{"Minimap",&use_minimap},
		{"Notepad",&use_notepad},
		{"Objective Timer",&use_objectivetimer},
		{"Party Window",&use_partywindowmodule},
		{"Pcons",&use_pcons},
		{"Timer",&use_timer},
		{"Trade",&use_trade},
		{"Travel",&use_travel},
		{"Twitch",&use_twitch},
		{"Vanquish counter",&use_vanquish}
	};
	ImGui::Columns(static_cast<int>(cols), "global_enable_cols", false);
	size_t items_per_col = (size_t)ceil(features.size() / cols);
	size_t col_count = 0;
	for (auto feature : features) {
		ImGui::Checkbox(feature.first, feature.second);
		col_count++;
		if (col_count > items_per_col) {
			ImGui::NextColumn();
			col_count = 0;
		}
	}
	ImGui::Columns(1);
	ImGui::PopID();
	ImGui::PushID("menubuttons");
	ImGui::Separator();
	ImGui::Text("Show the following in the main window:");

	std::vector<ToolboxUIElement *> ui = GWToolbox::Instance().GetUIElements();
	ImGui::Columns(static_cast<int>(cols), "menubuttons_cols", false);
	col_count = 0;
	std::vector<ToolboxUIElement*> valid_elements;
	for (size_t i = 0; i < ui.size();i++) {
		auto window = ui[i];
		if (window == &Updater::Instance()) continue;
		if (window == &MainWindow::Instance()) continue;
		if (!(window->IsWidget() || window->IsWindow())) 
			continue;
		valid_elements.push_back(window);
	}
	std::sort(valid_elements.begin(), valid_elements.end(), [](const ToolboxModule* lhs, const ToolboxModule* rhs) {
		return std::string(lhs->Name()).compare(rhs->Name()) < 0;
		});
	items_per_col = (size_t)ceil(valid_elements.size() / cols);
	for (size_t i = 0; i < valid_elements.size(); i++) {
		auto window = valid_elements[i];
		if (ImGui::Checkbox(window->Name(), &window->show_menubutton))
			MainWindow::Instance().pending_refresh_buttons = true;
		col_count++;
		if (col_count > items_per_col) {
			ImGui::NextColumn();
			col_count = 0;
		}
	}
	ImGui::Columns(1);
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
    use_objectivetimer = ini->GetBoolValue(Name(), VAR_NAME(use_objectivetimer), true);
	save_location_data = ini->GetBoolValue(Name(), VAR_NAME(save_location_data), false);
	use_gamesettings = ini->GetBoolValue(Name(), VAR_NAME(use_gamesettings), true);
	use_updater = ini->GetBoolValue(Name(), VAR_NAME(use_updater), true);
	use_chatfilter = ini->GetBoolValue(Name(), VAR_NAME(use_chatfilter), true);
	use_chatcommand = ini->GetBoolValue(Name(), VAR_NAME(use_chatcommand), true);
    use_discord = ini->GetBoolValue(Name(), VAR_NAME(use_discord), true);
	use_factionleaderboard = ini->GetBoolValue(Name(), VAR_NAME(use_factionleaderboard), use_factionleaderboard);
	use_twitch = ini->GetBoolValue(Name(), VAR_NAME(use_twitch), use_twitch);
	use_partywindowmodule = ini->GetBoolValue(Name(), VAR_NAME(use_partywindowmodule), use_partywindowmodule);
	use_friendlist = ini->GetBoolValue(Name(), VAR_NAME(use_friendlist), use_friendlist);
	use_serverinfo = ini->GetBoolValue(Name(), VAR_NAME(use_serverinfo), use_serverinfo);
	use_daily_quests = ini->GetBoolValue(Name(), VAR_NAME(use_daily_quests), use_daily_quests);
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
	ini->SetBoolValue(Name(), VAR_NAME(use_factionleaderboard), use_factionleaderboard);
    ini->SetBoolValue(Name(), VAR_NAME(use_discord), use_discord);
    ini->SetBoolValue(Name(), VAR_NAME(use_twitch), use_twitch);
	ini->SetBoolValue(Name(), VAR_NAME(use_partywindowmodule), use_partywindowmodule);
	ini->SetBoolValue(Name(), VAR_NAME(use_friendlist), use_friendlist);
	ini->SetBoolValue(Name(), VAR_NAME(use_serverinfo), use_serverinfo);
	ini->SetBoolValue(Name(), VAR_NAME(save_location_data), save_location_data);
	ini->SetBoolValue(Name(), VAR_NAME(use_gamesettings), use_gamesettings);
	ini->SetBoolValue(Name(), VAR_NAME(use_updater), use_updater);
	ini->SetBoolValue(Name(), VAR_NAME(use_chatfilter), use_chatfilter);
	ini->SetBoolValue(Name(), VAR_NAME(use_chatcommand), use_chatcommand);
	ini->SetBoolValue(Name(), VAR_NAME(use_daily_quests), use_daily_quests);
}

void ToolboxSettings::Update(float delta) {
    UNREFERENCED_PARAMETER(delta);
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
				GW::AgentLiving* me = GW::Agents::GetPlayerAsAgentLiving();
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

