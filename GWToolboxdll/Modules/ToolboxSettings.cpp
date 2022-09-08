#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/AgentMgr.h>

#include <Defines.h>
#include <Utils/GuiUtils.h>
#include <GWToolbox.h>

#include <Modules/Updater.h>
#include <Modules/Resources.h>
#include <Modules/ChatFilter.h>
#include <Modules/ItemFilter.h>
#include <Modules/ChatCommands.h>
#include <Modules/GameSettings.h>
#include <Modules/DiscordModule.h>
#include <Modules/TwitchModule.h>
#include <Modules/PartyWindowModule.h>
#include <Modules/ZrawDeepModule.h>
#include <Modules/AprilFools.h>
#include <Modules/InventoryManager.h>
#include <Modules/TeamspeakModule.h>
#include <Modules/ObserverModule.h>
#include <Modules/Obfuscator.h>
#include <Modules/ChatLog.h>
#include <Modules/HintsModule.h>
#if 0
#include <Modules/GWFileRequester.h>
#endif
#include <Modules/HallOfMonumentsModule.h>

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
#include <Windows/PartyStatisticsWindow.h>
#include <Windows/TradeWindow.h>
#include <Windows/ObjectiveTimerWindow.h>
#include <Windows/FactionLeaderboardWindow.h>
#include <Windows/DailyQuestsWindow.h>
#include <Windows/FriendListWindow.h>
#include <Windows/ObserverPlayerWindow.h>
#include <Windows/ObserverTargetWindow.h>
#include <Windows/ObserverPartyWindow.h>
#include <Windows/ObserverExportWindow.h>
#include <Windows/CompletionWindow.h>
#ifdef _DEBUG
#include <Windows/PacketLoggerWindow.h>
#include <Windows/DoorMonitorWindow.h>
#if 0
#include <Windows/PartySearchWindow.h>
#endif
#include <Windows/StringDecoderWindow.h>
#include <Windows/SkillListingWindow.h>
#endif
#include <Windows/RerollWindow.h>


#include <Widgets/TimerWidget.h>
#include <Widgets/HealthWidget.h>
#include <Widgets/DistanceWidget.h>
#include <Widgets/Minimap/Minimap.h>
#include <Widgets/PartyDamage.h>
#include <Widgets/BondsWidget.h>
#include <Widgets/ClockWidget.h>
#include <Widgets/VanquishWidget.h>
#include <Widgets/AlcoholWidget.h>
#include <Widgets/SkillbarWidget.h>
#include <Widgets/SkillMonitorWidget.h>
#include <Widgets/WorldMapWidget.h>
#include <Widgets/EffectsMonitorWidget.h>
#include <Widgets/LatencyWidget.h>
#include "ToolboxSettings.h"

bool ToolboxSettings::move_all = false;

void ToolboxSettings::LoadModules(CSimpleIni* ini) {
    SettingsWindow::Instance().sep_modules = optional_modules.size();
    optional_modules.push_back(&Updater::Instance());

#if 0
    optional_modules.push_back(&GWFileRequester::Instance());
#endif

    if (use_chatcommand) optional_modules.push_back(&ChatCommands::Instance());
    if (use_chatfilter) optional_modules.push_back(&ChatFilter::Instance());
    if (use_itemfilter) optional_modules.push_back(&ItemFilter::Instance());
    optional_modules.push_back(&GameSettings::Instance());
    optional_modules.push_back(&InventoryManager::Instance());
    if (use_partywindowmodule) optional_modules.push_back(&PartyWindowModule::Instance());
    optional_modules.push_back(&ZrawDeepModule::Instance());

    if (use_discord) optional_modules.push_back(&DiscordModule::Instance());
    if (use_twitch) optional_modules.push_back(&TwitchModule::Instance());
    if (use_teamspeak) optional_modules.push_back(&TeamspeakModule::Instance());
    if (use_observer) optional_modules.push_back(&ObserverModule::Instance());
    optional_modules.push_back(&ChatLog::Instance());
    optional_modules.push_back(&HintsModule::Instance());
    optional_modules.push_back(&HallOfMonumentsModule::Instance());

    SettingsWindow::Instance().sep_windows = optional_modules.size();
    optional_modules.push_back(&SettingsWindow::Instance());
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
    if (use_observer_player_window) optional_modules.push_back(&ObserverPlayerWindow::Instance());
    if (use_observer_target_window) optional_modules.push_back(&ObserverTargetWindow::Instance());
    if (use_observer_party_window) optional_modules.push_back(&ObserverPartyWindow::Instance());
    if (use_observer_export_window) optional_modules.push_back(&ObserverExportWindow::Instance());
    if (use_obfuscator) optional_modules.push_back(&Obfuscator::Instance());
    if (use_completion_window) optional_modules.push_back(&CompletionWindow::Instance());
    if (use_reroll_window) optional_modules.push_back(&RerollWindow::Instance());
    if (use_party_statistics) optional_modules.push_back(&PartyStatisticsWindow::Instance());

#ifdef _DEBUG
#if 0
    optional_modules.push_back(&PartySearchWindow::Instance());
#endif
    optional_modules.push_back(&PacketLoggerWindow::Instance());
    optional_modules.push_back(&StringDecoderWindow::Instance());
    optional_modules.push_back(&DoorMonitorWindow::Instance());
    optional_modules.push_back(&SkillListingWindow::Instance());
#endif
    std::sort(
        optional_modules.begin() + static_cast<int>(SettingsWindow::Instance().sep_windows),
        optional_modules.end(),
        [](const ToolboxModule* lhs, const ToolboxModule* rhs) {
            return std::string(lhs->SettingsName()).compare(rhs->SettingsName()) < 0;
        });

    SettingsWindow::Instance().sep_widgets = optional_modules.size();
    if (use_timer) optional_modules.push_back(&TimerWidget::Instance());
    if (use_health) optional_modules.push_back(&HealthWidget::Instance());
    if (use_skillbar) optional_modules.push_back(&SkillbarWidget::Instance());
    if (use_distance) optional_modules.push_back(&DistanceWidget::Instance());
    if (use_minimap) optional_modules.push_back(&Minimap::Instance());
    if (use_damage) optional_modules.push_back(&PartyDamage::Instance());
    if (use_bonds) optional_modules.push_back(&BondsWidget::Instance());
    if (use_clock) optional_modules.push_back(&ClockWidget::Instance());
    if (use_vanquish) optional_modules.push_back(&VanquishWidget::Instance());
    if (use_alcohol) optional_modules.push_back(&AlcoholWidget::Instance());
    if (use_world_map) optional_modules.push_back(&WorldMapWidget::Instance());
    if (use_effect_monitor) optional_modules.push_back(&EffectsMonitorWidget::Instance());
    if (use_latency_widget) optional_modules.push_back(&LatencyWidget::Instance());
    if (use_skill_monitor) optional_modules.push_back(&SkillMonitorWidget::Instance());
#if _DEBUG

#endif

    std::sort(
        optional_modules.begin() + static_cast<int>(SettingsWindow::Instance().sep_widgets),
        optional_modules.end(),
        [](const ToolboxModule* lhs, const ToolboxModule* rhs) {
            return std::string(lhs->SettingsName()).compare(rhs->SettingsName()) < 0;
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
    DrawFreezeSetting();
    ImGui::Separator();

    Updater::Instance().DrawSettingInternal();
    ImGui::Separator();

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
        {"Chatfilter",&use_chatfilter},
        {"Clock",&use_clock},
        {"Completion",&use_completion_window},
        {"Daily Quests",&use_daily_quests},
        {"Damage",&use_damage},
        {"Dialogs",&use_dialogs},
        {"Discord",&use_discord},
        {"Distance",&use_distance},
        {"Effect Monitor",&use_effect_monitor},
        {"Health",&use_health},
        {"Hotkeys",&use_hotkeys},
        {"Friend List",&use_friendlist},
        {"Hero Builds",&use_herobuilds},
        {"Info",&use_info},
        {"Itemfilter",&use_itemfilter},
        {"Latency Widget", &use_latency_widget},
        {"Materials",&use_materials},
        {"Minimap",&use_minimap},
        {"Notepad",&use_notepad},
        {"Objective Timer",&use_objectivetimer},
        {"Obfuscator",&use_obfuscator},
        {"Party Window",&use_partywindowmodule},
        {"Pcons",&use_pcons},
        {"Reroll",&use_reroll_window},
        {"Skill Monitor",&use_skill_monitor},
        {"Timer",&use_timer},
        {"Trade",&use_trade},
        {"Travel",&use_travel},
        {"Teamspeak",&use_teamspeak},
        {"Twitch",&use_twitch},
        {"Observer",&use_observer},
        {"Observer Player Window",&use_observer_player_window},
        {"Observer Target Window",&use_observer_target_window},
        {"Observer Party Window",&use_observer_party_window},
        {"Observer Export Window",&use_observer_export_window},
        {"Party Statistics",&use_party_statistics},
        {"Vanquish counter",&use_vanquish},
        {"World Map",&use_world_map},
    };
    ImGui::Columns(static_cast<int>(cols), "global_enable_cols", false);
    size_t items_per_col = (size_t)ceil(features.size() / static_cast<float>(cols));
    size_t col_count = 0;
    for (const auto& feature : features) {
        ImGui::Checkbox(feature.first, feature.second);
        col_count++;
        if (col_count == items_per_col) {
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
    use_pcons = ini->GetBoolValue(Name(), VAR_NAME(use_pcons), use_pcons);
    use_hotkeys = ini->GetBoolValue(Name(), VAR_NAME(use_hotkeys), use_hotkeys);
    use_builds = ini->GetBoolValue(Name(), VAR_NAME(use_builds), use_builds);
    use_herobuilds = ini->GetBoolValue(Name(), VAR_NAME(use_herobuilds), use_herobuilds);
    use_travel = ini->GetBoolValue(Name(), VAR_NAME(use_travel), use_travel);
    use_dialogs = ini->GetBoolValue(Name(), VAR_NAME(use_dialogs), use_dialogs);
    use_info = ini->GetBoolValue(Name(), VAR_NAME(use_info), use_info);
    use_materials = ini->GetBoolValue(Name(), VAR_NAME(use_materials), use_materials);
    use_timer = ini->GetBoolValue(Name(), VAR_NAME(use_timer), use_timer);
    use_health = ini->GetBoolValue(Name(), VAR_NAME(use_health), use_health);
    use_distance = ini->GetBoolValue(Name(), VAR_NAME(use_distance), use_distance);
    use_minimap = ini->GetBoolValue(Name(), VAR_NAME(use_minimap), use_minimap);
    use_damage = ini->GetBoolValue(Name(), VAR_NAME(use_damage), use_damage);
    use_bonds = ini->GetBoolValue(Name(), VAR_NAME(use_bonds), use_bonds);
    use_clock = ini->GetBoolValue(Name(), VAR_NAME(use_clock), use_clock);
    use_notepad = ini->GetBoolValue(Name(), VAR_NAME(use_notepad), use_notepad);
    use_vanquish = ini->GetBoolValue(Name(), VAR_NAME(use_vanquish), use_vanquish);
    use_alcohol = ini->GetBoolValue(Name(), VAR_NAME(use_alcohol), use_alcohol);
    use_trade = ini->GetBoolValue(Name(), VAR_NAME(use_trade), use_trade);
    use_objectivetimer = ini->GetBoolValue(Name(), VAR_NAME(use_objectivetimer), use_objectivetimer);
    save_location_data = ini->GetBoolValue(Name(), VAR_NAME(save_location_data), save_location_data);
    use_gamesettings = ini->GetBoolValue(Name(), VAR_NAME(use_gamesettings), use_gamesettings);
    use_updater = ini->GetBoolValue(Name(), VAR_NAME(use_updater), use_updater);
    use_chatfilter = ini->GetBoolValue(Name(), VAR_NAME(use_chatfilter), use_chatfilter);
    use_itemfilter = ini->GetBoolValue(Name(), VAR_NAME(use_itemfilter), use_itemfilter);
    use_chatcommand = ini->GetBoolValue(Name(), VAR_NAME(use_chatcommand), use_chatcommand);
    use_discord = ini->GetBoolValue(Name(), VAR_NAME(use_discord), use_discord);
    use_factionleaderboard = ini->GetBoolValue(Name(), VAR_NAME(use_factionleaderboard), use_factionleaderboard);
    use_teamspeak = ini->GetBoolValue(Name(), VAR_NAME(use_teamspeak), use_teamspeak);
    use_twitch = ini->GetBoolValue(Name(), VAR_NAME(use_twitch), use_twitch);
    use_observer = ini->GetBoolValue(Name(), VAR_NAME(use_observer), use_observer);
    use_observer_player_window = ini->GetBoolValue(Name(), VAR_NAME(use_observer_player_window), use_observer_player_window);
    use_observer_target_window = ini->GetBoolValue(Name(), VAR_NAME(use_observer_target_window), use_observer_target_window);
    use_observer_party_window = ini->GetBoolValue(Name(), VAR_NAME(use_observer_party_window), use_observer_party_window);
    use_observer_export_window = ini->GetBoolValue(Name(), VAR_NAME(use_observer_export_window), use_observer_export_window);
    use_partywindowmodule = ini->GetBoolValue(Name(), VAR_NAME(use_partywindowmodule), use_partywindowmodule);
    use_friendlist = ini->GetBoolValue(Name(), VAR_NAME(use_friendlist), use_friendlist);
    use_serverinfo = ini->GetBoolValue(Name(), VAR_NAME(use_serverinfo), use_serverinfo);
    use_daily_quests = ini->GetBoolValue(Name(), VAR_NAME(use_daily_quests), use_daily_quests);
    use_obfuscator = ini->GetBoolValue(Name(), VAR_NAME(use_obfuscator), use_obfuscator);
    use_completion_window = ini->GetBoolValue(Name(), VAR_NAME(use_completion_window), use_completion_window);
    use_world_map = ini->GetBoolValue(Name(), VAR_NAME(use_world_map), use_world_map);
    use_effect_monitor = ini->GetBoolValue(Name(), VAR_NAME(use_effect_monitor), use_effect_monitor);
    use_reroll_window = ini->GetBoolValue(Name(), VAR_NAME(use_reroll_window), use_reroll_window);
    use_party_statistics = ini->GetBoolValue(Name(), VAR_NAME(use_party_statistics), use_party_statistics);
    use_skill_monitor = ini->GetBoolValue(Name(), VAR_NAME(use_skill_monitor), use_skill_monitor);
    use_latency_widget = ini->GetBoolValue(Name(), VAR_NAME(use_latency_widget), use_latency_widget);
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
    ini->SetBoolValue(Name(), VAR_NAME(use_teamspeak), use_teamspeak);
    ini->SetBoolValue(Name(), VAR_NAME(use_twitch), use_twitch);
    ini->SetBoolValue(Name(), VAR_NAME(use_observer), use_observer);
    ini->SetBoolValue(Name(), VAR_NAME(use_observer_player_window), use_observer_player_window);
    ini->SetBoolValue(Name(), VAR_NAME(use_observer_target_window), use_observer_target_window);
    ini->SetBoolValue(Name(), VAR_NAME(use_observer_party_window), use_observer_party_window);
    ini->SetBoolValue(Name(), VAR_NAME(use_observer_export_window), use_observer_export_window);
    ini->SetBoolValue(Name(), VAR_NAME(use_partywindowmodule), use_partywindowmodule);
    ini->SetBoolValue(Name(), VAR_NAME(use_friendlist), use_friendlist);
    ini->SetBoolValue(Name(), VAR_NAME(use_serverinfo), use_serverinfo);
    ini->SetBoolValue(Name(), VAR_NAME(save_location_data), save_location_data);
    ini->SetBoolValue(Name(), VAR_NAME(use_gamesettings), use_gamesettings);
    ini->SetBoolValue(Name(), VAR_NAME(use_updater), use_updater);
    ini->SetBoolValue(Name(), VAR_NAME(use_chatfilter), use_chatfilter);
    ini->SetBoolValue(Name(), VAR_NAME(use_itemfilter), use_itemfilter);
    ini->SetBoolValue(Name(), VAR_NAME(use_chatcommand), use_chatcommand);
    ini->SetBoolValue(Name(), VAR_NAME(use_daily_quests), use_daily_quests);
    ini->SetBoolValue(Name(), VAR_NAME(use_obfuscator), use_obfuscator);
    ini->SetBoolValue(Name(), VAR_NAME(use_completion_window), use_completion_window);
    ini->SetBoolValue(Name(), VAR_NAME(use_world_map), use_world_map);
    ini->SetBoolValue(Name(), VAR_NAME(use_effect_monitor), use_effect_monitor);
    ini->SetBoolValue(Name(), VAR_NAME(use_reroll_window), use_reroll_window);
    ini->SetBoolValue(Name(), VAR_NAME(use_party_statistics), use_party_statistics);
    ini->SetBoolValue(Name(), VAR_NAME(use_skill_monitor), use_skill_monitor);
    ini->SetBoolValue(Name(), VAR_NAME(use_latency_widget), use_latency_widget);
}

void ToolboxSettings::Draw(IDirect3DDevice9*) {
    ImGui::GetStyle().WindowBorderSize = (move_all ? 1.0f : 0.0f);
}

void ToolboxSettings::Update(float delta) {
    UNREFERENCED_PARAMETER(delta);


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
                GW::AgentLiving* me = GW::Agents::GetCharacter();
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

            GW::Agent* me = GW::Agents::GetCharacter();
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

