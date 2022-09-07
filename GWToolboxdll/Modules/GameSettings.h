#pragma once

#include <GWCA/Packets/StoC.h>

#include <GWCA/Utilities/Hook.h>
#include <GWCA/Utilities/MemoryPatcher.h>

#include <Color.h>
#include <Defines.h>
#include <Logger.h>
#include <Timer.h>
#include <ToolboxModule.h>

#define NAMETAG_COLOR_DEFAULT_NPC 0xFFA0FF00
#define NAMETAG_COLOR_DEFAULT_PLAYER_SELF 0xFF40FF40
#define NAMETAG_COLOR_DEFAULT_PLAYER_OTHER 0xFF9BBEFF
#define NAMETAG_COLOR_DEFAULT_PLAYER_IN_PARTY 0xFF6060FF
#define NAMETAG_COLOR_DEFAULT_GADGET 0xFFFFFF00
#define NAMETAG_COLOR_DEFAULT_ENEMY 0xFFFF0000
#define NAMETAG_COLOR_DEFAULT_ITEM 0x0

namespace GW {
    namespace Chat {
        enum Channel : int;
    }
    struct Item;
    struct Friend;
    enum class FriendStatus : uint32_t;
    namespace Constants {
        enum class SkillID;
    }
    namespace UI {
        enum class UIMessage : uint32_t;
    }
}

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
    PendingChatMessage(GW::Chat::Channel channel, const wchar_t* enc_message, const wchar_t* enc_sender);
    static PendingChatMessage* queueSend(GW::Chat::Channel channel, const wchar_t* enc_message, const wchar_t* enc_sender);
    static PendingChatMessage* queuePrint(GW::Chat::Channel channel, const wchar_t* enc_message, const wchar_t* enc_sender);
    void SendIt() {  print = false; send = true;}
    static bool IsStringEncoded(const wchar_t* str) {
        return str && (str[0] < L' ' || str[0] > L'~');
    }
    bool IsDecoded() const {
        return !output_message.empty() && !output_sender.empty();
    }
    bool Consume() {
        if (print) return PrintMessage();
        if (send) return Send();
        return false;
    }
    bool IsSend() const {
        return send;
    }
    static bool Cooldown();
    bool invalid = true; // Set when we can't find the agent name for some reason, or arguments passed are empty.
protected:
    std::vector<std::wstring> SanitiseForSend();
    const bool PrintMessage();
    const bool Send();
    void Init();

};


class GameSettings : public ToolboxModule {
    GameSettings() = default;
    GameSettings(const GameSettings&) = delete;
    ~GameSettings() = default;

public:
    static GameSettings& Instance() {
        static GameSettings instance;
        return instance;
    }
    const char* Name() const override { return "Game Settings"; }
    const char8_t* Icon() const override { return ICON_FA_GAMEPAD; }
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
    bool WndProc(UINT Message, WPARAM wParam, LPARAM lParam) override;

    void DrawFOVSetting();
    bool maintain_fov = false;
    float fov = 1.308997f; // default fov

    void SetAfkMessage(std::wstring&& message);

    // Static callback functions
    static void OnPingWeaponSet(GW::HookStatus*, GW::UI::UIMessage , void*, void*);
    static void OnStartWhisper(GW::HookStatus*, wchar_t* _name);
    static void OnAgentLoopingAnimation(GW::HookStatus*, GW::Packet::StoC::GenericValue*);
    static void OnAgentMarker(GW::HookStatus* status, GW::Packet::StoC::GenericValue* pak);
    static void OnAgentEffect(GW::HookStatus*, GW::Packet::StoC::GenericValue*);
    static void OnFactionDonate(GW::HookStatus*, GW::UI::UIMessage, void*, void*);
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
    static void OnGlobalMessage(GW::HookStatus*, GW::Packet::StoC::MessageGlobal*);
    static void OnScreenShake(GW::HookStatus*, void* packet);
    static void OnCheckboxPreferenceChanged(GW::HookStatus*, GW::UI::UIMessage msgid, void* wParam, void* lParam);
    static void OnChangeTarget(GW::HookStatus*, GW::UI::UIMessage msgid, void* wParam, void* lParam);
    static void OnWriteChat(GW::HookStatus* status, GW::UI::UIMessage msgid, void* wParam, void*);
    static void OnSendChat(GW::HookStatus* status, GW::Chat::Channel , wchar_t*);
    static void OnAgentStartCast(GW::HookStatus* status, GW::UI::UIMessage, void*, void*);
    static void OnOpenWiki(GW::HookStatus*, GW::UI::UIMessage, void*, void*);
    static void OnCast(GW::HookStatus *, uint32_t agent_id, uint32_t slot, uint32_t target_id, uint32_t call_target);
    static void OnPlayerChatMessage(GW::HookStatus* status, GW::UI::UIMessage, void*, void*);
    static void OnAgentAdd(GW::HookStatus* status, GW::Packet::StoC::AgentAdd* packet);
    static void OnUpdateAgentState(GW::HookStatus* status, GW::Packet::StoC::AgentState* packet);
    static void OnUpdateSkillCount(GW::HookStatus*, void* packet);
    static void OnAgentNameTag(GW::HookStatus* status, GW::UI::UIMessage msgid, void* wParam, void*);

    static void OnDialogUIMessage(GW::HookStatus*, GW::UI::UIMessage, void*, void*);

    static void CmdReinvite(const wchar_t* message, int argc, LPWSTR* argv);

    GuiUtils::EncString* pending_wiki_search_term = 0;

    bool tick_is_toggle = false;

    bool limit_signets_of_capture = true;
    uint32_t actual_signets_of_capture_amount = 1;

    bool shorthand_item_ping = true;
    bool openlinks = false;
    bool auto_url = false;
    // bool select_with_chat_doubleclick = false;
    bool move_item_on_ctrl_click = false;
    bool move_item_to_current_storage_pane = true;
    bool move_materials_to_current_storage_pane = false;
    bool drop_ua_on_cast = false;

    bool focus_window_on_launch = true;
    bool flash_window_on_pm = false;
    bool flash_window_on_guild_chat = false;
    bool flash_window_on_party_invite = false;
    bool flash_window_on_zoning = false;
    bool focus_window_on_zoning = false;
    bool flash_window_on_cinematic = true;
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
    bool enable_chat_log = true;
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

    bool block_transmogrify_effect = false;
    bool block_sugar_rush_effect = false;
    bool block_snowman_summoner = false;
    bool block_party_poppers = false;
    bool block_bottle_rockets = false;
    bool block_ghostinthebox_effect = false;
    bool block_sparkly_drops_effect = false;

    bool lazy_chest_looting = false;

    bool auto_age2_on_age = true;
    bool auto_age_on_vanquish = false;

    bool auto_open_locked_chest = false;

    bool block_faction_gain = false;
    bool block_experience_gain = false;
    bool block_zero_experience_gain = true;



    Color nametag_color_npc = NAMETAG_COLOR_DEFAULT_NPC;
    Color nametag_color_player_self = NAMETAG_COLOR_DEFAULT_PLAYER_SELF;
    Color nametag_color_player_other = NAMETAG_COLOR_DEFAULT_PLAYER_OTHER;
    Color nametag_color_player_in_party = NAMETAG_COLOR_DEFAULT_PLAYER_IN_PARTY;
    Color nametag_color_gadget = NAMETAG_COLOR_DEFAULT_GADGET;
    Color nametag_color_enemy = NAMETAG_COLOR_DEFAULT_ENEMY;
    Color nametag_color_item = NAMETAG_COLOR_DEFAULT_ITEM;


    static GW::Friend* GetOnlineFriend(wchar_t* account, wchar_t* playing);

    std::vector<PendingChatMessage*> pending_messages;

private:


    void UpdateFOV();
    void FactionEarnedCheckAndWarn();
    bool faction_checked = false;

    void MessageOnPartyChange();



    std::vector<std::wstring> previous_party_names;

    std::vector<uint32_t> available_dialog_ids;

    bool was_leading = true;
    bool check_message_on_party_change = true;
    bool npc_speech_bubbles_as_chat = false;
    bool redirect_npc_messages_to_emote_chat = false;
    bool hide_dungeon_chest_popup = false;
    bool skip_entering_name_for_faction_donate = false;
    bool stop_screen_shake = false;
    bool disable_camera_smoothing = false;
    bool targeting_nearest_item = false;

    bool hide_player_speech_bubbles = false;
    bool improve_move_to_cast = false;

    bool is_prompting_hard_mode_mission = 0;

    static float GetSkillRange(GW::Constants::SkillID);

    void DrawChannelColor(const char *name, GW::Chat::Channel chan);
    static void FriendStatusCallback(
        GW::HookStatus *,
        GW::Friend* f,
        GW::FriendStatus status,
        const wchar_t *name,
        const wchar_t *charname);

    GW::HookEntry VanquishComplete_Entry;
    GW::HookEntry StartWhisperCallback_Entry;
    GW::HookEntry WhisperCallback_Entry;
    GW::HookEntry SendChatCallback_Entry;
    GW::HookEntry ItemClickCallback_Entry;
    GW::HookEntry FriendStatusCallback_Entry;
    GW::HookEntry PartyDefeated_Entry;
    GW::HookEntry OnAfterAgentAdd_Entry;
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
    GW::HookEntry MessageGlobal_Entry;
    GW::HookEntry MessageServer_Entry;
    GW::HookEntry PlayerJoinInstance_Entry;
    GW::HookEntry PlayerLeaveInstance_Entry;
    GW::HookEntry OnDialog_Entry;
    GW::HookEntry OnCheckboxPreferenceChanged_Entry;
    GW::HookEntry OnChangeTarget_Entry;
    GW::HookEntry OnPlayerChatMessage_Entry;
    GW::HookEntry OnWriteChat_Entry;
    GW::HookEntry OnAgentStartCast_Entry;
    GW::HookEntry OnOpenWikiUrl_Entry;
    GW::HookEntry OnScreenShake_Entry;
    GW::HookEntry OnCast_Entry;
    GW::HookEntry OnPartyTargetChange_Entry;
    GW::HookEntry OnAgentNameTag_Entry;
    GW::HookEntry OnEnterMission_Entry;
    GW::HookEntry OnPreSendDialog_Entry;
    GW::HookEntry OnPostSendDialog_Entry;
};