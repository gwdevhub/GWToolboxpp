#pragma once

#include <fstream>

#include <GWCA\Constants\Maps.h>

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

	void Update() override;

	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;
	void DrawSettingInternal() override;

	void DrawFreezeSetting();

	static bool move_all;
private:
	// === location stuff ===
	clock_t location_timer;
	GW::Constants::MapID location_current_map;
	std::ofstream location_file;
	bool save_location_data;
};
