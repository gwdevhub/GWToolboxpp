#pragma once

#include <vector>

#include <GWCA\Utilities\MemoryPatcher.h>
#include <GWCA\Managers\ChatMgr.h>

#include "ToolboxModule.h"
#include <Timer.h>

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

	void Update() override;
	bool WndProc(UINT Message, WPARAM wParam, LPARAM lParam);


	void DrawBorderlessSetting();

	// some settings that are either referenced from multiple places
	// or have nowhere else to be
	bool borderless_window = false;
	bool open_template_links = false;
	bool auto_transform_url;
	bool tick_is_toggle = false;
	bool select_with_chat_doubleclick = false;

	bool flash_window_on_pm = false;
	bool flash_window_on_party_invite = false;
	bool flash_window_on_zoning = false;

	bool auto_set_away = false;
	int auto_set_away_delay = 10;
	bool auto_set_online = false;
	clock_t activity_timer = 0;

	void ApplyBorderless(bool value);

private:
	std::vector<GW::MemoryPatcher*> patches;
	bool RectEquals(RECT a, RECT b);
	bool RectMultiscreen(RECT desktop, RECT gw);
	
};
