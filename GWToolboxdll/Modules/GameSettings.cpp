#include "stdafx.h"

#include <GWCA/Utilities/MemoryPatcher.h>
#include <GWCA/Utilities/Scanner.h>

#include <GWCA/Constants/AgentIDs.h>
#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Maps.h>
#include <GWCA/Constants/Skills.h>

#include <GWCA/Context/GuildContext.h>
#include <GWCA/Context/PartyContext.h>
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

#include <GWCA/Utilities/Hooker.h>

#include <Utils/GuiUtils.h>
#include <Utils/ToolboxUtils.h>

#ifdef _DEBUG
#include <Windows/StringDecoderWindow.h>
#endif

#include <Modules/ChatSettings.h>
#include <Modules/DialogModule.h>
#include <Modules/GameSettings.h>

#include <Color.h>
#include <hidusage.h>
#include <Logger.h>
#include <Timer.h>
#include <Defines.h>
#include <GWCA/Managers/QuestMgr.h>

#pragma warning(disable : 6011)

using namespace GuiUtils;
using namespace ToolboxUtils;

namespace {

    GW::MemoryPatcher ctrl_click_patch;
    GW::MemoryPatcher tome_patch;
    GW::MemoryPatcher gold_confirm_patch;
    GW::MemoryPatcher item_description_patch;
    GW::MemoryPatcher item_description_patch2;
    GW::MemoryPatcher skill_description_patch;

    void SetWindowTitle(bool enabled) {
        if (!enabled)
            return;
        HWND hwnd = GW::MemoryMgr::GetGWWindowHandle();
        if (!hwnd) return;
        std::wstring title = GetPlayerName();
        if (!title.empty())
            SetWindowTextW(hwnd, title.c_str());
    }

    void SaveChannelColor(ToolboxIni *ini, const char *section, const char *chanstr, GW::Chat::Channel chan) {
        char key[128];
        GW::Chat::Color sender, message;
        GW::Chat::GetChannelColors(chan, &sender, &message);
        // @Cleanup: We relie on the fact the Color and GW::Chat::Color are the same format
        snprintf(key, 128, "%s_color_sender", chanstr);
        Colors::Save(ini, section, key, (Color)sender);
        snprintf(key, 128, "%s_color_message", chanstr);
        Colors::Save(ini, section, key, (Color)message);
    }

    void LoadChannelColor(ToolboxIni *ini, const char *section, const char *chanstr, GW::Chat::Channel chan) {
        char key[128];
        GW::Chat::Color sender, message;
        GW::Chat::GetDefaultColors(chan, &sender, &message);
        snprintf(key, 128, "%s_color_sender", chanstr);
        sender = Colors::Load(ini, section, key, (Color)sender);
        GW::Chat::SetSenderColor(chan, sender);
        snprintf(key, 128, "%s_color_message", chanstr);
        message = Colors::Load(ini, section, key, (Color)message);
        GW::Chat::SetMessageColor(chan, message);
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

    GuiUtils::EncString* pending_wiki_search_term = nullptr;

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

    bool disable_gold_selling_confirmation = false;
    bool collectors_edition_emotes = true;

    bool block_transmogrify_effect = false;
    bool block_sugar_rush_effect = false;
    bool block_snowman_summoner = false;
    bool block_party_poppers = false;
    bool block_bottle_rockets = false;
    bool block_ghostinthebox_effect = false;
    bool block_sparkly_drops_effect = false;

    bool auto_age2_on_age = true;
    bool auto_age_on_vanquish = false;

    bool auto_open_locked_chest = false;

    bool keep_current_quest_when_new_quest_added = false;

    bool automatically_flag_pet_to_fight_called_target = true;

    Color nametag_color_npc = static_cast<Color>(DEFAULT_NAMETAG_COLOR::NPC);
    Color nametag_color_player_self = static_cast<Color>(DEFAULT_NAMETAG_COLOR::PLAYER_SELF);
    Color nametag_color_player_other = static_cast<Color>(DEFAULT_NAMETAG_COLOR::PLAYER_OTHER);
    Color nametag_color_player_in_party = static_cast<Color>(DEFAULT_NAMETAG_COLOR::PLAYER_IN_PARTY);
    Color nametag_color_gadget = static_cast<Color>(DEFAULT_NAMETAG_COLOR::GADGET);
    Color nametag_color_enemy = static_cast<Color>(DEFAULT_NAMETAG_COLOR::ENEMY);
    Color nametag_color_item = static_cast<Color>(DEFAULT_NAMETAG_COLOR::ITEM);

    constexpr float checkbox_w = 270.f;

    enum PING_PARTS {
        NAME=1,
        DESC=2
    };

    struct PendingCast {
        uint32_t slot = 0;
        uint32_t target_id = 0;
        uint32_t call_target = 0;
        bool cast_next_frame = false;
        void reset(uint32_t _slot = 0 , uint32_t _target_id = 0, uint32_t _call_target = 0) {
            slot = _slot;
            target_id = _target_id;
            call_target = _call_target;
            cast_next_frame = false;
        }
        const GW::AgentLiving* GetTarget() const {
            const GW::AgentLiving* target = static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(target_id));
            return target && target->GetIsLivingType() ? target : nullptr;
        }

        GW::Constants::SkillID GetSkill() const {
            const GW::Skillbar* skillbar = GW::SkillbarMgr::GetPlayerSkillbar();
            return skillbar && skillbar->IsValid() ? skillbar->skills[slot].skill_id : GW::Constants::SkillID::No_Skill;
        }
    } pending_cast;

    struct SkillData {
        GW::Constants::Profession primary;
        GW::Constants::Profession secondary;
        uint32_t attribute_count;
        GW::Constants::Attribute attribute_keys[12];
        uint32_t attribute_values[12];
        GW::Constants::SkillID skill_ids[8];
    };

    // prophecies = factions
    const std::map<GW::Constants::SkillID, GW::Constants::SkillID> duplicate_skills = {
        {GW::Constants::SkillID::Desperation_Blow, GW::Constants::SkillID::Drunken_Blow },
        {GW::Constants::SkillID::Galrath_Slash, GW::Constants::SkillID::Silverwing_Slash },
        {GW::Constants::SkillID::Griffons_Sweep, GW::Constants::SkillID::Leviathans_Sweep },
        {GW::Constants::SkillID::Penetrating_Blow, GW::Constants::SkillID::Penetrating_Chop },
        {GW::Constants::SkillID::Pure_Strike, GW::Constants::SkillID::Jaizhenju_Strike },

        {GW::Constants::SkillID::Bestial_Pounce, GW::Constants::SkillID::Savage_Pounce },
        {GW::Constants::SkillID::Dodge, GW::Constants::SkillID::Zojuns_Haste },
        {GW::Constants::SkillID::Penetrating_Attack, GW::Constants::SkillID::Sundering_Attack },
        {GW::Constants::SkillID::Point_Blank_Shot, GW::Constants::SkillID::Zojuns_Shot },
        {GW::Constants::SkillID::Tigers_Fury, GW::Constants::SkillID::Bestial_Fury },

        {GW::Constants::SkillID::Divine_Healing, GW::Constants::SkillID::Heavens_Delight },
        {GW::Constants::SkillID::Heal_Area, GW::Constants::SkillID::Kareis_Healing_Circle },
        {GW::Constants::SkillID::Heal_Other, GW::Constants::SkillID::Jameis_Gaze },
        {GW::Constants::SkillID::Holy_Strike, GW::Constants::SkillID::Stonesoul_Strike },
        {GW::Constants::SkillID::Symbol_of_Wrath, GW::Constants::SkillID::Kirins_Wrath },

        {GW::Constants::SkillID::Desecrate_Enchantments, GW::Constants::SkillID::Defile_Enchantments },
        {GW::Constants::SkillID::Shadow_Strike, GW::Constants::SkillID::Lifebane_Strike },
        {GW::Constants::SkillID::Spinal_Shivers, GW::Constants::SkillID::Shivers_of_Dread },
        {GW::Constants::SkillID::Touch_of_Agony, GW::Constants::SkillID::Wallows_Bite },
        {GW::Constants::SkillID::Vampiric_Touch, GW::Constants::SkillID::Vampiric_Bite },

        {GW::Constants::SkillID::Arcane_Thievery, GW::Constants::SkillID::Arcane_Larceny },
        {GW::Constants::SkillID::Ethereal_Burden, GW::Constants::SkillID::Kitahs_Burden },
        {GW::Constants::SkillID::Inspired_Enchantment, GW::Constants::SkillID::Revealed_Enchantment },
        {GW::Constants::SkillID::Inspired_Hex, GW::Constants::SkillID::Revealed_Hex },
        {GW::Constants::SkillID::Sympathetic_Visage, GW::Constants::SkillID::Ancestors_Visage },

        {GW::Constants::SkillID::Earthquake, GW::Constants::SkillID::Dragons_Stomp }
    };
    // luxon = kurzick
    const std::map<GW::Constants::SkillID, GW::Constants::SkillID> factions_skills = {
        {GW::Constants::SkillID::Save_Yourselves_luxon, GW::Constants::SkillID::Save_Yourselves_kurzick },
        {GW::Constants::SkillID::Aura_of_Holy_Might_luxon, GW::Constants::SkillID::Aura_of_Holy_Might_kurzick },
        {GW::Constants::SkillID::Elemental_Lord_luxon, GW::Constants::SkillID::Elemental_Lord_kurzick },
        {GW::Constants::SkillID::Ether_Nightmare_luxon, GW::Constants::SkillID::Ether_Nightmare_kurzick },
        {GW::Constants::SkillID::Selfless_Spirit_luxon, GW::Constants::SkillID::Selfless_Spirit_kurzick },
        {GW::Constants::SkillID::Shadow_Sanctuary_luxon, GW::Constants::SkillID::Shadow_Sanctuary_kurzick },
        {GW::Constants::SkillID::Signet_of_Corruption_luxon, GW::Constants::SkillID::Signet_of_Corruption_kurzick },
        {GW::Constants::SkillID::Spear_of_Fury_luxon, GW::Constants::SkillID::Spear_of_Fury_kurzick },
        {GW::Constants::SkillID::Summon_Spirits_luxon, GW::Constants::SkillID::Summon_Spirits_kurzick },
        {GW::Constants::SkillID::Triple_Shot_luxon, GW::Constants::SkillID::Triple_Shot_kurzick }
    };
    struct LoadSkillBarPacket {
        uint32_t header = 0;
        uint32_t agent_id = 0;
        uint32_t skill_ids_size = 8;
        GW::Constants::SkillID skill_ids[8]{};
    } skillbar_packet;

    // Before the game loads the skill bar you want, copy the data over for checking once the bar is loaded.
    void OnPreLoadSkillBar(GW::HookStatus*, GW::UI::UIMessage message_id, void* wparam, void*) {
        ASSERT(message_id == GW::UI::UIMessage::kSendLoadSkillbar && wparam);
        struct Pack {
            uint32_t agent_id = 0;
            GW::Constants::SkillID skill_ids[8];
        } *packet = static_cast<Pack*>(wparam);
        // @Enhancement: may cause weird stuff if we load loads of builds at once; heros could get mixed up with player. Use a map.
        memcpy(skillbar_packet.skill_ids, packet->skill_ids, sizeof(skillbar_packet.skill_ids));
        skillbar_packet.agent_id = packet->agent_id;
    }

    // Takes SkillData* ptr, rectifies any missing dupe skills. True if bar has been tweaked.
    bool FixLoadSkillData(GW::Constants::SkillID* skill_ids) {
        auto find_skill = [](const GW::Constants::SkillID* skill_ids, const GW::Constants::SkillID skill_id) {
            for (int i = 0; i < 8; i++) {
                if (skill_ids[i] == skill_id)
                    return i;
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
            if (found_first == -1 && found_second == -1)
                continue;
            unlocked_first = GW::SkillbarMgr::GetIsSkillUnlocked(first_skill);
            unlocked_second = GW::SkillbarMgr::GetIsSkillUnlocked(second_skill);

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
            if (found_first == -1 && found_second == -1)
                continue;
            if (found_first != -1 && found_second != -1)
                continue;
            unlocked_first = GW::SkillbarMgr::GetIsSkillUnlocked(luxon_skill);
            unlocked_second = GW::SkillbarMgr::GetIsSkillUnlocked(kurzick_skill);

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
                const int skillbar_index = std::max(found_first, found_second);
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
    void OnPostLoadSkillBar(GW::HookStatus*, void* packet) {
        LoadSkillBarPacket* post_pack = (LoadSkillBarPacket*)packet;
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

    typedef void(__cdecl* ShowAgentFactionGain_pt)(uint32_t agent_id, uint32_t stat_type, uint32_t amount_gained);
    ShowAgentFactionGain_pt ShowAgentFactionGain_Func = nullptr, ShowAgentFactionGain_Ret = nullptr;
    // Block overhead faction gain numbers
    void OnShowAgentFactionGain(uint32_t agent_id, uint32_t stat_type, uint32_t amount_gained) {
        GW::Hook::EnterHook();
        if (!block_faction_gain)
            ShowAgentFactionGain_Ret(agent_id, stat_type, amount_gained);
        GW::Hook::LeaveHook();
    }

    typedef void(__cdecl* ShowAgentExperienceGain_pt)(uint32_t agent_id, uint32_t amount_gained);
    ShowAgentExperienceGain_pt ShowAgentExperienceGain_Func = nullptr, ShowAgentExperienceGain_Ret = nullptr;
    // Block overhead experience gain numbers
    void OnShowAgentExperienceGain(uint32_t agent_id, uint32_t amount_gained) {
        GW::Hook::EnterHook();
        const bool blocked = (block_experience_gain || (block_zero_experience_gain && amount_gained == 0));
        if (!blocked)
            ShowAgentExperienceGain_Ret(agent_id, amount_gained);
        GW::Hook::LeaveHook();
    }

    // Key held to show/hide item descriptions
    constexpr int modifier_key_item_descriptions = VK_MENU;
    int modifier_key_item_descriptions_key_state = 0;

    typedef void(__cdecl* GetItemDescription_pt)(uint32_t item_id, uint32_t flags, uint32_t quantity, uint32_t unk, wchar_t** out, wchar_t** out2);
    GetItemDescription_pt GetItemDescription_Func = nullptr, GetItemDescription_Ret = nullptr;
    // Block full item descriptions
    void OnGetItemDescription(uint32_t item_id, uint32_t flags, uint32_t quantity, uint32_t unk, wchar_t** name_out, wchar_t** description_out) {
        GW::Hook::EnterHook();
        bool block_description = disable_item_descriptions_in_outpost && IsOutpost() || disable_item_descriptions_in_explorable && IsExplorable();
        block_description = block_description && GetKeyState(modifier_key_item_descriptions) >= 0;
        GetItemDescription_Ret(item_id, flags, quantity, unk, name_out, block_description ? nullptr : description_out);
        if (block_description && description_out)
            *description_out = nullptr;
        GW::Hook::LeaveHook();
    }

    // Key held to show/hide skill descriptions
    constexpr int modifier_key_skill_descriptions = VK_MENU;
    int modifier_key_skill_descriptions_key_state = 0;

    typedef void(__cdecl* CreateCodedTextLabel_pt)(uint32_t frame_id, const wchar_t* encoded_string);
    CreateCodedTextLabel_pt CreateEncodedTextLabel_Func = nullptr;
    // Hide skill description in tooltip; called by GW to add the description of the skill to a skill tooltip
    void CreateCodedTextLabel_SkillDescription(uint32_t frame_id, const wchar_t* encoded_string) {
        GW::Hook::EnterHook();
        bool block_description = disable_skill_descriptions_in_outpost && IsOutpost() || disable_skill_descriptions_in_explorable && IsExplorable();
        block_description = block_description && GetKeyState(modifier_key_item_descriptions) >= 0;
        if (block_description)
            encoded_string = L"\x101"; // Decodes to ""
        CreateEncodedTextLabel_Func(frame_id, encoded_string);
        GW::Hook::LeaveHook();
    }

    typedef void(__cdecl* SetGlobalNameTagVisibility_pt)(uint32_t flags);
    SetGlobalNameTagVisibility_pt SetGlobalNameTagVisibility_Func = 0;
    uint32_t* GlobalNameTagVisibilityFlags = 0;

    GW::UI::UIInteractionCallback OnMinOrRestoreOrExitBtnClicked_Func = 0;
    GW::UI::UIInteractionCallback OnMinOrRestoreOrExitBtnClicked_Ret = 0;
    bool closing_gw = false;
    void OnMinOrRestoreOrExitBtnClicked(GW::UI::InteractionMessage* message, void* wparam, void* lparam) {
        GW::Hook::EnterHook();
        if (message->message_id == 0x2f && wparam) {
            struct MouseParams {
                uint32_t button_id;
                uint32_t button_id_dupe;
                uint32_t current_state; // 0x5 = hovered, 0x6 = mouse down
            } *param = (MouseParams*)wparam;
            if (param->button_id == 0x3 && param->current_state == 0x6) {
                param->current_state = 0x5; // Revert state to avoid GW closing the window on mouse up

                // Left button clicked, on the exit button (ID 0x3)
                if (!closing_gw)
                    SendMessage(GW::MemoryMgr::GetGWWindowHandle(), WM_CLOSE, NULL, NULL);
                closing_gw = true;
                GW::Hook::LeaveHook();
                return;
            }
        }
        OnMinOrRestoreOrExitBtnClicked_Ret(message, wparam, lparam);
        GW::Hook::LeaveHook();
    }

    GW::MemoryPatcher skip_map_entry_message_patch;

    // Refresh agent name tags when allegiance changes ( https://github.com/HasKha/GWToolboxpp/issues/781 )
    void OnAgentAllegianceChanged(GW::HookStatus*, GW::Packet::StoC::AgentUpdateAllegiance*) {
        // Backup the current name tag flag state, then "flash" nametags to update.
        uint32_t prev_flags = *GlobalNameTagVisibilityFlags;
        SetGlobalNameTagVisibility_Func(0);
        SetGlobalNameTagVisibility_Func(prev_flags);
        ASSERT(*GlobalNameTagVisibilityFlags == prev_flags);
    }

    uint32_t current_party_target_id = 0;
    // Record current party target - this isn't always the same as the compass target.
    void OnPartyTargetChanged(GW::HookStatus*, GW::UI::UIMessage message_id, void* wparam, void*) {
        struct Packet {
            uint32_t source;
            uint32_t identifier;
        } *party_target_info = (Packet*)wparam;
        switch (message_id) {
        case GW::UI::UIMessage::kTargetPlayerPartyMember: {
            const GW::Player* p = GW::PlayerMgr::GetPlayerByID(party_target_info->identifier);
            if (p && p->agent_id) {
                current_party_target_id = p->agent_id;
            }
        } break;
        case GW::UI::UIMessage::kTargetNPCPartyMember: {
            if (IsAgentInParty(party_target_info->identifier)) {
                current_party_target_id = party_target_info->identifier;
            }
        } break;
        case GW::UI::UIMessage::kCalledTargetChange: {
            if (automatically_flag_pet_to_fight_called_target && party_target_info->source == GW::PlayerMgr::GetPlayerNumber()) {
                GW::PartyMgr::SetPetBehavior(GW::HeroBehavior::Fight, party_target_info->identifier);
            }
        } break;
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
        void reset(uint32_t _agent_id = 0) {
            identifier = _agent_id;
            stage = identifier ? Stage::Kick : Stage::None;
            start_ts = TIMER_INIT();
        }
    } pending_reinvite;

    // Handle processing from CmdReinvite command
    void UpdateReinvite() {
        if (pending_reinvite.stage == PendingReinvite::Stage::None)
            return; // Nothing to do
        if (!IsOutpost()) {
            return pending_reinvite.reset();
        }
        if (TIMER_DIFF(pending_reinvite.start_ts) > 2000) {
            // Timeout (map change etc)
            return pending_reinvite.reset();
        }
        GW::Player* first_player = nullptr;
        GW::Player* next_player = nullptr;
        GW::AgentLiving* me = GW::Agents::GetPlayerAsAgentLiving();
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
            if (!member.connected())
                continue;
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
        bool is_leader = me->login_number == first_player->player_number;

        switch (pending_reinvite.stage) {
        case PendingReinvite::Stage::Kick: {
            if (!IsAgentInParty(pending_reinvite.identifier)) {
                Log::Error("Choose target party member to reinvite");
                return pending_reinvite.reset();
            }
            // Player kick
            GW::AgentLiving* target_living = 0;
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
            GW::PartyInfo* check = 0;
            auto hero_info = GetHeroPartyMember(pending_reinvite.identifier, &check);
            if (hero_info) {
                if (check != party)
                    return;
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
            auto henchman_info = GetHenchmanPartyMember(pending_reinvite.identifier, &check);
            if (henchman_info) {
                if (check != party)
                    return;
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
        } break;
        case PendingReinvite::Stage::InviteHero: {
            GW::HeroInfo* hero_info = GetHeroInfo(pending_reinvite.identifier);
            if (!hero_info) {
                Log::Error("Failed to get hero info for %d", pending_reinvite.identifier);
                return pending_reinvite.reset();
            }
            if (hero_info->agent_id && IsHeroInParty(hero_info->agent_id))
                return;
            GW::PartyMgr::AddHero(pending_reinvite.identifier);
            return pending_reinvite.reset();
        } break;
        case PendingReinvite::Stage::InvitePlayer: {
            GW::Player* player = GW::PlayerMgr::GetPlayerByID(pending_reinvite.identifier);
            if (!player) {
                Log::Error("Failed to get player info for %d", pending_reinvite.identifier);
                return pending_reinvite.reset();
            }
            if (IsPlayerInParty(pending_reinvite.identifier))
                return;
            GW::PartyMgr::InvitePlayer(pending_reinvite.identifier);
            return pending_reinvite.reset();
        } break;
        case PendingReinvite::Stage::InviteHenchman: {
            if (!IsHenchman(pending_reinvite.identifier)) {
                Log::Error("Failed to get henchman info for %d", pending_reinvite.identifier);
                return pending_reinvite.reset();
            }
            if (IsHenchmanInParty(pending_reinvite.identifier))
                return;
            GW::PartyMgr::AddHenchman(pending_reinvite.identifier);
            return pending_reinvite.reset();
        } break;
        }
        pending_reinvite.reset();
    }
    // Check and re-render item tooltips if modifier key held
    void UpdateItemTooltip() {
        if (GetKeyState(modifier_key_item_descriptions) == modifier_key_item_descriptions_key_state)
            return;
        modifier_key_item_descriptions_key_state = GetKeyState(modifier_key_item_descriptions);
        if (IsExplorable()) {
            if (!disable_item_descriptions_in_explorable)
                return;
        }
        else if (IsOutpost()) {
            if (!disable_item_descriptions_in_outpost)
                return;
        }
        else {
            return; // Loading
        }
        // Trigger re-render of item tooltip
        const auto hovered_item = GW::Items::GetHoveredItem();
        if (!hovered_item)
            return;
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
        GW::GameThread::Enqueue([items = items_triggered]() {
            if (items[0])
                GW::UI::SendUIMessage(GW::UI::UIMessage::kItemUpdated, &items[0]);
            if (items[1])
                GW::UI::SendUIMessage(GW::UI::UIMessage::kItemUpdated, &items[1]);
            });
    }
    // Check and re-render item tooltips if modifier key held
    void UpdateSkillTooltip() {
        if (GetKeyState(modifier_key_skill_descriptions) == modifier_key_skill_descriptions_key_state)
            return;
        modifier_key_skill_descriptions_key_state = GetKeyState(modifier_key_skill_descriptions);
        if (IsExplorable()) {
            if (!disable_skill_descriptions_in_explorable)
                return;
        }
        else if (IsOutpost()) {
            if (!disable_skill_descriptions_in_outpost)
                return;
        }
        else {
            return; // Loading
        }
        // Trigger re-render of the tooltip by triggering a fake change of concise skill descriptions ui message
        auto skill = GW::SkillbarMgr::GetHoveredSkill();
        if (!skill)
            return;
        if (GW::SkillbarMgr::GetHoveredSkill()) {
            GW::GameThread::Enqueue([skill]() {
                GW::UI::SendUIMessage(GW::UI::UIMessage::kTitleProgressUpdated, (void*)skill->title);
                });
        }
    }
    GW::HookEntry OnCreateUIComponent_Entry;
    // Flag email address entry field as a password format (e.g. asterisks instead of email)
    void OnCreateUIComponent(GW::UI::CreateUIComponentPacket* msg) {
        if (hide_email_address && msg->component_label && wcscmp(msg->component_label, L"EditAccount") == 0) {
            msg->component_flags |= 0x01000000;
        }
    }

    void OnKeyDownAction(GW::HookStatus*, uint32_t key) {
        switch (static_cast<GW::UI::ControlAction>(key)) {
            case GW::UI::ControlAction_TargetNearestItem:
                if (lazy_chest_looting) {
                    targeting_nearest_item = true;
                    GW::Agents::ChangeTarget((uint32_t)0); // To ensure OnChangeTarget is triggered
                }
                break;
        }
    }
    void OnKeyUpAction(GW::HookStatus*, uint32_t key) {
        switch (static_cast<GW::UI::ControlAction>(key)) {
            case GW::UI::ControlAction_TargetNearestItem:
                targeting_nearest_item = false;
                break;
        }
    }
    // Logic for targetting nearest item; Don't target chest as nearest item, Target green items from chest last
    void OnChangeTarget(GW::HookStatus* status, GW::UI::UIMessage, void* wParam, void*)
    {
        if (!targeting_nearest_item)
            return;
        const auto msg = (GW::UI::ChangeTargetUIMsg*)wParam;
        auto chosen_target = static_cast<GW::Agent*>(GW::Agents::GetAgentByID(msg->manual_target_id));
        if (!chosen_target)
            return;
        uint32_t override_manual_agent_id = 0;
        GW::Item* target_item = nullptr;
        const auto agents = GW::Agents::GetAgentArray();
        const auto me = agents ? GW::Agents::GetPlayer() : nullptr;
        if (!me) return;
        // If the item targeted is a green that belongs to me, and its next to the chest, try to find another item instead.
        if (chosen_target->GetIsItemType() && ((GW::AgentItem*)chosen_target)->owner == me->agent_id) {
            target_item = GW::Items::GetItemById(((GW::AgentItem*)chosen_target)->item_id);
            if (!(target_item && (target_item->interaction & 0x10) == 0))
                return; // Failed to get target item, or is not green.
            for (auto* agent : *agents) {
                if (!(agent && agent->GetIsGadgetType())) continue;
                if (GW::GetDistance(agent->pos, chosen_target->pos) <= GW::Constants::Range::Nearby) {
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
                if (!(agent && agent->GetIsItemType())) continue;
                const auto agent_item = agent->GetAsAgentItem();
                if (agent_item->owner != me->agent_id) continue;
                target_item = GW::Items::GetItemById(agent_item->item_id);
                if ((target_item->interaction & 0x10) != 0)
                    continue; // Don't target green items.
                if (GW::GetDistance(agent->pos, chosen_target->pos) > GW::Constants::Range::Nearby)
                    continue;
                float dist = GW::GetDistance(me->pos, agent->pos);
                if (dist > closest_item_dist) continue;
                override_manual_agent_id = agent->agent_id;
                closest_item_dist = dist;
            }
        }
        if (override_manual_agent_id) {
            status->blocked = true;
            GW::Agents::ChangeTarget(override_manual_agent_id);
        }
    }

    bool IsOnline(GW::FriendStatus status) {
        switch (status) {
        case GW::FriendStatus::Away:
        case GW::FriendStatus::DND:
        case GW::FriendStatus::Online:
            return true;
        }
        return false;
    }

    void FriendStatusCallback(
        GW::HookStatus*,const GW::Friend* old_state, const GW::Friend* new_state)
    {
        if (!(new_state && old_state))
            return; // Friend added, or deleted
        if (old_state->status == GW::FriendStatus::Unknown)
            return; // First info about friend
        const bool was_online = IsOnline(old_state->status);
        const bool is_online = IsOnline(new_state->status);
        if (was_online == is_online)
            return;
        if (is_online && notify_when_friends_online) {
            GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GLOBAL, std::format(L"<a=1>{}</a> ({}) has just logged in.", new_state->charname, new_state->alias).c_str());
        }
        else if (notify_when_friends_offline) {
            GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GLOBAL, std::format(L"{} ({}) has just logged out.", old_state->charname, old_state->alias).c_str());
        }
    }

    void DrawNotificationsSettings() {
        ImGui::Text("Flash Guild Wars taskbar icon when:");
        ImGui::ShowHelp("Only triggers when Guild Wars is not the active window");
        ImGui::Indent();
        ImGui::StartSpacedElements(checkbox_w);
        ImGui::NextSpacedElement();
        ImGui::NextSpacedElement();
        ImGui::Checkbox("Receiving a party invite", &flash_window_on_party_invite);
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

            ImGui::Text("Show a message when a friend:");
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

        ImGui::Text("Show a message when a player:");
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
    void OnPostQuestUIMessage(GW::HookStatus* status, GW::UI::UIMessage message_id, void*, void*) {
        switch (message_id) {
        case GW::UI::UIMessage::kSendSetActiveQuest:
            if (status->blocked)
                break;
            player_requested_active_quest_id = GW::QuestMgr::GetActiveQuestId();
            break;
        case GW::UI::UIMessage::kQuestAdded:
            if (status->blocked)
                break;
            if (GW::QuestMgr::GetActiveQuestId() == player_requested_active_quest_id)
                break;
            if (keep_current_quest_when_new_quest_added) {
                // Re-request a quest change
                const auto quest = GW::QuestMgr::GetQuest(player_requested_active_quest_id);
                if (!quest)
                    break;
                GW::Packet::StoC::QuestAdd packet;
                packet.quest_id = quest->quest_id;
                packet.marker = quest->marker;
                packet.map_to = quest->map_to;
                packet.log_state = quest->log_state;
                packet.map_from = quest->map_from;
                wcscpy(packet.location, quest->location);
                wcscpy(packet.name, quest->name);
                wcscpy(packet.npc, quest->npc);
                GW::StoC::EmulatePacket(&packet);
                GW::QuestMgr::SetActiveQuestId(quest->quest_id);
            }

            break;
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

void GameSettings::PingItem(GW::Item* item, uint32_t parts) {
    if (!(item && GW::PlayerMgr::GetPlayerByID())) return;
    std::wstring out;
    if ((parts & PING_PARTS::NAME) && item->complete_name_enc) {
        if (out.length())
            out += L"\x2\x102\x2";
        out += item->complete_name_enc;
    }
    else if ((parts & PING_PARTS::NAME) && item->name_enc) {
        if (out.length())
            out += L"\x2\x102\x2";
        out += item->name_enc;
    }
    if ((parts & PING_PARTS::DESC) && item->info_string) {
        if (out.length())
            out += L"\x2\x102\x2";
        out += ShorthandItemDescription(item);
    }
    #ifdef _DEBUG
        printf("Pinged item:\n");
        StringDecoderWindow::PrintEncStr(out.c_str());
    #endif

    PendingChatMessage* m = PendingChatMessage::queueSend(GW::Chat::Channel::CHANNEL_GROUP, out.c_str(),GW::PlayerMgr::GetPlayerByID()->name_enc);
        if (m) ChatSettings::AddPendingMessage(m);
}
void GameSettings::PingItem(uint32_t item_id, uint32_t parts) {
    return PingItem(GW::Items::GetItemById(item_id), parts);
}

PendingChatMessage::PendingChatMessage(GW::Chat::Channel channel, const wchar_t* enc_message, const wchar_t* enc_sender) : channel(channel) {
    invalid = !enc_message || !enc_sender;
    if (!invalid) {
        wcscpy(encoded_sender, enc_sender);
        wcscpy(encoded_message, enc_message);
        Init();
    }
}
PendingChatMessage* PendingChatMessage::queueSend(GW::Chat::Channel channel, const wchar_t* enc_message, const wchar_t* enc_sender) {
    PendingChatMessage* m = new PendingChatMessage(channel, enc_message, enc_sender);
    if (m->invalid) {
        delete m;
        return nullptr;
    }
    m->SendIt();
    return m;
}

PendingChatMessage* PendingChatMessage::queuePrint(GW::Chat::Channel channel, const wchar_t* enc_message, const wchar_t* enc_sender) {
    PendingChatMessage* m = new PendingChatMessage(channel, enc_message, enc_sender);
    if (m->invalid) {
        delete m;
        return nullptr;
    }
    return m;
}

bool PendingChatMessage::Cooldown() {
    return last_send && clock() < last_send + (clock_t)(CLOCKS_PER_SEC / 2);
}

bool PendingChatMessage::Send()
{
    if (!IsDecoded() || this->invalid) return false; // Not ready or invalid.
    std::vector<std::wstring> sanitised_lines = SanitiseForSend();
    wchar_t buf[120];
    size_t len = 0;
    for (size_t i = 0; i < sanitised_lines.size(); i++) {
        size_t san_len = sanitised_lines[i].length();
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
void PendingChatMessage::Init() {
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
std::vector<std::wstring> PendingChatMessage::SanitiseForSend() {
    const std::wregex no_tags(L"<[^>]+>");
    const std::wregex no_new_lines(L"\n");
    std::wstring sanitised, sanitised2, temp;
    std::regex_replace(std::back_inserter(sanitised), output_message.begin(), output_message.end(), no_tags, L"");
    std::regex_replace(std::back_inserter(sanitised2), sanitised.begin(), sanitised.end(), no_new_lines, L"|");
    std::vector<std::wstring> parts;
    std::wstringstream wss(sanitised2);
    while (std::getline(wss, temp, L'|'))
        parts.push_back(temp);
    return parts;
}
bool PendingChatMessage::PrintMessage() {
    if (!IsDecoded() || this->invalid) return false; // Not ready or invalid.
    if (this->printed) return true; // Already printed.
    wchar_t buffer[512];
    switch (channel) {
    case GW::Chat::Channel::CHANNEL_EMOTE:
        GW::Chat::Color dummy, messageCol; // Needed for GW::Chat::GetChannelColors
        GW::Chat::GetChannelColors(GW::Chat::Channel::CHANNEL_ALLIES, &dummy, &messageCol); // ...but set the message to be same color as ally chat
        swprintf(buffer, 512, L"<a=2>%ls</a>: <c=#%06X>%ls</c>", output_sender.c_str(), messageCol & 0x00FFFFFF, output_message.c_str());
        GW::Chat::WriteChat(channel, buffer);
        break;
    default:
        swprintf(buffer, 512, L"<a=2>%ls</a>: %ls", output_sender.c_str(), output_message.c_str());
        GW::Chat::WriteChat(channel, buffer);
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

void GameSettings::Initialize() {
    ToolboxModule::Initialize();

    // Patch that allow storage page (and Anniversary page) to work.
    uintptr_t address = GW::Scanner::Find("\xEB\x17\x33\xD2\x8D\x4A\x06\xEB", "xxxxxxxx", -4);
    printf("[SCAN] StoragePatch = %p\n", (void *)address);

    // Xunlai Chest has a behavior where if you
    // 1. Open chest on page 1 to 14
    // 2. Close chest & open it again
    // -> You should still be on the same page
    // But, if you try with the material page (or anniversary page in the case when you bought all other storage page)
    // you will get back the the page 1. I think it was a intended use for material page & forgot to fix it
    // when they added anniversary page so we do it ourself.
    DWORD page_max = 14;
    ctrl_click_patch.SetPatch(address, (const char*)&page_max, 1);
    ctrl_click_patch.TogglePatch(true);

    address = GW::Scanner::Find("\x5F\x6A\x00\xFF\x75\xE4\x6A\x4C\xFF\x75\xF8", "xxxxxxxxxxx", -0x44);
    printf("[SCAN] TomePatch = %p\n", (void *)address);
    if (address) {
        tome_patch.SetPatch(address, "\x75\x1E\x90\x90\x90\x90\x90", 7);
    }

    address = GW::Scanner::Find("\x81\xff\x86\x02\x00\x00", "xxxxxx", 6);
    printf("[SCAN] MapEntryMessagePatch = %p\n", (void *)address);
    if (address) {
        skip_map_entry_message_patch.SetPatch(address, "\x90\xe9", 2);
    }

    address = GW::Scanner::Find("\xF7\x40\x0C\x10\x00\x02\x00\x75", "xxxxxx??", +7);
    printf("[SCAN] GoldConfirmationPatch = %p\n", (void *)address);
    if (address) {
        gold_confirm_patch.SetPatch(address, "\x90\x90", 2);
    }
    // This could be done with patches if we wanted to still show description for weapon sets and merchants etc, but its more signatures to log.
    GetItemDescription_Func = (GetItemDescription_pt)GW::Scanner::Find("\x8b\xc3\x25\xfd\x00\x00\x00\x3c\xfd", "xxxxxxxxx", -0x5f);
    if (GetItemDescription_Func) {
        GW::HookBase::CreateHook(GetItemDescription_Func, OnGetItemDescription, (void**)&GetItemDescription_Ret);
        GW::HookBase::EnableHooks(GetItemDescription_Func);
    }

    // Call our CreateCodedTextLabel function instead of default CreateCodedTextLabel for patching skill descriptions
    address = GW::Scanner::FindAssertion("p:\\code\\gw\\ui\\game\\gmtipskill.cpp", "!(m_tipSkillFlags & TipSkillMsgCreate::FLAG_SHOW_ENABLE_AI_HINT)", 0x7b);
    CreateEncodedTextLabel_Func = (CreateCodedTextLabel_pt)GW::Scanner::FunctionFromNearCall(address);
    skill_description_patch.SetRedirect(address, CreateCodedTextLabel_SkillDescription);
    skill_description_patch.TogglePatch(true);

    // See OnAgentAllegianceChanged
    address = GW::Scanner::Find("\x75\x18\x81\xce\x00\x00\x00\x02\x56", "xxxxxxxxx", 0x9);
    SetGlobalNameTagVisibility_Func = (SetGlobalNameTagVisibility_pt)GW::Scanner::FunctionFromNearCall(address);
    if (SetGlobalNameTagVisibility_Func) {
        GlobalNameTagVisibilityFlags = *(uint32_t**)(((uintptr_t)SetGlobalNameTagVisibility_Func) + 0xb);
        GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::AgentUpdateAllegiance>(&PartyDefeated_Entry, &OnAgentAllegianceChanged);
    }
    printf("[SCAN] SetGlobalNameTagVisibility_Func = %p", (void*)SetGlobalNameTagVisibility_Func);
    printf("[SCAN] GlobalNameTagVisibilityFlags = %p", (void*)GlobalNameTagVisibilityFlags);

    address = GW::Scanner::Find("\x8b\x7d\x08\x8b\x70\x2c\x83\xff\x0f","xxxxxxxxx");
    ShowAgentFactionGain_Func = (ShowAgentFactionGain_pt)GW::Scanner::FunctionFromNearCall(address + 0x6c);
    ShowAgentExperienceGain_Func = (ShowAgentExperienceGain_pt)GW::Scanner::FunctionFromNearCall(address + 0x4f);
    printf("[SCAN] ShowAgentFactionGain_Func = %p\n", (void*)ShowAgentFactionGain_Func);
    printf("[SCAN] ShowAgentExperienceGain_Func = %p\n", (void*)ShowAgentExperienceGain_Func);

    GW::HookBase::CreateHook(ShowAgentFactionGain_Func, OnShowAgentFactionGain, (void**)&ShowAgentFactionGain_Ret);
    GW::HookBase::EnableHooks(ShowAgentFactionGain_Func);
    GW::HookBase::CreateHook(ShowAgentExperienceGain_Func, OnShowAgentExperienceGain, (void**)&ShowAgentExperienceGain_Ret);
    GW::HookBase::EnableHooks(ShowAgentExperienceGain_Func);

    // Stop GW from force closing the game when clicking on the exit button in window fullscreen; instead route it through the close signal.
    OnMinOrRestoreOrExitBtnClicked_Func = (GW::UI::UIInteractionCallback) GW::Scanner::Find("\x83\xc4\x0c\xa9\x00\x00\x80\x00", "xxxxxxxx", -0x54);
    if (OnMinOrRestoreOrExitBtnClicked_Func) {
        GW::HookBase::CreateHook(OnMinOrRestoreOrExitBtnClicked_Func, OnMinOrRestoreOrExitBtnClicked, (void**)&OnMinOrRestoreOrExitBtnClicked_Ret);
        GW::HookBase::EnableHooks();
    }

    GW::UI::RegisterUIMessageCallback(&OnDialog_Entry, GW::UI::UIMessage::kSendDialog, bind_member(this, &GameSettings::OnFactionDonate));
    GW::UI::RegisterUIMessageCallback(&OnDialog_Entry, GW::UI::UIMessage::kSendLoadSkillbar, &OnPreLoadSkillBar);
    GW::StoC::RegisterPacketCallback(&OnDialog_Entry, GAME_SMSG_SKILLBAR_UPDATE, OnPostLoadSkillBar, 0x8000);
    GW::StoC::RegisterPacketCallback(&OnDialog_Entry, GAME_SMSG_SKILL_UPDATE_SKILL_COUNT_1, bind_member(this, &GameSettings::OnUpdateSkillCount), -0x3000);
    GW::StoC::RegisterPacketCallback(&OnDialog_Entry, GAME_SMSG_SKILL_UPDATE_SKILL_COUNT_2, bind_member(this, &GameSettings::OnUpdateSkillCount), -0x3000);

    GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::PartyDefeated>(&PartyDefeated_Entry, bind_member(this, &GameSettings::OnPartyDefeated));
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
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::TradeStart>(&TradeStart_Entry, bind_member(this, &GameSettings::OnTradeStarted));
    GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::PartyInviteReceived_Create>(&PartyPlayerAdd_Entry, bind_member(this, &GameSettings::OnPartyInviteReceived));
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::PartyPlayerAdd>(&PartyPlayerAdd_Entry, bind_member(this, &GameSettings::OnPartyPlayerJoined));
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GameSrvTransfer>(&GameSrvTransfer_Entry, bind_member(this, &GameSettings::OnMapTravel));
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::CinematicPlay>(&CinematicPlay_Entry, bind_member(this, &GameSettings::OnCinematic));
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::VanquishComplete>(&VanquishComplete_Entry, bind_member(this, &GameSettings::OnVanquishComplete));
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::DungeonReward>(&VanquishComplete_Entry, bind_member(this, &GameSettings::OnDungeonReward));
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::MapLoaded>(&PlayerJoinInstance_Entry, bind_member(this, &GameSettings::OnMapLoaded));
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::PlayerJoinInstance>(&PlayerJoinInstance_Entry, bind_member(this, &GameSettings::OnPlayerJoinInstance));
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::PlayerLeaveInstance>(&PlayerLeaveInstance_Entry, bind_member(this, &GameSettings::OnPlayerLeaveInstance));
    //GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::AgentAdd>(&OnAfterAgentAdd_Entry, &OnAfterAgentAdd);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentAdd>(&PartyDefeated_Entry, bind_member(this, &GameSettings::OnAgentAdd));
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentState>(&PartyDefeated_Entry, bind_member(this, &GameSettings::OnUpdateAgentState));
    // Trigger for message on party change
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::PartyPlayerRemove>(
        &PartyPlayerRemove_Entry,
        [&](GW::HookStatus* status, GW::Packet::StoC::PartyPlayerRemove*) {
            UNREFERENCED_PARAMETER(status);
            check_message_on_party_change = true;
        });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::ScreenShake>(&OnScreenShake_Entry, bind_member(this, &GameSettings::OnScreenShake));

    GW::UI::RegisterUIMessageCallback(&OnChangeTarget_Entry, GW::UI::UIMessage::kChangeTarget, OnChangeTarget);
    GW::UI::RegisterUIMessageCallback(&OnWriteChat_Entry, GW::UI::UIMessage::kWriteToChatLog, bind_member(this, &GameSettings::OnWriteChat));
    GW::UI::RegisterUIMessageCallback(&OnAgentStartCast_Entry, GW::UI::UIMessage::kAgentStartCasting, bind_member(this, &GameSettings::OnAgentStartCast));
    GW::UI::RegisterUIMessageCallback(&OnOpenWikiUrl_Entry, GW::UI::UIMessage::kOpenWikiUrl, bind_member(this, &GameSettings::OnOpenWiki));
    GW::UI::RegisterUIMessageCallback(&OnAgentNameTag_Entry, GW::UI::UIMessage::kShowAgentNameTag, bind_member(this, &GameSettings::OnAgentNameTag));
    GW::UI::RegisterUIMessageCallback(&OnAgentNameTag_Entry, GW::UI::UIMessage::kSetAgentNameTagAttribs, bind_member(this, &GameSettings::OnAgentNameTag));

    GW::UI::RegisterKeydownCallback(&OnChangeTarget_Entry, OnKeyDownAction);
    GW::UI::RegisterKeyupCallback(&OnChangeTarget_Entry, OnKeyUpAction);

    GW::FriendListMgr::RegisterFriendStatusCallback(&FriendStatusCallback_Entry, FriendStatusCallback);
    GW::UI::RegisterUIMessageCallback(&OnPreSendDialog_Entry, GW::UI::UIMessage::kSendPingWeaponSet, bind_member(this, &GameSettings::OnPingWeaponSet));
    GW::SkillbarMgr::RegisterUseSkillCallback(&OnCast_Entry, bind_member(this, &GameSettings::OnCast));

    constexpr GW::UI::UIMessage dialog_ui_messages[] = {
        GW::UI::UIMessage::kSendDialog,
        GW::UI::UIMessage::kDialogBody,
        GW::UI::UIMessage::kDialogButton
    };
    for (const auto message_id : dialog_ui_messages) {
        GW::UI::RegisterUIMessageCallback(
            &OnPostSendDialog_Entry,
            message_id,
            std::bind(&GameSettings::OnDialogUIMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4),
            0x8000
        );
    }

    constexpr GW::UI::UIMessage party_target_ui_messages[] = {
        GW::UI::UIMessage::kTargetPlayerPartyMember,
        GW::UI::UIMessage::kTargetNPCPartyMember,
        GW::UI::UIMessage::kCalledTargetChange
    };
    for (const auto message_id : party_target_ui_messages) {
        GW::UI::RegisterUIMessageCallback(&OnPostSendDialog_Entry, message_id, OnPartyTargetChanged, 0x8000);
    }

    GW::Chat::CreateCommand(L"reinvite", bind_member(this, &GameSettings::CmdReinvite));

    GW::UI::RegisterCreateUIComponentCallback(&OnCreateUIComponent_Entry, OnCreateUIComponent);

    set_window_title_delay = TIMER_INIT();


    GW::UI::UIMessage ui_message_ids[] = {
        GW::UI::UIMessage::kQuestAdded,
        GW::UI::UIMessage::kSendSetActiveQuest
    };
    for (auto message_id : ui_message_ids) {
        GW::UI::RegisterUIMessageCallback(&OnQuestUIMessage_HookEntry, message_id, OnPostQuestUIMessage,0x8000);
    }
    player_requested_active_quest_id = GW::QuestMgr::GetActiveQuestId();

#ifdef APRIL_FOOLS
    AF::ApplyPatchesIfItsTime();
#endif
}
void GameSettings::OnDialogUIMessage(GW::HookStatus*, GW::UI::UIMessage message_id, void* wparam, void*) const
{
    switch (message_id) {
        case GW::UI::UIMessage::kDialogButton: {
            const auto info = static_cast<GW::UI::DialogButtonInfo*>(wparam);
            // 8101 7f88 010a 8101 730e 0001
            if (auto_open_locked_chest && wcscmp(info->message, L"\x8101\x7f88\x010a\x8101\x730e\x1") == 0) {
                // Auto use lockpick
                DialogModule::SendDialog(info->dialog_id);
            }
        } break;
    }
}


void GameSettings::MessageOnPartyChange() {
    if (!check_message_on_party_change || GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost)
        return; // Don't need to check, or not an outpost.
    GW::PartyInfo* current_party = GW::PartyMgr::GetPartyInfo();
    GW::AgentLiving* me = GW::Agents::GetPlayerAsAgentLiving();
    if (!me || !current_party || !current_party->players.valid())
        return; // Party not ready yet.
    bool is_leading = false;
    std::vector<std::wstring> current_party_names;
    GW::PlayerPartyMemberArray& current_party_players = current_party->players; // Copy the player array here to avoid ptr issues.
    for (size_t i = 0; i < current_party_players.size(); i++) {
        if (!current_party_players[i].login_number)
            continue;
        if (i == 0)
            is_leading = current_party_players[i].login_number == me->login_number;
        wchar_t* player_name = GW::Agents::GetPlayerNameByLoginNumber(current_party_players[i].login_number);
        if (!player_name)
            continue;
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
                GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GLOBAL, buffer);
            }
        }
    } else if (!was_leading && is_leading && current_party_names.size() == 1 && previous_party_names.size() > 2) {
        // We left a party of at least 2 other people
    } else if (current_party_names.size() < previous_party_names.size()) {
        // Someone left my party
        for (size_t i = 0; i < previous_party_names.size() && notify_when_party_member_leaves; i++) {
            bool found = false;
            for (size_t j = 0; j < current_party_names.size() && !found; j++) {
                found = previous_party_names[i] == current_party_names[j];
            }
            if (!found) {
                wchar_t buffer[128];
                swprintf(buffer, 128, L"<a=1>%ls</a> left the party.", previous_party_names[i].c_str());
                GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GLOBAL, buffer);
            }
        }
    }
    was_leading = is_leading;
    previous_party_names = current_party_names;
    check_message_on_party_change = false;
}
void GameSettings::LoadSettings(ToolboxIni* ini) {
    ToolboxModule::LoadSettings(ini);

    disable_camera_smoothing = ini->GetBoolValue(Name(), VAR_NAME(disable_camera_smoothing), disable_camera_smoothing);
    tick_is_toggle = ini->GetBoolValue(Name(), VAR_NAME(tick_is_toggle), tick_is_toggle);

    shorthand_item_ping = ini->GetBoolValue(Name(), VAR_NAME(shorthand_item_ping), shorthand_item_ping);
    move_item_on_ctrl_click = ini->GetBoolValue(Name(), VAR_NAME(move_item_on_ctrl_click), move_item_on_ctrl_click);
    move_item_to_current_storage_pane = ini->GetBoolValue(Name(), VAR_NAME(move_item_to_current_storage_pane), move_item_to_current_storage_pane);
    move_materials_to_current_storage_pane = ini->GetBoolValue(Name(), VAR_NAME(move_materials_to_current_storage_pane), move_materials_to_current_storage_pane);
    
    flash_window_on_party_invite =
        ini->GetBoolValue(Name(), VAR_NAME(flash_window_on_party_invite), flash_window_on_party_invite);
    flash_window_on_zoning = ini->GetBoolValue(Name(), VAR_NAME(flash_window_on_zoning), flash_window_on_zoning);
    flash_window_on_cinematic = ini->GetBoolValue(Name(), VAR_NAME(flash_window_on_cinematic), flash_window_on_cinematic);
    focus_window_on_launch = ini->GetBoolValue(Name(), VAR_NAME(focus_window_on_launch), focus_window_on_launch);
    focus_window_on_zoning = ini->GetBoolValue(Name(), VAR_NAME(focus_window_on_zoning), focus_window_on_zoning);
    flash_window_on_trade = ini->GetBoolValue(Name(), VAR_NAME(flash_window_on_trade), flash_window_on_trade);
    focus_window_on_trade = ini->GetBoolValue(Name(), VAR_NAME(focus_window_on_trade), focus_window_on_trade);
    flash_window_on_name_ping = ini->GetBoolValue(Name(), VAR_NAME(flash_window_on_name_ping), flash_window_on_name_ping);
    set_window_title_as_charname = ini->GetBoolValue(Name(), VAR_NAME(set_window_title_as_charname), set_window_title_as_charname);

    auto_set_away = ini->GetBoolValue(Name(), VAR_NAME(auto_set_away), auto_set_away);
    auto_set_away_delay = ini->GetLongValue(Name(), VAR_NAME(auto_set_away_delay), auto_set_away_delay);
    auto_set_online = ini->GetBoolValue(Name(), VAR_NAME(auto_set_online), auto_set_online);
    auto_return_on_defeat = ini->GetBoolValue(Name(), VAR_NAME(auto_return_on_defeat), auto_return_on_defeat);

    show_unlearned_skill = ini->GetBoolValue(Name(), VAR_NAME(show_unlearned_skill), show_unlearned_skill);
    auto_skip_cinematic = ini->GetBoolValue(Name(), VAR_NAME(auto_skip_cinematic), auto_skip_cinematic);

    faction_warn_percent = ini->GetBoolValue(Name(), VAR_NAME(faction_warn_percent), faction_warn_percent);
    faction_warn_percent_amount = ini->GetLongValue(Name(), VAR_NAME(faction_warn_percent_amount), faction_warn_percent_amount);
    stop_screen_shake = ini->GetBoolValue(Name(), VAR_NAME(stop_screen_shake), stop_screen_shake);

    disable_gold_selling_confirmation = ini->GetBoolValue(Name(), VAR_NAME(disable_gold_selling_confirmation), disable_gold_selling_confirmation);
    notify_when_friends_online = ini->GetBoolValue(Name(), VAR_NAME(notify_when_friends_online), notify_when_friends_online);
    notify_when_friends_offline = ini->GetBoolValue(Name(), VAR_NAME(notify_when_friends_offline), notify_when_friends_offline);
    notify_when_friends_join_outpost = ini->GetBoolValue(Name(), VAR_NAME(notify_when_friends_join_outpost), notify_when_friends_join_outpost);
    notify_when_friends_leave_outpost = ini->GetBoolValue(Name(), VAR_NAME(notify_when_friends_leave_outpost), notify_when_friends_leave_outpost);

    notify_when_party_member_leaves = ini->GetBoolValue(Name(), VAR_NAME(notify_when_party_member_leaves), notify_when_party_member_leaves);
    notify_when_party_member_joins = ini->GetBoolValue(Name(), VAR_NAME(notify_when_party_member_joins), notify_when_party_member_joins);
    notify_when_players_join_outpost = ini->GetBoolValue(Name(), VAR_NAME(notify_when_players_join_outpost), notify_when_players_join_outpost);
    notify_when_players_leave_outpost = ini->GetBoolValue(Name(), VAR_NAME(notify_when_players_leave_outpost), notify_when_players_leave_outpost);

    auto_age_on_vanquish = ini->GetBoolValue(Name(), VAR_NAME(auto_age_on_vanquish), auto_age_on_vanquish);
    hide_dungeon_chest_popup = ini->GetBoolValue(Name(), VAR_NAME(hide_dungeon_chest_popup), hide_dungeon_chest_popup);
    auto_age2_on_age = ini->GetBoolValue(Name(), VAR_NAME(auto_age2_on_age), auto_age2_on_age);
    auto_accept_invites = ini->GetBoolValue(Name(), VAR_NAME(auto_accept_invites), auto_accept_invites);
    auto_accept_join_requests = ini->GetBoolValue(Name(), VAR_NAME(auto_accept_join_requests), auto_accept_join_requests);

    skip_entering_name_for_faction_donate = ini->GetBoolValue(Name(), VAR_NAME(skip_entering_name_for_faction_donate), skip_entering_name_for_faction_donate);
    improve_move_to_cast = ini->GetBoolValue(Name(), VAR_NAME(improve_move_to_cast), improve_move_to_cast);
    drop_ua_on_cast = ini->GetBoolValue(Name(), VAR_NAME(drop_ua_on_cast), drop_ua_on_cast);

    lazy_chest_looting = ini->GetBoolValue(Name(), VAR_NAME(lazy_chest_looting), lazy_chest_looting);

    block_transmogrify_effect = ini->GetBoolValue(Name(), VAR_NAME(block_transmogrify_effect), block_transmogrify_effect);
    block_sugar_rush_effect = ini->GetBoolValue(Name(), VAR_NAME(block_sugar_rush_effect), block_sugar_rush_effect);
    block_snowman_summoner = ini->GetBoolValue(Name(), VAR_NAME(block_snowman_summoner), block_snowman_summoner);
    block_party_poppers = ini->GetBoolValue(Name(), VAR_NAME(block_party_poppers), block_party_poppers);
    block_bottle_rockets = ini->GetBoolValue(Name(), VAR_NAME(block_bottle_rockets), block_bottle_rockets);
    block_ghostinthebox_effect = ini->GetBoolValue(Name(), VAR_NAME(block_ghostinthebox_effect), block_ghostinthebox_effect);
    block_sparkly_drops_effect = ini->GetBoolValue(Name(), VAR_NAME(block_sparkly_drops_effect), block_sparkly_drops_effect);
    limit_signets_of_capture = ini->GetBoolValue(Name(), VAR_NAME(limit_signets_of_capture), limit_signets_of_capture);
    auto_open_locked_chest = ini->GetBoolValue(Name(), VAR_NAME(auto_open_locked_chest), auto_open_locked_chest);
    block_faction_gain = ini->GetBoolValue(Name(), VAR_NAME(block_faction_gain), block_faction_gain);
    block_experience_gain = ini->GetBoolValue(Name(), VAR_NAME(block_experience_gain), block_experience_gain);
    block_zero_experience_gain = ini->GetBoolValue(Name(), VAR_NAME(block_zero_experience_gain), block_zero_experience_gain);

    disable_item_descriptions_in_outpost = ini->GetBoolValue(Name(), VAR_NAME(disable_item_descriptions_in_outpost), disable_item_descriptions_in_outpost);
    disable_item_descriptions_in_explorable = ini->GetBoolValue(Name(), VAR_NAME(disable_item_descriptions_in_explorable), disable_item_descriptions_in_explorable);
    hide_email_address = ini->GetBoolValue(Name(), VAR_NAME(hide_email_address), hide_email_address);
    disable_skill_descriptions_in_outpost = ini->GetBoolValue(Name(), VAR_NAME(disable_skill_descriptions_in_outpost), disable_skill_descriptions_in_outpost);
    disable_skill_descriptions_in_explorable = ini->GetBoolValue(Name(), VAR_NAME(disable_skill_descriptions_in_explorable), disable_skill_descriptions_in_explorable);

    ::LoadChannelColor(ini, Name(), "local", GW::Chat::Channel::CHANNEL_ALL);
    ::LoadChannelColor(ini, Name(), "guild", GW::Chat::Channel::CHANNEL_GUILD);
    ::LoadChannelColor(ini, Name(), "team", GW::Chat::Channel::CHANNEL_GROUP);
    ::LoadChannelColor(ini, Name(), "trade", GW::Chat::Channel::CHANNEL_TRADE);
    ::LoadChannelColor(ini, Name(), "alliance", GW::Chat::Channel::CHANNEL_ALLIANCE);
    ::LoadChannelColor(ini, Name(), "whispers", GW::Chat::Channel::CHANNEL_WHISPER);
    ::LoadChannelColor(ini, Name(), "emotes", GW::Chat::Channel::CHANNEL_EMOTE);
    ::LoadChannelColor(ini, Name(), "other", GW::Chat::Channel::CHANNEL_GLOBAL);

    nametag_color_enemy = Colors::Load(ini, Name(), VAR_NAME(nametag_color_enemy), nametag_color_enemy);
    nametag_color_gadget = Colors::Load(ini, Name(), VAR_NAME(nametag_color_gadget), nametag_color_gadget);
    nametag_color_item = Colors::Load(ini, Name(), VAR_NAME(nametag_color_item), nametag_color_item);
    nametag_color_npc = Colors::Load(ini, Name(), VAR_NAME(nametag_color_npc), nametag_color_npc);
    nametag_color_player_in_party = Colors::Load(ini, Name(), VAR_NAME(nametag_color_player_in_party), nametag_color_player_in_party);
    nametag_color_player_other = Colors::Load(ini, Name(), VAR_NAME(nametag_color_player_other), nametag_color_player_other);
    nametag_color_player_self = Colors::Load(ini, Name(), VAR_NAME(nametag_color_player_self), nametag_color_player_self);

    block_enter_area_message = ini->GetBoolValue(Name(), VAR_NAME(block_enter_area_message), block_enter_area_message);

    LOAD_BOOL(keep_current_quest_when_new_quest_added);

    LOAD_BOOL(automatically_flag_pet_to_fight_called_target);

    GW::PartyMgr::SetTickToggle(tick_is_toggle);
    SetWindowTitle(set_window_title_as_charname);

    tome_patch.TogglePatch(show_unlearned_skill);
    gold_confirm_patch.TogglePatch(disable_gold_selling_confirmation);
    skip_map_entry_message_patch.TogglePatch(block_enter_area_message);

    if (focus_window_on_launch) {
        FocusWindow();
    }
}

void GameSettings::RegisterSettingsContent() {
    ToolboxModule::RegisterSettingsContent();
    ToolboxModule::RegisterSettingsContent("Inventory Settings", ICON_FA_BOXES,
        [this](const std::string&, bool is_showing) {
            if (!is_showing) return;
            DrawInventorySettings();
        }, 0.9f);

    ToolboxModule::RegisterSettingsContent("Party Settings", ICON_FA_USERS,
        [this](const std::string&, bool is_showing) {
            if (!is_showing) return;
            DrawPartySettings();
        }, 0.9f);

        ToolboxModule::RegisterSettingsContent(
        "Notifications", ICON_FA_BULLHORN,
        [this](const std::string&, bool is_showing) {
            if (is_showing)
                DrawNotificationsSettings();
        },
        0.9f);
}

void GameSettings::Terminate() {
    ToolboxModule::Terminate();
    ctrl_click_patch.Reset();
    tome_patch.Reset();
    gold_confirm_patch.Reset();
    skill_description_patch.Reset();
    skip_map_entry_message_patch.Reset();

    GW::UI::RemoveUIMessageCallback(&OnQuestUIMessage_HookEntry);
}

void GameSettings::SaveSettings(ToolboxIni* ini) {
    ToolboxModule::SaveSettings(ini);

    ini->SetBoolValue(Name(), VAR_NAME(tick_is_toggle), tick_is_toggle);

    ini->SetBoolValue(Name(), VAR_NAME(disable_camera_smoothing), disable_camera_smoothing);
    ini->SetBoolValue(Name(), VAR_NAME(auto_return_on_defeat), auto_return_on_defeat);
    ini->SetBoolValue(Name(), VAR_NAME(shorthand_item_ping), shorthand_item_ping);

    ini->SetBoolValue(Name(), VAR_NAME(move_item_on_ctrl_click), move_item_on_ctrl_click);
    ini->SetBoolValue(Name(), VAR_NAME(move_item_to_current_storage_pane), move_item_to_current_storage_pane);
    ini->SetBoolValue(Name(), VAR_NAME(move_materials_to_current_storage_pane), move_materials_to_current_storage_pane);
    ini->SetBoolValue(Name(), VAR_NAME(stop_screen_shake), stop_screen_shake);

    ini->SetBoolValue(Name(), VAR_NAME(flash_window_on_party_invite), flash_window_on_party_invite);
    ini->SetBoolValue(Name(), VAR_NAME(flash_window_on_zoning), flash_window_on_zoning);
    ini->SetBoolValue(Name(), VAR_NAME(focus_window_on_launch), focus_window_on_launch);
    ini->SetBoolValue(Name(), VAR_NAME(focus_window_on_zoning), focus_window_on_zoning);
    ini->SetBoolValue(Name(), VAR_NAME(flash_window_on_cinematic), flash_window_on_cinematic);
    ini->SetBoolValue(Name(), VAR_NAME(flash_window_on_trade), flash_window_on_trade);
    ini->SetBoolValue(Name(), VAR_NAME(focus_window_on_trade), focus_window_on_trade);
    ini->SetBoolValue(Name(), VAR_NAME(flash_window_on_name_ping), flash_window_on_name_ping);
    ini->SetBoolValue(Name(), VAR_NAME(set_window_title_as_charname), set_window_title_as_charname);

    ini->SetBoolValue(Name(), VAR_NAME(auto_set_away), auto_set_away);
    ini->SetLongValue(Name(), VAR_NAME(auto_set_away_delay), auto_set_away_delay);
    ini->SetBoolValue(Name(), VAR_NAME(auto_set_online), auto_set_online);

    ini->SetBoolValue(Name(), VAR_NAME(show_unlearned_skill), show_unlearned_skill);
    ini->SetBoolValue(Name(), VAR_NAME(auto_skip_cinematic), auto_skip_cinematic);


    ini->SetBoolValue(Name(), VAR_NAME(faction_warn_percent), faction_warn_percent);
    ini->SetLongValue(Name(), VAR_NAME(faction_warn_percent_amount), faction_warn_percent_amount);
    ini->SetBoolValue(Name(), VAR_NAME(disable_gold_selling_confirmation), disable_gold_selling_confirmation);

    ini->SetBoolValue(Name(), VAR_NAME(notify_when_friends_online), notify_when_friends_online);
    ini->SetBoolValue(Name(), VAR_NAME(notify_when_friends_offline), notify_when_friends_offline);
    ini->SetBoolValue(Name(), VAR_NAME(notify_when_friends_join_outpost), notify_when_friends_join_outpost);
    ini->SetBoolValue(Name(), VAR_NAME(notify_when_friends_leave_outpost), notify_when_friends_leave_outpost);

    ini->SetBoolValue(Name(), VAR_NAME(notify_when_party_member_leaves), notify_when_party_member_leaves);
    ini->SetBoolValue(Name(), VAR_NAME(notify_when_party_member_joins), notify_when_party_member_joins);
    ini->SetBoolValue(Name(), VAR_NAME(notify_when_players_join_outpost), notify_when_players_join_outpost);
    ini->SetBoolValue(Name(), VAR_NAME(notify_when_players_leave_outpost), notify_when_players_leave_outpost);

    ini->SetBoolValue(Name(), VAR_NAME(auto_age_on_vanquish), auto_age_on_vanquish);
    ini->SetBoolValue(Name(), VAR_NAME(hide_dungeon_chest_popup), hide_dungeon_chest_popup);
    ini->SetBoolValue(Name(), VAR_NAME(auto_age2_on_age), auto_age2_on_age);
    ini->SetBoolValue(Name(), VAR_NAME(auto_accept_invites), auto_accept_invites);
    ini->SetBoolValue(Name(), VAR_NAME(auto_accept_join_requests), auto_accept_join_requests);
    ini->SetBoolValue(Name(), VAR_NAME(skip_entering_name_for_faction_donate), skip_entering_name_for_faction_donate);
    ini->SetBoolValue(Name(), VAR_NAME(improve_move_to_cast), improve_move_to_cast);
    ini->SetBoolValue(Name(), VAR_NAME(drop_ua_on_cast), drop_ua_on_cast);

    ini->SetBoolValue(Name(), VAR_NAME(lazy_chest_looting), lazy_chest_looting);

    ini->SetBoolValue(Name(), VAR_NAME(block_transmogrify_effect), block_transmogrify_effect);
    ini->SetBoolValue(Name(), VAR_NAME(block_sugar_rush_effect), block_sugar_rush_effect);
    ini->SetBoolValue(Name(), VAR_NAME(block_snowman_summoner), block_snowman_summoner);
    ini->SetBoolValue(Name(), VAR_NAME(block_party_poppers), block_party_poppers);
    ini->SetBoolValue(Name(), VAR_NAME(block_bottle_rockets), block_bottle_rockets);
    ini->SetBoolValue(Name(), VAR_NAME(block_ghostinthebox_effect), block_ghostinthebox_effect);
    ini->SetBoolValue(Name(), VAR_NAME(block_sparkly_drops_effect), block_sparkly_drops_effect);
    ini->SetBoolValue(Name(), VAR_NAME(limit_signets_of_capture), limit_signets_of_capture);
    ini->SetBoolValue(Name(), VAR_NAME(auto_open_locked_chest), auto_open_locked_chest);

    ini->SetBoolValue(Name(), VAR_NAME(block_faction_gain), block_faction_gain);
    ini->SetBoolValue(Name(), VAR_NAME(block_experience_gain), block_experience_gain);
    ini->SetBoolValue(Name(), VAR_NAME(block_zero_experience_gain), block_zero_experience_gain);
    ini->SetBoolValue(Name(), VAR_NAME(disable_item_descriptions_in_outpost), disable_item_descriptions_in_outpost);
    ini->SetBoolValue(Name(), VAR_NAME(disable_item_descriptions_in_explorable), disable_item_descriptions_in_explorable);

    ini->SetBoolValue(Name(), VAR_NAME(hide_email_address), hide_email_address);

    ini->SetBoolValue(Name(), VAR_NAME(disable_skill_descriptions_in_outpost), disable_skill_descriptions_in_outpost);
    ini->SetBoolValue(Name(), VAR_NAME(disable_skill_descriptions_in_explorable), disable_skill_descriptions_in_explorable);

    ini->SetBoolValue(Name(), VAR_NAME(block_enter_area_message), block_enter_area_message);

    ::SaveChannelColor(ini, Name(), "local", GW::Chat::Channel::CHANNEL_ALL);
    ::SaveChannelColor(ini, Name(), "guild", GW::Chat::Channel::CHANNEL_GUILD);
    ::SaveChannelColor(ini, Name(), "team", GW::Chat::Channel::CHANNEL_GROUP);
    ::SaveChannelColor(ini, Name(), "trade", GW::Chat::Channel::CHANNEL_TRADE);
    ::SaveChannelColor(ini, Name(), "alliance", GW::Chat::Channel::CHANNEL_ALLIANCE);
    ::SaveChannelColor(ini, Name(), "whispers", GW::Chat::Channel::CHANNEL_WHISPER);
    ::SaveChannelColor(ini, Name(), "emotes", GW::Chat::Channel::CHANNEL_EMOTE);
    ::SaveChannelColor(ini, Name(), "other", GW::Chat::Channel::CHANNEL_GLOBAL);

    Colors::Save(ini, Name(), VAR_NAME(nametag_color_enemy), nametag_color_enemy);
    Colors::Save(ini, Name(), VAR_NAME(nametag_color_gadget), nametag_color_gadget);
    Colors::Save(ini, Name(), VAR_NAME(nametag_color_item), nametag_color_item);
    Colors::Save(ini, Name(), VAR_NAME(nametag_color_npc), nametag_color_npc);
    Colors::Save(ini, Name(), VAR_NAME(nametag_color_player_in_party), nametag_color_player_in_party);
    Colors::Save(ini, Name(), VAR_NAME(nametag_color_player_other), nametag_color_player_other);
    Colors::Save(ini, Name(), VAR_NAME(nametag_color_player_self), nametag_color_player_self);

    SAVE_BOOL(keep_current_quest_when_new_quest_added);

    SAVE_BOOL(automatically_flag_pet_to_fight_called_target);


}

void GameSettings::DrawInventorySettings() {
    ImGui::Checkbox("Move items from/to storage with Control+Click", &move_item_on_ctrl_click);
    ImGui::Indent();
    ImGui::Checkbox("Move items to current open storage pane on click", &move_item_to_current_storage_pane);
    ImGui::ShowHelp("Materials follow different logic, see below");
    ImGui::Indent();
    const char* logic = "Storage logic: Any available stack/slot";
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

void GameSettings::DrawPartySettings() {
    if(ImGui::Checkbox("Tick is a toggle", &tick_is_toggle))
        GW::PartyMgr::SetTickToggle(tick_is_toggle);
    ImGui::ShowHelp("Ticking in party window will work as a toggle instead of opening the menu");
    ImGui::Checkbox("Automatically accept party invitations when ticked", &auto_accept_invites);
    ImGui::ShowHelp("When you're invited to join someone elses party");
    ImGui::Checkbox("Automatically accept party join requests when ticked", &auto_accept_join_requests);
    ImGui::ShowHelp("When a player wants to join your existing party");
    ImGui::Checkbox("Automatically flag your pet to fight when calling a target", &automatically_flag_pet_to_fight_called_target);
}

void GameSettings::DrawSettingInternal() {
    
    ImGui::Checkbox("Hide email address on login screen", &hide_email_address);
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

    if (ImGui::Checkbox("Only show non learned skills when using a tome", &show_unlearned_skill)) {
        tome_patch.TogglePatch(show_unlearned_skill);
    }

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
    ImGui::Checkbox("Disable camera smoothing", &disable_camera_smoothing);
    ImGui::Checkbox("Improve move to cast spell range", &improve_move_to_cast);
    ImGui::ShowHelp("This should make you stop to cast skills earlier by re-triggering the skill cast when in range.");
    ImGui::Checkbox("Auto-cancel Unyielding Aura when re-casting",&drop_ua_on_cast);
    ImGui::Checkbox("Auto use lockpick when interacting with locked chest", &auto_open_locked_chest);
    ImGui::Checkbox("Keep current quest when accepting a new one", &keep_current_quest_when_new_quest_added);

    ImGui::Text("Block floating numbers above character when:");
    ImGui::Indent();
    ImGui::StartSpacedElements(checkbox_w);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Gaining faction", &block_faction_gain);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Gaining experience", &block_experience_gain);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Gaining 0 experience", &block_zero_experience_gain);
    ImGui::Unindent();
    ImGui::Text("Disable animation and sound from consumables:");
    ImGui::Indent();
    ImGui::StartSpacedElements(checkbox_w);
    constexpr const char* doesnt_affect_me = "Only applies to other players";
    ImGui::NextSpacedElement(); ImGui::Checkbox("Tonics", &block_transmogrify_effect);
    ImGui::ShowHelp(doesnt_affect_me);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Sweets", &block_sugar_rush_effect);
    ImGui::ShowHelp(doesnt_affect_me);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Bottle rockets", &block_bottle_rockets);
    ImGui::ShowHelp(doesnt_affect_me);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Party poppers", &block_party_poppers);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Snowman Summoners", &block_snowman_summoner);
    ImGui::ShowHelp(doesnt_affect_me);
#if 0
    //@Cleanup: Ghost in the box spawn effect suppressed, but still need to figure out how to suppress the death effect.
    ImGui::SameLine(column_spacing); ImGui::Checkbox("Ghost-in-the-box", &block_ghostinthebox_effect);
    ImGui::ShowHelp("Also applies to ghost-in-the-boxes that you use");
#endif
    ImGui::Unindent();
    ImGui::Checkbox("Block sparkle effect on dropped items", &block_sparkly_drops_effect);
    ImGui::ShowHelp("Applies to drops that appear after this setting has been changed");
    ImGui::Checkbox("Limit signet of capture to 10 in skills window", &limit_signets_of_capture);
    ImGui::ShowHelp("If your character has purchased more than 10 signets of capture, only show 10 of them in the skills window");
    ImGui::Text("In-game name tag colors:");
    ImGui::Indent();
    ImGui::StartSpacedElements(checkbox_w);
    uint32_t flags = ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_NoInputs;
    ImGui::NextSpacedElement(); Colors::DrawSettingHueWheel("Myself", &nametag_color_player_self, flags);
    ImGui::NextSpacedElement(); Colors::DrawSettingHueWheel("NPC", &nametag_color_npc, flags);
    ImGui::NextSpacedElement(); Colors::DrawSettingHueWheel("Enemy", &nametag_color_enemy, flags);
    ImGui::NextSpacedElement(); Colors::DrawSettingHueWheel("Gadget", &nametag_color_gadget, flags);
    ImGui::NextSpacedElement(); Colors::DrawSettingHueWheel("Other Player", &nametag_color_player_other, flags);
    ImGui::NextSpacedElement(); Colors::DrawSettingHueWheel("Other Player (In Party)", &nametag_color_player_in_party, flags);
    ImGui::NextSpacedElement(); Colors::DrawSettingHueWheel("Item", &nametag_color_item, flags);
    ImGui::Unindent();

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
    if (ImGui::Checkbox("Block full screen message when entering a new area", &block_enter_area_message)) {
        skip_map_entry_message_patch.TogglePatch(block_enter_area_message);
    }
}

void GameSettings::FactionEarnedCheckAndWarn() {
    if (!faction_warn_percent)
        return; // Disabled
    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost) {
        faction_checked = false;
        return; // Loading or explorable area.
    }
    if (faction_checked)
        return; // Already checked.
    faction_checked = true;
    const auto world_context = GW::GetWorldContext();
    if (!world_context || !world_context->max_luxon || !world_context->total_earned_kurzick) {
        faction_checked = false;
        return; // No world context yet.
    }
    float percent;
    // Avoid invalid user input values.
    if (faction_warn_percent_amount < 0)
        faction_warn_percent_amount = 0;
    if (faction_warn_percent_amount > 100)
        faction_warn_percent_amount = 100;
    // Warn user to dump faction if we're in a luxon/kurzick mission outpost
    switch (GW::Map::GetMapID()) {
        case GW::Constants::MapID::The_Deep:
        case GW::Constants::MapID::The_Jade_Quarry_Luxon_outpost:
        case GW::Constants::MapID::Fort_Aspenwood_Luxon_outpost:
        case GW::Constants::MapID::Zos_Shivros_Channel:
        case GW::Constants::MapID::The_Aurios_Mines:
            // Player is in luxon mission outpost
            percent = 100.0f * (float)world_context->current_luxon / (float)world_context->max_luxon;
            if (percent >= (float)faction_warn_percent_amount) {
                // Faction earned is over 75% capacity
                Log::Warning("Luxon faction earned is %d of %d", world_context->current_luxon, world_context->max_luxon );
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
            percent = 100.0f * (float)world_context->current_kurzick / (float)world_context->max_kurzick;
            if (percent >= (float)faction_warn_percent_amount) {
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

void GameSettings::Update(float) {
    UpdateSkillTooltip();
    UpdateReinvite();
    UpdateItemTooltip();
    if (set_window_title_delay && TIMER_DIFF(set_window_title_delay) > 3000) {
        SetWindowTitle(set_window_title_as_charname);
        set_window_title_delay = 0;
    }

    // See OnSendChat
    if (pending_wiki_search_term && pending_wiki_search_term->wstring().length()) {
        GuiUtils::SearchWiki(pending_wiki_search_term->wstring());
        delete pending_wiki_search_term;
        pending_wiki_search_term = 0;
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
        cam->position = cam->camera_pos_to_go;
        cam->look_at_target = cam->look_at_to_go;
        cam->yaw = cam->yaw_to_go;
        cam->pitch = cam->pitch_to_go;
    }

    if (improve_move_to_cast && pending_cast.target_id) {
        const GW::AgentLiving *me = GW::Agents::GetCharacter();
        const GW::Skillbar *skillbar = GW::SkillbarMgr::GetPlayerSkillbar();
        if (!me || !skillbar) // I don't exist e.g. map change
            return pending_cast.reset();

        const GW::AgentLiving* target = pending_cast.GetTarget();
        if (!target)  // Target no longer valid
            return pending_cast.reset();

        const uint32_t& casting = skillbar->casting;
        if (pending_cast.cast_next_frame) { // Do cast now
            if(pending_cast.GetTarget())
                GW::SkillbarMgr::UseSkill(pending_cast.slot, pending_cast.target_id, pending_cast.call_target);
            pending_cast.reset();
            return;
        }

        if (casting && me->GetIsMoving() && !me->skill && !me->GetIsCasting()) { // casting/skill don't update fast enough, so delay the rupt
            const auto cast_skill = pending_cast.GetSkill();
            if (cast_skill == GW::Constants::SkillID::No_Skill) // Skill ID no longer valid
                return pending_cast.reset();

            const float range = GetSkillRange(cast_skill);
            if (GW::GetDistance(target->pos, me->pos) <= range && range > 0) {
                GW::UI::Keypress(GW::UI::ControlAction::ControlAction_MoveBackward);
                pending_cast.cast_next_frame = true;
                return;
            }
        }
        if (!casting) // abort the action if not auto walking anymore
            return pending_cast.reset();
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
        if (check_message_on_party_change)
            MessageOnPartyChange();
    }

}

bool GameSettings::WndProc(UINT Message, WPARAM, LPARAM)
{
    // I don't know what would be the best solution here, but the way we capture every messages as a sign of activity can be bad.
    // Added that because when someone was typing "/afk message" he was put online directly, because "enter-up" was captured.
    if (Message == WM_KEYUP)
        return false;

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
void GameSettings::OnPingWeaponSet(GW::HookStatus* status, GW::UI::UIMessage message_id, void* wparam, void*) const
{
    ASSERT(message_id == GW::UI::UIMessage::kSendPingWeaponSet && wparam);
    if (!shorthand_item_ping)
        return;
    struct Packet {
        uint32_t agent_id;
        uint32_t weapon_item_id;
        uint32_t offhand_item_id;
    } * pack = (Packet*)wparam;
    PingItem(pack->weapon_item_id, PING_PARTS::NAME | PING_PARTS::DESC);
    PingItem(pack->offhand_item_id, PING_PARTS::NAME | PING_PARTS::DESC);
    status->blocked = true;
}

// Show a message when player joins the outpost
void GameSettings::OnPlayerJoinInstance(GW::HookStatus*, GW::Packet::StoC::PlayerJoinInstance* pak) const
{
    if (!notify_when_players_join_outpost && !notify_when_friends_join_outpost)
        return; // Dont notify about player joining
    if (!(pak->player_name && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost))
        return; // Only message in an outpost.
    if (TIMER_DIFF(instance_entered_at) < 2000 && GW::Agents::GetPlayerId())
        return; // Only been in this map for less than 2 seconds or current player not loaded in yet; avoids spam on map load.
    if (GW::Agents::GetAgentByID(pak->agent_id))
        return; // Player already joined
    if (notify_when_friends_join_outpost) {
        if (const auto f = GetFriend(nullptr, pak->player_name, GW::FriendType::Friend, GW::FriendStatus::Online)) {
            GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GLOBAL, std::format(L"<a=1>{}</a> ({}) entered the outpost.", f->charname, f->alias).c_str());
            return;
        }
    }
    if (notify_when_players_join_outpost) {
        GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GLOBAL, std::format(L"<a=1>{}</a> entered the outpost.", pak->player_name).c_str());
    }
}

// Auto accept invitations, flash window on received party invite
void GameSettings::OnPartyInviteReceived(GW::HookStatus* status, GW::Packet::StoC::PartyInviteReceived_Create* packet) const
{
    UNREFERENCED_PARAMETER(status);
    if (status->blocked)
        return;
    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost || !GW::PartyMgr::GetIsLeader())
        return;

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
    if (flash_window_on_party_invite)
        FlashWindow();
}

// Flash window on player added
void GameSettings::OnPartyPlayerJoined(GW::HookStatus* status, GW::Packet::StoC::PartyPlayerAdd* packet) {
    UNREFERENCED_PARAMETER(status);
    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost)
        return;
    check_message_on_party_change = true;
    if (flash_window_on_party_invite) {
        GW::PartyInfo* current_party = GW::PartyMgr::GetPartyInfo();
        if (!current_party) return;
        GW::AgentLiving* me = GW::Agents::GetPlayerAsAgentLiving();
        if (!me) return;
        if (packet->player_id == me->login_number
            || (packet->party_id == current_party->party_id && GW::PartyMgr::GetIsLeader())) {
            FlashWindow();
        }
    }
}

// Block overhead arrow marker for zaishen scout
void GameSettings::OnAgentMarker(GW::HookStatus*, GW::Packet::StoC::GenericValue* pak) const
{
    const GW::Agent* a = GW::Agents::GetAgentByID(pak->agent_id);
    if (a && wcscmp(GW::Agents::GetAgentEncName(a),L"\x8102\x6ED9\xD94E\xBF68\x4409") == 0) {
        pak->value_id = 12;
    }
}

// Block annoying tonic sounds/effects from other players
void GameSettings::OnAgentEffect(GW::HookStatus* status, GW::Packet::StoC::GenericValue* pak) const
{
    if (pak->agent_id != GW::Agents::GetPlayerId()) {
        switch (pak->value) {
        case 905:
            status->blocked = block_snowman_summoner;
            break;
        case 1688:
            status->blocked = block_bottle_rockets;
            break;
        case 1689:
            status->blocked = block_party_poppers;
            break;
        case 758: // Chocolate bunny
        case 2063: // e.g. Fruitcake, sugary blue drink
        case 1176: // e.g. Delicious cake
            status->blocked = block_sugar_rush_effect;
            break;
        case 1491:
            status->blocked = block_transmogrify_effect;
            break;
        default:
            break;
        }
    }

}

// Block Ghost in the box spawn animation & sound
// Block sparkly item animation
void GameSettings::OnAgentAdd(GW::HookStatus*, GW::Packet::StoC::AgentAdd* packet) const
{
    if (block_sparkly_drops_effect && packet->type == 4 && packet->agent_type < 0xFFFFFF) {
        GW::Item* item = GW::Items::GetItemById(packet->agent_type);
        if (item) item->interaction |= 0x2000;
    }
    if (block_ghostinthebox_effect && false
        && (packet->agent_type & 0x20000000) != 0
        && (packet->agent_type ^ 0x20000000) == GW::Constants::ModelID::Boo) {
        // Boo spawning; reset initial state to 0 from 4096 - this stops the Boo from "animating" in and making the sound
        struct InitialEffectPacket : GW::Packet::StoC::PacketBase {
            uint32_t agent_id = 0;
            uint32_t state = 0;
        } packet2;
        packet2.header = GAME_SMSG_AGENT_INITIAL_EFFECTS;
        packet2.agent_id = packet->agent_id;
        GW::StoC::EmulatePacket(&packet2);
    }
}

// Block ghost in the box death animation & sound
void GameSettings::OnUpdateAgentState(GW::HookStatus* status, GW::Packet::StoC::AgentState* packet ) const
{
    // @Cleanup: Not found an elegent way to do this; prematurely destroying the agent will crash the client when the id it recycled. Disable for now, here for reference.
    if (packet->state == 0x10 && false) {
        GW::AgentLiving* agent = static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(packet->agent_id));
        if (agent && agent->GetIsLivingType() && agent->player_number == GW::Constants::ModelID::Boo) {
            // Boo spawning; reset initial state to 0 from 4096 - this stops the Boo from "animating" in and making the sound
            struct InitialEffectPacket : GW::Packet::StoC::PacketBase {
                uint32_t agent_id = 0;
                uint32_t state = 0x1000;
            } packet2;
            packet2.header = GAME_SMSG_AGENT_INITIAL_EFFECTS;
            packet2.agent_id = packet->agent_id;
            GW::StoC::EmulatePacket(&packet2);
            /*agent->animation_code = 0x32fc5eaf;
            agent->animation_id = 0x2;
            agent->animation_speed = 1.5f;
            agent->animation_type = 0x0;
            agent->type_map = 0x8;
            agent->model_state = 0x400;
            agent->effects = 0x10;*/
            status->blocked = true;
        }
    }
}

// Apply Collector's Edition animations on player dancing,
void GameSettings::OnAgentLoopingAnimation(GW::HookStatus*, GW::Packet::StoC::GenericValue* pak) const
{
    if (!(pak->agent_id == GW::Agents::GetPlayerId() && collectors_edition_emotes))
        return;
    static GW::Packet::StoC::GenericValue pak2;
    pak2.agent_id = pak->agent_id;
    pak2.value_id = 23;
    pak2.value = pak->value; // Glowing hands, any profession
    if (pak->value == 0x43394f1d) { // 0x31939cbb = /dance, 0x43394f1d = /dancenew
        switch (static_cast<GW::Constants::Profession>(GW::Agents::GetPlayerAsAgentLiving()->primary)) {
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
    uint32_t dialog_id = (uint32_t)wparam;
    if (!(dialog_id == 0x87 && skip_entering_name_for_faction_donate))
        return;
    uint32_t allegiance = 2;
    const wchar_t* raising_luxon_faction_cap = L"\x8102\x4A32\xAF32\xBDB5\x21AE";
    const wchar_t* raising_kurzick_faction_cap = L"\x8102\x4A26\x814C\x89CC\x5B0";
    // Look for "Raising Luxon/Kurzick faction cap" dialog option to check allegiance
    for (const auto dialog : DialogModule::Instance().GetDialogButtons()) {
        if (wcscmp(dialog->message, raising_luxon_faction_cap) == 0)
            allegiance = 1; // Luxon
        if (wcscmp(dialog->message, raising_kurzick_faction_cap) == 0)
            allegiance = 0; // Kurzick
    }
    uint32_t* current_faction = nullptr;
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
    if (!c || !c->player_guild_index || c->guilds[c->player_guild_index]->faction != allegiance)
        return; // Alliance isn't the right faction. Return here and the NPC will reply.
    if (*current_faction < 5000)
        return; // Not enough to donate. Return here and the NPC will reply.
    status->blocked = true;
    GW::PlayerMgr::DepositFaction(allegiance);
    GW::Agents::GoNPC(GW::Agents::GetAgentByID(DialogModule::GetDialogAgent()));
}

// Show a message when player leaves the outpost
void GameSettings::OnPlayerLeaveInstance(GW::HookStatus*, GW::Packet::StoC::PlayerLeaveInstance* pak) const
{
    if (!notify_when_players_leave_outpost && !notify_when_friends_leave_outpost)
        return; // Dont notify about player leaving
    if (!(pak->player_number && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost))
        return; // Only message in an outpost.
    wchar_t* player_name = GW::PlayerMgr::GetPlayerName(pak->player_number);
    if (!player_name)
        return; // Failed to get name
    if (notify_when_friends_leave_outpost) {
        if (const auto f = GetFriend(nullptr, player_name, GW::FriendType::Friend, GW::FriendStatus::Online)) {
            GW::Chat::WriteChatF(GW::Chat::Channel::CHANNEL_GLOBAL, L"<a=1>%ls</a> (%ls) left the outpost.", f->charname, f->alias);
            return;
        }
    }
    if (notify_when_players_leave_outpost) {
        GW::Chat::WriteChatF(GW::Chat::Channel::CHANNEL_GLOBAL, L"<a=1>%ls</a> left the outpost.", player_name);
    }
}

// Automatically return to outpost on defeat
void GameSettings::OnPartyDefeated(GW::HookStatus* status, GW::Packet::StoC::PartyDefeated*) const
{
    UNREFERENCED_PARAMETER(status);
    if (!auto_return_on_defeat || !GW::PartyMgr::GetIsLeader())
        return;
    GW::PartyMgr::ReturnToOutpost();
}

// Automatically send /age2 on /age.
void GameSettings::OnServerMessage(GW::HookStatus* status, GW::Packet::StoC::MessageServer* pak) const
{
    UNREFERENCED_PARAMETER(status);
    if (!auto_age2_on_age || static_cast<GW::Chat::Channel>(pak->channel) != GW::Chat::Channel::CHANNEL_GLOBAL) return; // Disabled or message pending
    const wchar_t* msg = GetMessageCore();
    // 0x8101 0x641F 0x86C3 0xE149 0x53E8 0x101 0x107 = You have been in this map for n minutes.
    // 0x8101 0x641E 0xE7AD 0xEF64 0x1676 0x101 0x107 0x102 0x107 = You have been in this map for n hours and n minutes.
    if (wmemcmp(msg, L"\x8101\x641F\x86C3\xE149\x53E8", 5) == 0 || wmemcmp(msg, L"\x8101\x641E\xE7AD\xEF64\x1676", 5) == 0) {
        GW::Chat::SendChat('/', "age2");
    }
}


// Allow clickable name when a player pings "I'm following X" or "I'm targeting X"
void GameSettings::OnLocalChatMessage(GW::HookStatus* status, GW::Packet::StoC::MessageLocal* pak) const
{
    if (status->blocked) return;                                                                                // Sender blocked, packet handled.
    if (pak->channel != static_cast<uint32_t>(GW::Chat::Channel::CHANNEL_GROUP) || !pak->player_number) return; // Not team chat or no sender
    const auto core = GetMessageCore();
    if (core[0] != 0x778 && core[0] != 0x781) return; // Not "I'm Following X" or "I'm Targeting X" message.
    std::wstring message(core);
    size_t start_idx = message.find(L"\xba9\x107");
    if (start_idx == std::wstring::npos) return; // Not a player name.
    start_idx += 2;
    const size_t end_idx = message.find(L'\x1', start_idx);
    if (end_idx == std::wstring::npos) return; // Not a player name, this should never happen.
    const auto player_pinged = GuiUtils::SanitizePlayerName(message.substr(start_idx, end_idx));
    if (player_pinged.empty()) return; // No recipient
    const auto sender = GW::PlayerMgr::GetPlayerByID(pak->player_number);
    if (!sender) return;                                                              // No sender
    if (flash_window_on_name_ping && GetPlayerName() == player_pinged) FlashWindow(); // Flash window - we've been followed!
    // Allow clickable player name
    message.insert(start_idx, L"<a=1>");
    message.insert(end_idx + 5, L"</a>");
    PendingChatMessage* m = PendingChatMessage::queuePrint(GW::Chat::Channel::CHANNEL_GROUP, message.c_str(), sender->name_enc);
    if (m) ChatSettings::AddPendingMessage(m);
    ::ClearMessageCore();
    status->blocked = true; // consume original packet.
}

// Automatic /age on vanquish
void GameSettings::OnVanquishComplete(GW::HookStatus* status, GW::Packet::StoC::VanquishComplete*) const
{
    UNREFERENCED_PARAMETER(status);
    if (!auto_age_on_vanquish)
        return;
    GW::Chat::SendChat('/', "age");
}

void GameSettings::OnDungeonReward(GW::HookStatus* status, GW::Packet::StoC::DungeonReward*) const
{
    if (hide_dungeon_chest_popup)
        status->blocked = true;
}

// Flash/focus window on trade
void GameSettings::OnTradeStarted(GW::HookStatus* status, GW::Packet::StoC::TradeStart*) const
{
    if (status->blocked)
        return;
    if (flash_window_on_trade)
        FlashWindow();
    if (focus_window_on_trade)
        FocusWindow();
}

// Stop screen shake from aftershock etc
void GameSettings::OnScreenShake(GW::HookStatus* status, void* packet) const
{
    UNREFERENCED_PARAMETER(packet);
    if (stop_screen_shake)
        status->blocked = true;
}

// Automatically skip cinematics, flash window on cinematic
void GameSettings::OnCinematic(GW::HookStatus* status, GW::Packet::StoC::CinematicPlay* packet) const
{
    UNREFERENCED_PARAMETER(status);
    if (packet->play && auto_skip_cinematic) {
        GW::Map::SkipCinematic();
        return;
    }
    if (flash_window_on_cinematic)
        FlashWindow();
}

// Flash/focus window on zoning
void GameSettings::OnMapTravel(GW::HookStatus* status, GW::Packet::StoC::GameSrvTransfer* pak) const
{
    UNREFERENCED_PARAMETER(status);
    if (flash_window_on_zoning) FlashWindow();
    if (focus_window_on_zoning && pak->is_explorable)
        FocusWindow();
}

void GameSettings::CmdReinvite(const wchar_t*, int, LPWSTR*) const
{
    pending_reinvite.reset(current_party_target_id);
}

// Turn screenshots into clickable links
void GameSettings::OnWriteChat(GW::HookStatus* status, GW::UI::UIMessage, void* wParam, void*) const
{
    static bool is_redirecting = false;
    if (is_redirecting) {
        is_redirecting = false;
        return;
    }
    PlayerChatMessage* msg = static_cast<PlayerChatMessage*>(wParam);
    if (msg->channel != GW::Chat::Channel::CHANNEL_GLOBAL)
        return;
    if (wmemcmp(L"\x846\x107", msg->message, 2) != 0)
        return;
    status->blocked = true;
    wchar_t file_path[256];
    size_t file_path_len = 0;
    wchar_t new_message[256];
    size_t new_message_len = 0;
    wcscpy(&new_message[new_message_len], L"\x846\x107<a=1>\x200C");
    new_message_len += 8;
    for (size_t i = 2; msg->message[i] && msg->message[i] != 0x1;i++) {
        new_message[new_message_len++] = msg->message[i];
        if (msg->message[i] == '\\' && msg->message[i - 1] == '\\')
            continue; // Skip double escaped directory separators when getting the actual file name
        file_path[file_path_len++] = msg->message[i];
    }
    file_path[file_path_len] = 0;
    wcscpy(&new_message[new_message_len], L"</a>\x1");
    new_message_len += 5;
    new_message[new_message_len] = 0;
    is_redirecting = true;
    GW::Chat::WriteChatEnc(static_cast<GW::Chat::Channel>(msg->channel), new_message);
    // Copy file to clipboard

    int size = sizeof(DROPFILES) + (file_path_len + 2) * sizeof(wchar_t);
    HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, size);
    ASSERT(hGlobal != nullptr);
    DROPFILES* df = (DROPFILES*)GlobalLock(hGlobal);
    ASSERT(df != nullptr);
    ZeroMemory(df, size);
    df->pFiles = sizeof(DROPFILES);
    df->fWide = TRUE;
    LPWSTR ptr = (LPWSTR)(df + 1);
    lstrcpyW(ptr, file_path);
    GlobalUnlock(hGlobal);

    // prepare the clipboard
    OpenClipboard(nullptr);
    EmptyClipboard();
    SetClipboardData(CF_HDROP, hGlobal);
    CloseClipboard();
}

// Auto-drop UA when recasting
void GameSettings::OnAgentStartCast(GW::HookStatus* , GW::UI::UIMessage, void* wParam, void*) const
{
    if (!(wParam && drop_ua_on_cast))
        return;
    struct Casting {
        uint32_t agent_id;
        GW::Constants::SkillID skill_id;
    } *casting = (Casting*)wParam;
    if (casting->agent_id == GW::Agents::GetPlayerId() && casting->skill_id == GW::Constants::SkillID::Unyielding_Aura) {
        // Cancel UA before recast
        const GW::Buff* buff = GW::Effects::GetPlayerBuffBySkillId(casting->skill_id);
        if (buff && buff->skill_id != GW::Constants::SkillID::No_Skill) {
            GW::Effects::DropBuff(buff->buff_id);
        }
    }
};

// Redirect /wiki commands to go to useful pages
void GameSettings::OnOpenWiki(GW::HookStatus* status, GW::UI::UIMessage message_id, void* wParam, void*) const
{
    std::string url = GuiUtils::ToLower((char*)wParam);
    if (strstr(url.c_str(), "/wiki/main_page")) {
        // Redirect /wiki to /wiki <current map name>
        status->blocked = true;
        GW::AreaInfo* map = GW::Map::GetCurrentMapInfo();
        pending_wiki_search_term = new GuiUtils::EncString(map->name_id);
    }
    else if (strstr(url.c_str(), "?search=quest")) {
        // Redirect /wiki quest to /wiki <current quest name>
        status->blocked = true;
        auto* quest = GW::QuestMgr::GetActiveQuest();
        if (quest) {
            char redirected_url[255];
            snprintf(redirected_url, _countof(redirected_url), "%sGame_link:Quest_%d", GuiUtils::WikiUrl(L"").c_str(), quest->quest_id);
            GW::UI::SendUIMessage(message_id, redirected_url);
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
            pending_wiki_search_term = new GuiUtils::EncString(GW::Agents::GetAgentEncName(a));
        }
        else {
            Log::Error("No current target");
        }
    }
}

void GameSettings::OnCast(GW::HookStatus *, uint32_t agent_id, uint32_t slot, uint32_t target_id, uint32_t /* call_target */) const
{
    if (!(target_id && agent_id == GW::Agents::GetPlayerId()))
        return;
    const GW::Skillbar* skill_bar = GW::SkillbarMgr::GetPlayerSkillbar();
    const GW::AgentLiving* me = GW::Agents::GetPlayerAsAgentLiving();
    const GW::Agent* target = GW::Agents::GetAgentByID(target_id);
    if (!skill_bar || !me || !target)
        return;
    if (me->max_energy <= 0 || me->player_number <= 0 || target->agent_id == me->agent_id)
        return;
    if (GW::GetDistance(me->pos, target->pos) > GetSkillRange(skill_bar->skills[slot].skill_id))
        pending_cast.reset(slot, target_id, 0);
}

// Set window title to player name on map load
void GameSettings::OnMapLoaded(GW::HookStatus*, GW::Packet::StoC::MapLoaded*) const
{
    instance_entered_at = TIMER_INIT();
    SetWindowTitle(set_window_title_as_charname);
}

// Hide more than 10 signets of capture
void GameSettings::OnUpdateSkillCount(GW::HookStatus*, void* packet) {
    const auto pak = static_cast<GW::Packet::StoC::UpdateSkillCountAfterMapLoad*>(packet);
    if (limit_signets_of_capture && static_cast<GW::Constants::SkillID>(pak->skill_id) == GW::Constants::SkillID::Signet_of_Capture) {
        actual_signets_of_capture_amount = pak->count;
        if (pak->count > 10)
            pak->count = 10;
    }
}

// Default colour for agent name tags
void GameSettings::OnAgentNameTag(GW::HookStatus*, GW::UI::UIMessage msgid, void* wParam, void*) const
{
    if (msgid != GW::UI::UIMessage::kShowAgentNameTag && msgid != GW::UI::UIMessage::kSetAgentNameTagAttribs)
        return;
    const auto tag = static_cast<GW::UI::AgentNameTagInfo*>(wParam);
    switch (static_cast<DEFAULT_NAMETAG_COLOR>(tag->text_color)) {
    case DEFAULT_NAMETAG_COLOR::NPC: tag->text_color = nametag_color_npc; break;
    case DEFAULT_NAMETAG_COLOR::ENEMY: tag->text_color = nametag_color_enemy; break;
    case DEFAULT_NAMETAG_COLOR::GADGET: tag->text_color = nametag_color_gadget; break;
    case DEFAULT_NAMETAG_COLOR::PLAYER_IN_PARTY: tag->text_color = nametag_color_player_in_party; break;
    case DEFAULT_NAMETAG_COLOR::PLAYER_OTHER: tag->text_color = nametag_color_player_other; break;
    case DEFAULT_NAMETAG_COLOR::PLAYER_SELF: tag->text_color = nametag_color_player_self; break;
    case DEFAULT_NAMETAG_COLOR::ITEM: tag->text_color = nametag_color_item; break;
    }
}
