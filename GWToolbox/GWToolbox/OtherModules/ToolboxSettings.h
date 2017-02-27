#pragma once

#include <fstream>

#include <GWCA\Constants\Maps.h>

#include "Timer.h"
#include "ToolboxModule.h"

class ToolboxSettings : public ToolboxModule {
public:
	static ToolboxSettings* Instance();

	const char* Name() const override { return "Toolbox Settings"; }

	void Update() override;

	void LoadSettingInternal(CSimpleIni* ini) override;
	void SaveSettingInternal(CSimpleIni* ini) override;
	void DrawSettingInternal() override;

	bool freeze_widgets;

private:
	// === location stuff ===
	clock_t location_timer;
	GW::Constants::MapID location_current_map;
	std::ofstream location_file;
	bool save_location_data;
};
