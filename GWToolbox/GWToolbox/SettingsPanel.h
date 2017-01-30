#pragma once

#include <fstream>

#include <GWCA\Constants\Maps.h>

#include "ToolboxPanel.h"
#include "Timer.h"
#include "Settings.h"

class SettingsPanel : public ToolboxPanel {
private:
	// === location stuff ===
	clock_t location_timer_;
	GW::Constants::MapID location_current_map_;
	std::ofstream location_file_;

public:
	SettingBool save_location_data;

	SettingsPanel(OSHGui::Control* parent);

	void BuildUI() override;

	// Update. Will always be called every frame.
	void Main() override;

	// Draw user interface. Will be called every frame if the element is visible
	void Draw() override;

	inline void Close() { if (location_file_.is_open()) location_file_.close(); }
};
