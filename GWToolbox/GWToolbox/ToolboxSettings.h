#pragma once

#include <fstream>

#include <GWCA\Constants\Maps.h>

#include "Timer.h"
#include "ToolboxModule.h"

class ToolboxSettings : public ToolboxModule {
public:
	const char* Name() const override { return "Toolbox Settings"; }

	void Update() override;

	void DrawSettings() override;

	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;

	bool freeze_widgets;

private:
	// === location stuff ===
	clock_t location_timer;
	GW::Constants::MapID location_current_map;
	std::ofstream location_file;
	bool save_location_data;
};
