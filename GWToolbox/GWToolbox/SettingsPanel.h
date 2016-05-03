#pragma once

#include <fstream>

#include "GWCA\GwConstants.h"

#include "ToolboxPanel.h"
#include "Timer.h"

class BoolSetting;

class SettingsPanel : public ToolboxPanel {
private:
	bool location_active_; // true if active
	clock_t location_timer_;
	GwConstants::MapID location_current_map_;
	std::ofstream location_file_;

	std::vector<BoolSetting*> boolsettings;

public:
	SettingsPanel(OSHGui::Control* parent) : ToolboxPanel(parent) {}

	void BuildUI() override;
	void UpdateUI() override {};
	void MainRoutine() override;

	void SetSaveLocationData(bool active) { location_active_ = active; }

	inline void Close() { if (location_file_.is_open()) location_file_.close(); }

	void ApplySettings();
};

