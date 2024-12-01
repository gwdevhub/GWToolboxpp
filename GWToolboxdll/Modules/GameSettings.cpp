#include "stdafx.h"

#include <GWCA/Utilities/MemoryPatcher.h>
#include <GWCA/Utilities/Scanner.h>

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Maps.h>
#include <GWCA/Constants/Skills.h>

#include <GWCA/Context/GuildContext.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Camera.h>
#include <GWCA/GameEntities/Friendslist.h>
#include <GWCA/GameEntities/Guild.h>
#include <GWCA/GameEntities/Hero.h>
#include <GWCA/GameEntities/Item.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Player.h>
#include <GWCA/GameEntities/Quest.h>
#include <GWCA/GameEntities/Skill.h>
#include <GWCA/GameEntities/Title.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/CameraMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/FriendListMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/RenderMgr.h>
#include <GWCA/Managers/QuestMgr.h>

#include <GWCA/Utilities/Hooker.h>

#include <Utils/GuiUtils.h>
#include <Utils/ToolboxUtils.h>

#include <Modules/PartyWindowModule.h>

#include <Modules/ChatSettings.h>
#include <Modules/DialogModule.h>
#include <Modules/GameSettings.h>
#include <Windows/CompletionWindow.h>

#include <Color.h>
#include <hidusage.h>
#include <Logger.h>
#include <Timer.h>
#include <Defines.h>
#include <Utils/FontLoader.h>
#include <Utils/TextUtils.h>

#pragma warning(disable : 6011)
#pragma comment(lib,"Version.lib")

using namespace GuiUtils;
using namespace ToolboxUtils;

namespace {
    GW::MemoryPatcher ctrl_click_patch;
    GW::MemoryPatcher gold_confirm_patch;
    GW::MemoryPatcher skill_description_patch;
    GW::MemoryPatcher remove_skill_warmup_duration_patch;

    void SetWindowTitle(const bool enabled)
    {
        if (!enabled) {
            return;
        }
        const HWND hwnd = GW::MemoryMgr::GetGWWindowHandle();
        if (!hwnd) {
            return;
        }
        const std::wstring title = GetPlayerName();
        if (!title.empty()) {
            SetWindowTextW(hwnd, title.c_str());
        }
    }

    void SaveChannelColor(ToolboxIni* ini, const char* section, const char* chanstr, const GW::Chat::Channel chan)
    {
        char key[128];
        GW::Chat::Color sender, message;
        GetChannelColors(chan, &sender, &message);
        // @Cleanup: We relie on the fact the Color and GW::Chat::Color are the same format
        snprintf(key, 128, "%s_color_sender", chanstr);
        Colors::Save(ini, section, key, sender);
        snprintf(key, 128, "%s_color_message", chanstr);
        Colors::Save(ini, section, key, message);
    }

    void LoadChannelColor(const ToolboxIni* ini, const char* section, const char* chanstr, const GW::Chat::Channel chan)
    {
        char key[128];
        GW::Chat::Color sender, message;
        GetDefaultColors(chan, &sender, &message);
        snprintf(key, 128, "%s_color_sender", chanstr);
        sender = Colors::Load(ini, section, key, sender);
        SetSenderColor(chan, sender);
        snprintf(key, 128, "%s_color_message", chanstr);
        message = Colors::Load(ini, section, key, message);
        SetMessageColor(chan, message);
    }

    // For some reason no matter whether this call is wrapped in Draw or Update or main thread, it passes garbage to window title on first load.
    // Even tried compiling in unicode, still no dice.
    // I've given up trying, so here is a timer that triggers a 3s delay to do it in the Update loop, whatever
    clock_t set_window_title_delay = 0;

    clock_t last_send = 0;

    clock_t instance_entered_at = 0;

    bool disable_item_descriptions_in_outpost = false;
    bool disable_item_descriptions_in_explorable = false;
    bool hide_email_address = false;
    bool disable_skill_descriptions_in_outpost = false;
    bool disable_skill_descriptions_in_explorable = false;
    bool block_faction_gain = false;
    bool block_experience_gain = false;
    bool block_zero_experience_gain = true;
    bool lazy_chest_looting = false;
    
    bool check_and_prompt_if_mission_already_completed = true; // When entering a mission you've completed, check whether you should be doing it in HM/NM instead

    uint32_t last_online_status = static_cast<uint32_t>(GW::FriendStatus::Online);
    bool remember_online_status = true;

    bool targeting_nearest_item = false;

    bool notify_when_friends_online = true;
    bool notify_when_friends_offline = false;
    bool notify_when_friends_join_outpost = true;
    bool notify_when_friends_leave_outpost = false;

    bool notify_when_players_join_outpost = false;
    bool notify_when_players_leave_outpost = false;

    bool notify_when_party_member_leaves = false;
    bool notify_when_party_member_joins = false;

    bool block_enter_area_message = false;

    EncString* pending_wiki_search_term = nullptr;

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
    bool block_ghostinthebox_effect = false;
    bool block_sparkly_drops_effect = false;

    bool auto_age2_on_age = true;
    bool auto_age_on_vanquish = false;

    bool auto_open_locked_chest = false;
    bool auto_open_locked_chest_with_key = false;

    bool keep_current_quest_when_new_quest_added = false;

    bool automatically_flag_pet_to_fight_called_target = true;

    bool skip_fade_animations = false;
    using FadeFrameContent_pt = void(__cdecl*)(uint32_t frame_id, float source_opacity, float target_opacity, float duration_seconds, uint32_t unk);
    FadeFrameContent_pt FadeFrameContent_Func = nullptr, FadeFrameContent_Ret = nullptr;

    void OnFadeFrameContent(uint32_t frame_id, float source_opacity, float target_opacity, float duration_seconds, uint32_t unk) {
        GW::Hook::EnterHook();
        if (skip_fade_animations) {
            duration_seconds = 0.0f;
            source_opacity = target_opacity;
        }
        FadeFrameContent_Ret(frame_id, source_opacity, target_opacity, duration_seconds, unk);
        GW::Hook::LeaveHook();
    }

    GW::HookEntry SkillList_UICallback_HookEntry;
    GW::UI::UIInteractionCallback SkillList_UICallback_Func = 0, SkillList_UICallback_Ret = 0;

    // If this ui message is adding an unlearnt skill to the tome window, block it
    void OnSkillList_UICallback(GW::UI::InteractionMessage* message, void* wParam, void* lParam) {
        GW::Hook::EnterHook();
        const auto is_show_unlearned_skill = message->message_id == (GW::UI::UIMessage)0x47 && ((uint32_t*)wParam)[1] == 0x3;
        if (!(is_show_unlearned_skill && show_unlearned_skill)) {
            SkillList_UICallback_Ret(message, wParam, lParam);
        }
        GW::Hook::LeaveHook();
    }

    Color nametag_color_npc = static_cast<Color>(DEFAULT_NAMETAG_COLOR::NPC);
    Color nametag_color_player_self = static_cast<Color>(DEFAULT_NAMETAG_COLOR::PLAYER_SELF);
    Color nametag_color_player_other = static_cast<Color>(DEFAULT_NAMETAG_COLOR::PLAYER_OTHER);
    Color nametag_color_player_in_party = static_cast<Color>(DEFAULT_NAMETAG_COLOR::PLAYER_IN_PARTY);
    Color nametag_color_gadget = static_cast<Color>(DEFAULT_NAMETAG_COLOR::GADGET);
    Color nametag_color_enemy = static_cast<Color>(DEFAULT_NAMETAG_COLOR::ENEMY);
    Color nametag_color_item = static_cast<Color>(DEFAULT_NAMETAG_COLOR::ITEM);

    constexpr float checkbox_w = 270.f;

    enum PING_PARTS {
        NAME = 1,
        DESC = 2
    };

    struct [[maybe_unused]] SkillData {
        GW::Constants::Profession primary;
        GW::Constants::Profession secondary;
        uint32_t attribute_count;
        GW::Constants::Attribute attribute_keys[12];
        uint32_t attribute_values[12];
        GW::Constants::SkillID skill_ids[8];
    };

    // prophecies = factions
    const std::map<GW::Constants::SkillID, GW::Constants::SkillID> duplicate_skills = {
        {GW::Constants::SkillID::Desperation_Blow, GW::Constants::SkillID::Drunken_Blow},
        {GW::Constants::SkillID::Galrath_Slash, GW::Constants::SkillID::Silverwing_Slash},
        {GW::Constants::SkillID::Griffons_Sweep, GW::Constants::SkillID::Leviathans_Sweep},
        {GW::Constants::SkillID::Penetrating_Blow, GW::Constants::SkillID::Penetrating_Chop},
        {GW::Constants::SkillID::Pure_Strike, GW::Constants::SkillID::Jaizhenju_Strike},

        {GW::Constants::SkillID::Bestial_Pounce, GW::Constants::SkillID::Savage_Pounce},
        {GW::Constants::SkillID::Dodge, GW::Constants::SkillID::Zojuns_Haste},
        {GW::Constants::SkillID::Penetrating_Attack, GW::Constants::SkillID::Sundering_Attack},
        {GW::Constants::SkillID::Point_Blank_Shot, GW::Constants::SkillID::Zojuns_Shot},
        {GW::Constants::SkillID::Tigers_Fury, GW::Constants::SkillID::Bestial_Fury},

        {GW::Constants::SkillID::Divine_Healing, GW::Constants::SkillID::Heavens_Delight},
        {GW::Constants::SkillID::Heal_Area, GW::Constants::SkillID::Kareis_Healing_Circle},
        {GW::Constants::SkillID::Heal_Other, GW::Constants::SkillID::Jameis_Gaze},
        {GW::Constants::SkillID::Holy_Strike, GW::Constants::SkillID::Stonesoul_Strike},
        {GW::Constants::SkillID::Symbol_of_Wrath, GW::Constants::SkillID::Kirins_Wrath},

        {GW::Constants::SkillID::Desecrate_Enchantments, GW::Constants::SkillID::Defile_Enchantments},
        {GW::Constants::SkillID::Shadow_Strike, GW::Constants::SkillID::Lifebane_Strike},
        {GW::Constants::SkillID::Spinal_Shivers, GW::Constants::SkillID::Shivers_of_Dread},
        {GW::Constants::SkillID::Touch_of_Agony, GW::Constants::SkillID::Wallows_Bite},
        {GW::Constants::SkillID::Vampiric_Touch, GW::Constants::SkillID::Vampiric_Bite},

        {GW::Constants::SkillID::Arcane_Thievery, GW::Constants::SkillID::Arcane_Larceny},
        {GW::Constants::SkillID::Ethereal_Burden, GW::Constants::SkillID::Kitahs_Burden},
        {GW::Constants::SkillID::Inspired_Enchantment, GW::Constants::SkillID::Revealed_Enchantment},
        {GW::Constants::SkillID::Inspired_Hex, GW::Constants::SkillID::Revealed_Hex},
        {GW::Constants::SkillID::Sympathetic_Visage, GW::Constants::SkillID::Ancestors_Visage},

        {GW::Constants::SkillID::Earthquake, GW::Constants::SkillID::Dragons_Stomp}
    };
    // luxon = kurzick
    const std::map<GW::Constants::SkillID, GW::Constants::SkillID> factions_skills = {
        {GW::Constants::SkillID::Save_Yourselves_luxon, GW::Constants::SkillID::Save_Yourselves_kurzick},
        {GW::Constants::SkillID::Aura_of_Holy_Might_luxon, GW::Constants::SkillID::Aura_of_Holy_Might_kurzick},
        {GW::Constants::SkillID::Elemental_Lord_luxon, GW::Constants::SkillID::Elemental_Lord_kurzick},
        {GW::Constants::SkillID::Ether_Nightmare_luxon, GW::Constants::SkillID::Ether_Nightmare_kurzick},
        {GW::Constants::SkillID::Selfless_Spirit_luxon, GW::Constants::SkillID::Selfless_Spirit_kurzick},
        {GW::Constants::SkillID::Shadow_Sanctuary_luxon, GW::Constants::SkillID::Shadow_Sanctuary_kurzick},
        {GW::Constants::SkillID::Signet_of_Corruption_luxon, GW::Constants::SkillID::Signet_of_Corruption_kurzick},
        {GW::Constants::SkillID::Spear_of_Fury_luxon, GW::Constants::SkillID::Spear_of_Fury_kurzick},
        {GW::Constants::SkillID::Summon_Spirits_luxon, GW::Constants::SkillID::Summon_Spirits_kurzick},
        {GW::Constants::SkillID::Triple_Shot_luxon, GW::Constants::SkillID::Triple_Shot_kurzick}
    };

    struct LoadSkillBarPacket {
        uint32_t header = 0;
        uint32_t agent_id = 0;
        uint32_t skill_ids_size = 8;
        GW::Constants::SkillID skill_ids[8]{};
    } skillbar_packet;

    // Before the game loads the skill bar you want, copy the data over for checking once the bar is loaded.
    void OnPreLoadSkillBar(GW::HookStatus*, const GW::UI::UIMessage message_id, void* wparam, void*)
    {
        ASSERT(message_id == GW::UI::UIMessage::kSendLoadSkillbar && wparam);
        const struct Pack {
            uint32_t agent_id = 0;
            GW::Constants::SkillID skill_ids[8]{};
        }* packet = static_cast<Pack*>(wparam);
        // @Enhancement: may cause weird stuff if we load loads of builds at once; heros could get mixed up with player. Use a map.
        memcpy(skillbar_packet.skill_ids, packet->skill_ids, sizeof(skillbar_packet.skill_ids));
        skillbar_packet.agent_id = packet->agent_id;
    }

    // Takes SkillData* ptr, rectifies any missing dupe skills. True if bar has been tweaked.
    bool FixLoadSkillData(GW::Constants::SkillID* skill_ids)
    {
        auto find_skill = [](const GW::Constants::SkillID* skill_ids, const GW::Constants::SkillID skill_id) {
            for (auto i = 0; i < 8; i++) {
                if (skill_ids[i] == skill_id) {
                    return i;
                }
            }
            return -1;
        };
        int found_first = -1;
        int found_second = -1;
        bool unlocked_first;
        bool unlocked_second;
        bool tweaked = false;
        for (const auto& [first_skill, second_skill] : duplicate_skills) {
            found_first = find_skill(skill_ids, first_skill);
            found_second = find_skill(skill_ids, second_skill);
            if (found_first == -1 && found_second == -1) {
                continue;
            }
            unlocked_first = GW::SkillbarMgr::GetIsSkillLearnt(first_skill);
            unlocked_second = GW::SkillbarMgr::GetIsSkillLearnt(second_skill);

            if (found_first != -1 && found_second == -1
                && !unlocked_first && unlocked_second) {
                // First skill found in build template, second skill not already in template, user only has second skill
                skill_ids[found_first] = second_skill;
                tweaked = true;
            }
            else if (found_second != -1 && found_first == -1
                     && !unlocked_second && unlocked_first) {
                // Second skill found in build template, first skill not already in template, user only has first skill
                skill_ids[found_second] = first_skill;
                tweaked = true;
            }
        }
        for (const auto& [luxon_skill, kurzick_skill] : factions_skills) {
            found_first = find_skill(skill_ids, luxon_skill);
            found_second = find_skill(skill_ids, kurzick_skill);
            if (found_first == -1 && found_second == -1) {
                continue;
            }
            if (found_first != -1 && found_second != -1) {
                continue;
            }
            unlocked_first = GW::SkillbarMgr::GetIsSkillLearnt(luxon_skill);
            unlocked_second = GW::SkillbarMgr::GetIsSkillLearnt(kurzick_skill);

            if (found_first != -1 && found_second == -1
                && !unlocked_first && unlocked_second) {
                // First skill found in build template, second skill not already in template, user only has second skill
                skill_ids[found_first] = kurzick_skill;
                tweaked = true;
            }
            else if (found_second != -1 && found_first == -1
                     && !unlocked_second && unlocked_first) {
                // Second skill found in build template, first skill not already in template, user only has first skill
                skill_ids[found_second] = luxon_skill;
                tweaked = true;
            }
            else if (unlocked_first && unlocked_second) {
                // Find skill with higher title track
                const auto kurzick_title = GW::PlayerMgr::GetTitleTrack(GW::Constants::TitleID::Kurzick);
                const uint32_t kurzick_rank = kurzick_title ? kurzick_title->points_needed_current_rank : 0;
                const auto luxon_title = GW::PlayerMgr::GetTitleTrack(GW::Constants::TitleID::Luxon);
                const uint32_t luxon_rank = luxon_title ? luxon_title->points_needed_current_rank : 0;
                const auto skillbar_index = std::max(found_first, found_second);
                ASSERT(skillbar_index >= 0 && skillbar_index < 8);
                if (kurzick_rank > luxon_rank) {
                    skill_ids[skillbar_index] = kurzick_skill;
                    tweaked = true;
                }
                else if (kurzick_rank < luxon_rank) {
                    skill_ids[skillbar_index] = luxon_skill;
                    tweaked = true;
                }
            }
        }
        return tweaked;
    }

    // Checks loaded skillbar for any missing skills once the game has sent the packet
    void OnPostLoadSkillBar(GW::HookStatus*, void* packet)
    {
        const auto post_pack = static_cast<LoadSkillBarPacket*>(packet);
        if (post_pack->agent_id != skillbar_packet.agent_id) {
            skillbar_packet.agent_id = 0;
            return;
        }
        if (std::ranges::equal(skillbar_packet.skill_ids, post_pack->skill_ids)) {
            skillbar_packet.agent_id = 0;
            return;
        }
        if (skillbar_packet.agent_id && FixLoadSkillData(skillbar_packet.skill_ids)) {
            GW::SkillbarMgr::LoadSkillbar(skillbar_packet.skill_ids,_countof(skillbar_packet.skill_ids), GW::PartyMgr::GetAgentHeroID(skillbar_packet.agent_id));
        }
        skillbar_packet.agent_id = 0;
    }

    using ShowAgentFactionGain_pt = void(__cdecl*)(uint32_t agent_id, uint32_t stat_type, uint32_t amount_gained);
    ShowAgentFactionGain_pt ShowAgentFactionGain_Func = nullptr, ShowAgentFactionGain_Ret = nullptr;
    // Block overhead faction gain numbers
    void OnShowAgentFactionGain(const uint32_t agent_id, const uint32_t stat_type, const uint32_t amount_gained)
    {
        GW::Hook::EnterHook();
        if (!block_faction_gain) {
            ShowAgentFactionGain_Ret(agent_id, stat_type, amount_gained);
        }
        GW::Hook::LeaveHook();
    }

    using ShowAgentExperienceGain_pt = void(__cdecl*)(uint32_t agent_id, uint32_t amount_gained);
    ShowAgentExperienceGain_pt ShowAgentExperienceGain_Func = nullptr, ShowAgentExperienceGain_Ret = nullptr;
    // Block overhead experience gain numbers
    void OnShowAgentExperienceGain(const uint32_t agent_id, const uint32_t amount_gained)
    {
        GW::Hook::EnterHook();
        const bool blocked = block_experience_gain || (block_zero_experience_gain && amount_gained == 0);
        if (!blocked) {
            ShowAgentExperienceGain_Ret(agent_id, amount_gained);
        }
        GW::Hook::LeaveHook();
    }

    // Key held to show/hide item descriptions
    constexpr int modifier_key_item_descriptions = VK_MENU;
    int modifier_key_item_descriptions_key_state = 0;

    // Key held to show/hide skill descriptions
    constexpr int modifier_key_skill_descriptions = VK_MENU;
    int modifier_key_skill_descriptions_key_state = 0;

    using CreateCodedTextLabel_pt = void(__cdecl*)(uint32_t frame_id, const wchar_t* encoded_string);
    CreateCodedTextLabel_pt CreateEncodedTextLabel_Func = nullptr;
    // Hide skill description in tooltip; called by GW to add the description of the skill to a skill tooltip
    void CreateCodedTextLabel_SkillDescription(const uint32_t frame_id, const wchar_t* encoded_string)
    {
        GW::Hook::EnterHook();
        bool block_description = disable_skill_descriptions_in_outpost && IsOutpost() || disable_skill_descriptions_in_explorable && IsExplorable();
        block_description = block_description && GetKeyState(modifier_key_item_descriptions) >= 0;
        if (block_description) {
            encoded_string = L"\x101"; // Decodes to ""
        }
        CreateEncodedTextLabel_Func(frame_id, encoded_string);
        GW::Hook::LeaveHook();
    }

    using SetGlobalNameTagVisibility_pt = void(__cdecl*)(uint32_t flags);
    SetGlobalNameTagVisibility_pt SetGlobalNameTagVisibility_Func = nullptr;
    uint32_t* GlobalNameTagVisibilityFlags = nullptr;

    GW::MemoryPatcher skip_map_entry_message_patch;

    // Refresh agent name tags when allegiance changes ( https://github.com/gwdevhub/GWToolboxpp/issues/781 )
    void OnAgentAllegianceChanged(GW::HookStatus*, GW::Packet::StoC::AgentUpdateAllegiance*)
    {
        // Backup the current name tag flag state, then "flash" nametags to update.
        const uint32_t prev_flags = *GlobalNameTagVisibilityFlags;
        SetGlobalNameTagVisibility_Func(0);
        SetGlobalNameTagVisibility_Func(prev_flags);
        ASSERT(*GlobalNameTagVisibilityFlags == prev_flags);
    }

    uint32_t current_party_target_id = 0;
    // Record current party target - this isn't always the same as the compass target.
    void OnPartyTargetChanged(GW::HookStatus*, const GW::UI::UIMessage message_id, void* wparam, void*)
    {
        const struct Packet {
            uint32_t source;
            uint32_t identifier;
        }* party_target_info = static_cast<Packet*>(wparam);
        switch (message_id) {
            case GW::UI::UIMessage::kTargetPlayerPartyMember: {
                const GW::Player* p = GW::PlayerMgr::GetPlayerByID(party_target_info->identifier);
                if (p && p->agent_id) {
                    current_party_target_id = p->agent_id;
                }
            }
            break;
            case GW::UI::UIMessage::kTargetNPCPartyMember: {
                if (IsAgentInParty(party_target_info->identifier)) {
                    current_party_target_id = party_target_info->identifier;
                }
            }
            break;
            case GW::UI::UIMessage::kCalledTargetChange: {
                if (automatically_flag_pet_to_fight_called_target && party_target_info->source == GW::PlayerMgr::GetPlayerNumber()) {
                    GW::PartyMgr::SetPetBehavior(GW::HeroBehavior::Fight, party_target_info->identifier);
                }
            }
            break;
        }
    }

    struct PendingReinvite {
        uint32_t identifier = 0;

        enum class Stage {
            None,
            Kick,
            InvitePlayer,
            InviteHenchman,
            InviteHero
        } stage = Stage::None;

        time_t start_ts = 0;

        void reset(const uint32_t _agent_id = 0)
        {
            identifier = _agent_id;
            stage = identifier ? Stage::Kick : Stage::None;
            start_ts = TIMER_INIT();
        }
    } pending_reinvite;

    // Handle processing from CmdReinvite command
    void UpdateReinvite()
    {
        if (pending_reinvite.stage == PendingReinvite::Stage::None) {
            return; // Nothing to do
        }
        if (!IsOutpost()) {
            return pending_reinvite.reset();
        }
        if (TIMER_DIFF(static_cast<clock_t>(pending_reinvite.start_ts)) > 2000) {
            // Timeout (map change etc)
            return pending_reinvite.reset();
        }
        const GW::Player* first_player = nullptr;
        const GW::Player* next_player = nullptr;
        const GW::AgentLiving* me = GW::Agents::GetControlledCharacter();
        if (!me) {
            // Can't find myself
            Log::Error("Failed to find me");
            return pending_reinvite.reset();
        }
        GW::PartyInfo* party = GW::PartyMgr::GetPartyInfo();
        if (!party || !party->players.valid()) {
            // Can't find party
            Log::Error("Failed to find party");
            return pending_reinvite.reset();
        }
        // Find leader and next valid player
        for (size_t i = 0; party->players.valid() && i < party->players.size(); i++) {
            auto& member = party->players[i];
            if (!member.connected()) {
                continue;
            }
            if (!first_player) {
                first_player = GW::PlayerMgr::GetPlayerByID(member.login_number);
            }
            else if (!next_player) {
                next_player = GW::PlayerMgr::GetPlayerByID(member.login_number);
                break;
            }
        }
        if (!first_player) {
            Log::Error("Failed to find party leader");
            return pending_reinvite.reset();
        }
        if (next_player && next_player->player_number == me->login_number) {
            // Swap next player to first player for reinviting myself
            next_player = first_player;
        }
        const bool is_leader = me->login_number == first_player->player_number;

        switch (pending_reinvite.stage) {
            case PendingReinvite::Stage::Kick: {
                if (!IsAgentInParty(pending_reinvite.identifier)) {
                    Log::Error("Choose target party member to reinvite");
                    return pending_reinvite.reset();
                }
                // Player kick
                GW::AgentLiving* target_living = nullptr;
                if (GetPlayerByAgentId(pending_reinvite.identifier, &target_living)) {
                    if (target_living == me) {
                        if (!next_player) {
                            Log::Error("Couldn't find next player in party to re-join");
                            return pending_reinvite.reset();
                        }
                        pending_reinvite.identifier = next_player->player_number;
                        pending_reinvite.stage = PendingReinvite::Stage::InvitePlayer;
                        GW::PartyMgr::LeaveParty();
                        return;
                    }
                    if (!is_leader) {
                        Log::Error("Only party leader can reinvite players");
                        return pending_reinvite.reset();
                    }
                    pending_reinvite.identifier = target_living->player_number;
                    pending_reinvite.stage = PendingReinvite::Stage::InvitePlayer;
                    GW::PartyMgr::KickPlayer(target_living->login_number);
                    return;
                }

                // Hero kick
                GW::PartyInfo* check = nullptr;
                const auto hero_info = GetHeroPartyMember(pending_reinvite.identifier, &check);
                if (hero_info) {
                    if (check != party) {
                        return;
                    }
                    if (hero_info->owner_player_id != me->login_number) {
                        Log::Error("The targetted hero doesn't belong to you");
                        return pending_reinvite.reset();
                    }
                    pending_reinvite.identifier = hero_info->hero_id;
                    GW::PartyMgr::KickHero(pending_reinvite.identifier);
                    pending_reinvite.stage = PendingReinvite::Stage::InviteHero;
                    return;
                }
                // Henchman kick
                const auto henchman_info = GetHenchmanPartyMember(pending_reinvite.identifier, &check);
                if (henchman_info) {
                    if (check != party) {
                        return;
                    }
                    if (!is_leader) {
                        Log::Error("Only party leader can reinvite henchmen");
                        return pending_reinvite.reset();
                    }
                    GW::PartyMgr::KickHenchman(pending_reinvite.identifier);
                    pending_reinvite.stage = PendingReinvite::Stage::InviteHenchman;
                    return;
                }
                Log::Error("Failed to determine agent type for %d", pending_reinvite.identifier);
                return pending_reinvite.reset();
            }
            case PendingReinvite::Stage::InviteHero: {
                const GW::HeroInfo* hero_info = GW::PartyMgr::GetHeroInfo(pending_reinvite.identifier);
                if (!hero_info) {
                    Log::Error("Failed to get hero info for %d", pending_reinvite.identifier);
                    return pending_reinvite.reset();
                }
                if (hero_info->agent_id && IsHeroInParty(hero_info->agent_id)) {
                    return;
                }
                GW::PartyMgr::AddHero(pending_reinvite.identifier);
                return pending_reinvite.reset();
            }
            case PendingReinvite::Stage::InvitePlayer: {
                const GW::Player* player = GW::PlayerMgr::GetPlayerByID(pending_reinvite.identifier);
                if (!player) {
                    Log::Error("Failed to get player info for %d", pending_reinvite.identifier);
                    return pending_reinvite.reset();
                }
                if (IsPlayerInParty(pending_reinvite.identifier)) {
                    return;
                }
                const auto aliases = PartyWindowModule::Instance().GetAliasedPlayerNames();
                if (aliases.contains(player->name)) {
                    const auto orig_name = aliases.at(player->name).c_str();
                    GW::PartyMgr::InvitePlayer(const_cast<wchar_t*>(orig_name));
                }
                else {
                    GW::PartyMgr::InvitePlayer(pending_reinvite.identifier);
                }
                return pending_reinvite.reset();
            }
            case PendingReinvite::Stage::InviteHenchman: {
                if (!IsHenchman(pending_reinvite.identifier)) {
                    Log::Error("Failed to get henchman info for %d", pending_reinvite.identifier);
                    return pending_reinvite.reset();
                }
                if (IsHenchmanInParty(pending_reinvite.identifier)) {
                    return;
                }
                GW::PartyMgr::AddHenchman(pending_reinvite.identifier);
                return pending_reinvite.reset();
            }
        }
        pending_reinvite.reset();
    }

    // Check and re-render item tooltips if modifier key held
    void UpdateItemTooltip()
    {
        if (GetKeyState(modifier_key_item_descriptions) == modifier_key_item_descriptions_key_state) {
            return;
        }
        modifier_key_item_descriptions_key_state = GetKeyState(modifier_key_item_descriptions);
        if (IsExplorable()) {
            if (!disable_item_descriptions_in_explorable) {
                return;
            }
        }
        else if (IsOutpost()) {
            if (!disable_item_descriptions_in_outpost) {
                return;
            }
        }
        else {
            return; // Loading
        }
        const auto tooltip = GW::UI::GetCurrentTooltip();
        if (!tooltip)
            return;
        // Trigger re-render of item tooltip
        const auto hovered_item = GW::Items::GetHoveredItem();
        if (!hovered_item) {
            return;
        }
        uint32_t items_triggered[2]{};
        const auto inv = GW::Items::GetInventory();
        if (hovered_item == inv->weapon_set0 || hovered_item == inv->offhand_set0) {
            items_triggered[0] = inv->weapon_set0 ? inv->weapon_set0->item_id : 0;
            items_triggered[1] = inv->offhand_set0 ? inv->offhand_set0->item_id : 0;
        }
        else if (hovered_item == inv->weapon_set1 || hovered_item == inv->offhand_set1) {
            items_triggered[0] = inv->weapon_set1 ? inv->weapon_set1->item_id : 0;
            items_triggered[1] = inv->offhand_set1 ? inv->offhand_set1->item_id : 0;
        }
        else if (hovered_item == inv->weapon_set2 || hovered_item == inv->offhand_set2) {
            items_triggered[0] = inv->weapon_set2 ? inv->weapon_set2->item_id : 0;
            items_triggered[1] = inv->offhand_set2 ? inv->offhand_set2->item_id : 0;
        }
        else if (hovered_item == inv->weapon_set3 || hovered_item == inv->offhand_set3) {
            items_triggered[0] = inv->weapon_set3 ? inv->weapon_set3->item_id : 0;
            items_triggered[1] = inv->offhand_set3 ? inv->offhand_set3->item_id : 0;
        }
        else {
            items_triggered[0] = hovered_item->item_id;
            items_triggered[1] = 0;
        }
        GW::GameThread::Enqueue([items = items_triggered] {
            if (items[0]) {
                GW::UI::SendFrameUIMessage(GW::UI::GetChildFrame(GW::UI::GetRootFrame(), 0xffffffff), GW::UI::UIMessage::kItemUpdated, &items[0]);
            }
            if (items[1]) {
                GW::UI::SendFrameUIMessage(GW::UI::GetChildFrame(GW::UI::GetRootFrame(), 0xffffffff), GW::UI::UIMessage::kItemUpdated, &items[0]);
            }
        });
    }

    // Check and re-render item tooltips if modifier key held
    void UpdateSkillTooltip()
    {
        if (GetKeyState(modifier_key_skill_descriptions) == modifier_key_skill_descriptions_key_state) {
            return;
        }
        modifier_key_skill_descriptions_key_state = GetKeyState(modifier_key_skill_descriptions);
        if (IsExplorable()) {
            if (!disable_skill_descriptions_in_explorable) {
                return;
            }
        }
        else if (IsOutpost()) {
            if (!disable_skill_descriptions_in_outpost) {
                return;
            }
        }
        else {
            return; // Loading
        }
        // Trigger re-render of the tooltip by triggering a fake change of concise skill descriptions ui message
        auto skill = GW::SkillbarMgr::GetHoveredSkill();
        if (!skill) {
            return;
        }
        if (GW::SkillbarMgr::GetHoveredSkill()) {
            GW::GameThread::Enqueue([skill] {
                GW::UI::SendFrameUIMessage(GW::UI::GetChildFrame(GW::UI::GetRootFrame(), 0xffffffff), GW::UI::UIMessage::kTitleProgressUpdated, (void*)skill->title);
            });
        }
    }

    // Override the login status dropdown by sending ui message 0x51 if found
    void OverrideDefaultOnlineStatus() {
        uint32_t value_override = 0;
        switch (static_cast<GW::FriendStatus>(last_online_status)) {
        case GW::FriendStatus::Online: value_override = 0; break;
        case GW::FriendStatus::Away: value_override = 1; break;
        case GW::FriendStatus::DND: value_override = 2; break;
        case GW::FriendStatus::Offline: value_override = 3;break;
        }
        GW::GameThread::Enqueue([value_override]() {
            const auto frame = GW::UI::GetFrameByLabel(L"StatusOverride");
            if (!frame) return;
            GW::UI::SendFrameUIMessage(frame, (GW::UI::UIMessage)0x51, (void*)value_override);
            });
    }
    
    GW::HookEntry OnCreateUIComponent_Entry;
    // Flag email address entry field as a password format (e.g. asterisks instead of email)
    void OnCreateUIComponent(GW::UI::CreateUIComponentPacket* msg)
    {
        if (!(msg && msg->component_label))
            return;
        if (hide_email_address && wcscmp(msg->component_label, L"EditAccount") == 0) {
            msg->component_flags |= 0x01000000;
        }
        if (wcscmp(msg->component_label, L"StatusOverride") == 0) {
            OverrideDefaultOnlineStatus();
        }
    }

    void OnKeyDownAction(GW::HookStatus*, uint32_t key)
    {
        switch (static_cast<GW::UI::ControlAction>(key)) {
            case GW::UI::ControlAction_TargetNearestItem:
                if (lazy_chest_looting) {
                    targeting_nearest_item = true;
                    GW::Agents::ChangeTarget(0u); // To ensure OnChangeTarget is triggered
                }
                break;
        }
    }

    void OnKeyUpAction(GW::HookStatus*, uint32_t key)
    {
        switch (static_cast<GW::UI::ControlAction>(key)) {
            case GW::UI::ControlAction_TargetNearestItem:
                targeting_nearest_item = false;
                break;
        }
    }

    // Logic for targetting nearest item; Don't target chest as nearest item, Target green items from chest last
    void OnChangeTarget(GW::HookStatus* status, GW::UI::UIMessage, void* wParam, void*)
    {
        if (!targeting_nearest_item) {
            return;
        }
        const auto msg = static_cast<GW::UI::ChangeTargetUIMsg*>(wParam);
        auto chosen_target = GW::Agents::GetAgentByID(msg->manual_target_id);
        if (!chosen_target) {
            return;
        }
        uint32_t override_manual_agent_id = 0;
        const GW::Item* target_item = nullptr;
        const auto agents = GW::Agents::GetAgentArray();
        const auto me = agents ? GW::Agents::GetControlledCharacter() : nullptr;
        if (!me) {
            return;
        }
        // If the item targeted is a green that belongs to me, and its next to the chest, try to find another item instead.
        if (chosen_target->GetIsItemType() && static_cast<GW::AgentItem*>(chosen_target)->owner == me->agent_id) {
            target_item = GW::Items::GetItemById(static_cast<GW::AgentItem*>(chosen_target)->item_id);
            if (!(target_item && (target_item->interaction & 0x10) == 0)) {
                return; // Failed to get target item, or is not green.
            }
            for (auto* agent : *agents) {
                if (!(agent && agent->GetIsGadgetType())) {
                    continue;
                }
                if (GetDistance(agent->pos, chosen_target->pos) <= GW::Constants::Range::Nearby) {
                    // Choose the chest as the target instead of this green item, and drop through to the next loop
                    chosen_target = agent;
                    override_manual_agent_id = agent->agent_id;
                    break;
                }
            }
        }

        // If we're targeting a gadget (presume its the chest), try to find adjacent items that belong to me instead.
        if (chosen_target->GetIsGadgetType()) {
            float closest_item_dist = GW::Constants::Range::Compass;
            for (auto* agent : *agents) {
                if (!(agent && agent->GetIsItemType())) {
                    continue;
                }
                const auto agent_item = agent->GetAsAgentItem();
                if (agent_item->owner != me->agent_id) {
                    continue;
                }
                target_item = GW::Items::GetItemById(agent_item->item_id);
                if ((target_item->interaction & 0x10) != 0) {
                    continue; // Don't target green items.
                }
                if (GetDistance(agent->pos, chosen_target->pos) > GW::Constants::Range::Nearby) {
                    continue;
                }
                const float dist = GetDistance(me->pos, agent->pos);
                if (dist > closest_item_dist) {
                    continue;
                }
                override_manual_agent_id = agent->agent_id;
                closest_item_dist = dist;
            }
        }
        if (override_manual_agent_id) {
            status->blocked = true;
            GW::Agents::ChangeTarget(override_manual_agent_id);
        }
    }

    bool IsOnline(const GW::FriendStatus status)
    {
        switch (status) {
            case GW::FriendStatus::Away:
            case GW::FriendStatus::DND:
            case GW::FriendStatus::Online:
                return true;
        }
        return false;
    }

    void FriendStatusCallback(
        GW::HookStatus*, const GW::Friend* old_state, const GW::Friend* new_state)
    {
        if (!(new_state && old_state)) {
            return; // Friend added, or deleted
        }
        if (old_state->status == GW::FriendStatus::Unknown) {
            return; // First info about friend
        }
        const bool was_online = IsOnline(old_state->status);
        const bool is_online = IsOnline(new_state->status);
        if (was_online == is_online) {
            return;
        }
        if (is_online && notify_when_friends_online) {
            WriteChat(GW::Chat::Channel::CHANNEL_GLOBAL, std::format(L"<a=1>{}</a> ({}) has just logged in.", new_state->charname, new_state->alias).c_str());
        }
        else if (notify_when_friends_offline) {
            WriteChat(GW::Chat::Channel::CHANNEL_GLOBAL, std::format(L"{} ({}) has just logged out.", old_state->charname, old_state->alias).c_str());
        }
    }

    void DrawNotificationsSettings()
    {
        ImGui::Text("Flash Guild Wars taskbar icon when:");
        ImGui::ShowHelp("Only triggers when Guild Wars is not the active window");
        ImGui::Indent();
        ImGui::StartSpacedElements(checkbox_w);
        ImGui::NextSpacedElement();
        ImGui::Checkbox("Zoning in a new map", &flash_window_on_zoning);
        ImGui::NextSpacedElement();
        ImGui::Checkbox("Cinematic start/end", &flash_window_on_cinematic);
        ImGui::NextSpacedElement();
        ImGui::Checkbox("A player starts trade with you###flash_window_on_trade", &flash_window_on_trade);
        ImGui::NextSpacedElement();
        ImGui::Checkbox("A party member pings your name", &flash_window_on_name_ping);
        ImGui::Unindent();

        ImGui::Text("Show Guild Wars in foreground when:");
        ImGui::ShowHelp("When enabled, GWToolbox++ can automatically restore\n"
            "the window from a minimized state when important events\n"
            "occur, such as entering instances.");
        ImGui::Indent();
        ImGui::StartSpacedElements(checkbox_w);
        ImGui::NextSpacedElement();
        ImGui::Checkbox("Launching GWToolbox++###focus_window_on_launch", &focus_window_on_launch);
        ImGui::NextSpacedElement();
        ImGui::Checkbox("Zoning in a new map###focus_window_on_zoning", &focus_window_on_zoning);
        ImGui::NextSpacedElement();
        ImGui::Checkbox("A player starts trade with you###focus_window_on_trade", &focus_window_on_trade);
        ImGui::Unindent();

        ImGui::Text("Show a chat message when a friend:");
        ImGui::Indent();
        ImGui::StartSpacedElements(checkbox_w);
        ImGui::NextSpacedElement();
        ImGui::Checkbox("Logs in", &notify_when_friends_online);
        ImGui::NextSpacedElement();
        ImGui::Checkbox("Joins your outpost###notify_when_friends_join_outpost", &notify_when_friends_join_outpost);
        ImGui::NextSpacedElement();
        ImGui::Checkbox("Logs out", &notify_when_friends_offline);
        ImGui::NextSpacedElement();
        ImGui::Checkbox("Leaves your outpost###notify_when_friends_leave_outpost", &notify_when_friends_leave_outpost);
        ImGui::Unindent();

        ImGui::Text("Show a chat message when a player:");
        ImGui::Indent();
        ImGui::StartSpacedElements(checkbox_w);
        ImGui::NextSpacedElement();
        ImGui::Checkbox("Joins your party", &notify_when_party_member_joins);
        ImGui::NextSpacedElement();
        ImGui::Checkbox("Joins your outpost###notify_when_players_join_outpost", &notify_when_players_join_outpost);
        ImGui::NextSpacedElement();
        ImGui::Checkbox("Leaves your party", &notify_when_party_member_leaves);
        ImGui::NextSpacedElement();
        ImGui::Checkbox("Leaves your outpost###notify_when_players_leave_outpost", &notify_when_players_leave_outpost);
        ImGui::Unindent();
    }


    GW::Constants::QuestID player_requested_active_quest_id = GW::Constants::QuestID::None;

    GW::HookEntry OnQuestUIMessage_HookEntry;

    void OnPostQuestUIMessage(const GW::HookStatus* status, const GW::UI::UIMessage message_id, void*, void*)
    {
        switch (message_id) {
            case GW::UI::UIMessage::kSendSetActiveQuest:
                if (status->blocked) {
                    break;
                }
                player_requested_active_quest_id = GW::QuestMgr::GetActiveQuestId();
                break;
            case GW::UI::UIMessage::kQuestAdded:
                if (status->blocked) {
                    break;
                }
                if (GW::QuestMgr::GetActiveQuestId() == player_requested_active_quest_id) {
                    break;
                }
                if (keep_current_quest_when_new_quest_added) {
                    // Re-request a quest change
                    const auto quest = GW::QuestMgr::GetQuest(player_requested_active_quest_id);
                    if (!quest) {
                        break;
                    }
                    GW::Packet::StoC::QuestAdd packet;
                    packet.quest_id = quest->quest_id;
                    packet.marker = quest->marker;
                    packet.map_to = quest->map_to;
                    packet.log_state = quest->log_state;
                    packet.map_from = quest->map_from;
                    wcscpy(packet.location, quest->location);
                    wcscpy(packet.name, quest->name);
                    wcscpy(packet.npc, quest->npc);
                    GW::StoC::EmulatePacket(&packet); // Why? Can we not send more ui messages if thats the need?
                    GW::QuestMgr::SetActiveQuestId(quest->quest_id);
                }

                break;
        }
    }

    void CHAT_CMD_FUNC(CmdReinvite)
    {
        pending_reinvite.reset(current_party_target_id);
    }

    bool ShouldBlockEffect(uint32_t effect_id) {
        if (effect_id >= 1292 && effect_id <= 1303) {
            return block_fireworks;
        }
        if (effect_id >= 1685 && effect_id <= 1687) {
            return block_fireworks;
        }
        switch (effect_id) {
        case 905:
            return block_snowman_summoner;
        case 1688:
            return block_bottle_rockets;
        case 1689:
            return block_party_poppers;
        case 758:  // Chocolate bunny
        case 2063: // e.g. Fruitcake, sugary blue drink
        case 1176: // e.g. Delicious cake
            return block_sugar_rush_effect;
        case 1491:
            return block_transmogrify_effect;
        }
        return false;
    }

    void OnPlayEffect(GW::HookStatus* status, const GW::Packet::StoC::PlayEffect* pak)
    {
        status->blocked |= ShouldBlockEffect(pak->effect_id);
    }

    // Block full item descriptions
    void OnGetItemDescription(uint32_t, uint32_t, uint32_t, uint32_t, wchar_t**, wchar_t** out_desc) 
    {
        bool block_description = disable_item_descriptions_in_outpost && IsOutpost() || disable_item_descriptions_in_explorable && IsExplorable();
        block_description = block_description && GetKeyState(modifier_key_item_descriptions) >= 0;

        if (block_description && out_desc) {
            *out_desc = nullptr;
        }
    }

    const wchar_t* GetPartySearchLeader(uint32_t party_search_id) {
        const auto p = GW::PartyMgr::GetPartySearch(party_search_id);
        return p && p->party_leader && *p->party_leader ? p->party_leader : nullptr;
    }


    bool mission_prompted = false;

    // We've just asked the game to enter mission; check (and prompt) if we should really be in NM or HM instead
    void CheckPromptBeforeEnterMission(GW::HookStatus* status) {
        if (!check_and_prompt_if_mission_already_completed)
            return;
        if (mission_prompted || GW::PartyMgr::GetPartyPlayerCount() > 1)
            return;
        const auto map_id = GW::Map::GetMapID();
        const auto nm_complete = CompletionWindow::IsAreaComplete(map_id, CompletionCheck::NormalMode);
        const auto hm_complete = CompletionWindow::IsAreaComplete(map_id, CompletionCheck::HardMode);

        auto on_enter_mission_prompt = [](bool result, void*) {
            mission_prompted = true;
            if (result) {
                // User want to change mode
                GW::PartyMgr::SetHardMode(!GW::PartyMgr::GetIsPartyInHardMode());
            }
            };
        const char* confirm_text = nullptr;
        if (GW::PartyMgr::GetIsPartyInHardMode() && hm_complete && !nm_complete) {
            confirm_text = "You're about to enter a mission in Hard Mode,\nbut you've already completed it on this character.\n\nWould you like to switch to Normal Mode?";
        }
        if (!GW::PartyMgr::GetIsPartyInHardMode() && GW::PartyMgr::GetIsHardModeUnlocked() && nm_complete && !hm_complete) {
            confirm_text = "You're about to enter a mission in Normal Mode,\nbut you've already completed it on this character.\n\nWould you like to switch to Hard Mode?";
        }
        if (confirm_text) {
            ImGui::ConfirmDialog(confirm_text, on_enter_mission_prompt);

            // TODO: Re-enable the clicked dialog button if it was triggered via talking to NPC
            DialogModule::ReloadDialog();
            status->blocked = true;
        }
    }

    GW::HookEntry OnPreUIMessage_HookEntry;
    void OnPreUIMessage(GW::HookStatus* status, GW::UI::UIMessage message_id, void* wParam, void*) {
        switch (message_id) {
        case GW::UI::UIMessage::kSendCallTarget: {
            auto packet = (GW::UI::UIPacket::kSendCallTarget*)wParam;
            const auto agent = static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(packet->agent_id));
            if (packet->call_type == GW::CallTargetType::Following && agent
                && agent->GetIsLivingType() && agent->allegiance == GW::Constants::Allegiance::Enemy) {
                packet->call_type = GW::CallTargetType::AttackingOrTargetting;
            }
        } break;
        case GW::UI::UIMessage::kMapLoaded: {
            mission_prompted = false;
        } break;
        case GW::UI::UIMessage::kSendAgentDialog: {
            const auto dialog_id = (uint32_t)wParam;
            const auto& buttons = DialogModule::GetDialogButtons();
            const auto button = std::ranges::find_if(buttons, [dialog_id](GW::UI::DialogButtonInfo* btn) {
                return btn->dialog_id == dialog_id && btn->message && wcscmp(btn->message, L"\x8101\x13D5\x8B48\xD2EF\x7E5A") == 0;
                });
            if (button == buttons.end())
                break;
            CheckPromptBeforeEnterMission(status);
            if (status->blocked) {
                DialogModule::ReloadDialog();
            }
        } break;
        case GW::UI::UIMessage::kSendEnterMission: {
            CheckPromptBeforeEnterMission(status);
            if (status->blocked) {
                // Re-enable the enter mission button if triggered via party window
                uint32_t packet = 0;
                GW::UI::SendUIMessage((GW::UI::UIMessage)0x10000128, &packet);
            }
            break;
        }
        }
    }

    GW::HookEntry OnPostUIMessage_HookEntry;
    void OnPostUIMessage(GW::HookStatus* status, GW::UI::UIMessage message_id, void* wParam, void*) {
        if (status->blocked)
            return;
        switch (message_id) {
        case GW::UI::UIMessage::kTradeSessionStart: {
            if (flash_window_on_trade) {
                FlashWindow();
            }
            if (focus_window_on_trade) {
                FocusWindow();
            }
        } break;
        case GW::UI::UIMessage::kPartySearchInviteSent: {
            // Automatically send a party window invite when a party search invite is sent
            const auto packet = (GW::UI::UIPacket::kPartySearchInvite*)wParam;
            if(GW::PartyMgr::GetIsLeader())
                GW::PartyMgr::InvitePlayer(GetPartySearchLeader(packet->source_party_search_id));            
        } break;
        case GW::UI::UIMessage::kPreferenceValueChanged: {
            const auto packet = (GW::UI::UIPacket::kPreferenceValueChanged*)wParam;
            if (packet->preference_id == GW::UI::NumberPreference::TextLanguage)
                FontLoader::LoadFonts(true);
        } break;
        case GW::UI::UIMessage::kPartyDefeated: {
            if (auto_return_on_defeat && GW::PartyMgr::GetIsLeader() && !GW::PartyMgr::ReturnToOutpost())
                Log::Warning("Failed to return to outpost");
        } break;
        case GW::UI::UIMessage::kMapLoaded: {
            last_online_status = static_cast<uint32_t>(GW::FriendListMgr::GetMyStatus());
        } break;
        }
    }

}

bool GameSettings::GetSettingBool(const char* setting)
{
#define RETURN_SETTING_IF_MATCH(var) if (strcmp(setting, #var) == 0) return var
    RETURN_SETTING_IF_MATCH(auto_age2_on_age);
    RETURN_SETTING_IF_MATCH(flash_window_on_name_ping);
    RETURN_SETTING_IF_MATCH(move_materials_to_current_storage_pane);
    RETURN_SETTING_IF_MATCH(move_item_to_current_storage_pane);
    RETURN_SETTING_IF_MATCH(move_item_on_ctrl_click);

    ASSERT("Failed to find valid setting" && false);
    return false;
}

void GameSettings::PingItem(GW::Item* item, const uint32_t parts)
{
    if (!(item && GW::PlayerMgr::GetPlayerByID())) {
        return;
    }
    std::wstring out;
    if (parts & NAME && item->complete_name_enc) {
        if (out.length()) {
            out += L"\x2\x102\x2";
        }
        out += item->complete_name_enc;
    }
    else if (parts & NAME && item->name_enc) {
        if (out.length()) {
            out += L"\x2\x102\x2";
        }
        out += item->name_enc;
    }
    if (parts & DESC && item->info_string) {
        if (out.length()) {
            out += L"\x2\x102\x2";
        }
        out += ShorthandItemDescription(item);
    }

    PendingChatMessage* m = PendingChatMessage::queueSend(GW::Chat::Channel::CHANNEL_GROUP, out.c_str(), GW::PlayerMgr::GetPlayerByID()->name_enc);
    if (m) {
        ChatSettings::AddPendingMessage(m);
    }
}

void GameSettings::PingItem(const uint32_t item_id, const uint32_t parts)
{
    return PingItem(GW::Items::GetItemById(item_id), parts);
}

PendingChatMessage::PendingChatMessage(const GW::Chat::Channel channel, const wchar_t* enc_message, const wchar_t* enc_sender)
    : channel(channel)
{
    invalid = !enc_message || !enc_sender;
    if (!invalid) {
        wcscpy(encoded_sender, enc_sender);
        wcscpy(encoded_message, enc_message);
        Init();
    }
}

PendingChatMessage* PendingChatMessage::queueSend(const GW::Chat::Channel channel, const wchar_t* enc_message, const wchar_t* enc_sender)
{
    const auto m = new PendingChatMessage(channel, enc_message, enc_sender);
    if (m->invalid) {
        delete m;
        return nullptr;
    }
    m->SendIt();
    return m;
}

PendingChatMessage* PendingChatMessage::queuePrint(const GW::Chat::Channel channel, const wchar_t* enc_message, const wchar_t* enc_sender)
{
    const auto m = new PendingChatMessage(channel, enc_message, enc_sender);
    if (m->invalid) {
        delete m;
        return nullptr;
    }
    return m;
}

bool PendingChatMessage::Cooldown()
{
    return last_send && clock() < last_send + CLOCKS_PER_SEC / 2;
}

bool PendingChatMessage::Send()
{
    if (!IsDecoded() || this->invalid) {
        return false; // Not ready or invalid.
    }
    const std::vector<std::wstring> sanitised_lines = SanitiseForSend();
    wchar_t buf[120];
    size_t len = 0;
    for (size_t i = 0; i < sanitised_lines.size(); i++) {
        const size_t san_len = sanitised_lines[i].length();
        const wchar_t* str = sanitised_lines[i].c_str();
        if (len + san_len + 3 > 120) {
            GW::Chat::SendChat('#', buf);
            buf[0] = '\0';
            len = 0;
        }
        if (len) {
            buf[len] = ',';
            buf[len + 1] = ' ';
            len += 2;
        }
        for (size_t j = 0; j < san_len; j++) {
            buf[len] = str[j];
            len++;
        }
        buf[len] = '\0';
    }
    if (len) {
        GW::Chat::SendChat('#', buf);
        last_send = clock();
    }
    printed = true;
    return printed;
}

void PendingChatMessage::Init()
{
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

std::vector<std::wstring> PendingChatMessage::SanitiseForSend()
{
    const std::wregex no_tags(L"<[^>]+>");
    const std::wregex no_new_lines(L"\n");
    std::wstring sanitised, sanitised2, temp;
    std::regex_replace(std::back_inserter(sanitised), output_message.begin(), output_message.end(), no_tags, L"");
    std::regex_replace(std::back_inserter(sanitised2), sanitised.begin(), sanitised.end(), no_new_lines, L"|");
    std::vector<std::wstring> parts;
    std::wstringstream wss(sanitised2);
    while (std::getline(wss, temp, L'|')) {
        parts.push_back(temp);
    }
    return parts;
}

bool PendingChatMessage::PrintMessage()
{
    if (!IsDecoded() || this->invalid) {
        return false; // Not ready or invalid.
    }
    if (this->printed) {
        return true; // Already printed.
    }
    wchar_t buffer[512];
    switch (channel) {
        case GW::Chat::Channel::CHANNEL_EMOTE:
            GW::Chat::Color dummy, messageCol;                                        // Needed for GW::Chat::GetChannelColors
            GetChannelColors(GW::Chat::Channel::CHANNEL_ALLIES, &dummy, &messageCol); // ...but set the message to be same color as ally chat
            swprintf(buffer, 512, L"<a=2>%ls</a>: <c=#%06X>%ls</c>", output_sender.c_str(), messageCol & 0x00FFFFFF, output_message.c_str());
            WriteChat(channel, buffer);
            break;
        default:
            swprintf(buffer, 512, L"<a=2>%ls</a>: %ls", output_sender.c_str(), output_message.c_str());
            WriteChat(channel, buffer);
            break;
    }
    output_message.clear();
    output_sender.clear();
    printed = true;
    return printed;
};

/*
*   GWToolbox chat log end
*/

void GameSettings::Initialize()
{
    ToolboxModule::Initialize();
    //return;
    // Patch that allow storage page (and Anniversary page) to work.
    uintptr_t address = GW::Scanner::Find("\xEB\x17\x33\xD2\x8D\x4A\x06\xEB", "xxxxxxxx", -4);
    if (address) {
        // Xunlai Chest has a behavior where if you
        // 1. Open chest on page 1 to 14
        // 2. Close chest & open it again
        // -> You should still be on the same page
        // But, if you try with the material page (or anniversary page in the case when you bought all other storage page)
        // you will get back the the page 1. I think it was a intended use for material page & forgot to fix it
        // when they added anniversary page so we do it ourself.

        // @Cleanup: Change this to be a post CreateUIComponent hook instead
        constexpr DWORD page_max = 14;
        ctrl_click_patch.SetPatch(address, (const char*)&page_max, 1);
        ctrl_click_patch.TogglePatch(true);
    }
    Log::Log("[GameSettings] ctrl_click_patch = %p\n", ctrl_click_patch.GetAddress());

    SkillList_UICallback_Func = (GW::UI::UIInteractionCallback)GW::Scanner::ToFunctionStart(GW::Scanner::FindAssertion("GmCtlSkList.cpp", "!obj", 0xc71,0));
    Log::Log("[GameSettings] SkillList_UICallback_Func = %p\n", SkillList_UICallback_Func);
   
    address = GW::Scanner::Find("\x81\xff\x86\x02\x00\x00", "xxxxxx", 6);
    if (address)
        skip_map_entry_message_patch.SetPatch(address, "\x90\xe9", 2);
    Log::Log("[GameSettings] skip_map_entry_message_patch = %p\n", skip_map_entry_message_patch.GetAddress());

    address = GW::Scanner::Find("\xF7\x40\x0C\x10\x00\x02\x00\x75", "xxxxxx??", +7);
    if (address)
        gold_confirm_patch.SetPatch(address, "\x90\x90", 2);
    Log::Log("[GameSettings] gold_confirm_patch = %p\n", gold_confirm_patch.GetAddress());

    address = GW::Scanner::Find("\xdf\xe0\xf6\xc4\x41\x7a\x79", "xxxxxxx", 0x5);
    if (address)
        remove_skill_warmup_duration_patch.SetPatch(address, "\x90\x90", 2);
    Log::Log("[GameSettings] remove_skill_warmup_duration_patch = %p\n", remove_skill_warmup_duration_patch.GetAddress());

    ItemDescriptionHandler::RegisterDescriptionCallback(OnGetItemDescription, 9999);

    // Call our CreateCodedTextLabel function instead of default CreateCodedTextLabel for patching skill descriptions
    address = GW::Scanner::FindAssertion("GmTipSkill.cpp", "!(m_tipSkillFlags & TipSkillMsgCreate::FLAG_SHOW_ENABLE_AI_HINT)", 0, 0x7b);
    if (address) {
        CreateEncodedTextLabel_Func = (CreateCodedTextLabel_pt)GW::Scanner::FunctionFromNearCall(address);
        skill_description_patch.SetRedirect(address, CreateCodedTextLabel_SkillDescription);
        skill_description_patch.TogglePatch(true);
    }
    Log::Log("[GameSettings] CreateEncodedTextLabel_Func = %p\n", CreateEncodedTextLabel_Func);
    Log::Log("[GameSettings] skill_description_patch = %p\n", skill_description_patch.GetAddress());

    // See OnAgentAllegianceChanged
    address = GW::Scanner::Find("\x81\xce\xa0\x06\x00\x00", "xxxxxx");
    if (address)
        address = GW::Scanner::FunctionFromNearCall(GW::Scanner::FindInRange("\xe8", "x", 0, address, address + 0xff));
    if (address) {
        SetGlobalNameTagVisibility_Func = (SetGlobalNameTagVisibility_pt)address;
        if (GW::Scanner::IsValidPtr(*(uintptr_t*)(address + 0xa)))
            GlobalNameTagVisibilityFlags = *(uint32_t**)(address + 0xa);
        else if (GW::Scanner::IsValidPtr(*(uintptr_t*)(address + 0xb)))
            GlobalNameTagVisibilityFlags = *(uint32_t**)(address + 0xb);
        GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::AgentUpdateAllegiance>(&PartyDefeated_Entry, &OnAgentAllegianceChanged);
    }
    Log::Log("[GameSettings] SetGlobalNameTagVisibility_Func = %p", (void*)SetGlobalNameTagVisibility_Func);
    Log::Log("[GameSettings] GlobalNameTagVisibilityFlags = %p", static_cast<void*>(GlobalNameTagVisibilityFlags));

    address = GW::Scanner::Find("\x8b\x7d\x08\x8b\x70\x2c\x83\xff\x0f", "xxxxxxxxx");
    if (address) {
        ShowAgentFactionGain_Func = (ShowAgentFactionGain_pt)GW::Scanner::FunctionFromNearCall(address + 0x6c);
        ShowAgentExperienceGain_Func = (ShowAgentExperienceGain_pt)GW::Scanner::FunctionFromNearCall(address + 0x4f);
    }
    Log::Log("[GameSettings] ShowAgentFactionGain_Func = %p\n", (void*)ShowAgentFactionGain_Func);
    Log::Log("[GameSettings] ShowAgentExperienceGain_Func = %p\n", (void*)ShowAgentExperienceGain_Func);

    FadeFrameContent_Func = (FadeFrameContent_pt)GW::Scanner::ToFunctionStart(GW::Scanner::FindAssertion("\\Code\\Engine\\Frame\\FrApi.cpp", "sourceOpacity >= 0",0,0));
    printf("[GameSettings] FadeFrameContent_Func = %p\n", (void*)FadeFrameContent_Func);

#ifdef _DEBUG
    ASSERT(ctrl_click_patch.IsValid());
    ASSERT(SkillList_UICallback_Func);
    ASSERT(skip_map_entry_message_patch.IsValid());
    ASSERT(gold_confirm_patch.IsValid());
    ASSERT(remove_skill_warmup_duration_patch.IsValid());
    ASSERT(CreateEncodedTextLabel_Func);
    ASSERT(skill_description_patch.IsValid());
    ASSERT(SetGlobalNameTagVisibility_Func);
    ASSERT(GlobalNameTagVisibilityFlags);
    ASSERT(ShowAgentFactionGain_Func);
    ASSERT(ShowAgentExperienceGain_Func);
    ASSERT(FadeFrameContent_Func);
#endif

    if (SkillList_UICallback_Func) {
        GW::HookBase::CreateHook((void**)&SkillList_UICallback_Func, OnSkillList_UICallback, reinterpret_cast<void**>(&SkillList_UICallback_Ret));
        GW::HookBase::EnableHooks(SkillList_UICallback_Func);
    }

    if (ShowAgentFactionGain_Func) {
        GW::HookBase::CreateHook((void**)&ShowAgentFactionGain_Func, OnShowAgentFactionGain, reinterpret_cast<void**>(&ShowAgentFactionGain_Ret));
        GW::HookBase::EnableHooks(ShowAgentFactionGain_Func);
    }
    if (ShowAgentExperienceGain_Func) {
        GW::HookBase::CreateHook((void**)&ShowAgentExperienceGain_Func, OnShowAgentExperienceGain, reinterpret_cast<void**>(&ShowAgentExperienceGain_Ret));
        GW::HookBase::EnableHooks(ShowAgentExperienceGain_Func);
    }
    if (FadeFrameContent_Func) {
        GW::HookBase::CreateHook((void**)&FadeFrameContent_Func, OnFadeFrameContent, reinterpret_cast<void**>(&FadeFrameContent_Ret));
        GW::HookBase::EnableHooks(FadeFrameContent_Func);
    }


    RegisterUIMessageCallback(&OnDialog_Entry, GW::UI::UIMessage::kSendAgentDialog, bind_member(this, &GameSettings::OnFactionDonate));
    RegisterUIMessageCallback(&OnDialog_Entry, GW::UI::UIMessage::kSendLoadSkillbar, &OnPreLoadSkillBar);
    GW::StoC::RegisterPacketCallback(&OnDialog_Entry, GAME_SMSG_SKILLBAR_UPDATE, OnPostLoadSkillBar, 0x8000);
    GW::StoC::RegisterPacketCallback(&OnDialog_Entry, GAME_SMSG_SKILL_UPDATE_SKILL_COUNT_1, OnUpdateSkillCount, -0x3000);
    GW::StoC::RegisterPacketCallback(&OnDialog_Entry, GAME_SMSG_SKILL_UPDATE_SKILL_COUNT_2, OnUpdateSkillCount, -0x3000);

    //GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::PartyDefeated>(&PartyDefeated_Entry, &GameSettings::OnPartyDefeated);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericValue>(&PartyDefeated_Entry, [this](GW::HookStatus* status, GW::Packet::StoC::GenericValue* packet) {
        switch (packet->value_id) {
            case 11:
                OnAgentMarker(status, packet);
            case 21:
                OnAgentEffect(status, packet);
                break;
            case 22:
                //OnAgentAnimation(status, packet);
                break;
            case 28:
                OnAgentLoopingAnimation(status, packet);
                break;
        }
    });

    // Sanity check to prevent GW crash trying to despawn an agent that we may have already despawned.
    /*GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentRemove>(&PartyDefeated_Entry, [](GW::HookStatus* status, GW::Packet::StoC::AgentRemove* packet) {
        if (false && !GW::Agents::GetAgentByID(packet->agent_id))
            status->blocked = true;
        });*/
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::PlayEffect>(&TradeStart_Entry, OnPlayEffect);
    GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::PartyInviteReceived_Create>(&PartyPlayerAdd_Entry, OnPartyInviteReceived);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::PartyPlayerAdd>(&PartyPlayerAdd_Entry, bind_member(this, &GameSettings::OnPartyPlayerJoined));
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GameSrvTransfer>(&GameSrvTransfer_Entry, OnMapTravel);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::CinematicPlay>(&CinematicPlay_Entry, OnCinematic);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::VanquishComplete>(&VanquishComplete_Entry, OnVanquishComplete);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::DungeonReward>(&VanquishComplete_Entry, bind_member(this, &GameSettings::OnDungeonReward));
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::MapLoaded>(&PlayerJoinInstance_Entry, OnMapLoaded);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::PlayerJoinInstance>(&PlayerJoinInstance_Entry, OnPlayerJoinInstance);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::PlayerLeaveInstance>(&PlayerLeaveInstance_Entry, OnPlayerLeaveInstance);
    //GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::AgentAdd>(&OnAfterAgentAdd_Entry, &OnAfterAgentAdd);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentAdd>(&PartyDefeated_Entry, OnAgentAdd);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentState>(&PartyDefeated_Entry, OnUpdateAgentState);
    // Trigger for message on party change
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::PartyPlayerRemove>(
        &PartyPlayerRemove_Entry,
        [&](const GW::HookStatus*, GW::Packet::StoC::PartyPlayerRemove*) {
            check_message_on_party_change = true;
        });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::ScreenShake>(&OnScreenShake_Entry, bind_member(this, &GameSettings::OnScreenShake));

    RegisterUIMessageCallback(&OnChangeTarget_Entry, GW::UI::UIMessage::kChangeTarget, OnChangeTarget);
    RegisterUIMessageCallback(&OnWriteChat_Entry, GW::UI::UIMessage::kWriteToChatLog, OnWriteChat);
    RegisterUIMessageCallback(&OnAgentStartCast_Entry, GW::UI::UIMessage::kAgentStartCasting, OnAgentStartCast);
    RegisterUIMessageCallback(&OnOpenWikiUrl_Entry, GW::UI::UIMessage::kOpenWikiUrl, OnOpenWiki);
    RegisterUIMessageCallback(&OnAgentNameTag_Entry, GW::UI::UIMessage::kShowAgentNameTag, OnAgentNameTag);
    RegisterUIMessageCallback(&OnAgentNameTag_Entry, GW::UI::UIMessage::kSetAgentNameTagAttribs, OnAgentNameTag);

    GW::UI::RegisterKeydownCallback(&OnChangeTarget_Entry, OnKeyDownAction);
    GW::UI::RegisterKeyupCallback(&OnChangeTarget_Entry, OnKeyUpAction);

    GW::FriendListMgr::RegisterFriendStatusCallback(&FriendStatusCallback_Entry, FriendStatusCallback);
    RegisterUIMessageCallback(&OnPreSendDialog_Entry, GW::UI::UIMessage::kSendPingWeaponSet, OnPingWeaponSet);

    constexpr GW::UI::UIMessage dialog_ui_messages[] = {
        GW::UI::UIMessage::kSendAgentDialog,
        GW::UI::UIMessage::kDialogBody,
        GW::UI::UIMessage::kDialogButton
    };
    for (const auto message_id : dialog_ui_messages) {
        RegisterUIMessageCallback(
            &OnPostSendDialog_Entry,
            message_id,
            OnDialogUIMessage,
            0x8000
        );
    }

    constexpr GW::UI::UIMessage party_target_ui_messages[] = {
        GW::UI::UIMessage::kTargetPlayerPartyMember,
        GW::UI::UIMessage::kTargetNPCPartyMember,
        GW::UI::UIMessage::kCalledTargetChange
    };
    for (const auto message_id : party_target_ui_messages) {
        RegisterUIMessageCallback(&OnPostSendDialog_Entry, message_id, OnPartyTargetChanged, 0x8000);
    }

    constexpr GW::UI::UIMessage pre_ui_messages[] = {
        GW::UI::UIMessage::kSendCallTarget,
        GW::UI::UIMessage::kSendEnterMission,
        GW::UI::UIMessage::kSendAgentDialog,
        GW::UI::UIMessage::kMapLoaded
    };
    for (const auto message_id : pre_ui_messages) {
        RegisterUIMessageCallback(&OnPreUIMessage_HookEntry, message_id, OnPreUIMessage, -0x8000);
    }

    constexpr GW::UI::UIMessage post_ui_messages[] = {
        GW::UI::UIMessage::kPartySearchInviteSent,
        GW::UI::UIMessage::kPreferenceValueChanged,
        GW::UI::UIMessage::kMapLoaded,
        GW::UI::UIMessage::kTradeSessionStart,
        GW::UI::UIMessage::kPartyDefeated
    };
    for (const auto message_id : post_ui_messages) {
        RegisterUIMessageCallback(&OnPostUIMessage_HookEntry, message_id, OnPostUIMessage, 0x8000);
    }
    

    GW::Chat::CreateCommand(L"reinvite", CmdReinvite);

    RegisterCreateUIComponentCallback(&OnCreateUIComponent_Entry, OnCreateUIComponent);

    set_window_title_delay = TIMER_INIT();

    GW::UI::UIMessage ui_message_ids[] = {
        GW::UI::UIMessage::kQuestAdded,
        GW::UI::UIMessage::kSendSetActiveQuest
    };
    for (const auto message_id : ui_message_ids) {
        RegisterUIMessageCallback(&OnQuestUIMessage_HookEntry, message_id, OnPostQuestUIMessage, 0x8000);
    }
    player_requested_active_quest_id = GW::QuestMgr::GetActiveQuestId();

    last_online_status = static_cast<uint32_t>(GW::FriendListMgr::GetMyStatus());

#ifdef APRIL_FOOLS
    AF::ApplyPatchesIfItsTime();
#endif
}

void GameSettings::OnDialogUIMessage(GW::HookStatus*, const GW::UI::UIMessage message_id, void* wparam, void*)
{
    switch (message_id) {
        case GW::UI::UIMessage::kDialogButton: {
            const auto info = static_cast<GW::UI::DialogButtonInfo*>(wparam);
            if (auto_open_locked_chest_with_key && wcscmp(info->message, L"\x8101\x7F88\x10A\x8101\x13BE\x1") == 0) {
                // Auto use key
                DialogModule::SendDialog(info->dialog_id);
            }
            if (auto_open_locked_chest && wcscmp(info->message, L"\x8101\x7f88\x010a\x8101\x730e\x1") == 0) {
                // Auto use lockpick
                DialogModule::SendDialog(info->dialog_id);
            }
        }
        break;
    }
}


void GameSettings::MessageOnPartyChange()
{
    if (!check_message_on_party_change || GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost) {
        return; // Don't need to check, or not an outpost.
    }
    GW::PartyInfo* current_party = GW::PartyMgr::GetPartyInfo();
    const GW::AgentLiving* me = GW::Agents::GetControlledCharacter();
    if (!me || !current_party || !current_party->players.valid()) {
        return; // Party not ready yet.
    }
    bool is_leading = false;
    std::vector<std::wstring> current_party_names;
    GW::PlayerPartyMemberArray& current_party_players = current_party->players; // Copy the player array here to avoid ptr issues.
    for (size_t i = 0; i < current_party_players.size(); i++) {
        if (!current_party_players[i].login_number) {
            continue;
        }
        if (i == 0) {
            is_leading = current_party_players[i].login_number == me->login_number;
        }
        const wchar_t* player_name = GW::Agents::GetPlayerNameByLoginNumber(current_party_players[i].login_number);
        if (!player_name) {
            continue;
        }
        current_party_names.push_back(player_name);
    }
    // If previous party list is empty (i.e. map change), then initialise
    if (!previous_party_names.size()) {
        previous_party_names = current_party_names;
        was_leading = is_leading;
    }
    if (current_party_names.size() > previous_party_names.size()) {
        // Someone joined my party
        for (size_t i = 0; i < current_party_names.size() && notify_when_party_member_joins; i++) {
            bool found = false;
            for (size_t j = 0; j < previous_party_names.size() && !found; j++) {
                found = previous_party_names[j] == current_party_names[i];
            }
            if (!found) {
                wchar_t buffer[128];
                swprintf(buffer, 128, L"<a=1>%ls</a> joined the party.", current_party_names[i].c_str());
                WriteChat(GW::Chat::Channel::CHANNEL_GLOBAL, buffer, nullptr, true);
            }
        }
    }
    else if (!was_leading && is_leading && current_party_names.size() == 1 && previous_party_names.size() > 2) {
        // We left a party of at least 2 other people
    }
    else if (current_party_names.size() < previous_party_names.size()) {
        // Someone left my party
        for (size_t i = 0; i < previous_party_names.size() && notify_when_party_member_leaves; i++) {
            bool found = false;
            for (size_t j = 0; j < current_party_names.size() && !found; j++) {
                found = previous_party_names[i] == current_party_names[j];
            }
            if (!found) {
                wchar_t buffer[128];
                swprintf(buffer, 128, L"<a=1>%ls</a> left the party.", previous_party_names[i].c_str());
                WriteChat(GW::Chat::Channel::CHANNEL_GLOBAL, buffer, nullptr, true);
            }
        }
    }
    was_leading = is_leading;
    previous_party_names = current_party_names;
    check_message_on_party_change = false;
}

void GameSettings::LoadSettings(ToolboxIni* ini)
{
    ToolboxModule::LoadSettings(ini);

    LOAD_BOOL(skip_fade_animations);

    LOAD_BOOL(disable_camera_smoothing);
    LOAD_BOOL(tick_is_toggle);

    LOAD_BOOL(shorthand_item_ping);
    LOAD_BOOL(move_item_on_ctrl_click);
    LOAD_BOOL(move_item_to_current_storage_pane);
    LOAD_BOOL(move_materials_to_current_storage_pane);

    LOAD_BOOL(flash_window_on_zoning);
    LOAD_BOOL(flash_window_on_cinematic);
    LOAD_BOOL(focus_window_on_launch);
    LOAD_BOOL(focus_window_on_zoning);
    LOAD_BOOL(flash_window_on_trade);
    LOAD_BOOL(focus_window_on_trade);
    LOAD_BOOL(flash_window_on_name_ping);
    LOAD_BOOL(set_window_title_as_charname);

    LOAD_BOOL(auto_set_away);
    LOAD_UINT(auto_set_away_delay);
    LOAD_BOOL(auto_set_online);
    LOAD_BOOL(auto_return_on_defeat);

    LOAD_BOOL(remove_min_skill_warmup_duration);
    LOAD_BOOL(show_unlearned_skill);
    LOAD_BOOL(auto_skip_cinematic);

    LOAD_BOOL(faction_warn_percent);
    LOAD_UINT(faction_warn_percent_amount);
    LOAD_BOOL(stop_screen_shake);

    LOAD_BOOL(disable_gold_selling_confirmation);
    LOAD_BOOL(collectors_edition_emotes);

    LOAD_BOOL(notify_when_friends_online);
    LOAD_BOOL(notify_when_friends_offline);
    LOAD_BOOL(notify_when_friends_join_outpost);
    LOAD_BOOL(notify_when_friends_leave_outpost);

    LOAD_BOOL(notify_when_party_member_leaves);
    LOAD_BOOL(notify_when_party_member_joins);
    LOAD_BOOL(notify_when_players_join_outpost);
    LOAD_BOOL(notify_when_players_leave_outpost);

    LOAD_BOOL(auto_age_on_vanquish);
    LOAD_BOOL(hide_dungeon_chest_popup);
    LOAD_BOOL(auto_age2_on_age);
    LOAD_BOOL(auto_accept_invites);
    LOAD_BOOL(auto_accept_join_requests);

    LOAD_BOOL(skip_entering_name_for_faction_donate);
    LOAD_BOOL(drop_ua_on_cast);

    LOAD_BOOL(lazy_chest_looting);

    LOAD_BOOL(check_and_prompt_if_mission_already_completed);

    LOAD_BOOL(block_transmogrify_effect);
    LOAD_BOOL(block_sugar_rush_effect);
    LOAD_BOOL(block_snowman_summoner);
    LOAD_BOOL(block_fireworks);
    LOAD_BOOL(block_party_poppers);
    LOAD_BOOL(block_bottle_rockets);
    LOAD_BOOL(block_ghostinthebox_effect);
    LOAD_BOOL(block_sparkly_drops_effect);
    LOAD_BOOL(limit_signets_of_capture);
    LOAD_BOOL(auto_open_locked_chest);
    LOAD_BOOL(auto_open_locked_chest_with_key);
    LOAD_BOOL(block_faction_gain);
    LOAD_BOOL(block_experience_gain);
    LOAD_BOOL(block_zero_experience_gain);

    LOAD_BOOL(disable_item_descriptions_in_outpost);
    LOAD_BOOL(disable_item_descriptions_in_explorable);
    LOAD_BOOL(hide_email_address);
    LOAD_BOOL(disable_skill_descriptions_in_outpost);
    LOAD_BOOL(disable_skill_descriptions_in_explorable);

    LoadChannelColor(ini, Name(), "local", GW::Chat::Channel::CHANNEL_ALL);
    LoadChannelColor(ini, Name(), "guild", GW::Chat::Channel::CHANNEL_GUILD);
    LoadChannelColor(ini, Name(), "team", GW::Chat::Channel::CHANNEL_GROUP);
    LoadChannelColor(ini, Name(), "trade", GW::Chat::Channel::CHANNEL_TRADE);
    LoadChannelColor(ini, Name(), "alliance", GW::Chat::Channel::CHANNEL_ALLIANCE);
    LoadChannelColor(ini, Name(), "whispers", GW::Chat::Channel::CHANNEL_WHISPER);
    LoadChannelColor(ini, Name(), "emotes", GW::Chat::Channel::CHANNEL_EMOTE);
    LoadChannelColor(ini, Name(), "other", GW::Chat::Channel::CHANNEL_GLOBAL);

    LOAD_COLOR(nametag_color_enemy);
    LOAD_COLOR(nametag_color_gadget);
    LOAD_COLOR(nametag_color_item);
    LOAD_COLOR(nametag_color_npc);
    LOAD_COLOR(nametag_color_player_in_party);
    LOAD_COLOR(nametag_color_player_other);
    LOAD_COLOR(nametag_color_player_self);

    LOAD_BOOL(block_enter_area_message);

    LOAD_BOOL(keep_current_quest_when_new_quest_added);

    LOAD_BOOL(automatically_flag_pet_to_fight_called_target);

    LOAD_UINT(last_online_status);

    GW::PartyMgr::SetTickToggle(tick_is_toggle);
    SetWindowTitle(set_window_title_as_charname);

    remove_skill_warmup_duration_patch.TogglePatch(remove_min_skill_warmup_duration);
    gold_confirm_patch.TogglePatch(disable_gold_selling_confirmation);
    skip_map_entry_message_patch.TogglePatch(block_enter_area_message);

    if (focus_window_on_launch) {
        FocusWindow();
    }
}

void GameSettings::RegisterSettingsContent()
{
    ToolboxModule::RegisterSettingsContent();

    ToolboxModule::RegisterSettingsContent(
        "Party Settings", ICON_FA_USERS,
        [this](const std::string&, const bool is_showing) {
            if (!is_showing) {
                return;
            }
            DrawPartySettings();
        }, 0.9f);

    ToolboxModule::RegisterSettingsContent(
        "Notifications", ICON_FA_BULLHORN,
        [this](const std::string&, const bool is_showing) {
            if (is_showing) {
                DrawNotificationsSettings();
            }
        },
        0.9f);

    ToolboxModule::RegisterSettingsContent(
        "Inventory Settings", ICON_FA_BOXES,
        [this](const std::string&, const bool is_showing) {
            if (is_showing) {
                DrawInventorySettings();
            }
        },
        0.9f);
}

void GameSettings::Terminate()
{
    ToolboxModule::Terminate();
    ctrl_click_patch.Reset();
    gold_confirm_patch.Reset();
    skill_description_patch.Reset();
    skip_map_entry_message_patch.Reset();
    remove_skill_warmup_duration_patch.Reset();

    GW::UI::RemoveUIMessageCallback(&OnQuestUIMessage_HookEntry);
    GW::UI::RemoveUIMessageCallback(&OnPostUIMessage_HookEntry);
    GW::UI::RemoveUIMessageCallback(&OnPreUIMessage_HookEntry);

    if (SkillList_UICallback_Func)
        GW::Hook::RemoveHook(SkillList_UICallback_Func);
    if(FadeFrameContent_Func)
        GW::Hook::RemoveHook(FadeFrameContent_Func);
}

void GameSettings::SaveSettings(ToolboxIni* ini)
{
    ToolboxModule::SaveSettings(ini);

    SAVE_BOOL(skip_fade_animations);

    SAVE_BOOL(tick_is_toggle);

    SAVE_BOOL(disable_camera_smoothing);
    SAVE_BOOL(auto_return_on_defeat);

    SAVE_BOOL(shorthand_item_ping);
    SAVE_BOOL(move_item_on_ctrl_click);
    SAVE_BOOL(move_item_to_current_storage_pane);
    SAVE_BOOL(move_materials_to_current_storage_pane);
    SAVE_BOOL(stop_screen_shake);

    SAVE_BOOL(flash_window_on_zoning);
    SAVE_BOOL(focus_window_on_launch);
    SAVE_BOOL(focus_window_on_zoning);
    SAVE_BOOL(flash_window_on_cinematic);
    SAVE_BOOL(flash_window_on_trade);
    SAVE_BOOL(focus_window_on_trade);
    SAVE_BOOL(flash_window_on_name_ping);
    SAVE_BOOL(set_window_title_as_charname);

    SAVE_BOOL(auto_set_away);
    SAVE_UINT(auto_set_away_delay);
    SAVE_BOOL(auto_set_online);

    SAVE_BOOL(remove_min_skill_warmup_duration);
    SAVE_BOOL(show_unlearned_skill);
    SAVE_BOOL(auto_skip_cinematic);

    SAVE_BOOL(faction_warn_percent);
    SAVE_UINT(faction_warn_percent_amount);

    SAVE_BOOL(disable_gold_selling_confirmation);
    SAVE_BOOL(collectors_edition_emotes);

    SAVE_BOOL(notify_when_friends_online);
    SAVE_BOOL(notify_when_friends_offline);
    SAVE_BOOL(notify_when_friends_join_outpost);
    SAVE_BOOL(notify_when_friends_leave_outpost);

    SAVE_BOOL(notify_when_party_member_leaves);
    SAVE_BOOL(notify_when_party_member_joins);
    SAVE_BOOL(notify_when_players_join_outpost);
    SAVE_BOOL(notify_when_players_leave_outpost);

    SAVE_BOOL(auto_age_on_vanquish);
    SAVE_BOOL(hide_dungeon_chest_popup);
    SAVE_BOOL(auto_age2_on_age);
    SAVE_BOOL(auto_accept_invites);
    SAVE_BOOL(auto_accept_join_requests);
    SAVE_BOOL(skip_entering_name_for_faction_donate);
    SAVE_BOOL(drop_ua_on_cast);

    SAVE_BOOL(check_and_prompt_if_mission_already_completed);

    SAVE_BOOL(lazy_chest_looting);

    SAVE_BOOL(block_transmogrify_effect);
    SAVE_BOOL(block_sugar_rush_effect);
    SAVE_BOOL(block_fireworks);
    SAVE_BOOL(block_snowman_summoner);
    SAVE_BOOL(block_party_poppers);
    SAVE_BOOL(block_bottle_rockets);
    SAVE_BOOL(block_ghostinthebox_effect);
    SAVE_BOOL(block_sparkly_drops_effect);
    SAVE_BOOL(limit_signets_of_capture);
    SAVE_BOOL(auto_open_locked_chest);
    SAVE_BOOL(auto_open_locked_chest_with_key);

    SAVE_BOOL(block_faction_gain);
    SAVE_BOOL(block_experience_gain);
    SAVE_BOOL(block_zero_experience_gain);
    SAVE_BOOL(disable_item_descriptions_in_outpost);
    SAVE_BOOL(disable_item_descriptions_in_explorable);

    SAVE_BOOL(hide_email_address);

    SAVE_BOOL(disable_skill_descriptions_in_outpost);
    SAVE_BOOL(disable_skill_descriptions_in_explorable);

    SAVE_BOOL(block_enter_area_message);

    SAVE_UINT(last_online_status);

    SaveChannelColor(ini, Name(), "local", GW::Chat::Channel::CHANNEL_ALL);
    SaveChannelColor(ini, Name(), "guild", GW::Chat::Channel::CHANNEL_GUILD);
    SaveChannelColor(ini, Name(), "team", GW::Chat::Channel::CHANNEL_GROUP);
    SaveChannelColor(ini, Name(), "trade", GW::Chat::Channel::CHANNEL_TRADE);
    SaveChannelColor(ini, Name(), "alliance", GW::Chat::Channel::CHANNEL_ALLIANCE);
    SaveChannelColor(ini, Name(), "whispers", GW::Chat::Channel::CHANNEL_WHISPER);
    SaveChannelColor(ini, Name(), "emotes", GW::Chat::Channel::CHANNEL_EMOTE);
    SaveChannelColor(ini, Name(), "other", GW::Chat::Channel::CHANNEL_GLOBAL);

    SAVE_COLOR(nametag_color_enemy);
    SAVE_COLOR(nametag_color_gadget);
    SAVE_COLOR(nametag_color_item);
    SAVE_COLOR(nametag_color_npc);
    SAVE_COLOR(nametag_color_player_in_party);
    SAVE_COLOR(nametag_color_player_other);
    SAVE_COLOR(nametag_color_player_self);

    SAVE_BOOL(keep_current_quest_when_new_quest_added);

    SAVE_BOOL(automatically_flag_pet_to_fight_called_target);
}

void GameSettings::DrawInventorySettings()
{
    ImGui::Checkbox("Move items from/to storage with Control+Click", &move_item_on_ctrl_click);
    ImGui::Indent();
    ImGui::Checkbox("Move items to current open storage pane on click", &move_item_to_current_storage_pane);
    ImGui::ShowHelp("Materials follow different logic, see below");
    ImGui::Indent();
    auto logic = "Storage logic: Any available stack/slot";
    if (move_item_to_current_storage_pane) {
        logic = "Storage logic: Current storage pane > Any available stack/slot";
    }
    ImGui::TextDisabled(logic);
    ImGui::Unindent();
    ImGui::Checkbox("Move materials to current open storage pane on click", &move_materials_to_current_storage_pane);
    ImGui::Indent();
    logic = "Storage logic: Materials pane > Any available stack/slot";
    if (move_materials_to_current_storage_pane) {
        logic = "Storage logic: Current storage pane > Materials pane > Any available stack/slot";
    }
    else if (move_item_to_current_storage_pane) {
        logic = "Storage logic: Materials pane > Current storage pane > Any available stack/slot";
    }
    ImGui::TextDisabled(logic);
    ImGui::Unindent();
    ImGui::Unindent();

    ImGui::Checkbox("Shorthand item description on weapon ping", &shorthand_item_ping);
    ImGui::ShowHelp("Include a concise description of your equipped weapon when ctrl+clicking a weapon set");

    ImGui::Checkbox("Lazy chest looting", &lazy_chest_looting);
    ImGui::ShowHelp("Toolbox will try to target any nearby reserved items\nwhen using the 'target nearest item' key next to a chest\nto pick stuff up.");
    ImGui::Text("Hide item descriptions in:");
    ImGui::ShowHelp("When hovering an item in inventory or weapon sets,\nonly show the item name in the tooltip that appears.");
    ImGui::Indent();
    ImGui::Checkbox("Explorable Area###disable_item_descriptions_in_explorable", &disable_item_descriptions_in_explorable);
    ImGui::SameLine();
    ImGui::Checkbox("Outpost###disable_item_descriptions_in_outpost", &disable_item_descriptions_in_outpost);
    if (disable_item_descriptions_in_explorable || disable_item_descriptions_in_outpost) {
        ImGui::Indent();
        ImGui::TextDisabled("Hold Alt when hovering an item to show full description");
        ImGui::Unindent();
    }
    ImGui::Unindent();
}

void GameSettings::DrawPartySettings()
{
    if (ImGui::Checkbox("Tick is a toggle", &tick_is_toggle)) {
        GW::PartyMgr::SetTickToggle(tick_is_toggle);
    }
    ImGui::ShowHelp("Ticking in party window will work as a toggle instead of opening the menu");
    ImGui::Checkbox("Automatically accept party invitations when ticked", &auto_accept_invites);
    ImGui::ShowHelp("When you're invited to join someone elses party");
    ImGui::Checkbox("Automatically accept party join requests when ticked", &auto_accept_join_requests);
    ImGui::ShowHelp("When a player wants to join your existing party");
    ImGui::Checkbox("Automatically flag your pet to fight when calling a target", &automatically_flag_pet_to_fight_called_target);
}

void GameSettings::DrawSettingsInternal()
{
    ImGui::Checkbox("Hide email address on login screen", &hide_email_address);
    ImGui::Checkbox("Remember my online status when returning to character select screen", &remember_online_status);
    ImGui::Checkbox("Automatic /age on vanquish", &auto_age_on_vanquish);
    ImGui::ShowHelp("As soon as a vanquish is complete, send /age command to game server to receive server-side completion time.");
    ImGui::Checkbox("Automatic /age2 on /age", &auto_age2_on_age);
    ImGui::ShowHelp("GWToolbox++ will show /age2 time after /age is shown in chat");

    ImGui::Checkbox("Automatically set 'Away' after ", &auto_set_away);
    ImGui::SameLine();
    ImGui::PushItemWidth(50.0f * ImGui::FontScale());
    ImGui::InputInt("##awaydelay", &auto_set_away_delay, 0);
    ImGui::PopItemWidth();
    ImGui::SameLine();
    ImGui::Text("minutes of inactivity");
    ImGui::ShowHelp("Only if you were 'Online'");

    ImGui::Checkbox("Automatically set 'Online' after an input to Guild Wars", &auto_set_online);
    ImGui::ShowHelp("Only if you were 'Away'");

    ImGui::Checkbox("Only show non learned skills when using a tome", &show_unlearned_skill);

    if (ImGui::Checkbox("Remove 1.5 second minimum for the cast bar to show.", &remove_min_skill_warmup_duration)) {
        remove_skill_warmup_duration_patch.TogglePatch(remove_min_skill_warmup_duration);
    }
    ImGui::Checkbox("Disable camera smoothing", &disable_camera_smoothing);

    ImGui::Checkbox("Disable loading screen fade animation", &skip_fade_animations);
    ImGui::Checkbox("Prompt if entring a mission you've already completed", &check_and_prompt_if_mission_already_completed);
    ImGui::Checkbox("Automatically skip cinematics", &auto_skip_cinematic);
    ImGui::Checkbox("Automatically return to outpost on defeat", &auto_return_on_defeat);
    ImGui::ShowHelp("Automatically return party to outpost on party wipe if player is leading");
    ImGui::Checkbox("Show warning when earned faction reaches ", &faction_warn_percent);
    ImGui::SameLine();
    ImGui::PushItemWidth(40.0f * ImGui::GetIO().FontGlobalScale);
    ImGui::InputInt("##faction_warn_percent_amount", &faction_warn_percent_amount, 0);
    ImGui::PopItemWidth();
    ImGui::SameLine();
    ImGui::Text("%%");
    ImGui::ShowHelp("Displays when in a challenge mission or elite mission outpost");
    ImGui::Checkbox("Skip character name input when donating faction", &skip_entering_name_for_faction_donate);

    if (ImGui::Checkbox("Disable Gold/Green items confirmation", &disable_gold_selling_confirmation)) {
        gold_confirm_patch.TogglePatch(disable_gold_selling_confirmation);
    }
    ImGui::ShowHelp(
        "Disable the confirmation request when\n"
        "selling Gold and Green items introduced\n"
        "in February 5, 2019 update.");
    ImGui::Checkbox("Hide dungeon chest popup", &hide_dungeon_chest_popup);
    ImGui::Checkbox("Stop screen shake from skills or effects", &stop_screen_shake);
    ImGui::ShowHelp("e.g. Aftershock, Earth shaker, Avalanche effect");
    if (ImGui::Checkbox("Set Guild Wars window title as current logged-in character", &set_window_title_as_charname)) {
        SetWindowTitle(set_window_title_as_charname);
    }
    ImGui::Checkbox("Apply Collector's Edition animations on player dance", &collectors_edition_emotes);
    ImGui::ShowHelp("Only applies to your own character");
    ImGui::Checkbox("Auto-cancel Unyielding Aura when re-casting", &drop_ua_on_cast);
    ImGui::Checkbox("Auto use available keys when interacting with locked chest", &auto_open_locked_chest_with_key);
    ImGui::Checkbox("Auto use lockpick when interacting with locked chest", &auto_open_locked_chest);
    ImGui::Checkbox("Keep current quest when accepting a new one", &keep_current_quest_when_new_quest_added);
    ImGui::Checkbox("Block sparkle effect on dropped items", &block_sparkly_drops_effect);
    ImGui::ShowHelp("Applies to drops that appear after this setting has been changed");
    ImGui::Checkbox("Limit signet of capture to 10 in skills window", &limit_signets_of_capture);
    ImGui::ShowHelp("If your character has purchased more than 10 signets of capture, only show 10 of them in the skills window");
    if (ImGui::Checkbox("Block full screen message when entering a new area", &block_enter_area_message)) {
        skip_map_entry_message_patch.TogglePatch(block_enter_area_message);
    }
    ImGui::NewLine();
    ImGui::Text("Block floating numbers above character when:");
    ImGui::Indent();
    ImGui::StartSpacedElements(checkbox_w);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Gaining faction", &block_faction_gain);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Gaining experience", &block_experience_gain);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Gaining 0 experience", &block_zero_experience_gain);
    ImGui::Unindent();

    ImGui::NewLine();
    ImGui::Text("Disable animation and sound from consumables:");
    ImGui::Indent();
    ImGui::StartSpacedElements(checkbox_w);
    constexpr auto doesnt_affect_me = "Only applies to other players";
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Tonics", &block_transmogrify_effect);
    ImGui::ShowHelp(doesnt_affect_me);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Sweets", &block_sugar_rush_effect);
    ImGui::ShowHelp(doesnt_affect_me);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Bottle rockets", &block_bottle_rockets);
    ImGui::ShowHelp(doesnt_affect_me);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Party poppers", &block_party_poppers);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Snowman Summoners", &block_snowman_summoner);
    ImGui::ShowHelp(doesnt_affect_me);
    ImGui::Checkbox("Fireworks", &block_fireworks);
#if 0
    //@Cleanup: Ghost in the box spawn effect suppressed, but still need to figure out how to suppress the death effect.
    ImGui::SameLine(column_spacing); ImGui::Checkbox("Ghost-in-the-box", &block_ghostinthebox_effect);
    ImGui::ShowHelp("Also applies to ghost-in-the-boxes that you use");
#endif
    ImGui::Unindent();
    ImGui::NewLine();
    ImGui::Text("In-game name tag colors:");
    ImGui::Indent();
    ImGui::StartSpacedElements(checkbox_w);
    constexpr uint32_t flags = ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_NoInputs;
    ImGui::NextSpacedElement();
    Colors::DrawSettingHueWheel("Myself", &nametag_color_player_self, flags);
    ImGui::NextSpacedElement();
    Colors::DrawSettingHueWheel("NPC", &nametag_color_npc, flags);
    ImGui::NextSpacedElement();
    Colors::DrawSettingHueWheel("Enemy", &nametag_color_enemy, flags);
    ImGui::NextSpacedElement();
    Colors::DrawSettingHueWheel("Gadget", &nametag_color_gadget, flags);
    ImGui::NextSpacedElement();
    Colors::DrawSettingHueWheel("Other Player", &nametag_color_player_other, flags);
    ImGui::NextSpacedElement();
    Colors::DrawSettingHueWheel("Other Player (In Party)", &nametag_color_player_in_party, flags);
    ImGui::NextSpacedElement();
    Colors::DrawSettingHueWheel("Item", &nametag_color_item, flags);
    ImGui::Unindent();

    ImGui::NewLine();
    ImGui::Text("Hide skill descriptions in:");
    ImGui::ShowHelp("When hovering a skill in the game,\nonly show the skill name  and cooldown etc in the tooltip that appears.");
    ImGui::Indent();
    ImGui::Checkbox("Explorable Area###disable_skill_descriptions_in_explorable", &disable_skill_descriptions_in_explorable);
    ImGui::SameLine();
    ImGui::Checkbox("Outpost###disable_skill_descriptions_in_outpost", &disable_skill_descriptions_in_outpost);
    if (disable_skill_descriptions_in_explorable || disable_skill_descriptions_in_outpost) {
        ImGui::Indent();
        ImGui::TextDisabled("Hold Alt when hovering a skill to show full description");
        ImGui::Unindent();
    }
    ImGui::Unindent();
}

void GameSettings::FactionEarnedCheckAndWarn()
{
    if (!faction_warn_percent) {
        return; // Disabled
    }
    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost) {
        faction_checked = false;
        return; // Loading or explorable area.
    }
    if (faction_checked) {
        return; // Already checked.
    }
    faction_checked = true;
    const auto world_context = GW::GetWorldContext();
    if (!world_context || !world_context->max_luxon || !world_context->total_earned_kurzick) {
        faction_checked = false;
        return; // No world context yet.
    }
    float percent;
    // Avoid invalid user input values.
    if (faction_warn_percent_amount < 0) {
        faction_warn_percent_amount = 0;
    }
    if (faction_warn_percent_amount > 100) {
        faction_warn_percent_amount = 100;
    }
    // Warn user to dump faction if we're in a luxon/kurzick mission outpost
    switch (GW::Map::GetMapID()) {
        case GW::Constants::MapID::The_Deep:
        case GW::Constants::MapID::The_Jade_Quarry_Luxon_outpost:
        case GW::Constants::MapID::Fort_Aspenwood_Luxon_outpost:
        case GW::Constants::MapID::Zos_Shivros_Channel:
        case GW::Constants::MapID::The_Aurios_Mines:
            // Player is in luxon mission outpost
            percent = 100.0f * static_cast<float>(world_context->current_luxon) / static_cast<float>(world_context->max_luxon);
            if (percent >= static_cast<float>(faction_warn_percent_amount)) {
                // Faction earned is over 75% capacity
                Log::Warning("Luxon faction earned is %d of %d", world_context->current_luxon, world_context->max_luxon);
            }
            else if (world_context->current_kurzick > 4999 && world_context->current_kurzick > world_context->current_luxon) {
                // Kurzick faction > Luxon faction
                Log::Warning("Kurzick faction earned is greater than Luxon");
            }
            break;
        case GW::Constants::MapID::Urgozs_Warren:
        case GW::Constants::MapID::The_Jade_Quarry_Kurzick_outpost:
        case GW::Constants::MapID::Fort_Aspenwood_Kurzick_outpost:
        case GW::Constants::MapID::Altrumm_Ruins:
        case GW::Constants::MapID::Amatz_Basin:
            // Player is in kurzick mission outpost
            percent = 100.0f * static_cast<float>(world_context->current_kurzick) / static_cast<float>(world_context->max_kurzick);
            if (percent >= static_cast<float>(faction_warn_percent_amount)) {
                // Faction earned is over 75% capacity
                Log::Warning("Kurzick faction earned is %d of %d", world_context->current_kurzick, world_context->max_kurzick);
            }
            else if (world_context->current_luxon > 4999 && world_context->current_luxon > world_context->current_kurzick) {
                // Luxon faction > Kurzick faction
                Log::Warning("Luxon faction earned is greater than Kurzick");
            }
            break;
        default:
            break;
    }
}

void GameSettings::Update(float)
{
    UpdateSkillTooltip();
    UpdateReinvite();
    UpdateItemTooltip();
    if (set_window_title_delay && TIMER_DIFF(set_window_title_delay) > 3000) {
        SetWindowTitle(set_window_title_as_charname);
        set_window_title_delay = 0;
    }

    // See OnSendChat
    if (pending_wiki_search_term && pending_wiki_search_term->wstring().length()) {
        SearchWiki(pending_wiki_search_term->wstring());
        delete pending_wiki_search_term;
        pending_wiki_search_term = nullptr;
    }

    if (auto_set_away
        && TIMER_DIFF(activity_timer) > auto_set_away_delay * 60000
        && GW::FriendListMgr::GetMyStatus() == GW::FriendStatus::Online) {
        GW::FriendListMgr::SetFriendListStatus(GW::FriendStatus::Away);
        activity_timer = TIMER_INIT(); // refresh the timer to avoid spamming in case the set status call fails
    }
    //UpdateFOV();
    FactionEarnedCheckAndWarn();

    if (disable_camera_smoothing && !GW::CameraMgr::GetCameraUnlock()) {
        GW::Camera* cam = GW::CameraMgr::GetCamera();
        if (cam) {
            cam->position = cam->camera_pos_to_go;
            cam->look_at_target = cam->look_at_to_go;
            cam->cam_pos_inverted = cam->cam_pos_inverted_to_go;
            cam->yaw = cam->yaw_to_go;
            cam->pitch = cam->pitch_to_go;
        }
    }

#ifdef APRIL_FOOLS
    AF::ApplyPatchesIfItsTime();
#endif
    if (notify_when_party_member_joins || notify_when_party_member_leaves) {
        if (!check_message_on_party_change && !GW::Map::GetIsMapLoaded()) {
            // Map changed - clear previous party names, flag to re-populate on map load
            previous_party_names.clear();
            check_message_on_party_change = true;
        }
        if (check_message_on_party_change) {
            MessageOnPartyChange();
        }
    }
}

bool GameSettings::WndProc(const UINT Message, WPARAM, LPARAM)
{
    // I don't know what would be the best solution here, but the way we capture every messages as a sign of activity can be bad.
    // Added that because when someone was typing "/afk message" he was put online directly, because "enter-up" was captured.
    if (Message == WM_KEYUP) {
        return false;
    }

    activity_timer = TIMER_INIT();
    static clock_t set_online_timer = TIMER_INIT();
    if (auto_set_online
        && TIMER_DIFF(set_online_timer) > 5000 // to avoid spamming in case of failure
        && GW::FriendListMgr::GetMyStatus() == GW::FriendStatus::Away) {
        printf("%X\n", Message);
        GW::FriendListMgr::SetFriendListStatus(GW::FriendStatus::Online);
        set_online_timer = TIMER_INIT();
    }

    return false;
}

// Show weapon description/mods when pinged
void GameSettings::OnPingWeaponSet(GW::HookStatus* status, const GW::UI::UIMessage message_id, void* wparam, void*)
{
    ASSERT(message_id == GW::UI::UIMessage::kSendPingWeaponSet && wparam);
    if (!shorthand_item_ping) {
        return;
    }
    const struct Packet {
        uint32_t agent_id;
        uint32_t weapon_item_id;
        uint32_t offhand_item_id;
    }* pack = static_cast<Packet*>(wparam);
    PingItem(pack->weapon_item_id, NAME | DESC);
    PingItem(pack->offhand_item_id, NAME | DESC);
    status->blocked = true;
}

// Show a message when player joins the outpost
void GameSettings::OnPlayerJoinInstance(GW::HookStatus*, GW::Packet::StoC::PlayerJoinInstance* pak)
{
    if (!notify_when_players_join_outpost && !notify_when_friends_join_outpost) {
        return; // Dont notify about player joining
    }
    if (!(pak->player_name && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost)) {
        return; // Only message in an outpost.
    }
    if (!(TIMER_DIFF(instance_entered_at) > 2000 && GW::Agents::GetControlledCharacter())) {
        return; // Only been in this map for less than 2 seconds or current player not loaded in yet; avoids spam on map load.
    }
    if (GW::Agents::GetAgentByID(pak->agent_id)) {
        return; // Player already joined
    }
    if (notify_when_friends_join_outpost) {
        if (const auto f = GetFriend(nullptr, pak->player_name, GW::FriendType::Friend, GW::FriendStatus::Online)) {
            WriteChat(GW::Chat::Channel::CHANNEL_GLOBAL, std::format(L"<a=1>{}</a> ({}) entered the outpost.", f->charname, f->alias).c_str(), nullptr, true);
            return;
        }
    }
    if (notify_when_players_join_outpost) {
        WriteChat(GW::Chat::Channel::CHANNEL_GLOBAL, std::format(L"<a=1>{}</a> entered the outpost.", pak->player_name).c_str(), nullptr, true);
    }
}

// Auto accept invitations, flash window on received party invite
void GameSettings::OnPartyInviteReceived(const GW::HookStatus* status, const GW::Packet::StoC::PartyInviteReceived_Create* packet)
{
    if (status->blocked) {
        return;
    }
    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost || !GW::PartyMgr::GetIsLeader()) {
        return;
    }

    if (GW::PartyMgr::GetIsPlayerTicked()) {
        GW::PartyInfo* other_party = GW::PartyMgr::GetPartyInfo(packet->target_party_id);
        GW::PartyInfo* my_party = GW::PartyMgr::GetPartyInfo();
        if (auto_accept_invites && other_party && my_party && my_party->GetPartySize() <= other_party->GetPartySize()) {
            // Auto accept if I'm joining a bigger party
            GW::PartyMgr::RespondToPartyRequest(packet->target_party_id, true);
        }
        if (auto_accept_join_requests && other_party && my_party && my_party->GetPartySize() > other_party->GetPartySize()) {
            // Auto accept join requests if I'm the bigger party
            GW::PartyMgr::RespondToPartyRequest(packet->target_party_id, true);
        }
    }
}

// Flash window on player added
void GameSettings::OnPartyPlayerJoined(const GW::HookStatus*, const GW::Packet::StoC::PartyPlayerAdd*)
{
    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost) {
        return;
    }
    check_message_on_party_change = true;
}

// Block overhead arrow marker for zaishen scout
void GameSettings::OnAgentMarker(GW::HookStatus*, GW::Packet::StoC::GenericValue* pak)
{
    const GW::Agent* a = GW::Agents::GetAgentByID(pak->agent_id);
    if (a && wcscmp(GW::Agents::GetAgentEncName(a), L"\x8102\x6ED9\xD94E\xBF68\x4409") == 0) {
        pak->value_id = 12;
    }
}

// Block annoying tonic sounds/effects from other players
void GameSettings::OnAgentEffect(GW::HookStatus* status, const GW::Packet::StoC::GenericValue* pak)
{
    if (pak->agent_id != GW::Agents::GetControlledCharacterId()) {
        status->blocked |= ShouldBlockEffect(pak->value);
    }
}

// Block Ghost in the box spawn animation & sound
// Block sparkly item animation
void GameSettings::OnAgentAdd(GW::HookStatus*, const GW::Packet::StoC::AgentAdd* packet)
{
    if (block_sparkly_drops_effect && packet->type == 4 && packet->agent_type < 0xFFFFFF) {
        GW::Item* item = GW::Items::GetItemById(packet->agent_type);
        if (item) {
            item->interaction |= 0x2000;
        }
    }
}

// Block ghost in the box death animation & sound
void GameSettings::OnUpdateAgentState(GW::HookStatus*, GW::Packet::StoC::AgentState*)
{
    // @Cleanup: Not found an elegent way to do this; prematurely destroying the agent will crash the client when the id it recycled. Disable for now, here for reference.
}

// Apply Collector's Edition animations on player dancing,
void GameSettings::OnAgentLoopingAnimation(GW::HookStatus*, const GW::Packet::StoC::GenericValue* pak)
{
    if (!(pak->agent_id == GW::Agents::GetControlledCharacterId() && collectors_edition_emotes)) {
        return;
    }
    static GW::Packet::StoC::GenericValue pak2;
    pak2.agent_id = pak->agent_id;
    pak2.value_id = 23;
    pak2.value = pak->value; // Glowing hands, any profession
    if (pak->value == 0x43394f1d) {
        // 0x31939cbb = /dance, 0x43394f1d = /dancenew
        switch (static_cast<GW::Constants::Profession>(GW::Agents::GetControlledCharacter()->primary)) {
            case GW::Constants::Profession::Assassin:
            case GW::Constants::Profession::Ritualist:
            case GW::Constants::Profession::Dervish:
            case GW::Constants::Profession::Paragon:
                pak2.value = 14; // Collectors edition Nightfall/Factions
                break;
            default:
                break;
        }
    }
    GW::StoC::EmulatePacket(&pak2);
}

// Skip char name entry dialog when donating faction
void GameSettings::OnFactionDonate(GW::HookStatus* status, GW::UI::UIMessage, void* wparam, void*) const
{
    const auto dialog_id = (uint32_t)wparam;
    if (!(dialog_id == 0x87 && skip_entering_name_for_faction_donate)) {
        return;
    }
    uint32_t allegiance = 2;
    const auto raising_luxon_faction_cap = L"\x8102\x4A32\xAF32\xBDB5\x21AE";
    const auto raising_kurzick_faction_cap = L"\x8102\x4A26\x814C\x89CC\x5B0";
    // Look for "Raising Luxon/Kurzick faction cap" dialog option to check allegiance
    for (const auto dialog : DialogModule::Instance().GetDialogButtons()) {
        if (wcscmp(dialog->message, raising_luxon_faction_cap) == 0) {
            allegiance = 1; // Luxon
        }
        if (wcscmp(dialog->message, raising_kurzick_faction_cap) == 0) {
            allegiance = 0; // Kurzick
        }
    }
    const uint32_t* current_faction = nullptr;
    switch (allegiance) {
        case 0: // Kurzick
            current_faction = &GW::GetWorldContext()->current_kurzick;
            break;
        case 1: // Luxon
            current_faction = &GW::GetWorldContext()->current_luxon;
            break;
        default: // Didn't find an allegiance, not the relevent dialog
            return;
    }
    GW::GuildContext* c = GW::GetGuildContext();
    if (!c || !c->player_guild_index || c->guilds[c->player_guild_index]->faction != allegiance) {
        return; // Alliance isn't the right faction. Return here and the NPC will reply.
    }
    if (*current_faction < 5000) {
        return; // Not enough to donate. Return here and the NPC will reply.
    }
    status->blocked = true;
    GW::PlayerMgr::DepositFaction(allegiance);
    GW::Agents::InteractAgent(GW::Agents::GetAgentByID(DialogModule::GetDialogAgent()));
}

// Show a message when player leaves the outpost
void GameSettings::OnPlayerLeaveInstance(GW::HookStatus*, const GW::Packet::StoC::PlayerLeaveInstance* pak)
{
    if (!notify_when_players_leave_outpost && !notify_when_friends_leave_outpost) {
        return; // Dont notify about player leaving
    }
    if (!(pak->player_number && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost)) {
        return; // Only message in an outpost.
    }
    wchar_t* player_name = GW::PlayerMgr::GetPlayerName(pak->player_number);
    if (!player_name) {
        return; // Failed to get name
    }
    if (notify_when_friends_leave_outpost) {
        if (const auto f = GetFriend(nullptr, player_name, GW::FriendType::Friend, GW::FriendStatus::Online)) {
            WriteChatF(GW::Chat::Channel::CHANNEL_GLOBAL, L"<a=1>%ls</a> (%ls) left the outpost.", f->charname, f->alias);
            return;
        }
    }
    if (notify_when_players_leave_outpost) {
        WriteChatF(GW::Chat::Channel::CHANNEL_GLOBAL, L"<a=1>%ls</a> left the outpost.", player_name);
    }
}

// Automatically return to outpost on defeat
void GameSettings::OnPartyDefeated(const GW::HookStatus*, GW::Packet::StoC::PartyDefeated*)
{

}

// Automatically send /age2 on /age.
void GameSettings::OnServerMessage(const GW::HookStatus*, GW::Packet::StoC::MessageServer* pak)
{
    if (!auto_age2_on_age || static_cast<GW::Chat::Channel>(pak->channel) != GW::Chat::Channel::CHANNEL_GLOBAL) {
        return; // Disabled or message pending
    }
    const wchar_t* msg = GetMessageCore();
    // 0x8101 0x641F 0x86C3 0xE149 0x53E8 0x101 0x107 = You have been in this map for n minutes.
    // 0x8101 0x641E 0xE7AD 0xEF64 0x1676 0x101 0x107 0x102 0x107 = You have been in this map for n hours and n minutes.
    if (wmemcmp(msg, L"\x8101\x641F\x86C3\xE149\x53E8", 5) == 0 || wmemcmp(msg, L"\x8101\x641E\xE7AD\xEF64\x1676", 5) == 0) {
        GW::Chat::SendChat('/', "age2");
    }
}

// Automatic /age on vanquish
void GameSettings::OnVanquishComplete(const GW::HookStatus*, GW::Packet::StoC::VanquishComplete*)
{
    if (!auto_age_on_vanquish) {
        return;
    }
    GW::Chat::SendChat('/', "age");
}

void GameSettings::OnDungeonReward(GW::HookStatus* status, GW::Packet::StoC::DungeonReward*) const
{
    if (hide_dungeon_chest_popup) {
        status->blocked = true;
    }
}

// Stop screen shake from aftershock etc
void GameSettings::OnScreenShake(GW::HookStatus* status, const void*) const
{
    if (stop_screen_shake) {
        status->blocked = true;
    }
}

// Automatically skip cinematics, flash window on cinematic
void GameSettings::OnCinematic(const GW::HookStatus*, const GW::Packet::StoC::CinematicPlay* packet)
{
    if (packet->play && auto_skip_cinematic) {
        GW::Map::SkipCinematic();
        return;
    }
    if (flash_window_on_cinematic) {
        FlashWindow();
    }
}

// Flash/focus window on zoning
void GameSettings::OnMapTravel(const GW::HookStatus*, const GW::Packet::StoC::GameSrvTransfer* pak)
{
    if (flash_window_on_zoning) {
        FlashWindow();
    }
    if (focus_window_on_zoning && pak->is_explorable) {
        FocusWindow();
    }
}

// Turn screenshots into clickable links
void GameSettings::OnWriteChat(GW::HookStatus* status, GW::UI::UIMessage, void* wParam, void*)
{
    static bool is_redirecting = false;
    if (is_redirecting) {
        is_redirecting = false;
        return;
    }
    const auto msg = static_cast<PlayerChatMessage*>(wParam);
    if (msg->channel != GW::Chat::Channel::CHANNEL_GLOBAL) {
        return;
    }
    if (wmemcmp(L"\x846\x107", msg->message, 2) != 0) {
        return;
    }
    status->blocked = true;
    wchar_t file_path[256];
    size_t file_path_len = 0;
    wchar_t new_message[256];
    size_t new_message_len = 0;
    wcscpy(&new_message[new_message_len], L"\x846\x107<a=1>\x200C");
    new_message_len += 8;
    for (size_t i = 2; msg->message[i] && msg->message[i] != 0x1; i++) {
        new_message[new_message_len++] = msg->message[i];
        if (msg->message[i] == '\\' && msg->message[i - 1] == '\\') {
            continue; // Skip double escaped directory separators when getting the actual file name
        }
        file_path[file_path_len++] = msg->message[i];
    }
    file_path[file_path_len] = 0;
    wcscpy(&new_message[new_message_len], L"</a>\x1");
    new_message_len += 5;
    new_message[new_message_len] = 0;
    is_redirecting = true;
    WriteChatEnc(static_cast<GW::Chat::Channel>(msg->channel), new_message);
    // Copy file to clipboard

    const auto size = sizeof(DROPFILES) + (file_path_len + 2) * sizeof(wchar_t);
    const HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, size);
    ASSERT(hGlobal != nullptr);
    const auto df = static_cast<DROPFILES*>(GlobalLock(hGlobal));
    ASSERT(df != nullptr);
    ZeroMemory(df, size);
    df->pFiles = sizeof(DROPFILES);
    df->fWide = TRUE;
    const auto ptr = (LPWSTR)(df + 1);
    lstrcpyW(ptr, file_path);
    GlobalUnlock(hGlobal);

    // prepare the clipboard
    OpenClipboard(nullptr);
    EmptyClipboard();
    SetClipboardData(CF_HDROP, hGlobal);
    CloseClipboard();
}

// Auto-drop UA when recasting
void GameSettings::OnAgentStartCast(GW::HookStatus*, GW::UI::UIMessage, void* wParam, void*)
{
    if (!(wParam && drop_ua_on_cast)) {
        return;
    }
    const struct Casting {
        uint32_t agent_id;
        GW::Constants::SkillID skill_id;
    }* casting = static_cast<Casting*>(wParam);
    if (casting->agent_id == GW::Agents::GetControlledCharacterId() && casting->skill_id == GW::Constants::SkillID::Unyielding_Aura) {
        // Cancel UA before recast
        const GW::Buff* buff = GW::Effects::GetPlayerBuffBySkillId(casting->skill_id);
        if (buff && buff->skill_id != GW::Constants::SkillID::No_Skill) {
            GW::Effects::DropBuff(buff->buff_id);
        }
    }
};

// Redirect /wiki commands to go to useful pages
void GameSettings::OnOpenWiki(GW::HookStatus* status, const GW::UI::UIMessage message_id, void* wParam, void*)
{
    const std::string url = TextUtils::ToLower(static_cast<char*>(wParam));
    if (strstr(url.c_str(), "/wiki/main_page")) {
        // Redirect /wiki to /wiki <current map name>
        status->blocked = true;
        const GW::AreaInfo* map = GW::Map::GetCurrentMapInfo();
        pending_wiki_search_term = new EncString(map->name_id);
    }
    else if (strstr(url.c_str(), "?search=quest")) {
        // Redirect /wiki quest to /wiki <current quest name>
        status->blocked = true;
        const auto* quest = GW::QuestMgr::GetActiveQuest();
        if (quest) {
            char redirected_url[255];
            snprintf(redirected_url, _countof(redirected_url), "%sGame_link:Quest_%d", WikiUrl(L"").c_str(), quest->quest_id);
            SendUIMessage(message_id, redirected_url);
        }
        else {
            Log::Error("No current active quest");
        }
    }
    else if (strstr(url.c_str(), "?search=target")) {
        // Redirect /wiki target to /wiki <current target name>
        status->blocked = true;
        const GW::Agent* a = GW::Agents::GetTarget();
        if (a) {
            pending_wiki_search_term = new EncString(GW::Agents::GetAgentEncName(a));
        }
        else {
            Log::Error("No current target");
        }
    }
}

// Set window title to player name on map load
void GameSettings::OnMapLoaded(GW::HookStatus*, GW::Packet::StoC::MapLoaded*)
{
    instance_entered_at = TIMER_INIT();
    SetWindowTitle(set_window_title_as_charname);
}

// Hide more than 10 signets of capture
void GameSettings::OnUpdateSkillCount(GW::HookStatus*, void* packet)
{
    const auto pak = static_cast<GW::Packet::StoC::UpdateSkillCountAfterMapLoad*>(packet);
    if (limit_signets_of_capture && static_cast<GW::Constants::SkillID>(pak->skill_id) == GW::Constants::SkillID::Signet_of_Capture) {
        actual_signets_of_capture_amount = pak->count;
        if (pak->count > 10) {
            pak->count = 10;
        }
    }
}

// Default colour for agent name tags
void GameSettings::OnAgentNameTag(GW::HookStatus*, const GW::UI::UIMessage msgid, void* wParam, void*)
{
    if (msgid != GW::UI::UIMessage::kShowAgentNameTag && msgid != GW::UI::UIMessage::kSetAgentNameTagAttribs) {
        return;
    }
    const auto tag = static_cast<GW::UI::AgentNameTagInfo*>(wParam);
    switch (static_cast<DEFAULT_NAMETAG_COLOR>(tag->text_color)) {
        case DEFAULT_NAMETAG_COLOR::NPC:
            tag->text_color = nametag_color_npc;
            break;
        case DEFAULT_NAMETAG_COLOR::ENEMY:
            tag->text_color = nametag_color_enemy;
            break;
        case DEFAULT_NAMETAG_COLOR::GADGET:
            tag->text_color = nametag_color_gadget;
            break;
        case DEFAULT_NAMETAG_COLOR::PLAYER_IN_PARTY:
            tag->text_color = nametag_color_player_in_party;
            break;
        case DEFAULT_NAMETAG_COLOR::PLAYER_OTHER:
            tag->text_color = nametag_color_player_other;
            break;
        case DEFAULT_NAMETAG_COLOR::PLAYER_SELF:
            tag->text_color = nametag_color_player_self;
            break;
        case DEFAULT_NAMETAG_COLOR::ITEM:
            tag->text_color = nametag_color_item;
            break;
    }
}
