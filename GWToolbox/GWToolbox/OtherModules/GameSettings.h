#pragma once

#include <vector>

#include <GWCA\Utilities\MemoryPatcher.h>

#include "ToolboxModule.h"

class GameSettings : public ToolboxModule {
public:
	static GameSettings* Instance();

	const char* Name() const override { return "Game Settings"; }

	GameSettings();

	void LoadSettingInternal(CSimpleIni* ini) override;
	void SaveSettingInternal(CSimpleIni* ini) override;
	void DrawSettingInternal() override;

	// some settings that are either referenced from multiple places
	// or have nowhere else to be
	bool borderless_window;
	bool open_template_links;

	void ApplyBorderless(bool value);
private:
	std::vector<GW::MemoryPatcher*> patches;
};
