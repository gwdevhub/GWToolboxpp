#pragma once

#include <vector>

#include <GWCA\Utilities\MemoryPatcher.h>

#include "ToolboxModule.h"

class OtherSettings : public ToolboxModule {
public:
	const char* Name() override { return "Other Settings"; }

	OtherSettings();

	void DrawSettings() override;

	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;

	// some settings that are either referenced from multiple places
	// or have nowhere else to be
	bool borderless_window;
	bool open_template_links;
	bool freeze_widgets;

private:
	std::vector<GW::MemoryPatcher*> patches;
	void ApplyBorderless(bool value);
};
