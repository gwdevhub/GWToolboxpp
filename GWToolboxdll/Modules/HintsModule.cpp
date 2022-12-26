#include "stdafx.h"


#include <GWCA/Constants/Maps.h>
#include <GWCA/Constants/Skills.h>

#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Utilities/Hook.h>

#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Skill.h>

#include <ImGuiAddons.h>
#include <Utils/GuiUtils.h>

#include <Modules/HintsModule.h>
#include <Defines.h>

namespace {
    struct TBHint {
        uint32_t message_id;
        const wchar_t* message;
    };

    GW::Constants::Campaign GetCharacterCampaign() {
        if (GW::Map::GetIsMapUnlocked(GW::Constants::MapID::Island_of_Shehkah))
            return GW::Constants::Campaign::Nightfall;
        if (GW::Map::GetIsMapUnlocked(GW::Constants::MapID::Ascalon_City_pre_searing))
            return GW::Constants::Campaign::Prophecies;
        return GW::Constants::Campaign::Factions;
    }

    GW::SkillbarSkill* GetPlayerSkillbarSkill(GW::Constants::SkillID skill_id) {
        auto skillbar = GW::SkillbarMgr::GetPlayerSkillbar();
        if (!skillbar) return 0;
        return skillbar->GetSkillById(skill_id);
    }

    std::vector<uint32_t> hints_shown;
    struct HintUIMessage;
    std::vector<std::pair<clock_t, HintUIMessage*>> delayed_hints;
    struct HintUIMessage {
        uint32_t message_id = 0x10000000; // Used internally to avoid queueing more than 1 of the same hint
        wchar_t* message_encoded;
        uint32_t image_file_id = 0; // e.g. mouse imaage, light bulb, exclamation mark
        uint32_t message_timeout_ms = 15000;
        uint32_t style_bitmap = 0x12; // 0x18 = hint with left padding
        HintUIMessage(const wchar_t* message, uint32_t duration = 30000, uint32_t _message_id = 0) {
            size_t strlen = (wcslen(message) + 1) * sizeof(wchar_t);
            if (message[0] == 0x108) {
                message_encoded = new wchar_t[strlen];
                // Already encoded
                wcscpy(message_encoded, message);
            }
            else {
                strlen += (3 * sizeof(wchar_t));
                message_encoded = new wchar_t[strlen];
                swprintf(message_encoded, strlen, L"\x108\x107%s\x1", message);
            }
            if(!_message_id)
                _message_id = (uint32_t)message;
            message_id = _message_id;
            message_timeout_ms = duration;
        }
        HintUIMessage(const TBHint& hint) : HintUIMessage(hint.message, 30000, hint.message_id) {}
        ~HintUIMessage() {
            delete[] message_encoded;
        }
        void Show() {

            GW::UI::SendUIMessage(GW::UI::UIMessage::kShowHint, this);
        }
        void Delay(clock_t delay_ms) {
            delayed_hints.push_back(std::pair( clock() + delay_ms, new HintUIMessage(message_encoded,message_timeout_ms,message_id)));
        }
    };
    struct LastQuote {
        uint32_t item_id = 0;
        uint32_t price = 0;
    } last_quote;
    clock_t last_quoted_item_timestamp = 0;

    constexpr const wchar_t* embark_beach_campaign_npcs[] = {
        L"",
        L"\x8102\x6F1E\xE846\xFFBF\x57E0", // Kenai [Tyrian Travel]
        L"\x8102\x6F05\xE3C3\xBF66\x234C", // Shirayuki [Canthan Travel]
        L"\x8102\x6F1E\xE846\xFFBF\x57E0" // Zinshao [Elonian Travel]
    };
    constexpr const wchar_t* endgame_reward_npcs[] = {
    L"",
    L"\x399E\x8A19\xC3B6\x2FE4", // King Jalis (Droks Explorable)
    L"\x108\x107" "Suun\x1", // Suun (Divine Path) TODO: Encoded version of this name!
    L"\x108\x107" "Keeper of Secrets\x1", // Keeper of Secrets (Throne of secrets) TODO: Encoded version of this name!
    L"\x108\x107" "Droknar\x1",
    };
    constexpr const wchar_t* endgame_reward_trophies[] = {
        L"",
        L"\x108\x107" "Deldrimor Talisman" "\x1", // King Jalis (Droks Explorable)
        L"\x108\x107" "Amulet of the Mists" "\x1", // Suun (Divine Path) TODO: Encoded version of this name!
        L"\x108\x107" "Book of Secrets" "\x1", // Keeper of Secrets (Throne of secrets) TODO: Encoded version of this name!
        L"\x108\x107" "Droknar's Key" "\x1",
    };

    constexpr TBHint HINT_Q9_STR_SHIELDS = { 0x20000001, L"PvP Strength shields give 9 armor when you don't meet the requirement, so unless you can meet the req on a different attribute, use a Strength shield." };
    constexpr TBHint HINT_HERO_EXP = { 0x20000002, L"Heroes in your party gain experience from quests, so remember to add your low level heroes when accepting quest rewards." };
    constexpr TBHint CHEST_CMD = { 0x20000003, L"Type '/chest' into chat to open your Xunlai Chest from anywhere in an outpost, so you won't have to run to the chest every time." };
    constexpr TBHint BULK_BUY = { 0x20000004, L"Hold Ctrl when requesting a quote to bulk buy or sell from a trader" };
    constexpr TBHint EMBARK_WITHOUT_HOMELAND = { 0x20001000, L"To get back from Embark Beach to where you came from, talk to \x1\x2%s\x2\x108\x107 or use the '/tb travel' chat command." };
    constexpr TBHint ENDGAME_TROPHY = { 0x20002000, L"Talk to \x1\x2%s\x2\x108\x107 to receive a \x1\x2%s\x2\x108\x107. Those are worth a lot of money if you sell to another player, so rather than trading it in for a weapon, search on https://kamadan.gwtoolbox.com for a buyer." };
    constexpr TBHint QUEST_HINT_ADVENTURE_WITH_AN_ALLY = { 0x20000005, L"If you don't have a friend available to join you for this quest, try adding everyone in Ascalon City (America English district) to your party. Someone will surely be happy to help you." };
    constexpr TBHint NOLANI_ACADEMY_SHORTCUT = { 0x20000006, L"Turn right and head south to kill Bonfaaz Burntfur.It's a quicker route to the end - Prince Rurik will be fine on his own." };
    constexpr TBHint CHARM_ANIMAL = { 0x20000007, L"Charm Animal is only needed for charming a pet. Consider bringing Comfort Animal instead." };
    constexpr TBHint HEROS_HANDBOOK = { 0x2000008, L"Talk to Gedrel of Ascalon in Eye of the North to get a Hero's Handbook and Master Dungeon Guide." };
    constexpr TBHint BLACK_WIDOW_CHARM = { 0x2000009, L"If you're planning to charm a Black Widow, remember to flag your heroes away so they don't kill it." };

    static bool only_show_hints_once = false;
    static GW::HookEntry hints_entry;

    static void OnObjectiveComplete_UIMessage(GW::HookStatus*, GW::UI::UIMessage, void* wparam, void*) {
        uint32_t objective_id = *(uint32_t*)wparam;
        if (objective_id == 150
            && GW::Map::GetMapID() == GW::Constants::MapID::The_Underworld
            && GetPlayerSkillbarSkill(GW::Constants::SkillID::Charm_Animal)
            && GW::PartyMgr::GetPartyHeroCount()) {
            HintUIMessage(BLACK_WIDOW_CHARM).Show();
        }
    }
    static void OnStartMapLoad_UIMessage(GW::HookStatus*, GW::UI::UIMessage, void*, void*) {
        if (GW::Map::GetIsInCinematic() && GW::Map::GetMapID() == GW::Constants::MapID::Cinematic_Eye_Vision_A) {
            HintUIMessage(HEROS_HANDBOOK).Delay(1000);
        }
    }
    static void OnShowHint_UIMessage(GW::HookStatus* status, GW::UI::UIMessage, void* wparam, void*) {
        HintUIMessage* msg = (HintUIMessage*)wparam;
        if (std::ranges::find(hints_shown, msg->message_id) != hints_shown.end()) {
            if (only_show_hints_once)
                status->blocked = true;
        }
        else {
            hints_shown.push_back(msg->message_id);
        }
    }

    static void OnMapLoaded_UIMessage(GW::HookStatus*, GW::UI::UIMessage, void*, void*) {
        uint32_t endgame_msg_idx = 0;
        switch (GW::Map::GetMapID()) {
        case GW::Constants::MapID::Embark_Beach: {
            if (GW::Map::GetIsMapUnlocked(GW::Constants::MapID::Kaineng_Center_outpost)
                || GW::Map::GetIsMapUnlocked(GW::Constants::MapID::Lions_Arch_outpost)
                || GW::Map::GetIsMapUnlocked(GW::Constants::MapID::Kamadan_Jewel_of_Istan_outpost))
                break;
            wchar_t out[256];
            uint32_t campaign = (uint32_t)GetCharacterCampaign();
            swprintf(out, 256, EMBARK_WITHOUT_HOMELAND.message, embark_beach_campaign_npcs[campaign]);
            HintUIMessage(out, 30000, EMBARK_WITHOUT_HOMELAND.message_id | campaign).Show();
        } break;
        case GW::Constants::MapID::Droknars_Forge_cinematic:
            endgame_msg_idx = 1;
            break;
        case GW::Constants::MapID::Divine_Path:
            endgame_msg_idx = 2;
            break;
        case GW::Constants::MapID::Throne_of_Secrets:
            endgame_msg_idx = 3;
            break;
        case GW::Constants::MapID::Epilogue:
            endgame_msg_idx = 4;
            break;
        }
        if (endgame_msg_idx) {
            wchar_t out[256];
            swprintf(out, 256, ENDGAME_TROPHY.message, endgame_reward_npcs[endgame_msg_idx], endgame_reward_trophies[endgame_msg_idx]);
            HintUIMessage(out, 30000, ENDGAME_TROPHY.message_id | endgame_msg_idx).Show();
        }
        if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable && GetPlayerSkillbarSkill(GW::Constants::SkillID::Charm_Animal)) {
            HintUIMessage(CHARM_ANIMAL).Show();
        }
    }
    static void OnWriteToChatLog_UIMessage(GW::HookStatus*, GW::UI::UIMessage, void* wparam, void*) {
        GW::UI::UIChatMessage* msg = (GW::UI::UIChatMessage*)wparam;
        if (msg->channel == GW::Chat::Channel::CHANNEL_GLOBAL && wcsncmp(msg->message, L"\x8101\x4793\xfda0\xe8e2\x6844", 5) == 0) {
            HintUIMessage(HINT_HERO_EXP).Show();
        }
    }
    static void OnShowXunlaiChest_UIMessage(GW::HookStatus*, GW::UI::UIMessage, void*, void*) {
        GW::AgentLiving* chest = GW::Agents::GetTargetAsAgentLiving();
        if (chest && chest->player_number == 5001 && GW::GetDistance(GW::Agents::GetPlayer()->pos, chest->pos) < GW::Constants::Range::Nearby) {
            HintUIMessage(CHEST_CMD).Show();
        }
    }
    static void OnQuestAdded_UIMessage(GW::HookStatus*, GW::UI::UIMessage, void* wparam, void*) {
        uint32_t quest_id = *(uint32_t*)wparam; // NB: wParam is just a pointer to packet content for QuestAdded
        switch (quest_id) {
        case 56: // Adventure with an ally
            HintUIMessage(QUEST_HINT_ADVENTURE_WITH_AN_ALLY).Show();
            break;
        }
    }
    static void OnQuotedItemPrice_UIMessage(GW::HookStatus*, GW::UI::UIMessage, void* wparam, void*) {
        clock_t _now = clock();
        LastQuote* q = (LastQuote*)wparam;
        if (last_quote.item_id == q->item_id && _now - last_quoted_item_timestamp < 5 * CLOCKS_PER_SEC) {
            HintUIMessage(BULK_BUY).Show();
        }
        last_quote = *q;
        last_quoted_item_timestamp = _now;
    }
    static void OnShowPvpWindowContent_UIMessage(GW::HookStatus*, GW::UI::UIMessage, void*, void*) {
        HintUIMessage(HINT_Q9_STR_SHIELDS).Show();
    }
}

//#define PRINT_CHAT_PACKETS
void HintsModule::Initialize() {
    ToolboxModule::Initialize();
    GW::UI::RegisterUIMessageCallback(&hints_entry, GW::UI::UIMessage::kObjectiveComplete, OnObjectiveComplete_UIMessage);
    GW::UI::RegisterUIMessageCallback(&hints_entry, GW::UI::UIMessage::kMapChange, OnStartMapLoad_UIMessage);
    GW::UI::RegisterUIMessageCallback(&hints_entry, GW::UI::UIMessage::kShowHint, OnShowHint_UIMessage);
    GW::UI::RegisterUIMessageCallback(&hints_entry, GW::UI::UIMessage::kMapLoaded, OnMapLoaded_UIMessage);
    GW::UI::RegisterUIMessageCallback(&hints_entry, GW::UI::UIMessage::kWriteToChatLog, OnWriteToChatLog_UIMessage);
    GW::UI::RegisterUIMessageCallback(&hints_entry, GW::UI::UIMessage::kShowXunlaiChest, OnShowXunlaiChest_UIMessage);
    GW::UI::RegisterUIMessageCallback(&hints_entry, GW::UI::UIMessage::kQuestAdded, OnQuestAdded_UIMessage);
    GW::UI::RegisterUIMessageCallback(&hints_entry, GW::UI::UIMessage::kQuotedItemPrice, OnQuotedItemPrice_UIMessage);
    GW::UI::RegisterUIMessageCallback(&hints_entry, GW::UI::UIMessage::kPvPWindowContent, OnShowPvpWindowContent_UIMessage);
}
void HintsModule::Update(float) {
    if (!delayed_hints.empty()
        && GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading
        && GW::Agents::GetPlayer()) {
        clock_t _now = clock();
        for (auto it = delayed_hints.begin(); it != delayed_hints.end(); it++) {
            if (it->first < _now) {
                it->second->Show();
                delete it->second;
                delayed_hints.erase(it);
                break; // Skip frame
            }
        }
    }
}
void HintsModule::DrawSettingInternal() {
    ImGui::Checkbox("Only show hints once", &only_show_hints_once);
    ImGui::ShowHelp("GWToolbox will stop hint messages (e.g. 'ordering your character to attack repeatedly') from showing more than once in-game");
    if (only_show_hints_once) {
        ImGui::TextDisabled("%d hint(s) have already been shown in-game and won't be shown again", hints_shown.size());
        if (ImGui::Button("Clear cached hints"))
            hints_shown.clear();
    }
}
void HintsModule::SaveSettings(ToolboxIni* ini) {
    ToolboxModule::SaveSettings(ini);
    std::string ini_str;
    ini->SetBoolValue(Name(), VAR_NAME(only_show_hints_once), only_show_hints_once);
    ASSERT(GuiUtils::ArrayToIni(hints_shown.data(), hints_shown.size(), &ini_str));
    ini->SetValue(Name(), VAR_NAME(hints_shown), ini_str.c_str());
}
void HintsModule::LoadSettings(ToolboxIni* ini) {
    ToolboxModule::SaveSettings(ini);
    only_show_hints_once = ini->GetBoolValue(Name(), VAR_NAME(only_show_hints_once), only_show_hints_once);
    std::string ini_str = ini->GetValue(Name(), VAR_NAME(hints_shown), "");
    if (!ini_str.empty()) {
        hints_shown.resize((ini_str.size() + 1) / 9);
        ASSERT(GuiUtils::IniToArray(ini_str, hints_shown.data(), hints_shown.size()));
    }
}
