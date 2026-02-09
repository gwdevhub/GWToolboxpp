#include "stdafx.h"

#include <GWCA/Utilities/MemoryPatcher.h>
#include <GWCA/Utilities/Scanner.h>

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Maps.h>
#include <GWCA/Constants/Skills.h>

#include <GWCA/Context/WorldContext.h>
#include <GWCA/Context/CharContext.h>

#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Camera.h>
#include <GWCA/GameEntities/Friendslist.h>
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
#include <GWCA/Managers/QuestMgr.h>

#include <GWCA/Utilities/Hooker.h>

#include <Utils/GuiUtils.h>
#include <Utils/ToolboxUtils.h>

#include <Modules/PartyWindowModule.h>

#include <Modules/ChatSettings.h>
#include <Modules/DialogModule.h>
#include <Modules/GameSettings.h>
#include <Modules/Resources.h>
#include <Windows/CompletionWindow.h>

#include <Color.h>
#include <hidusage.h>
#include <Logger.h>
#include <Timer.h>
#include <Defines.h>
#include <Utils/TextUtils.h>
#include <Constants/EncStrings.h>
#include <GWCA/GameEntities/Frame.h>

#pragma warning(disable : 6011)
#pragma comment(lib,"Version.lib")

using namespace GuiUtils;
using namespace ToolboxUtils;

namespace {
    GW::MemoryPatcher gold_confirm_patch;
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
    bool block_ghostinthebox_effect = false;
    bool block_sparkly_drops_effect = false;
    bool skip_characters_from_another_campaign_prompt = true;
    bool auto_age2_on_age = true;
    bool auto_age_on_vanquish = false;
    bool auto_screenshot_on_boss_kill = false;
    bool auto_screenshot_on_vanquish = false;
    bool auto_screenshot_on_mission_complete = false;
    bool auto_screenshot_on_dungeon_complete = false;
    bool auto_screenshot_on_title_maxed = false;
    bool auto_open_locked_chest = false;
    bool auto_open_locked_chest_with_key = false;
    bool keep_current_quest_when_new_quest_added = false;
    bool automatically_flag_pet_to_fight_called_target = true;
    bool remove_window_border_in_windowed_mode = false;
    bool prevent_weapon_spell_animation_on_player = false;
    bool block_vanquish_complete_popup = false;

    bool was_leading = true;
    bool hide_dungeon_chest_popup = false;
    bool skip_entering_name_for_faction_donate = false;
    bool stop_screen_shake = false;
    bool disable_camera_smoothing = false;
    bool disable_camera_smoothing_with_controller = false;

    bool check_message_on_party_change = true;

    bool is_prompting_hard_mode_mission = false;

    bool add_agent_id_to_enemy_names = false;

    bool useful_level_progress_label = true;

    std::map<GW::Constants::TitleID, uint32_t> last_recorded_tiers;

    GW::HookEntry SkillList_UICallback_HookEntry;
    GW::UI::UIInteractionCallback SkillList_UICallback_Func = 0, SkillList_UICallback_Ret = 0;

    // If this ui message is adding an unlearnt skill to the tome window, block it
    void OnSkillList_UICallback(GW::UI::InteractionMessage* message, void* wParam, void* lParam)
    {
        GW::Hook::EnterHook();
        if (message->message_id == GW::UI::UIMessage::kFrameMessage_0x47) {
            if (hide_known_skills && (((uint32_t*)wParam)[1] & 0x3) != 0) {
                GW::Hook::LeaveHook();
                return; // Only show unlearned skills from tomes and skill trainers
            }

            if (hide_nonelites_on_capture) {
                const auto parent = GW::UI::GetFrameByLabel(L"DlgSkillCapture");
                if (parent && GW::UI::BelongsToFrame(parent, GW::UI::GetFrameById(message->frame_id))) {
                    const auto skill = GW::SkillbarMgr::GetSkillConstantData(*(GW::Constants::SkillID*)wParam);
                    if (skill && !skill->IsElite()) {
                        GW::Hook::LeaveHook();
                        return; // Hide non-elites when capturing skills
                    }
                }
            }
        }
        SkillList_UICallback_Ret(message, wParam, lParam);
        GW::Hook::LeaveHook();
    }

    Color nametag_color_npc = static_cast<Color>(DEFAULT_NAMETAG_COLOR::NPC);
    Color nametag_color_player_self = static_cast<Color>(DEFAULT_NAMETAG_COLOR::PLAYER_SELF);
    Color nametag_color_player_other = static_cast<Color>(DEFAULT_NAMETAG_COLOR::PLAYER_OTHER);
    Color nametag_color_player_in_party = static_cast<Color>(DEFAULT_NAMETAG_COLOR::PLAYER_IN_PARTY);
    Color nametag_color_gadget = static_cast<Color>(DEFAULT_NAMETAG_COLOR::GADGET);
    Color nametag_color_enemy = static_cast<Color>(DEFAULT_NAMETAG_COLOR::ENEMY);
    Color nametag_color_item = static_cast<Color>(DEFAULT_NAMETAG_COLOR::ITEM);

    GW::HookEntry ChatCmd_HookEntry;

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

    // Before the game loads the skill bar you want, copy the data over for checking once the bar is loaded.
    void OnPreLoadSkillBar(GW::HookStatus*, const GW::UI::UIMessage message_id, void* wparam, void*)
    {
        ASSERT(message_id == GW::UI::UIMessage::kSendLoadSkillTemplate && wparam);
        const auto packet = (GW::UI::UIPacket::kSendLoadSkillTemplate*)wparam;
        if (packet->agent_id != GW::Agents::GetControlledCharacterId())
            return;
        FixLoadSkillData(packet->skill_template->skills);
    }

    enum class CharStat : uint32_t {
        Experience,
        KurzickFactionCurrent,
        KurzickFactionTotal,
        LuxonFactionCurrent,
        LuxonFactionTotal,
        ImperialFactionCurrent,
        ImperialFactionTotal,
        MysteryMagicalFactionCurrent,
        MysteryMagicalFactionTotal,
        Level,
        Morale,
        BalthazarFactionCurrent,
        BalthazarFactionTotal,
        CurrentSkillPoints
    };

    using CharacterStatIncreased_pt = void(__cdecl*)(CharStat stat, uint32_t amount_gained);
    CharacterStatIncreased_pt CharacterStatIncreased_Func = nullptr, CharacterStatIncreased_Ret = nullptr;
    // Block overhead experience gain numbers
    void OnCharacterStatIncreased(CharStat stat, uint32_t amount_gained)
    {
        bool blocked = false;
        GW::Hook::EnterHook();
        switch (stat) {
            case CharStat::ImperialFactionCurrent:
            case CharStat::BalthazarFactionCurrent:
            case CharStat::KurzickFactionCurrent:
            case CharStat::LuxonFactionCurrent:
                blocked = block_faction_gain;
                break;
            case CharStat::Experience:
                blocked = block_experience_gain || (block_zero_experience_gain && amount_gained == 0);
                break;
        }
        if (!blocked) {
            CharacterStatIncreased_Ret(stat, amount_gained);
        }
        else {
            // we need to be sure to increase the stat ourselves because this function would normally do it for us
            const auto w = GW::GetWorldContext();
            uint32_t* char_stat_values = &w->experience;
            char_stat_values[(uint32_t)stat * 2] += amount_gained;
        }
        GW::Hook::LeaveHook();
    }

    // Key held to show/hide item descriptions
    constexpr int modifier_key_item_descriptions = VK_MENU;
    int modifier_key_item_descriptions_key_state = 0;

    // Key held to show/hide skill descriptions
    constexpr int modifier_key_skill_descriptions = VK_MENU;
    int modifier_key_skill_descriptions_key_state = 0;

    struct SetFrameSkillDescriptionParam {
        uint32_t frame_id;
        GW::Constants::SkillID skill_id;
        uint32_t h0008;
        uint32_t h000c;
        uint32_t h0010;
    };

    clock_t pending_screenshot = 0;

    using SetFrameSkillDescription_pt = void(__fastcall*)(SetFrameSkillDescriptionParam* param);
    SetFrameSkillDescription_pt SetFrameSkillDescription_Func = nullptr, SetFrameSkillDescription_Ret = nullptr;

    // Hide skill description in tooltip; called by GW to add the description of the skill to a skill tooltip
    void __fastcall OnSetFrameSkillDescription(SetFrameSkillDescriptionParam* param)
    {
        GW::Hook::EnterHook();
        constexpr auto frame_set_text_ui_message = GW::UI::UIMessage::kFrameMessage_0x52;
        const auto frame = GW::UI::GetChildFrame(GW::UI::GetFrameById(param->frame_id), 0xb);
        bool block_description = disable_skill_descriptions_in_outpost && IsOutpost() || disable_skill_descriptions_in_explorable && IsExplorable();
        block_description = block_description && GetKeyState(modifier_key_item_descriptions) >= 0;
        if (block_description) {
            GW::UI::SendFrameUIMessage(frame, frame_set_text_ui_message, (void*)L"\x101");
            GW::Hook::LeaveHook();
            return;
        }
        // Add a catch to grab the encoded text back out
        static std::wstring encoded_text_set;
        static GW::UI::UIInteractionCallback prev_callback = 0;
        prev_callback = 0;
        if (frame && frame->frame_callbacks.size()) {
            prev_callback = frame->frame_callbacks[0].callback;
            frame->frame_callbacks[0].callback = [](GW::UI::InteractionMessage* message, void* wParam, void* lParam) {
                if (message->message_id == frame_set_text_ui_message)
                    encoded_text_set = (wchar_t*)wParam;
                prev_callback(message, wParam, lParam);
            };
        }
        SetFrameSkillDescription_Ret(param);
        if (prev_callback) frame->frame_callbacks[0].callback = prev_callback;

        // Add campaign info
        const auto skill = GW::SkillbarMgr::GetSkillConstantData(param->skill_id);
        const auto campaign_id = (uint32_t)skill->campaign;
        if (campaign_id < _countof(GW::EncStrings::Campaign)) {
            wchar_t buf[16] = {0};
            if (GW::UI::UInt32ToEncStr(GW::EncStrings::Campaign[(uint32_t)skill->campaign], buf, _countof(buf))) {
                encoded_text_set += std::format(L"\x2\x102\x2\x108\x107<c=@SkillDull>\x1\x2{}\x2\x108\x107</c>\x1", buf);
                GW::UI::SendFrameUIMessage(frame, frame_set_text_ui_message, (void*)encoded_text_set.c_str());
            }
        }
        GW::Hook::LeaveHook();
    }

    using SetGlobalNameTagVisibility_pt = void(__cdecl*)(uint32_t flags);
    SetGlobalNameTagVisibility_pt SetGlobalNameTagVisibility_Func = nullptr;
    uint32_t* GlobalNameTagVisibilityFlags = nullptr;

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
                    if (const auto w = GW::GetWorldContext()) {
                        for (auto& pet : w->pets) {
                            GW::PartyMgr::SetPetBehavior(pet.owner_agent_id, GW::HeroBehavior::Fight);
                        }
                        for (auto& hero : w->hero_flags) {
                            if (!GW::Agents::IsAgentCarryingBundle(hero.agent_id)) 
                                GW::PartyMgr::SetHeroTarget(hero.agent_id, party_target_info->identifier);
                        }
                    }
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
    void OverrideDefaultOnlineStatus()
    {
        GW::GameThread::Enqueue([] {
            GW::UI::SelectDropdownOption(GW::UI::GetFrameByLabel(L"StatusOverride"), last_online_status);
        });
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
        const auto msg = static_cast<GW::UI::UIPacket::kChangeTarget*>(wParam);
        auto chosen_target = GW::Agents::GetAgentByID(msg->evaluated_target_id);
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

    void OnQuestAdded(uint32_t)
    {
        if (GW::QuestMgr::GetActiveQuestId() == player_requested_active_quest_id) {
            return;
        }
        if (keep_current_quest_when_new_quest_added) {
            // Re-request a quest change
            const auto quest = GW::QuestMgr::GetQuest(player_requested_active_quest_id);
            if (!quest) {
                return;
            }
            // Emulate the StoC packet because the packet handler itself sorts memory out, and then calls affected modules via UI messages with the new (current) data...
            GW::Packet::StoC::QuestAdd packet;
            packet.quest_id = quest->quest_id;
            packet.marker = quest->marker;
            packet.map_to = quest->map_to;
            packet.log_state = quest->log_state;
            packet.map_from = quest->map_from;
            wcscpy(packet.location, quest->location);
            wcscpy(packet.name, quest->name);
            wcscpy(packet.npc, quest->npc);
            // ...We do this because sending UI messages doesn't actually update the quest data, it only notifies te ui that stuff has changed
            GW::StoC::EmulatePacket(&packet);
            GW::QuestMgr::SetActiveQuestId(quest->quest_id);
        }
    }

    void CHAT_CMD_FUNC(CmdReinvite)
    {
        pending_reinvite.reset(current_party_target_id);
    }

    bool ShouldBlockEffect(uint32_t effect_id)
    {
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

    const wchar_t* GetPartySearchLeader(uint32_t party_search_id)
    {
        const auto p = GW::PartyMgr::GetPartySearch(party_search_id);
        return p && p->party_leader && *p->party_leader ? p->party_leader : nullptr;
    }


    bool mission_prompted = false;

    // We've just asked the game to enter mission; check (and prompt) if we should really be in NM or HM instead
    void CheckPromptBeforeEnterMission(GW::HookStatus* status)
    {
        if (!check_and_prompt_if_mission_already_completed)
            return;
        if (mission_prompted || GW::PartyMgr::GetPartyPlayerCount() > 1)
            return;
        const auto map_id = GW::Map::GetMapID();
        const auto nm_complete = CompletionWindow::IsAreaComplete(map_id, CompletionCheck::NormalMode);
        const auto hm_complete = CompletionWindow::IsAreaComplete(map_id, CompletionCheck::HardMode);

        auto on_enter_mission_prompt = [](const bool result, void*) {
            mission_prompted = true;
            result && GW::PartyMgr::SetHardMode(!GW::PartyMgr::GetIsPartyInHardMode());
            const auto buttons = DialogModule::GetDialogButtons();
            const auto button = std::ranges::find_if(buttons, [](GW::UI::DialogButtonInfo* btn) {
                return btn->message && wcscmp(btn->message, GW::EncStrings::WeAreReady) == 0;
            });
            GW::Map::EnterChallenge() || (button != buttons.end() && GW::Agents::SendDialog((*button)->dialog_id));
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
            status->blocked = true;
        }
    }

    GW::HookEntry OnPreUIMessage_HookEntry;
    
    bool need_to_hide_inventory_window_after_trade = false;

    void OnPreUIMessage(GW::HookStatus* status, GW::UI::UIMessage message_id, void* wParam, void*)
    {
        switch (message_id) {
            case GW::UI::UIMessage::kSendCallTarget: {
                auto packet = (GW::UI::UIPacket::kSendCallTarget*)wParam;
                const auto agent = static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(packet->agent_id));
                if (packet->call_type == GW::CallTargetType::Following && agent
                    && agent->GetIsLivingType() && agent->allegiance == GW::Constants::Allegiance::Enemy) {
                    packet->call_type = GW::CallTargetType::AttackingOrTargetting;
                }
            }
            break;
            case GW::UI::UIMessage::kMapLoaded: {
                mission_prompted = false;
            }
            break;
            case GW::UI::UIMessage::kSendDialog: {
                const auto dialog_id = (uint32_t)wParam;
                const auto& buttons = DialogModule::GetDialogButtons();
                const auto button = std::ranges::find_if(buttons, [dialog_id](GW::UI::DialogButtonInfo* btn) {
                    return btn->dialog_id == dialog_id && btn->message && wcscmp(btn->message, GW::EncStrings::WeAreReady) == 0;
                });
                if (button == buttons.end())
                    break;
                CheckPromptBeforeEnterMission(status);
                if (status->blocked) {
                    GW::GameThread::Enqueue([] {
                        DialogModule::ReloadDialog();
                    });
                }
            }
            break;
            case GW::UI::UIMessage::kTradeSessionStart: {
                // TODO: This only catches the scenario where the trade window is shown straight away (i.e. player initiates trade)
                // If another player trades you, this ui message will show the trade "prompt" window and won't show inv until you click "view"
                // 
                // Probably need to hook into CreateFloatingWindow and block it if the inv window is trying to be opened without user interaction
                need_to_hide_inventory_window_after_trade = GW::UI::GetFrameByLabel(L"Inventory") == nullptr;
            } break;
        }
    }

    void CheckRemoveWindowBorder()
    {
        // @TODO: When frame is removed, the game "expands" to fill the space, but the UI is still offset as if its factoring in the for title bar. Intercept SetWindowPos on the game side instead of doing this???
        const auto pref = GW::UI::GetPreference(GW::UI::NumberPreference::ScreenBorderless);
        //Log::Log("Pref changed %d", pref);
        if (remove_window_border_in_windowed_mode && pref == 0) {
            const auto hwnd = GW::MemoryMgr::GetGWWindowHandle();
            if (!hwnd) return;

            auto remove_styles = (WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
            auto lStyle = GetWindowLong(hwnd, GWL_STYLE);

            if (!lStyle) return;
            if ((lStyle & remove_styles) != 0) {
                lStyle &= ~remove_styles;
                SetWindowLong(hwnd, GWL_STYLE, lStyle);
            }

            remove_styles = (WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);
            lStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
            if ((lStyle & remove_styles) != 0) {
                lStyle &= ~remove_styles;
                //SetWindowLong(hwnd, GWL_EXSTYLE, lStyle);
            }
            //SetWindowLong(hwnd, GWL_EXSTYLE, lExStyle);

            //SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER);

            // Display close/restore/min buttons top right
            GW::UI::SetFrameVisible(GW::UI::GetFrameByLabel(L"BtnMin"), true);
            GW::UI::SetFrameVisible(GW::UI::GetFrameByLabel(L"BtnRestore"), false); // @TODO: Show this, but make it maximise the window on click instead
            GW::UI::SetFrameVisible(GW::UI::GetFrameByLabel(L"BtnExit"), true);
        }
    }

    // Pre-fill character name when donating faction
    void SkipCharacterNameEntryForFactionDonation(bool immediate = true)
    {
        if (!skip_entering_name_for_faction_donate) return;
        if (!immediate) {
            Resources::EnqueueWorkerTask([] {
                // When a donation is complete, there are several different ui messages that come in varying sequence; give 500ms to ensure all are processed by the game first
                Sleep(500);
                GW::GameThread::Enqueue([] {
                    SkipCharacterNameEntryForFactionDonation(true);
                });
            });
            return;
        }
        auto frame = GW::UI::GetChildFrame(GW::UI::GetFrameByLabel(L"NPCInteract"), 0, 0);
        const auto sign_btn = GW::UI::GetChildFrame(frame, 2);
        if (!(sign_btn && sign_btn->IsVisible() && sign_btn->IsDisabled())) return; // If sign button isn't visible, the player doesn't have enough faction
        const auto name_input = (GW::EditableTextFrame*)GW::UI::GetChildFrame(frame, 4, 2);
        if (!name_input) return;
        // Prefill and hide the name input
        name_input->SetValue(GW::PlayerMgr::GetPlayerName());
        GW::UI::SetFrameVisible(name_input, 0);
        // Show and enable the "Sign" button
        GW::UI::SetFrameDisabled(sign_btn, 0);
    }

    void OnDialogButton(GW::UI::DialogButtonInfo* packet)
    {
        if (auto_open_locked_chest_with_key && wcscmp(packet->message, L"\x8101\x7F88\x10A\x8101\x13BE\x1") == 0) {
            // Auto use key
            DialogModule::SendDialog(packet->dialog_id);
        }
        if (auto_open_locked_chest && wcscmp(packet->message, L"\x8101\x7f88\x010a\x8101\x730e\x1") == 0) {
            // Auto use lockpick
            DialogModule::SendDialog(packet->dialog_id);
        }
    }

    GW::HookEntry OnPostUIMessage_HookEntry;

    uint32_t XpReqForLevel(uint32_t level)
    {
        if (!level) return 0;
        level--; // 0 based
        if (level < 22) { // Changed from 23 to 22
            return (level * 300 + 0x6a4) * level;
        }
        return level * 15000 - 0x23fc8;
    }

    uint32_t LevelFromXp(uint32_t currentXp)
    {
        uint32_t level = 1;                             // This is correct - start at 0
        while (XpReqForLevel(level + 1) <= currentXp) { // Check next level
            level++;
        }
        return level;// 1 Based
    }

    bool SetXpBarLabel() {
        const auto xp_bar = (GW::ProgressBar*)GW::UI::GetChildFrame(GW::UI::GetFrameByLabel(L"LevelProgress"), 1);
        if(!xp_bar) return false;
        const auto current_xp = GW::GetWorldContext()->experience;
        const auto current_level = LevelFromXp(current_xp);
        if (useful_level_progress_label) {
            const auto current_level_req = XpReqForLevel(current_level);
            const auto next_level_req = XpReqForLevel(current_level + 1);

            wchar_t max_amount[4] = {0};
            ASSERT(GW::UI::UInt32ToEncStr(next_level_req - current_level_req, max_amount, _countof(max_amount)));

            wchar_t current_amount[4] = {0};
            ASSERT(GW::UI::UInt32ToEncStr(current_xp - current_level_req, current_amount, _countof(current_amount)));

            const auto label = std::format(L"\x7b1e\x101{}\x102{}\x2\x408", current_amount, max_amount);
            return xp_bar->SetLabel(label.c_str());
        }
        const auto label = std::format(L"\x435\x101{}", static_cast<wchar_t>(std::min(current_level,20u) + 0x100));
        return xp_bar->SetLabel(label.c_str());
    }

    GW::UI::UIInteractionCallback OnSkillTomeWindow_UIMessage_Func = 0, OnSkillTomeWindow_UIMessage_Ret = 0;
    void OnSkillTomeWindow_UIMessage(GW::UI::InteractionMessage* message, void* wParam, void* lParam)
    {
        GW::Hook::EnterHook();
        if (message->message_id == GW::UI::UIMessage::kMouseClick2) {
            const auto packet = (GW::UI::UIPacket::kMouseAction*)wParam;
            if (packet->child_offset_id == 0 && packet->current_state == GW::UI::UIPacket::ActionState::MouseUp) {
                const auto context = (uint32_t*)GW::UI::GetFrameContext(GW::UI::GetFrameById(message->frame_id));
                const auto item = context ? GW::Items::GetItemById(context[1]) : nullptr;
                GW::Items::UseItem(item); // Auto re-use tome when "Learn" is clicked
            }
        }

        OnSkillTomeWindow_UIMessage_Ret(message, wParam, lParam);
        GW::Hook::LeaveHook();
    }

    void RecordTitleTiers() {
        last_recorded_tiers.clear();
        if (!GW::Map::GetIsMapLoaded()) return;
        const auto w = GW::GetWorldContext();
        if (!w) return;
        for (size_t title_id = 0; title_id < w->titles.size(); title_id++) {
            last_recorded_tiers[static_cast<GW::Constants::TitleID>(title_id)] = w->titles[title_id].current_title_tier_index;
        }
    }

    void OnPostUIMessage(GW::HookStatus* status, GW::UI::UIMessage message_id, void* wParam, void*)
    {
        if (status->blocked)
            return;
        switch (message_id) {
            case GW::UI::UIMessage::kPreBuildLoginScene: {
                GW::GameThread::Enqueue([] {
                    OverrideDefaultOnlineStatus();
                },true);        
            } break;
            case GW::UI::UIMessage::kPartyShowConfirmDialog: {
                const auto packet = static_cast<GW::UI::UIPacket::kPartyShowConfirmDialog*>(wParam);
                if (skip_characters_from_another_campaign_prompt && wcscmp(packet->prompt_enc_str, L"\x8101\x05d2") == 0) {
                    // "Yes" to skip the confirm prompt
                    GW::UI::ButtonClick(GW::UI::GetChildFrame(GW::UI::GetFrameByLabel(L"Party"), 1, 10, 6));
                }
            }
            break;
            case GW::UI::UIMessage::kSendSetActiveQuest:
                player_requested_active_quest_id = GW::QuestMgr::GetActiveQuestId();
                break;
            case GW::UI::UIMessage::kQuestAdded:
                OnQuestAdded(*(uint32_t*)wParam);
                break;
            case GW::UI::UIMessage::kTradeSessionStart: {
                if (flash_window_on_trade) {
                    FlashWindow();
                }
                if (focus_window_on_trade) {
                    FocusWindow();
                }
                if (need_to_hide_inventory_window_after_trade) {
                    GW::UI::DestroyUIComponent(GW::UI::GetFrameByLabel(L"Inventory"));
                }
            }
            break;
            case GW::UI::UIMessage::kVendorTransComplete: {
                SkipCharacterNameEntryForFactionDonation(false);
            }
            break;
            case GW::UI::UIMessage::kVendorWindow: {
                SkipCharacterNameEntryForFactionDonation(true);
            }
            break;
            case GW::UI::UIMessage::kPartySearchInviteSent: {
                // Automatically send a party window invite when a party search invite is sent
                const auto packet = (GW::UI::UIPacket::kPartySearchInvite*)wParam;
                if (GW::PartyMgr::GetIsLeader())
                    GW::PartyMgr::InvitePlayer(GetPartySearchLeader(packet->source_party_search_id));
            }
            break;
            case GW::UI::UIMessage::kPreferenceValueChanged: {
                const auto packet = (GW::UI::UIPacket::kPreferenceValueChanged*)wParam;
                if (packet->preference_id == GW::UI::NumberPreference::ScreenBorderless)
                    CheckRemoveWindowBorder();
            }
            break;
            case GW::UI::UIMessage::kPartyDefeated: {
                if (auto_return_on_defeat && GW::PartyMgr::GetIsLeader())
                    GW::PartyMgr::ReturnToOutpost() || (Log::Warning("Failed to return to outpost"), true);
            }
            break;
            case GW::UI::UIMessage::kMapChange: {
                RecordTitleTiers();
            } break;
            case GW::UI::UIMessage::kMapLoaded: {
                last_online_status = static_cast<uint32_t>(GW::FriendListMgr::GetMyStatus());
                SetXpBarLabel();
                block_enter_area_message && GW::UI::SetFrameVisible(GW::UI::GetChildFrame(GW::UI::GetFrameByLabel(L"Game"), 6, 0), false);
                RecordTitleTiers();
            }
            break;
            case GW::UI::UIMessage::kExperienceGained: {
                SetXpBarLabel();
            } break;
            case GW::UI::UIMessage::kShowCancelEnterMissionBtn: {
                CheckPromptBeforeEnterMission(status);
                if (status->blocked)
                    GW::Map::CancelEnterChallenge() || (Log::Warning("Failed to cancel mission entry"), true);
                break;
            }
            break;
            case GW::UI::UIMessage::kVanquishComplete: {
                if (auto_age_on_vanquish)
                    GW::Chat::SendChat('/', L"age");
                if (auto_screenshot_on_vanquish) pending_screenshot = TIMER_INIT();
                if (block_vanquish_complete_popup)
                    GW::UI::SetFrameVisible(GW::UI::GetChildFrame(GW::UI::GetFrameByLabel(L"Game"), 6, 8), false) || (Log::Warning("Failed to hide vanquish popup"), true);
            }
            break;
            case GW::UI::UIMessage::kMissionComplete: {
                if (auto_screenshot_on_mission_complete) pending_screenshot = TIMER_INIT();
            }
            break;
            case GW::UI::UIMessage::kDungeonComplete: {
                if (auto_screenshot_on_dungeon_complete) pending_screenshot = TIMER_INIT();
            }  
            break;
            case GW::UI::UIMessage::kDialogButton: {
                OnDialogButton((GW::UI::DialogButtonInfo*)wParam);
            } break;
            case GW::UI::UIMessage::kTitleProgressUpdated: {
                if (auto_screenshot_on_title_maxed) {
                    const auto title_id = static_cast<GW::Constants::TitleID>(reinterpret_cast<uint32_t>(wParam));
                    const auto title = GW::PlayerMgr::GetTitleTrack(title_id);
                    // @Cleanup: I don't think this behaves itself.
                    if (title && title->max_title_tier_index == title->current_title_tier_index && last_recorded_tiers.contains(title_id) && last_recorded_tiers[title_id] != title->current_title_tier_index) {
                        pending_screenshot = TIMER_INIT();
                        last_recorded_tiers[title_id] = title->current_title_tier_index;
                    }
                }
            } break;
            
        }
    }

    void DrawAudioSettings()
    {
        if (ImGui::Button("Open Advanced Audio Window")) {
            GW::GameThread::Enqueue([] {
                GW::GetCharContext()->player_flags |= 0x8;
                GW::UI::Keypress((GW::UI::ControlAction)0x24);
                GW::GetCharContext()->player_flags ^= 0x8;
            });
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
        if (!out.empty()) {
            out += L"\x2\x102\x2";
        }
        out += item->complete_name_enc;
    }
    else if (parts & NAME && item->name_enc) {
        if (!out.empty()) {
            out += L"\x2\x102\x2";
        }
        out += item->name_enc;
    }
    if (parts & DESC && item->info_string) {
        if (!out.empty()) {
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

std::vector<std::wstring> PendingChatMessage::SanitiseForSend() const
{
    const std::wstring sanitised = TextUtils::ctre_regex_replace<L"<[^>]+>", L"">(output_message);
    const std::wstring sanitised2 = TextUtils::ctre_regex_replace<L"\n", L"|">(sanitised);

    // Split the string by '|' character
    std::vector<std::wstring> parts;
    std::wstringstream wss(sanitised2);
    std::wstring temp;
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

    OnSkillTomeWindow_UIMessage_Func = (GW::UI::UIInteractionCallback)GW::Scanner::ToFunctionStart(GW::Scanner::FindAssertion("GmSkTome.cpp", "selection.skillId", 0, 0),0xfff);
    Log::Log("[GameSettings] OnSkillTomeWindow_UIMessage_Func = %p\n", OnSkillTomeWindow_UIMessage_Func);

    SkillList_UICallback_Func = (GW::UI::UIInteractionCallback)GW::Scanner::ToFunctionStart(GW::Scanner::FindAssertion("GmCtlSkList.cpp", "!obj", 0xc71, 0));
    Log::Log("[GameSettings] SkillList_UICallback_Func = %p\n", SkillList_UICallback_Func);

    auto address = GW::Scanner::Find("\xF7\x40\x0C\x10\x00\x02\x00\x75", "xxxxxx??", +7);
    if (address)
        gold_confirm_patch.SetPatch(address, "\x90\x90", 2);
    Log::Log("[GameSettings] gold_confirm_patch = %p\n", gold_confirm_patch.GetAddress());

    address = GW::Scanner::Find("\xdf\xe0\xf6\xc4\x41\x7a\x79", "xxxxxxx", 0x5);
    if (address)
        remove_skill_warmup_duration_patch.SetPatch(address, "\x90\x90", 2);
    Log::Log("[GameSettings] remove_skill_warmup_duration_patch = %p\n", remove_skill_warmup_duration_patch.GetAddress());

    ItemDescriptionHandler::RegisterDescriptionCallback(OnGetItemDescription, 9999);

    // Call our CreateCodedTextLabel function instead of default CreateCodedTextLabel for patching skill descriptions
    SetFrameSkillDescription_Func = (SetFrameSkillDescription_pt)GW::Scanner::ToFunctionStart(GW::Scanner::FindAssertion("GmTipSkill.cpp", "No valid case for switch variable \'m_powerType\'", 0, 0));

    Log::Log("[GameSettings] SetFrameSkillDescription_Func = %p\n", SetFrameSkillDescription_Func);

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

    CharacterStatIncreased_Func = (CharacterStatIncreased_pt)GW::Scanner::ToFunctionStart(GW::Scanner::FindAssertion("ChCliApi.cpp", "stat < CHAR_STATS", 0, 0));
    Log::Log("[GameSettings] CharacterStatIncreased_Func = %p\n", (void*)CharacterStatIncreased_Func);

#ifdef _DEBUG
    ASSERT(SkillList_UICallback_Func);
    ASSERT(remove_skill_warmup_duration_patch.IsValid());
    ASSERT(SetFrameSkillDescription_Func);
    ASSERT(SetGlobalNameTagVisibility_Func);
    ASSERT(GlobalNameTagVisibilityFlags);
    ASSERT(CharacterStatIncreased_Func);
    ASSERT(OnSkillTomeWindow_UIMessage_Func);
#endif

    if (OnSkillTomeWindow_UIMessage_Func) {
        GW::Hook::CreateHook((void**)&OnSkillTomeWindow_UIMessage_Func, OnSkillTomeWindow_UIMessage, reinterpret_cast<void**>(&OnSkillTomeWindow_UIMessage_Ret));
        GW::Hook::EnableHooks(OnSkillTomeWindow_UIMessage_Func);
    }
    if (SetFrameSkillDescription_Func) {
        GW::Hook::CreateHook((void**)&SetFrameSkillDescription_Func, OnSetFrameSkillDescription, reinterpret_cast<void**>(&SetFrameSkillDescription_Ret));
        GW::Hook::EnableHooks(SetFrameSkillDescription_Func);
    }
    if (SkillList_UICallback_Func) {
        GW::Hook::CreateHook((void**)&SkillList_UICallback_Func, OnSkillList_UICallback, reinterpret_cast<void**>(&SkillList_UICallback_Ret));
        GW::Hook::EnableHooks(SkillList_UICallback_Func);
    }
    if (CharacterStatIncreased_Func) {
        GW::Hook::CreateHook((void**)&CharacterStatIncreased_Func, OnCharacterStatIncreased, reinterpret_cast<void**>(&CharacterStatIncreased_Ret));
        GW::Hook::EnableHooks(CharacterStatIncreased_Func);
    }

    RegisterUIMessageCallback(&OnDialog_Entry, GW::UI::UIMessage::kSendLoadSkillTemplate, &OnPreLoadSkillBar);
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
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::PartyPlayerAdd>(&PartyPlayerAdd_Entry, OnPartyPlayerJoined);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GameSrvTransfer>(&GameSrvTransfer_Entry, OnMapTravel);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::CinematicPlay>(&CinematicPlay_Entry, OnCinematic);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::DungeonReward>(&VanquishComplete_Entry, OnDungeonReward);
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
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::ScreenShake>(&OnScreenShake_Entry, OnScreenShake);

    RegisterUIMessageCallback(&OnChangeTarget_Entry, GW::UI::UIMessage::kChangeTarget, OnChangeTarget);
    RegisterUIMessageCallback(&OnWriteChat_Entry, GW::UI::UIMessage::kWriteToChatLog, OnWriteChat);
    RegisterUIMessageCallback(&OnAgentStartCast_Entry, GW::UI::UIMessage::kAgentSkillStartedCast, OnAgentStartCast);
    RegisterUIMessageCallback(&OnOpenWikiUrl_Entry, GW::UI::UIMessage::kOpenWikiUrl, OnOpenWiki);
    RegisterUIMessageCallback(&OnAgentNameTag_Entry, GW::UI::UIMessage::kShowAgentNameTag, OnAgentNameTag);
    RegisterUIMessageCallback(&OnAgentNameTag_Entry, GW::UI::UIMessage::kSetAgentNameTagAttribs, OnAgentNameTag);

    GW::UI::RegisterKeydownCallback(&OnChangeTarget_Entry, OnKeyDownAction);
    GW::UI::RegisterKeyupCallback(&OnChangeTarget_Entry, OnKeyUpAction);

    GW::FriendListMgr::RegisterFriendStatusCallback(&FriendStatusCallback_Entry, FriendStatusCallback);
    RegisterUIMessageCallback(&OnPreSendDialog_Entry, GW::UI::UIMessage::kSendPingWeaponSet, OnPingWeaponSet);

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
        GW::UI::UIMessage::kVanquishComplete,
        GW::UI::UIMessage::kSendDialog,
        GW::UI::UIMessage::kMapLoaded, 
        GW::UI::UIMessage::kTradeSessionStart
    };
    for (const auto message_id : pre_ui_messages) {
        RegisterUIMessageCallback(&OnPreUIMessage_HookEntry, message_id, OnPreUIMessage, -0x8000);
    }

    constexpr GW::UI::UIMessage post_ui_messages[] = {
        GW::UI::UIMessage::kPartySearchInviteSent,
        GW::UI::UIMessage::kPreferenceValueChanged,
        GW::UI::UIMessage::kMapLoaded,
        GW::UI::UIMessage::kTradeSessionStart,
        GW::UI::UIMessage::kShowCancelEnterMissionBtn,
        GW::UI::UIMessage::kPartyDefeated,
        GW::UI::UIMessage::kVanquishComplete,
        GW::UI::UIMessage::kMissionComplete,
        GW::UI::UIMessage::kDungeonComplete,
        GW::UI::UIMessage::kPartyShowConfirmDialog,
        GW::UI::UIMessage::kVendorWindow,
        GW::UI::UIMessage::kDialogButton,
        GW::UI::UIMessage::kQuestAdded,
        GW::UI::UIMessage::kSendSetActiveQuest,
        GW::UI::UIMessage::kVendorTransComplete,
        GW::UI::UIMessage::kExperienceGained,
        GW::UI::UIMessage::kTitleProgressUpdated
    };
    for (const auto message_id : post_ui_messages) {
        RegisterUIMessageCallback(&OnPostUIMessage_HookEntry, message_id, OnPostUIMessage, 0x8000);
    }


    GW::Chat::CreateCommand(&ChatCmd_HookEntry, L"reinvite", CmdReinvite);

    set_window_title_delay = TIMER_INIT();

    player_requested_active_quest_id = GW::QuestMgr::GetActiveQuestId();

    last_online_status = static_cast<uint32_t>(GW::FriendListMgr::GetMyStatus());

    //Log::Log("[GameSettings] Enqueueing CheckRemoveWindowBorder");
    //GW::GameThread::Enqueue(CheckRemoveWindowBorder);
    //Log::Log("[GameSettings] Enqueued CheckRemoveWindowBorder");

#ifdef APRIL_FOOLS
    AF::ApplyPatchesIfItsTime();
#endif
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


    LOAD_BOOL(disable_camera_smoothing);
    LOAD_BOOL(disable_camera_smoothing_with_controller);
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
    LOAD_BOOL(hide_known_skills);
    LOAD_BOOL(hide_nonelites_on_capture);
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
    LOAD_BOOL(auto_screenshot_on_boss_kill);
    LOAD_BOOL(auto_screenshot_on_vanquish);
    LOAD_BOOL(auto_screenshot_on_mission_complete);
    LOAD_BOOL(auto_screenshot_on_dungeon_complete);
    LOAD_BOOL(auto_screenshot_on_title_maxed);
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
    LOAD_BOOL(disable_skill_descriptions_in_outpost);
    LOAD_BOOL(disable_skill_descriptions_in_explorable);

    LOAD_BOOL(prevent_weapon_spell_animation_on_player);
    LOAD_BOOL(block_vanquish_complete_popup);

    LOAD_BOOL(useful_level_progress_label);

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

    if (focus_window_on_launch) {
        FocusWindow();
    }
    GW::GameThread::Enqueue(SetXpBarLabel);
}

void GameSettings::RegisterSettingsContent()
{
    ToolboxModule::RegisterSettingsContent();

    ToolboxModule::RegisterSettingsContent(
        "Party Settings", ICON_FA_USERS,
        [this](const std::string&, const bool is_showing) {
            if (is_showing) {
                DrawPartySettings();
            }
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
    ToolboxModule::RegisterSettingsContent(
        "Audio Settings", ICON_FA_MUSIC,
        [](const std::string&, const bool is_showing) {
            if (is_showing) {
                DrawAudioSettings();
            }
        },
        0.9f);
}

void GameSettings::Terminate()
{
    ToolboxModule::Terminate();
    gold_confirm_patch.Reset();
    remove_skill_warmup_duration_patch.Reset();

    GW::UI::RemoveUIMessageCallback(&OnQuestUIMessage_HookEntry);
    GW::UI::RemoveUIMessageCallback(&OnPostUIMessage_HookEntry);
    GW::UI::RemoveUIMessageCallback(&OnPreUIMessage_HookEntry);

    if (SkillList_UICallback_Func)
        GW::Hook::RemoveHook(SkillList_UICallback_Func);
    if (CharacterStatIncreased_Func)
        GW::Hook::RemoveHook(CharacterStatIncreased_Func);
    if (SetFrameSkillDescription_Func)
        GW::Hook::RemoveHook(SetFrameSkillDescription_Func);
    if (OnSkillTomeWindow_UIMessage_Func) 
        GW::Hook::RemoveHook(OnSkillTomeWindow_UIMessage_Func);

    GW::Chat::DeleteCommand(&ChatCmd_HookEntry);
}

void GameSettings::SaveSettings(ToolboxIni* ini)
{
    ToolboxModule::SaveSettings(ini);


    SAVE_BOOL(tick_is_toggle);

    SAVE_BOOL(disable_camera_smoothing);
    SAVE_BOOL(disable_camera_smoothing_with_controller);
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
    SAVE_BOOL(hide_known_skills);
    SAVE_BOOL(hide_nonelites_on_capture);
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

    SAVE_BOOL(auto_screenshot_on_boss_kill);
    SAVE_BOOL(auto_screenshot_on_vanquish);
    SAVE_BOOL(auto_screenshot_on_mission_complete);
    SAVE_BOOL(auto_screenshot_on_dungeon_complete);
    SAVE_BOOL(auto_screenshot_on_title_maxed);

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

    SAVE_BOOL(disable_skill_descriptions_in_outpost);
    SAVE_BOOL(disable_skill_descriptions_in_explorable);

    SAVE_BOOL(block_enter_area_message);

    SAVE_UINT(last_online_status);

    SAVE_BOOL(prevent_weapon_spell_animation_on_player);

    SAVE_BOOL(useful_level_progress_label);

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
    SAVE_BOOL(block_vanquish_complete_popup);
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
    ImGui::Checkbox("Automatically lock heroes and pets onto your called target", &automatically_flag_pet_to_fight_called_target);
}

void GameSettings::DrawSettingsInternal()
{
    ImGui::Checkbox("Apply Collector's Edition animations on player dance", &collectors_edition_emotes);
    ImGui::ShowHelp("Only applies to your own character");

    ImGui::Checkbox("Automatically cancel Unyielding Aura when re-casting", &drop_ua_on_cast);

    ImGui::Checkbox("Automatically use available keys when interacting with locked chest", &auto_open_locked_chest_with_key);

    ImGui::Checkbox("Automatically use lockpick when interacting with locked chest", &auto_open_locked_chest);

    ImGui::Checkbox("Automatically return to outpost on defeat", &auto_return_on_defeat);
    ImGui::ShowHelp("Automatically return party to outpost on party wipe if player is leading");

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

    ImGui::Checkbox("Automatically skip cinematics", &auto_skip_cinematic);

    ImGui::Checkbox("Automatic /age on vanquish", &auto_age_on_vanquish);
    ImGui::ShowHelp("As soon as a vanquish is complete, send /age command to game server to receive server-side completion time.");

    ImGui::Checkbox("Automatic /age2 on /age", &auto_age2_on_age);
    ImGui::ShowHelp("GWToolbox++ will show /age2 time after /age is shown in chat");

    ImGui::TextUnformatted("Automatic screenshot on:");
    ImGui::Indent();
    ImGui::StartSpacedElements(checkbox_w);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Vanquish", &auto_screenshot_on_vanquish);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Boss kill", &auto_screenshot_on_boss_kill);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Mission complete", &auto_screenshot_on_mission_complete);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Dungeon complete", &auto_screenshot_on_dungeon_complete);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Title maxed", &auto_screenshot_on_title_maxed);
    ImGui::Unindent();

    ImGui::Checkbox("Block full screen message when entering a new area", &block_enter_area_message);

    ImGui::Checkbox("Block full screen popup what shows when completing a vanquish", &block_vanquish_complete_popup);

    ImGui::Checkbox("Block full screen popup what shows when opening a dungeon chest", &hide_dungeon_chest_popup);

    ImGui::Checkbox("Block sparkle effect on dropped items", &block_sparkly_drops_effect);
    ImGui::ShowHelp("Applies to drops that appear after this setting has been changed");

    const char* hint = "The default mouse camera movement isn't instant, and instead smoothes the action when you move the mouse.\nTick this to disable this smoothing behaviour.";
    ImGui::Checkbox("Disable camera smoothing with mouse", &disable_camera_smoothing);
    ImGui::ShowHelp(hint);
    ImGui::Checkbox("Disable camera smoothing with controller", &disable_camera_smoothing_with_controller);
    ImGui::ShowHelp(hint);
    if (ImGui::Checkbox("Disable Gold/Green items confirmation", &disable_gold_selling_confirmation)) {
        gold_confirm_patch.TogglePatch(disable_gold_selling_confirmation);
    }
    ImGui::ShowHelp("Disable the confirmation request when\n"
                    "selling Gold and Green items introduced\n"
                    "in February 5, 2019 update.");

    ImGui::Checkbox("Keep current quest when accepting a new one", &keep_current_quest_when_new_quest_added);
    ImGui::ShowHelp(
        "By default, Guild Wars changes your currently selected quest to the one you've just taken from an NPC.\nThis can be annoying if you don't realise your quest marker is now taking you somewhere different!\nTick this to make sure your current quest isn't changed when a new quest is added to your log."
    );

    ImGui::Checkbox("Limit signet of capture to 10 in skills window", &limit_signets_of_capture);
    ImGui::ShowHelp("If your character has purchased more than 10 signets of capture, only show 10 of them in the skills window");

    ImGui::Checkbox("Hide known skills when using a tome, capturing a skill or talking to a skill trainer", &hide_known_skills);
    ImGui::ShowHelp("When you double click on a tome, the skills window that appears has all skills available for that profession.\nTick this to hide skills that your current character already has.");

    ImGui::Checkbox("Hide all non-elite skills when capturing a skill", &hide_nonelites_on_capture);

    ImGui::Checkbox("Prevent weapon spell skin showing on player weapons", &prevent_weapon_spell_animation_on_player);

    ImGui::Checkbox("Prompt if entering a mission you've already completed", &check_and_prompt_if_mission_already_completed);
    ImGui::ShowHelp(
        "Sometimes a player can forget to set Hard Mode/Normal Mode when starting a mission for their character.\nGwtoolbox can catch this and check your current character's achievements,\nand can show an 'Are you sure?' prompt if you're trying to do a mission\nthat you've already completed in the chosen mode."
    );

    ImGui::Checkbox("Remember my online status when returning to character select screen", &remember_online_status);
    ImGui::ShowHelp(
        "Guild Wars doesn't remember your friend list status when you return to the character select screen,\n and sets your status to 'Online' when you select a character to play.\nTick this to avoid having to change it when you switch characters."
    );

    if (ImGui::Checkbox("Remove 1.5 second minimum for the cast bar to show.", &remove_min_skill_warmup_duration)) {
        remove_skill_warmup_duration_patch.TogglePatch(remove_min_skill_warmup_duration);
    }
    ImGui::ShowHelp("When casting a skill, the in-game cast bar only shows up if the skill's cast time is more than 1.5 seconds.\nTick this to show the cast bar regardless of casting time.");

    if (ImGui::Checkbox("Set Guild Wars window title as current logged-in character", &set_window_title_as_charname)) {
        SetWindowTitle(set_window_title_as_charname);
    }

    ImGui::Checkbox("Show warning when earned faction reaches ", &faction_warn_percent);
    ImGui::SameLine();
    ImGui::PushItemWidth(40.0f * ImGui::GetIO().FontGlobalScale);
    ImGui::InputInt("##faction_warn_percent_amount", &faction_warn_percent_amount, 0);
    ImGui::PopItemWidth();
    ImGui::SameLine();
    ImGui::Text("%%");
    ImGui::ShowHelp("Displays when in a challenge mission or elite mission outpost");

    ImGui::Checkbox("Skip character name input when donating faction", &skip_entering_name_for_faction_donate);

    ImGui::Checkbox("Stop screen shake from skills or effects", &stop_screen_shake);
    ImGui::ShowHelp("e.g. Aftershock, Earth shaker, Avalanche effect");

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
    if (ImGui::Checkbox("Show experience progress instead of current level on your experience bar", &useful_level_progress_label)) {
        GW::GameThread::Enqueue(SetXpBarLabel);
    }

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
    if (pending_screenshot && TIMER_DIFF(pending_screenshot) > 250) {
        GW::UI::Screenshot();
        pending_screenshot = 0;
    }

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

    if (((disable_camera_smoothing && !GW::UI::IsInControllerMode()) || (disable_camera_smoothing_with_controller && GW::UI::IsInControllerMode())) && !GW::CameraMgr::GetCameraUnlock()) {
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
void GameSettings::OnUpdateAgentState(GW::HookStatus*, GW::Packet::StoC::AgentState* packet)
{
    if (auto_screenshot_on_boss_kill) {
        const auto agent = GW::Agents::GetAgentByID(packet->agent_id);
        const auto living = agent ? agent->GetAsAgentLiving() : nullptr;
        if (living && living->GetHasBossGlow()) {
            if (!living->GetIsDead() && (packet->state & 0x0010)) {
                pending_screenshot = TIMER_INIT();
            }
        }
    }

    if (prevent_weapon_spell_animation_on_player && (packet->state & 0x8000) && packet->agent_id == GW::Agents::GetControlledCharacterId()) {
        packet->state ^= 0x8000;
    }

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
void GameSettings::OnPartyDefeated(const GW::HookStatus*, GW::Packet::StoC::PartyDefeated*) {}

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
        GW::Chat::SendChat('/', L"age2");
    }
}

void GameSettings::OnDungeonReward(GW::HookStatus* status, GW::Packet::StoC::DungeonReward*)
{
    if (hide_dungeon_chest_popup) {
        status->blocked = true;
    }
}

// Stop screen shake from aftershock etc
void GameSettings::OnScreenShake(GW::HookStatus* status, const void*)
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
    std::wstring file_path;
    std::wstring new_message;
    file_path.reserve(256);
    new_message.reserve(256);
    new_message.append(L"\x846\x107<a=1>\x200C");
    for (size_t i = 2; msg->message[i] && msg->message[i] != 0x1; i++) {
        const wchar_t ch = msg->message[i];
        if (ch == L'\r' || ch == L'\n') {
            new_message.push_back(L' ');
            continue;
        }
        if (ch == L' ') {
            new_message.push_back(L'\x00A0'); // NBSP keeps the link intact while looking like a space
        } else {
            new_message.push_back(ch);
        }
        if (ch == '\\' && msg->message[i - 1] == '\\') {
            continue; // Skip double escaped directory separators when getting the actual file name
        }
        file_path.push_back(ch);
    }
    new_message.append(L"</a>\x1");
    is_redirecting = true;
    WriteChatEnc(static_cast<GW::Chat::Channel>(msg->channel), new_message.c_str());
    // Copy file to clipboard

    const auto size = sizeof(DROPFILES) + (file_path.size() + 2) * sizeof(wchar_t);
    const HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, size);
    ASSERT(hGlobal != nullptr);
    const auto df = static_cast<DROPFILES*>(GlobalLock(hGlobal));
    ASSERT(df != nullptr);
    ZeroMemory(df, size);
    df->pFiles = sizeof(DROPFILES);
    df->fWide = TRUE;
    const auto ptr = (LPWSTR)(df + 1);
    lstrcpyW(ptr, file_path.c_str());
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
    const auto packet = (GW::UI::UIPacket::kAgentSkillPacket*)wParam;
    if (drop_ua_on_cast && packet && packet->skill_id == GW::Constants::SkillID::Unyielding_Aura) {
        const auto buffs = GW::Effects::GetAgentBuffs(packet->agent_id);
        if (buffs) {
            for (auto& buff : *buffs) {
                if (buff.skill_id == packet->skill_id) {
                    GW::Effects::DropBuff(buff.buff_id);
                }
            }
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
        pak->count = std::min<uint32_t>(pak->count, 10);
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
