#pragma once

#include <GWCA/Packets/StoC.h>

#include <GWCA/Utilities/Hook.h>

#include <Color.h>
#include <ToolboxModule.h>

enum class DEFAULT_NAMETAG_COLOR : Color {
    NPC             = 0xFFA0FF00,
    PLAYER_SELF     = 0xFF40FF40,
    PLAYER_OTHER    = 0xFF9BBEFF,
    PLAYER_IN_PARTY = 0xFF6060FF,
    GADGET          = 0xFFFFFF00,
    ENEMY           = 0xFFFF0000,
    ITEM            = 0x0,
};

namespace GW {
    struct Item;
    struct Friend;
    enum class FriendStatus : uint32_t;

    namespace Constants {
        enum class SkillID : uint32_t;
    }

    namespace UI {
        enum class UIMessage : uint32_t;
    }
}

class GameSettings : public ToolboxModule {
    GameSettings() = default;
    ~GameSettings() override = default;

public:
    static GameSettings& Instance()
    {
        static GameSettings instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Game Settings"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_GAMEPAD; }
    static void PingItem(GW::Item* item, uint32_t parts = 3);
    static void PingItem(uint32_t item_id, uint32_t parts = 3);

    struct Settings {
        bool disable_skill_descriptions_in_outpost = false;
        bool disable_skill_descriptions_in_explorable = false;

        bool block_faction_gain = false;
        bool block_experience_gain = false;
        bool block_zero_experience_gain = true;

        bool block_zero_damage_or_energy = false;

        bool block_receiving_damage = false;
        bool block_dealing_damage = false;
        bool block_giving_heals = false;
        bool block_receiving_heals = false;
        bool combine_overhead_numbers = false;

        bool lazy_chest_looting = false;
        bool show_amount_of_lockpicks_under_locked_chest_nametag = false;

        // When entering a mission you've completed, check whether you should be doing it in HM/NM instead
        bool check_and_prompt_if_mission_already_completed = true;

        unsigned int last_online_status = 1; // GW::FriendStatus::Online
        bool remember_online_status = true;

        bool notify_when_friends_online = true;
        bool notify_when_friends_offline = false;
        bool notify_when_friends_join_outpost = true;
        bool notify_when_friends_leave_outpost = false;

        bool notify_when_players_join_outpost = false;
        bool notify_when_players_leave_outpost = false;

        bool notify_when_party_member_leaves = false;
        bool notify_when_party_member_joins = false;

        bool block_enter_area_message = false;

        bool tick_is_toggle = false;

        bool limit_signets_of_capture = true;

        bool shorthand_item_ping = true;
        bool move_item_on_ctrl_click = false;
        bool move_item_to_current_storage_pane = true;
        bool move_materials_to_current_storage_pane = false;
        bool drop_ua_on_cast = false;

        bool focus_window_on_launch = true;
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

        bool auto_skip_cinematic = false;
        bool hide_known_skills = false;
        bool hide_nonelites_on_capture = false;
        bool remove_min_skill_warmup_duration = false;

        bool faction_warn_percent = true;
        int faction_warn_percent_amount = 75;

        bool disable_gold_selling_confirmation = false;
        bool collectors_edition_emotes = true;
        bool block_transmogrify_effect = false;
        bool block_sugar_rush_effect = false;
        bool block_snowman_summoner = false;
        bool block_party_poppers = false;
        bool block_fireworks = false;
        bool block_bottle_rockets = false;
        bool block_sparkly_drops_effect = false;
        bool auto_age2_on_age = true;
        bool auto_age_on_vanquish = false;
        bool auto_screenshot_on_boss_kill = false;
        bool auto_screenshot_on_vanquish = false;
        bool auto_screenshot_on_mission_complete = false;
        bool auto_screenshot_on_dungeon_complete = false;
        bool auto_screenshot_on_title_maxed = false;
        bool auto_open_locked_chest = false;
        bool auto_open_locked_chest_with_key = false;
        bool automatically_flag_pet_to_fight_called_target = true;
        bool prevent_weapon_spell_animation_on_player = false;
        bool block_dervish_avatar_form = false;
        bool block_vanquish_complete_popup = false;

        bool hide_dungeon_chest_popup = false;
        bool hide_window_buttons_in_fullscreen = false;
        bool skip_entering_name_for_faction_donate = false;
        bool stop_screen_shake = false;
        bool disable_camera_smoothing = false;
        bool disable_camera_smoothing_with_controller = false;

        bool useful_level_progress_label = true;
        bool hide_store_page_on_char_select = false;

        Colors::SettingColor nametag_color_npc = static_cast<Color>(DEFAULT_NAMETAG_COLOR::NPC);
        Colors::SettingColor nametag_color_player_self = static_cast<Color>(DEFAULT_NAMETAG_COLOR::PLAYER_SELF);
        Colors::SettingColor nametag_color_player_other = static_cast<Color>(DEFAULT_NAMETAG_COLOR::PLAYER_OTHER);
        Colors::SettingColor nametag_color_player_in_party = static_cast<Color>(DEFAULT_NAMETAG_COLOR::PLAYER_IN_PARTY);
        Colors::SettingColor nametag_color_gadget = static_cast<Color>(DEFAULT_NAMETAG_COLOR::GADGET);
        Colors::SettingColor nametag_color_enemy = static_cast<Color>(DEFAULT_NAMETAG_COLOR::ENEMY);
        Colors::SettingColor nametag_color_item = static_cast<Color>(DEFAULT_NAMETAG_COLOR::ITEM);
    };

    void Initialize() override;
    void Terminate() override;
    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy) override;
    void RegisterSettingsContent() override;
    void SaveSettings(SettingsDoc& doc) override;
    void DrawSettingsInternal() override;
    static void DrawInventorySettings();
    static void DrawPartySettings();

    static bool GetSettingBool(const char* setting);

    void Update(float delta) override;
    bool WndProc(UINT Message, WPARAM wParam, LPARAM lParam) override;

    // callback functions
    static void OnPingWeaponSet(GW::HookStatus*, GW::UI::UIMessage, void*, void*);
    static void OnAgentLoopingAnimation(GW::HookStatus*, const GW::Packet::StoC::GenericValue*);
    static void OnAgentMarker(GW::HookStatus* status, GW::Packet::StoC::GenericValue* pak);
    static void OnAgentEffect(GW::HookStatus*, const GW::Packet::StoC::GenericValue*);
    static void OnDungeonReward(GW::HookStatus*, GW::Packet::StoC::DungeonReward*);
    static void OnMapLoaded(GW::HookStatus*, GW::Packet::StoC::MapLoaded*);
    static void OnCinematic(const GW::HookStatus*, const GW::Packet::StoC::CinematicPlay*);
    static void OnMapTravel(const GW::HookStatus*, const GW::Packet::StoC::GameSrvTransfer*);
    static void OnPlayerLeaveInstance(GW::HookStatus*, const GW::Packet::StoC::PlayerLeaveInstance*);
    static void OnPlayerJoinInstance(GW::HookStatus*, GW::Packet::StoC::PlayerJoinInstance*);
    static void OnPartyInviteReceived(const GW::HookStatus*, const GW::Packet::StoC::PartyInviteReceived_Create*);
    static void OnPartyPlayerJoined(const GW::HookStatus*, const GW::Packet::StoC::PartyPlayerAdd*);
    static void OnServerMessage(const GW::HookStatus*, GW::Packet::StoC::MessageServer*);
    static void OnScreenShake(GW::HookStatus*, const void* packet);
    static void OnWriteChat(GW::HookStatus* status, GW::UI::UIMessage msgid, void* wParam, void*);
    static void OnAgentStartCast(GW::HookStatus* status, GW::UI::UIMessage, void*, void*);
    static void OnOpenWiki(GW::HookStatus*, GW::UI::UIMessage, void*, void*);
    static void OnAgentAdd(GW::HookStatus* status, const GW::Packet::StoC::AgentAdd* packet);
    static void OnUpdateAgentState(GW::HookStatus* status, GW::Packet::StoC::AgentState* packet);
    static void OnUpdateSkillCount(GW::HookStatus*, void* packet);
    static void OnAgentNameTag(GW::HookStatus* status, GW::UI::UIMessage msgid, void* wParam, void*);
private:
    void FactionEarnedCheckAndWarn();
    bool faction_checked = false;

    void MessageOnPartyChange();

    std::vector<std::wstring> previous_party_names{};

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
    GW::HookEntry PartyPlayerReady_Entry;
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
    GW::HookEntry OnAgentModel_Entry;
};
