#pragma once

#include <GWCA/Utilities/Hook.h>
#include <GWCA/Utilities/MemoryPatcher.h>

#include <GWCA/Constants/Constants.h>

#include <GWCA/GameEntities/Item.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/NPC.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Player.h>
#include <GWCA/Packets/StoC.h>

#include <GWCA/GameContainers/List.h>

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


#include <Color.h>
#include <Defines.h>
#include <Logger.h>
#include <Timer.h>
#include <ToolboxModule.h>

class PendingChatMessage {
protected:
    bool printed = false;
    bool print = true;
    bool send = false;
    
    wchar_t encoded_message[256] = { '\0' };
    wchar_t encoded_sender[32] = { '\0' };
    std::wstring output_message;
    std::wstring output_sender;
    GW::Chat::Channel channel;
public:
    PendingChatMessage(GW::Chat::Channel channel, const wchar_t* enc_message, const wchar_t* enc_sender) : channel(channel) {
        invalid = !enc_message || !enc_sender;
        if (!invalid) {
            wcscpy(encoded_sender, enc_sender);
            wcscpy(encoded_message, enc_message);
            Init();
        }
    };
    static PendingChatMessage* queueSend(GW::Chat::Channel channel, const wchar_t* enc_message, const wchar_t* enc_sender) {
        PendingChatMessage* m = new PendingChatMessage(channel, enc_message, enc_sender);
        if (m->invalid) {
            delete m;
            return nullptr;
        }
        m->SendIt();
        return m;
    }
    static PendingChatMessage* queuePrint(GW::Chat::Channel channel, const wchar_t* enc_message, const wchar_t* enc_sender) {
        PendingChatMessage* m = new PendingChatMessage(channel, enc_message, enc_sender);
        if (m->invalid) {
            delete m;
            return nullptr;
        }
        return m;
    }
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
        if (send) return Send();
        return false;
    }
    const bool IsSend() {
        return send;
    }
    static bool Cooldown();
    bool invalid = true; // Set when we can't find the agent name for some reason, or arguments passed are empty.
protected:
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
    const bool Send();
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
    GameSettings(const GameSettings&) = delete;
    ~GameSettings() {};
public:
    static GameSettings& Instance() {
        static GameSettings instance;
        return instance;
    }
    const char* Name() const override { return "Game Settings"; }
    static void PingItem(GW::Item* item, uint32_t parts = 3);
    static void PingItem(uint32_t item_id, uint32_t parts = 3);

    void Initialize() override;
    void Terminate() override;
    void LoadSettings(CSimpleIni* ini) override;
    void RegisterSettingsContent() override;
    void SaveSettings(CSimpleIni* ini) override;
    void DrawSettingInternal() override;
    void DrawInventorySettings();
    void DrawChatSettings();
    void DrawPartySettings();

    void Update(float delta) override;
    bool WndProc(UINT Message, WPARAM wParam, LPARAM lParam);

    void DrawFOVSetting();
    bool maintain_fov = false;
    float fov = 1.308997f; // default fov

    void SetAfkMessage(std::wstring&& message);

    // Static callback functions
    static void ItemClickCallback(GW::HookStatus *, uint32_t type, uint32_t slot, GW::Bag *bag);
    static void OnPingWeaponSet(GW::HookStatus*, void* packet);
    static void OnStartWhisper(GW::HookStatus*, wchar_t* _name);
    static void OnPlayerDance(GW::HookStatus*, GW::Packet::StoC::GenericValue*);
    static void OnFactionDonate(GW::HookStatus*, uint32_t dialog_id);
    static void OnPartyDefeated(GW::HookStatus*, GW::Packet::StoC::PartyDefeated*);
    static void OnVanquishComplete(GW::HookStatus*, GW::Packet::StoC::VanquishComplete*);
    static void OnDungeonReward(GW::HookStatus*, GW::Packet::StoC::DungeonReward*);
    static void OnMapLoaded(GW::HookStatus*, GW::Packet::StoC::MapLoaded*);
    static void OnCinematic(GW::HookStatus*, GW::Packet::StoC::CinematicPlay*);
    static void OnMapTravel(GW::HookStatus*, GW::Packet::StoC::GameSrvTransfer*);
    static void OnTradeStarted(GW::HookStatus*, GW::Packet::StoC::TradeStart*);
    static void OnPlayerLeaveInstance(GW::HookStatus*, GW::Packet::StoC::PlayerLeaveInstance*);
    static void OnPlayerJoinInstance(GW::HookStatus*, GW::Packet::StoC::PlayerJoinInstance*);
    static void OnPartyInviteReceived(GW::HookStatus*, GW::Packet::StoC::PartyInviteReceived_Create*);
    static void OnPartyPlayerJoined(GW::HookStatus*, GW::Packet::StoC::PartyPlayerAdd*);
    static void OnLocalChatMessage(GW::HookStatus*, GW::Packet::StoC::MessageLocal*);
    static void OnNPCChatMessage(GW::HookStatus*, GW::Packet::StoC::MessageNPC*);
    static void OnSpeechBubble(GW::HookStatus*, GW::Packet::StoC::SpeechBubble*);
    static void OnSpeechDialogue(GW::HookStatus*, GW::Packet::StoC::DisplayDialogue*);
    static void OnServerMessage(GW::HookStatus*, GW::Packet::StoC::MessageServer*);
    static void OnScreenShake(GW::HookStatus*, void* packet);
    static void OnCheckboxPreferenceChanged(GW::HookStatus*, uint32_t msgid, void* wParam, void* lParam);
    

    bool tick_is_toggle = false;

    bool shorthand_item_ping = true;
    bool openlinks = false;
    bool auto_url = false;
    // bool select_with_chat_doubleclick = false;
    bool move_item_on_ctrl_click = false;
    bool move_item_to_current_storage_pane = true;

    bool flash_window_on_pm = false;
    bool flash_window_on_party_invite = false;
    bool flash_window_on_zoning = false;
    bool focus_window_on_zoning = false;
    bool flash_window_on_cinematic = false;
    bool flash_window_on_trade = true;
    bool focus_window_on_trade = false;
    bool flash_window_on_name_ping = true;
    bool set_window_title_as_charname = true;

    bool auto_return_on_defeat = false;
    bool auto_accept_invites = false;
    bool auto_accept_join_requests = false;

    bool auto_set_away = false;
    int auto_set_away_delay = 10;
    bool auto_set_online = false;
    clock_t activity_timer = 0;

    bool auto_skip_cinematic = false;
    bool show_unlearned_skill = false;

    bool faction_warn_percent = true;
    int faction_warn_percent_amount = 75;

    std::wstring afk_message;
    clock_t afk_message_time = 0;

    bool show_timestamps = false;
    bool show_timestamp_seconds = false;
    bool show_timestamp_24h = false;
    Color timestamps_color = 0;

    bool notify_when_friends_online = true;
    bool notify_when_friends_offline = false;
    bool notify_when_friends_join_outpost = true;
    bool notify_when_friends_leave_outpost = false;

    bool notify_when_players_join_outpost = false;
    bool notify_when_players_leave_outpost = false;

    bool notify_when_party_member_leaves = false;
    bool notify_when_party_member_joins = false;

    bool disable_gold_selling_confirmation = false;
    bool collectors_edition_emotes = true;

    static GW::Friend* GetOnlineFriend(wchar_t* account, wchar_t* playing);

    std::vector<PendingChatMessage*> pending_messages;

private:
    void UpdateFOV();
    void FactionEarnedCheckAndWarn();
    bool faction_checked = false;

    void MessageOnPartyChange();

    GW::MemoryPatcher ctrl_click_patch;
    GW::MemoryPatcher tome_patch;
    GW::MemoryPatcher gold_confirm_patch;
    std::vector<std::wstring> previous_party_names;

    bool was_leading = true;
    bool check_message_on_party_change = true;
    bool npc_speech_bubbles_as_chat = false;
    bool redirect_npc_messages_to_emote_chat = false;
    bool auto_age2_on_age = true;
    bool auto_age_on_vanquish = false;
    bool hide_dungeon_chest_popup = false;
    bool skip_entering_name_for_faction_donate = false;
    bool stop_screen_shake = false;

    void DrawChannelColor(const char *name, GW::Chat::Channel chan);
    static void FriendStatusCallback(
        GW::HookStatus *,
        GW::Friend* f,
        GW::FriendStatus status,
        const wchar_t *name,
        const wchar_t *charname);

    static bool GetPlayerIsLeader();

    GW::HookEntry VanquishComplete_Entry;
    GW::HookEntry StartWhisperCallback_Entry;
    GW::HookEntry WhisperCallback_Entry;
    GW::HookEntry SendChatCallback_Entry;
    GW::HookEntry ItemClickCallback_Entry;
    GW::HookEntry FriendStatusCallback_Entry;
    GW::HookEntry PartyDefeated_Entry;
    GW::HookEntry AgentState_Entry;
    GW::HookEntry AgentRemove_Entry;
    GW::HookEntry AgentAdd_Entry;
    GW::HookEntry TradeStart_Entry;
    GW::HookEntry PartyPlayerAdd_Entry;
    GW::HookEntry PartyPlayerRemove_Entry;
    GW::HookEntry GameSrvTransfer_Entry;
    GW::HookEntry CinematicPlay_Entry;
    GW::HookEntry SpeechBubble_Entry;
    GW::HookEntry DisplayDialogue_Entry;
    GW::HookEntry MessageNPC_Entry;
    GW::HookEntry MessageLocal_Entry;
    GW::HookEntry MessageServer_Entry;
    GW::HookEntry PlayerJoinInstance_Entry;
    GW::HookEntry PlayerLeaveInstance_Entry;
    GW::HookEntry OnDialog_Entry;
    GW::HookEntry OnCheckboxPreferenceChanged_Entry;
    GW::HookEntry OnScreenShake_Entry;
};
