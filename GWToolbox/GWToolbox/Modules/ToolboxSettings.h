#pragma once

#include <GWCA/Constants/Maps.h>

#include "Timer.h"
#include "ToolboxModule.h"

class ToolboxSettings : public ToolboxModule {
	ToolboxSettings() {};
	~ToolboxSettings() {};
public:
	static ToolboxSettings& Instance() {
		static ToolboxSettings instance;
		return instance;
	}

	const char* Name() const override { return "Toolbox Settings"; }

	void LoadModules(CSimpleIni* ini);
	
	void Update(float delta) override;

	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;
	void DrawSettingInternal() override;

	void DrawFreezeSetting();

	const std::vector<ToolboxModule*>& GetOptionalModules() const { return optional_modules; }

	static bool move_all;

private:
	// === location stuff ===
	clock_t location_timer = 0;
	GW::Constants::MapID location_current_map = GW::Constants::MapID::None;
	std::wofstream location_file;
	bool save_location_data = false;

	std::vector<ToolboxModule*> optional_modules;

	bool use_pcons = true;
	bool use_hotkeys = true;
	bool use_builds = true;
	bool use_herobuilds = true;
	bool use_travel = true;
	bool use_dialogs = true;
	bool use_info = true;
	bool use_materials = true;
	bool use_trade = true;
	bool use_timer = true;
	bool use_health = true;
	bool use_distance = true;
	bool use_minimap = true;
	bool use_damage = true;
	bool use_bonds = true;
	bool use_clock = true;
	bool use_notepad = true;
	bool use_vanquish = true;
	bool use_alcohol = true;
	bool use_objectivetimer = true;
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
	bool use_chatcommand = true;
	bool use_discordintegration = false;
};
