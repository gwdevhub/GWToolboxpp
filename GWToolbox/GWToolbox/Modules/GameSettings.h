#pragma once

#include <Timer.h>
#include <Defines.h>

#include <GWCA/Utilities/Hook.h>
#include <GWCA/Utilities/MemoryPatcher.h>
#include <GWCA/GameEntities/Item.h>

#include <GWCA/Managers/FriendListMgr.h>

#include <Color.h>
#include "ToolboxModule.h"

namespace GW
{
    namespace Chat
    {
        enum Channel;
    }
}

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
	void Terminate() override;
	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;
	void DrawSettingInternal() override;

	void Update(float delta) override;
	bool WndProc(UINT Message, WPARAM wParam, LPARAM lParam);

	void SetAfkMessage(std::wstring&& message);
	static void ItemClickCallback(GW::HookStatus *, uint32_t type, uint32_t slot, GW::Bag *bag);

	bool tick_is_toggle = false;

	bool openlinks = false;
	bool auto_url = false;
	// bool select_with_chat_doubleclick = false;
	bool move_item_on_ctrl_click = false;

	bool flash_window_on_pm = false;
	bool flash_window_on_party_invite = false;
	bool flash_window_on_zoning = false;
	bool focus_window_on_zoning = false;

	bool auto_set_away = false;
	int auto_set_away_delay = 10;
	bool auto_set_online = false;
	clock_t activity_timer = 0;

	bool auto_skip_cinematic = false;
	bool show_unlearned_skill = false;

	std::wstring afk_message;
	clock_t afk_message_time = 0;

	bool show_timestamps = false;
	Color timestamps_color = 0;

	bool notify_when_friends_online = true;
    bool notify_when_friends_offline = false;

	bool disable_gold_selling_confirmation = false;
	bool skip_entering_name_for_faction_donate = true;

private:
	GW::MemoryPatcher ctrl_click_patch;
	GW::MemoryPatcher tome_patch;
	GW::MemoryPatcher gold_confirm_patch;

	void DrawChannelColor(const char *name, GW::Chat::Channel chan);
	static void FriendStatusCallback(
		GW::HookStatus *,
		GW::Friend* f,
		GW::FriendStatus status,
		const wchar_t *name,
		const wchar_t *charname);

	GW::HookEntry WhisperCallback_Entry;
	GW::HookEntry SendChatCallback_Entry;
	GW::HookEntry ItemClickCallback_Entry;
	GW::HookEntry FriendStatusCallback_Entry;

	GW::HookEntry PartyPlayerAdd_Entry;
	GW::HookEntry GameSrvTransfer_Entry;
	GW::HookEntry CinematicPlay_Entry;
};
