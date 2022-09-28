#pragma once

#include <GWCA/Packets/StoC.h>

#include <GWCA/Utilities/Hook.h>

#include <Color.h>
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

class GameSettings : public ToolboxModule {
    GameSettings() = default;
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
    void DrawPartySettings();

    void Update(float delta) override;
    bool WndProc(UINT Message, WPARAM wParam, LPARAM lParam) override;

    void DrawFOVSetting();
    bool maintain_fov = false;
    float fov = 1.308997f; // default fov

    // callback functions
    void OnPingWeaponSet(GW::HookStatus*, GW::UI::UIMessage, void*, void*) const;
    void OnAgentLoopingAnimation(GW::HookStatus*, GW::Packet::StoC::GenericValue*) const;
    void OnAgentMarker(GW::HookStatus* status, GW::Packet::StoC::GenericValue* pak) const;
    void OnAgentEffect(GW::HookStatus*, GW::Packet::StoC::GenericValue*) const;
    void OnFactionDonate(GW::HookStatus*, GW::UI::UIMessage, void*, void*) const;
    void OnPartyDefeated(GW::HookStatus*, GW::Packet::StoC::PartyDefeated*) const;
    void OnVanquishComplete(GW::HookStatus*, GW::Packet::StoC::VanquishComplete*) const;
    void OnDungeonReward(GW::HookStatus*, GW::Packet::StoC::DungeonReward*) const;
    void OnMapLoaded(GW::HookStatus*, GW::Packet::StoC::MapLoaded*) const;
    void OnCinematic(GW::HookStatus*, GW::Packet::StoC::CinematicPlay*) const;
    void OnMapTravel(GW::HookStatus*, GW::Packet::StoC::GameSrvTransfer*) const;
    void OnTradeStarted(GW::HookStatus*, GW::Packet::StoC::TradeStart*) const;
    void OnPlayerLeaveInstance(GW::HookStatus*, GW::Packet::StoC::PlayerLeaveInstance*) const;
    void OnPlayerJoinInstance(GW::HookStatus*, GW::Packet::StoC::PlayerJoinInstance*) const;
    void OnPartyInviteReceived(GW::HookStatus*, GW::Packet::StoC::PartyInviteReceived_Create*) const;
    void OnPartyPlayerJoined(GW::HookStatus*, GW::Packet::StoC::PartyPlayerAdd*);
    void OnLocalChatMessage(GW::HookStatus*, GW::Packet::StoC::MessageLocal*);
    void OnServerMessage(GW::HookStatus*, GW::Packet::StoC::MessageServer*) const;
    void OnGlobalMessage(GW::HookStatus*, GW::Packet::StoC::MessageGlobal*) const;
    void OnScreenShake(GW::HookStatus*, void* packet) const;
    void OnChangeTarget(GW::HookStatus*, GW::UI::UIMessage msgid, void* wParam, void* lParam) const;
    void OnWriteChat(GW::HookStatus* status, GW::UI::UIMessage msgid, void* wParam, void*) const;
    void OnAgentStartCast(GW::HookStatus* status, GW::UI::UIMessage, void*, void*) const;
    void OnOpenWiki(GW::HookStatus*, GW::UI::UIMessage, void*, void*);
    void OnCast(GW::HookStatus *, uint32_t agent_id, uint32_t slot, uint32_t target_id, uint32_t call_target) const;
    void OnAgentAdd(GW::HookStatus* status, GW::Packet::StoC::AgentAdd* packet) const;
    void OnUpdateAgentState(GW::HookStatus* status, GW::Packet::StoC::AgentState* packet) const;
    void OnUpdateSkillCount(GW::HookStatus*, void* packet);
    void OnAgentNameTag(GW::HookStatus* status, GW::UI::UIMessage msgid, void* wParam, void*) const;
    void OnWhisper(GW::HookStatus*, const wchar_t* from, const wchar_t* msg) const;
    void OnDialogUIMessage(GW::HookStatus*, GW::UI::UIMessage, void*, void*) const;
    void FriendStatusCallback(GW::HookStatus*, GW::Friend* f, GW::FriendStatus status, const wchar_t* alias, const wchar_t* charname) const;
    void CmdReinvite(const wchar_t* message, int argc, LPWSTR* argv) const;

    GuiUtils::EncString* pending_wiki_search_term = 0;

    bool tick_is_toggle = false;

    bool limit_signets_of_capture = true;
    uint32_t actual_signets_of_capture_amount = 1;

    bool shorthand_item_ping = true;
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

private:


    void UpdateFOV() const;
    void FactionEarnedCheckAndWarn();
    bool faction_checked = false;

    void MessageOnPartyChange();


    std::vector<std::wstring> previous_party_names;

    std::vector<uint32_t> available_dialog_ids;

    bool was_leading = true;
    bool hide_dungeon_chest_popup = false;
    bool skip_entering_name_for_faction_donate = false;
    bool stop_screen_shake = false;
    bool disable_camera_smoothing = false;
    bool targeting_nearest_item = false;
    bool improve_move_to_cast = false;
    bool check_message_on_party_change = true;

    bool is_prompting_hard_mode_mission = 0;

    static float GetSkillRange(GW::Constants::SkillID);

    GW::HookEntry VanquishComplete_Entry;
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
    GW::HookEntry PlayerJoinInstance_Entry;
    GW::HookEntry PlayerLeaveInstance_Entry;
    GW::HookEntry OnDialog_Entry;
    GW::HookEntry OnChangeTarget_Entry;
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