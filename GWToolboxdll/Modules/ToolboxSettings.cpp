#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/AgentMgr.h>

#include <Utils/GuiUtils.h>
#include <GWToolbox.h>

#include <Modules/Updater.h>
#include <Modules/Resources.h>
#include <Modules/ChatFilter.h>
#include <Modules/ItemDrops.h>
#include <Modules/DiscordModule.h>
#include <Modules/TwitchModule.h>
#include <Modules/PartyWindowModule.h>
#include <Modules/TeamspeakModule.h>
#include <Modules/Teamspeak5Module.h>
#include <Modules/ObserverModule.h>
#include <Modules/HintsModule.h>
#include <Modules/PluginModule.h>
#include <Modules/QuestModule.h>
#include <Modules/KeyboardLanguageFix.h>
#if 0
#include <Modules/GWFileRequester.h>
#endif
#include <Modules/ToastNotifications.h>
#include <Modules/MouseFix.h>
#include <Modules/GuildWarsSettingsModule.h>
#include <Modules/PriceCheckerModule.h>
#include <Modules/ItemTooltipModule.h>
#include <Modules/ResignLogModule.h>
#include <Modules/PartyBroadcastModule.h>
#include <Modules/CodeOptimiserModule.h>
#include <Modules/VendorFix.h>
#include <Modules/AudioSettings.h>
#include <Modules/TextToSpeechModule.h>
#include <Modules/FpsFix.h>
#include <Modules/GamepadModule.h>
#include <Modules/CameraUnlockModule.h>
#include <Modules/LoginModule.h>
#include <Widgets/VanquishMapOverlayWidget.h>

#include <Windows/AccountInventoryWindow.h>
#include <Windows/PconsWindow.h>
#include <Windows/HotkeysWindow.h>
#include <Windows/BuildsWindow.h>
#include <Windows/HeroBuildsWindow.h>
#include <Windows/TravelWindow.h>
#include <Windows/InfoWindow.h>
#include <Windows/MaterialsWindow.h>
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
#include <Windows/DupingWindow.h>
#include <Windows/RerollWindow.h>
#include <Windows/ArmoryWindow.h>
#include <Windows/EnemyWindow.h>
#include <Windows/Pathfinding/PathfindingWindow.h>
#include <Windows/DropTrackerWindow.h>
#ifdef _DEBUG
#include <Windows/PacketLoggerWindow.h>
#include <Windows/DoorMonitorWindow.h>
#include <Windows/StringDecoderWindow.h>
#include <Windows/SkillListingWindow.h>
#endif
#include <Windows/TargetInfoWindow.h>
#include <Windows/GWMarketWindow.h>
#include <Windows/InventorySorting.h>
#include <Windows/PerformanceWindow.h>
#include <Windows/SettingsWindow.h>

#include <Widgets/TimerWidget.h>
#include <Widgets/HealthWidget.h>
#include <Widgets/DistanceWidget.h>
#include <Widgets/Minimap/Minimap.h>
#include <Widgets/PartyDamage.h>
#include <Widgets/BondsWidget.h>
#include <Widgets/ClockWidget.h>
#include <Widgets/VanquishWidget.h>
#include <Widgets/ExploitableCorpseWidget.h>
#include <Widgets/AlcoholWidget.h>
#include <Widgets/SkillbarWidget.h>
#include <Widgets/SkillMonitorWidget.h>
#include <Widgets/WorldMapWidget.h>
#include <Widgets/EffectsMonitorWidget.h>
#include <Widgets/LatencyWidget.h>
#include <Widgets/ActiveQuestWidget.h>
#include <Widgets/MissionMapWidget.h>
#include <Widgets/InventoryOverlayWidget.h>
#include <Widgets/TitleTrackerWidget.h>
#include <Widgets/FavorTracker.h>
#include "ToolboxSettings.h"


#define USE_OBFUSCATOR 1
#if USE_OBFUSCATOR
#include <Modules/Obfuscator.h>
#endif

namespace {
    ToolboxIni* inifile = nullptr;

    class ModuleToggle {
    public:
        ToolboxModule* toolbox_module;
        const char* name;
        bool enabled;

        ModuleToggle(ToolboxModule& m, const bool _enabled = true)
            : toolbox_module(&m), name(m.Name()), enabled(_enabled) { }
    };

    class WidgetToggle {
    public:
        ToolboxWidget* toolbox_module;
        const char* name;
        bool enabled;

        WidgetToggle(ToolboxWidget& m, const bool _enabled = true)
            : toolbox_module(&m), name(m.Name()), enabled(_enabled) { }
    };

    class WindowToggle {
    public:
        ToolboxWindow* toolbox_module;
        const char* name;
        bool enabled;

        WindowToggle(ToolboxWindow& m, const bool _enabled = true)
            : toolbox_module(&m), name(m.Name()), enabled(_enabled) { }
    };

    constexpr const char* modules_ini_section = "Toolbox Modules";

    std::vector<ModuleToggle> optional_modules = {
#if USE_OBFUSCATOR
        Obfuscator::Instance(),
#endif
        PluginModule::Instance(),
        ChatFilter::Instance(),
        ItemDrops::Instance(),
        PartyWindowModule::Instance(),
        ToastNotifications::Instance(),
        DiscordModule::Instance(),
        TwitchModule::Instance(),
        TeamspeakModule::Instance(),
        Teamspeak5Module::Instance(),
        ObserverModule::Instance(),
        HintsModule::Instance(),
        MouseFix::Instance(),
        KeyboardLanguageFix::Instance(),
        GuildWarsSettingsModule::Instance(),
        PriceCheckerModule::Instance(),
        ItemTooltipModule::Instance(),
        ResignLogModule::Instance(),
        QuestModule::Instance(),
        VanquishMapOverlayWidget::Instance(),
        PartyBroadcast::Instance(),
        CodeOptimiserModule::Instance(),
#if 0
        ExtraWeaponSets::Instance(),
#endif
        {TextToSpeechModule::Instance(), false},
        FpsFix::Instance(),          
        GamepadModule::Instance(),
        CameraUnlockModule::Instance(),
        TimerWidget::Instance(),
        HealthWidget::Instance(),
        SkillbarWidget::Instance(), 
        {DistanceWidget::Instance(), false},
        Minimap::Instance(),
        PartyDamage::Instance(), 
        BondsWidget::Instance(), 
        ClockWidget::Instance(),
        VanquishWidget::Instance(),
        ExploitableCorpseWidget::Instance(),
        AlcoholWidget::Instance(),
        WorldMapWidget::Instance(),
        EffectsMonitorWidget::Instance(),
        LatencyWidget::Instance(),
        SkillMonitorWidget::Instance(),
        MissionMapWidget::Instance(),
#if _DEBUG
        InventoryOverlayWidget::Instance(),
#endif
        ActiveQuestWidget::Instance(),
        TitleTrackerWidget::Instance(),
        PconsWindow::Instance(),
        HotkeysWindow::Instance(),
        BuildsWindow::Instance(),
        HeroBuildsWindow::Instance(),
        TravelWindow::Instance(),
        InfoWindow::Instance(),
        MaterialsWindow::Instance(),
        TradeWindow::Instance(),
        NotePadWindow::Instance(),
        ObjectiveTimerWindow::Instance(),
        FactionLeaderboardWindow::Instance(),
        DailyQuests::Instance(),
        FriendListWindow::Instance(),
        ObserverPlayerWindow::Instance(),
        ObserverTargetWindow::Instance(),
        ObserverPartyWindow::Instance(),
        ObserverExportWindow::Instance(),
        CompletionWindow::Instance(),
        RerollWindow::Instance(),
        PartyStatisticsWindow::Instance(),
        DupingWindow::Instance(),
        ArmoryWindow::Instance(),
        EnemyWindow::Instance(),
        TargetInfoWindow::Instance(),
        DropTrackerWindow::Instance(),
        GWMarketWindow::Instance(),
        InventorySorting::Instance(),
        FavorTracker::Instance(),
        LoginModule::Instance(),
        {AccountInventoryWindow::Instance(), false},
        {PerformanceWindow::Instance(), false}
    };

    bool modules_sorted = false;
}

void ToolboxSettings::LoadModules(ToolboxIni* ini)
{
    if (!modules_sorted) {
        modules_sorted = true;
        auto sort = [](const auto& a, const auto& b) {
            return strcmp(a.toolbox_module->Name(), b.toolbox_module->Name()) < 0;
        };
        std::ranges::sort(optional_modules, sort);
    }

    inifile = ini;

#ifdef _DEBUG
#if 0
    GWToolbox::ToggleModule(PartySearchWindow::Instance());
    GWToolbox::ToggleModule(GWFileRequester::Instance());
#endif
    GWToolbox::ToggleModule(PacketLoggerWindow::Instance());
    GWToolbox::ToggleModule(StringDecoderWindow::Instance());
    GWToolbox::ToggleModule(DoorMonitorWindow::Instance());
    GWToolbox::ToggleModule(SkillListingWindow::Instance());
#endif
    GWToolbox::ToggleModule(PathfindingWindow::Instance());
    GWToolbox::ToggleModule(VendorFix::Instance());
    GWToolbox::ToggleModule(AudioSettings::Instance()); 

    for (const auto& m : optional_modules) {
        GWToolbox::ToggleModule(*m.toolbox_module, m.enabled);
    }
}

void ToolboxSettings::DrawSettingsInternal()
{
    DrawFreezeSetting();
    ImGui::Separator();

    Updater::Instance().DrawSettingsInternal();
    ImGui::Separator();

    ImGui::Checkbox("Save Location Data", &save_location_data);
    ImGui::ShowHelp("Toolbox will save your location every second in a file in Settings Folder.");
    const auto cols = static_cast<size_t>(floor(ImGui::GetWindowWidth() / (170.0f * ImGui::FontScale())));

    ImGui::Separator();
    ImGui::PushID("global_enable");
    ImGui::TextUnformatted("Enable the following features:");
    ImGui::TextDisabled("Unticking will completely disable a feature from initializing and running. Requires Toolbox restart.");

    auto items_per_col = static_cast<size_t>(ceil(optional_modules.size() / static_cast<float>(cols)));
    size_t col_count = 0;
    ImGui::Columns(static_cast<int>(cols), "global_enable_cols", false);
    for (auto& m : optional_modules) {
        if (ImGui::Checkbox(m.name, &m.enabled)) {
            GWToolbox::SaveSettings();
            const auto p = &m;
            GW::GameThread::Enqueue([p]() {
                GWToolbox::ToggleModule(*p->toolbox_module, p->enabled);
                });

        }
        if (ImGui::IsItemHovered() && m.toolbox_module->Description()) {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(m.toolbox_module->Description());
            ImGui::EndTooltip();
        }

        col_count++;
        if (col_count == items_per_col) {
            ImGui::NextColumn();
            col_count = 0;
        }
    }
    ImGui::EndColumns();
    ImGui::PopID();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
}

void ToolboxSettings::DrawFreezeSetting()
{
    ImGui::StartSpacedElements(300.f);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Unlock Move All", &move_all);
    ImGui::ShowHelp("Will allow movement and resize of all widgets and windows");
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Clamp growing windows to screen bounds", &clamp_windows_to_screen);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Hide toolbox on loading screens", &hide_on_loading_screen);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Show settings button in window title bars", &show_settings_cog);
    ImGui::ShowHelp("Show a " ICON_FA_COG " button in the title bar of each window.\nClick it to quickly open that window's settings.");
}

void ToolboxSettings::LoadSettings(ToolboxIni* ini)
{
    ToolboxModule::LoadSettings(ini);
    inifile = ini; // Keep this to load module info

    move_all = false;
    LOAD_BOOL(clamp_windows_to_screen);
    LOAD_BOOL(hide_on_loading_screen);
    LOAD_BOOL(send_anonymous_gameplay_info);
    LOAD_BOOL(show_settings_cog);

    for (auto& m : optional_modules) {
        m.enabled = ini->GetBoolValue(modules_ini_section, m.name, m.enabled);
    }
}

void ToolboxSettings::SaveSettings(ToolboxIni* ini)
{
    ToolboxModule::SaveSettings(ini);
    if (location_file.is_open()) {
        location_file.close();
    }

    SAVE_BOOL(clamp_windows_to_screen);
    SAVE_BOOL(hide_on_loading_screen);
    SAVE_BOOL(send_anonymous_gameplay_info);
    SAVE_BOOL(show_settings_cog);

    for (const auto& m : optional_modules) {
        ini->SetBoolValue(modules_ini_section, m.name, m.enabled);
    }
}

void ToolboxSettings::Draw(IDirect3DDevice9*)
{
    ImGui::GetStyle().WindowBorderSize = move_all ? 1.0f : 0.0f;
    DrawSettingsCogButtons();
}

void ToolboxSettings::DrawSettingsCogButtons()
{
    if (!show_settings_cog) return;

    const ImVec2 mouse_pos = ImGui::GetIO().MousePos;
    ToolboxUIElement* hovered_elem = nullptr;

    for (const auto* elem : GWToolbox::GetUIElements()) {
        if (!elem->visible || !elem->show_titlebar) continue;

        const ImGuiWindow* window = ImGui::FindWindowByName(elem->Name());
        if (!window || (window->Flags & ImGuiWindowFlags_NoTitleBar)) continue;
        if (!window->Viewport) continue;

        const ImRect tb = window->TitleBarRect();
        const float btn_h = tb.GetHeight();

        // Leave room for the ImGui close button if one is shown
        const bool close_btn_shown = elem->IsWindow() && elem->GetVisiblePtr() != nullptr;
        const float close_offset = close_btn_shown ? btn_h : 0.f;

        const ImVec2 btn_min = {tb.Max.x - close_offset - btn_h, tb.Min.y};
        const ImVec2 btn_max = {btn_min.x + btn_h, tb.Max.y};

        const bool hovered = mouse_pos.x >= btn_min.x && mouse_pos.x < btn_max.x
                          && mouse_pos.y >= btn_min.y && mouse_pos.y < btn_max.y;

        ImDrawList* dl = ImGui::GetForegroundDrawList(window->Viewport);

        if (hovered) {
            hovered_elem = const_cast<ToolboxUIElement*>(elem);
            dl->AddRectFilled(btn_min, btn_max, IM_COL32(255, 255, 255, 30));

            const ImVec2 drag = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && drag.x == 0.f && drag.y == 0.f) {
                SettingsWindow::Instance().NavigateToSection(elem->SettingsName());
            }
        }

        const ImVec2 text_size = ImGui::CalcTextSize(ICON_FA_COG);
        const ImVec2 icon_pos = {
            btn_min.x + (btn_h - text_size.x) * 0.5f,
            tb.Min.y  + (btn_h - text_size.y) * 0.5f
        };
        ImVec4 col = ImGui::GetStyle().Colors[ImGuiCol_Text];
        if (!hovered) col.w *= 0.5f;
        dl->AddText(icon_pos, ImGui::ColorConvertFloat4ToU32(col), ICON_FA_COG);
    }

    if (hovered_elem) {
        ImGui::SetTooltip("Open %s settings", hovered_elem->SettingsName());
    }
}

void ToolboxSettings::Update(float)
{
    if (!(save_location_data && TIMER_DIFF(location_timer) > 1000))
        return;
    location_timer = TIMER_INIT();
    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable) {
        location_current_map = GW::Constants::MapID::None;
        location_file.close();
        return;
    }
    GW::Constants::MapID current = GW::Map::GetMapID();
    const auto me = GW::Agents::GetControlledCharacter();
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

        std::wstring prof_string;
        if (me) {
            prof_string += L" - ";
            prof_string += GetWProfessionAcronym(
                static_cast<GW::Constants::Profession>(me->primary));
            prof_string += L"-";
            prof_string += GetWProfessionAcronym(
                static_cast<GW::Constants::Profession>(me->secondary));
        }

        SYSTEMTIME localtime;
        GetLocalTime(&localtime);
        const std::wstring filename = std::to_wstring(localtime.wYear)
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

    
    if (location_file.is_open() && me != nullptr) {
        location_file << "Time=" << GW::Map::GetInstanceTime();
        location_file << " X=" << me->pos.x;
        location_file << " Y=" << me->pos.y;
        location_file << "\n";
    }
}
