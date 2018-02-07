#pragma once

#include <vector>
#include <Timer.h>
#include <Defines.h>

#include <GWCA\Utilities\MemoryPatcher.h>
#include <GWCA\Managers\ChatMgr.h>

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

	void Update(float delta) override;
	bool WndProc(UINT Message, WPARAM wParam, LPARAM lParam);

	void DrawBorderlessSetting();

	// some settings that are either referenced from multiple places
	// or have nowhere else to be
	enum { 
		Ok,
		WantBorderless,
		WantWindowed
	} borderless_status = Ok; // current actual status of borderless
	bool borderlesswindow = false; // status of the borderless checkbox and setting
	bool tick_is_toggle = false;

	bool openlinks = false;
	bool auto_url = false;
	bool select_with_chat_doubleclick = false;

	bool flash_window_on_pm = false;
	bool flash_window_on_party_invite = false;
	bool flash_window_on_zoning = false;
	bool focus_window_on_zoning = false;

	bool auto_set_away = false;
	int auto_set_away_delay = 10;
	bool auto_set_online = false;
	clock_t activity_timer = 0;

	void ApplyBorderless(bool value);

private:
	void UpdateBorderless();
	std::vector<GW::MemoryPatcher*> patches;	
};
