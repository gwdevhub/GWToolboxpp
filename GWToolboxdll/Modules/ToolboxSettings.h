#pragma once

#include <ToolboxUIElement.h>

namespace GW {
    namespace Constants {
        enum class MapID;
    }
}
class ToolboxSettings : public ToolboxUIElement {
    ToolboxSettings() {};
    ToolboxSettings(const ToolboxSettings&) = delete;
    ~ToolboxSettings() {};
public:
    static ToolboxSettings& Instance() {
        static ToolboxSettings instance;
        return instance;
    }

    const char* Name() const override { return "Toolbox Settings"; }
    const char8_t* Icon() const override { return ICON_FA_TOOLBOX;  }

    void LoadModules(CSimpleIni* ini);

    void Update(float delta) override;

    void LoadSettings(CSimpleIni* ini) override;
    void SaveSettings(CSimpleIni* ini) override;
    void DrawSettingInternal() override;
    void Draw(IDirect3DDevice9*) override;
    void ShowVisibleRadio() override { };

    void DrawFreezeSetting();
    void DrawSizeAndPositionSettings() override {}

    const std::vector<ToolboxModule*>& GetOptionalModules() const { return optional_modules; }

    static bool move_all;

private:
    // === location stuff ===
    clock_t location_timer = 0;
    GW::Constants::MapID location_current_map = (GW::Constants::MapID)0;
    std::wofstream location_file;
    bool save_location_data = false;

    std::vector<ToolboxModule*> optional_modules;

    bool use_pcons = true;
    bool use_hotkeys = true;
    bool use_builds = true;
    bool use_herobuilds = true;
    bool use_travel = true;
    bool use_teamspeak = true;
    bool use_dialogs = true;
    bool use_info = true;
    bool use_materials = true;
    bool use_trade = true;
    bool use_timer = true;
    bool use_health = true;
    bool use_skillbar = true;
    bool use_distance = true;
    bool use_minimap = true;
    bool use_damage = true;
    bool use_bonds = true;
    bool use_clock = true;
    bool use_notepad = true;
    bool use_vanquish = true;
    bool use_alcohol = true;
    bool use_world_map = true;
    bool use_effect_monitor = true;
    bool use_objectivetimer = true;
    bool use_observer = false;
    bool use_observer_player_window = false;
    bool use_observer_target_window = false;
    bool use_observer_party_window = false;
    bool use_observer_export_window = false;
    bool use_factionleaderboard = true;
    bool use_daily_quests = true;
    bool use_discord = true;
    bool use_twitch = true;
    bool use_partywindowmodule = true;
    bool use_friendlist = true;
    bool use_serverinfo = true;
    bool use_gamesettings = true;
    bool use_updater = true;
    bool use_chatfilter = true;
    bool use_itemfilter = true;
    bool use_chatcommand = true;
    bool use_discordintegration = false;
    bool use_obfuscator = true;
    bool use_completion_window = true;
    bool use_reroll_window = true;
    bool use_party_statistics = true;
    bool use_latency_widget = true;
    bool use_skill_monitor = true;
};
