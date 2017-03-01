#pragma once

#include <vector>

#include <GWCA\Utilities\MemoryPatcher.h>

#include "ToolboxModule.h"

class GameSettings : public ToolboxModule {
	GameSettings() {};
	~GameSettings() {};
public:
	static GameSettings& Instance() {
		static GameSettings instance;
		return instance;
	}

	const char* Name() const override { return "Game Settings"; }

	void Initialize() override;
	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;
	void DrawSettingInternal() override;

	void DrawBorderlessSetting();

	// some settings that are either referenced from multiple places
	// or have nowhere else to be
	bool borderless_window;
	bool open_template_links;

	void ApplyBorderless(bool value);
private:
	std::vector<GW::MemoryPatcher*> patches;
};
