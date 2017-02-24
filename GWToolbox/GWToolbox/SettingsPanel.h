#pragma once

#include <fstream>

#include <GWCA\Constants\Maps.h>

#include "ToolboxPanel.h"
#include "Timer.h"
#include "Settings.h"

class SettingsPanel : public ToolboxPanel {
public:
	const char* Name() const override { return "Settings Panel"; }
	const char* TabButtonText() const override { return "Settings"; }

	SettingsPanel(IDirect3DDevice9* pDevice);
	~SettingsPanel() {};

	// Update. Will always be called every frame.
	void Update() override;

	// Draw user interface. Will be called every frame if the element is visible
	void Draw(IDirect3DDevice9* pDevice) override;

	inline void Close() { if (location_file_.is_open()) location_file_.close(); }
private:
	// === location stuff ===
	clock_t location_timer_;
	GW::Constants::MapID location_current_map_;
	std::ofstream location_file_;
	bool save_location_data;
};
