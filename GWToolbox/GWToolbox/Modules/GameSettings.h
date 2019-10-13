#pragma once

#include <Timer.h>
#include <Defines.h>
#include <regex>

#include <GWCA/Utilities/Hook.h>
#include <GWCA/Utilities/MemoryPatcher.h>

#include <GWCA/Constants/Constants.h>

#include <GWCA/GameEntities/Item.h>
#include <GWCA\GameEntities\Party.h>
#include <GWCA\GameEntities/NPC.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Player.h>
#include <GWCA/Packets/StoC.h>

#include <GWCA\GameContainers\List.h>

#include <GWCA/Context/AgentContext.h>
#include <GWCA/Context/GadgetContext.h>
#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/WorldContext.h>
#include <GWCA/Context/CharContext.h>

#include <GWCA/Managers/FriendListMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/ItemMgr.h>


#include <logger.h>
#include <Color.h>
#include "ToolboxModule.h"

class PendingChatMessage {
public:
    PendingChatMessage(GW::Chat::Channel channel, const wchar_t* enc_message, const wchar_t* enc_sender) : channel(channel) {
        invalid = !enc_message || !enc_sender;
        if (!invalid) {
            wcscpy(encoded_sender, enc_sender);
            wcscpy(encoded_message, enc_message);
            Init();
        }
    };
    void SendIt() {
        print = false;
        send = true;
    }
    static bool IsStringEncoded(const wchar_t* str) {
        return str && (str[0] < L' ' || str[0] > L'~');
    }
    const bool IsDecoded() {
        return !output_message.empty() && !output_sender.empty();
    }
    const bool Consume() {
        if (print) return PrintMessage();
        if (send) return SendMessage();
        return false;
    }


    static wchar_t* GetAgentNameEncoded(GW::Agent* agent) {
        if (!agent) return NULL;
        if (agent->GetIsCharacterType()) {
            // Player
            if (agent->login_number) {
                GW::PlayerArray players = GW::GameContext::instance()->world->players;
                if (!players.valid()) return NULL;

                GW::Player* player = &players[agent->login_number];
                if (player) return player->name_enc;
            }
            // NPC
            GW::Array<GW::AgentInfo> agent_infos = GW::GameContext::instance()->world->agent_infos;
            if (agent->agent_id >= agent_infos.size()) return NULL;
            if (agent_infos[agent->agent_id].name_enc) return agent_infos[agent->agent_id].name_enc;
            GW::NPCArray npcs = GW::GameContext::instance()->world->npcs;
            if (npcs.valid()) return npcs[agent->player_number].name_enc;
        }
        if (agent->GetIsGadgetType()) {
            GW::AgentContext* ctx = GW::GameContext::instance()->agent;
            GW::GadgetContext* gadget = GW::GameContext::instance()->gadget;
            if (!ctx || !gadget) return NULL;
            auto* GadgetIds = ctx->gadget_data[agent->agent_id].gadget_ids;
            if (!GadgetIds) return NULL;
            if (GadgetIds->name_enc) return GadgetIds->name_enc;
            size_t id = GadgetIds->gadget_id;
            if (gadget->GadgetInfo.size() <= id) return NULL;
            return gadget->GadgetInfo[id].name_enc;
        }
        if (agent->GetIsItemType()) {
            GW::ItemArray items = GW::Items::GetItemArray();
            if (!items.valid()) return false;
            GW::Item* item = items[agent->item_id];
            return item->name_enc;
        }
        return NULL;
    }
    bool invalid = true; // Set when we can't find the agent name for some reason, or arguments passed are empty.
protected:
    bool printed = false;
    bool print = true;
    bool send = false;
    wchar_t encoded_message[256] = { '\0' };
    wchar_t encoded_sender[32] = { '\0' };
    std::wstring output_message;
    std::wstring output_sender;
    GW::Chat::Channel channel;
    std::vector<std::wstring> SanitiseForSend() {
        std::wregex no_tags(L"<[^>]+>"), no_new_lines(L"\n");
        std::wstring sanitised, sanitised2, temp;
        std::regex_replace(std::back_inserter(sanitised), output_message.begin(), output_message.end(), no_tags, L"");
        std::regex_replace(std::back_inserter(sanitised2), sanitised.begin(), sanitised.end(), no_new_lines, L"|");
        std::vector<std::wstring> parts;
        std::wstringstream wss(sanitised2);
        while (std::getline(wss, temp, L'|'))
            parts.push_back(temp);
        return parts;
    }
    const bool PrintMessage();
    const bool SendMessage();
    void Init() {
        if (!invalid) {
            if (IsStringEncoded(this->encoded_message)) {
                //Log::LogW(L"message IS encoded, ");
                GW::UI::AsyncDecodeStr(encoded_message, &output_message);
            }
            else {
                output_message = std::wstring(encoded_message);
                //Log::LogW(L"message NOT encoded, ");
            }
            if (IsStringEncoded(this->encoded_sender)) {
                //Log::LogW(L"sender IS encoded\n");
                GW::UI::AsyncDecodeStr(encoded_sender, &output_sender);
            }
            else {
                //Log::LogW(L"sender NOT encoded\n");
                output_sender = std::wstring(encoded_sender);
            }
        }
    }

};


class GameSettings : public ToolboxModule {
	GameSettings() {};
	~GameSettings() {};
public:
	static GameSettings& Instance() {
		static GameSettings instance;
		return instance;
	}

	const char* Name() const override { return "Game Settings"; }

    time_t PendingChatMessage_last_send = 0;

	void Initialize() override;
	void Terminate() override;
	void LoadSettings(CSimpleIni* ini) override;
	void SaveSettings(CSimpleIni* ini) override;
	void DrawSettingInternal() override;

	void Update(float delta) override;
	bool WndProc(UINT Message, WPARAM wParam, LPARAM lParam);

	void DrawBorderlessSetting();
	void DrawFOVSetting();
	bool maintain_fov = false;
	float fov = 1.308997f; // default fov

	// some settings that are either referenced from multiple places
	// or have nowhere else to be
	enum { 
		Ok,
		WantBorderless,
		WantWindowed
	} borderless_status = Ok; // current actual status of borderless
	bool borderlesswindow = false; // status of the borderless checkbox and setting
	bool tick_is_toggle = false;

	bool shorthand_item_ping = true;
	bool openlinks = false;
	bool auto_url = false;
	// bool select_with_chat_doubleclick = false;
	bool move_item_on_ctrl_click = false;
	bool move_item_to_current_storage_pane = true;

	bool flash_window_on_pm = true;
	bool flash_window_on_party_invite = true;
	bool flash_window_on_zoning = true;
	bool focus_window_on_zoning = false;
    bool flash_window_on_cinematic = true;
    bool flash_window_on_trade = true;
    bool focus_window_on_trade = false;
    bool flash_window_on_name_ping = true;

	bool auto_return_on_defeat = false;

	bool auto_set_away = false;
	int auto_set_away_delay = 10;
	bool auto_set_online = false;
	clock_t activity_timer = 0;

	bool auto_skip_cinematic = false;
	bool show_unlearned_skill = false;

	bool faction_warn_percent = true;
	int faction_warn_percent_amount = 75;

	std::wstring afk_message;
	clock_t afk_message_time;

	bool show_timestamps = false;
    bool show_timestamp_seconds = false;
    bool show_timestamp_24h = false;
	Color timestamps_color;

	bool notify_when_friends_online = true;
    bool notify_when_friends_offline = false;
    bool notify_when_friends_join_outpost = true;
    bool notify_when_friends_leave_outpost = false;

    bool notify_when_players_join_outpost = false;
    bool notify_when_players_leave_outpost = false;

	bool notify_when_party_member_leaves = false;
	bool notify_when_party_member_joins = false;

	bool disable_gold_selling_confirmation = false;

    bool add_special_npcs_to_party_window = true;

	void ApplyBorderless(bool value);
	void SetAfkMessage(std::wstring&& message);
	static void ItemClickCallback(GW::HookStatus *, uint32_t type, uint32_t slot, GW::Bag *bag);

    static GW::Friend* GetOnlineFriend(wchar_t* account, wchar_t* playing);

    std::vector<PendingChatMessage*> pending_messages;

private:
	void UpdateBorderless();
	void UpdateFOV();
	void FactionEarnedCheckAndWarn();
	bool faction_checked = false;

    // Packet callbacks
	void MessageOnPartyChange();

	std::vector<GW::MemoryPatcher*> patches;
	GW::MemoryPatcher *ctrl_click_patch;
	GW::MemoryPatcher *tome_patch;
	GW::MemoryPatcher *gold_confirm_patch;
	std::vector<std::wstring> previous_party_names;
	bool was_leading = true;
	bool check_message_on_party_change = true;

    bool npc_speech_bubbles_as_chat = false;
    bool emulated_speech_bubble = false;
    bool redirect_npc_messages_to_emote_chat = false;
    
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
    bool ShouldAddAgentToPartyWindow(GW::Packet::StoC::AgentAdd* pak);
    bool ShouldRemoveAgentFromPartyWindow(uint32_t agent_id);
    bool GetPlayerIsLeader();

	GW::HookEntry PartyPlayerAdd_Entry;
	GW::HookEntry PlayerLeaveInstance_Entry;
	GW::HookEntry PlayerJoinInstance_Entry;
	GW::HookEntry GameSrvTransfer_Entry;
	GW::HookEntry CinematicPlay_Entry;
};
