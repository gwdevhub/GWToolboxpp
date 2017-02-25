#pragma once

#include <vector>

#include <GWCA\Utilities\MemoryPatcher.h>

#include "ToolboxModule.h"

class GameSettings : public ToolboxModule {
public:
	const char* Name() const override { return "Game Settings"; }

	GameSettings();

	void DrawSettings() override;

	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;

	// some settings that are either referenced from multiple places
	// or have nowhere else to be
	bool borderless_window;
	bool open_template_links;

private:
	std::vector<GW::MemoryPatcher*> patches;
	void ApplyBorderless(bool value);
};
