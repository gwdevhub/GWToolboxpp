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
#include <GWCA/GameEntities/Hero.h>
#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Context/WorldContext.h>

#include <ImGuiAddons.h>
#include <Utils/GuiUtils.h>

#include <Modules/HintsModule.h>
#include <Defines.h>

namespace {
    struct TBHint {
        uint32_t message_id;
        const wchar_t* message;
    };

    GW::Constants::Campaign GetCharacterCampaign()
    {
        if (GW::Map::GetIsMapUnlocked(GW::Constants::MapID::Island_of_Shehkah)) {
            return GW::Constants::Campaign::Nightfall;
        }
        if (GW::Map::GetIsMapUnlocked(GW::Constants::MapID::Ascalon_City_pre_searing)) {
            return GW::Constants::Campaign::Prophecies;
        }
        return GW::Constants::Campaign::Factions;
    }

    GW::SkillbarSkill* GetPlayerSkillbarSkill(const GW::Constants::SkillID skill_id)
    {
        const auto skillbar = GW::SkillbarMgr::GetPlayerSkillbar();
        if (!skillbar) {
            return nullptr;
        }
        return skillbar->GetSkillById(skill_id);
    }

    std::vector<uint32_t> hints_shown;
    struct HintUIMessage;
    std::vector<std::pair<clock_t, HintUIMessage*>> delayed_hints;

    struct HintUIMessage {
        uint32_t message_id = 0x10000000; // Used internally to avoid queueing more than 1 of the same hint
        wchar_t* message_encoded;
        wchar_t* scaled_message_encoded = 0;
        wchar_t* gamepad_message_encoded = 0;
        uint32_t default_image_file_id = 0; // e.g. mouse imaage, light bulb, exclamation mark
        uint32_t scaled_image_file_id = 0;
        uint32_t gamepad_image_file_id = 0;
        uint32_t message_timeout_ms = 15000;
        uint32_t style_bitmap = 0x12; // 0x18 = hint with left padding

        HintUIMessage(const wchar_t* message, const uint32_t duration = 30000, uint32_t _message_id = 0)
        {
            ASSERT(message);
            size_t strlen = (wcslen(message) + 1) * sizeof(wchar_t);
            if (message[0] == 0x108) {
                message_encoded = new wchar_t[strlen];
                // Already encoded
                wcscpy(message_encoded, message);
            }
            else {
                strlen += 3 * sizeof(wchar_t);
                message_encoded = new wchar_t[strlen];
                swprintf(message_encoded, strlen, L"\x108\x107%s\x1", message);
            }
            if (!_message_id) {
                _message_id = (uint32_t)message;
            }
            message_id = _message_id;
            message_timeout_ms = duration;
        }

        HintUIMessage(const TBHint& hint)
            : HintUIMessage(hint.message, 30000, hint.message_id) { }

        ~HintUIMessage()
        {
            if (message_encoded)
                delete[] message_encoded;
            if (scaled_message_encoded) 
                delete[] message_encoded;
            if (gamepad_message_encoded) 
                delete[] message_encoded;
        }

        void Show()
        {
            SendUIMessage(GW::UI::UIMessage::kShowHint, this);
        }

        void Delay(const clock_t delay_ms) const
        {
            delayed_hints.push_back(std::pair(clock() + delay_ms, new HintUIMessage(message_encoded, message_timeout_ms, message_id)));
        }
    };
    static_assert(sizeof(HintUIMessage) == 0x24);

    struct LastQuote {
        uint32_t item_id = 0;
        uint32_t price = 0;
    } last_quote;

    clock_t last_quoted_item_timestamp = 0;

    constexpr const wchar_t* embark_beach_campaign_npcs[] = {
        L"",
        L"\x8102\x6F1E\xE846\xFFBF\x57E0", // Kenai [Tyrian Travel]
        L"\x8102\x6F05\xE3C3\xBF66\x234C", // Shirayuki [Canthan Travel]
        L"\x8102\x6F1E\xE846\xFFBF\x57E0"  // Zinshao [Elonian Travel]
    };
    constexpr const wchar_t* endgame_reward_npcs[] = {
        L"",
        L"\x399E\x8A19\xC3B6\x2FE4",          // King Jalis (Droks Explorable)
        L"\x108\x107" "Suun\x1",              // Suun (Divine Path) TODO: Encoded version of this name!
        L"\x108\x107" "Keeper of Secrets\x1", // Keeper of Secrets (Throne of secrets) TODO: Encoded version of this name!
        L"\x108\x107" "Droknar\x1",
    };
    constexpr const wchar_t* endgame_reward_trophies[] = {
        L"",
        L"\x108\x107" "Deldrimor Talisman" "\x1",  // King Jalis (Droks Explorable)
        L"\x108\x107" "Amulet of the Mists" "\x1", // Suun (Divine Path) TODO: Encoded version of this name!
        L"\x108\x107" "Book of Secrets" "\x1",     // Keeper of Secrets (Throne of secrets) TODO: Encoded version of this name!
        L"\x108\x107" "Droknar's Key" "\x1",
    };

    constexpr TBHint HINT_Q9_STR_SHIELDS = {0x20000001, L"PvP 力量盾牌在您不满足需求时提供 9 点护甲，因此除非您能在其他属性上满足需求，否则请使用力量盾牌。"};
    constexpr TBHint HINT_HERO_EXP = {0x20000002, L"队伍中的英雄会从任务中获得经验，因此记得在接受任务奖励时带上您的低等级英雄。"};
    constexpr TBHint CHEST_CMD = {0x20000003, L"在聊天中输入 '/chest' 可在前哨站的任何位置打开您的迅雷仓库，无需每次都跑到仓库处。"};
    constexpr TBHint BULK_BUY = {0x20000004, L"在请求报价时按住 Ctrl 键可批量购买或出售商品。"};
    constexpr TBHint EMBARK_WITHOUT_HOMELAND = {0x20001000, L"要从启程海滩返回您来的地方，请与 \x1\x2%s\x2\x108\x107 对话或使用 '/tb travel' 聊天命令。"};
    constexpr TBHint ENDGAME_TROPHY = {
        0x20002000, L"与 \x1\x2%s\x2\x108\x107 对话可获得一个 \x1\x2%s\x2\x108\x107。如果您将其出售给其他玩家，可以卖很多钱，因此与其用它兑换武器，不如在 https://kamadan.gwtoolbox.com 上搜索买家。"};
    constexpr TBHint QUEST_HINT_ADVENTURE_WITH_AN_ALLY = {
        0x20000005, L"如果您没有朋友可以一起完成这个任务，可以尝试将阿斯卡隆城（美洲英语分区）中的所有人添加到您的队伍中。一定会有人乐意帮助您的。"};
    constexpr TBHint NOLANI_ACADEMY_SHORTCUT = {0x20000006, L"向右转并向南走击杀 Bonfaaz Burntfur。这是通往终点的更快捷径——Prince Rurik 自己会没事的。"};
    constexpr TBHint CHARM_ANIMAL = {0x20000007, L"驯服动物只在驯服宠物时需要。请考虑携带舒适动物代替。"};
    constexpr TBHint HEROS_HANDBOOK = {0x2000008, L"在北方之眼与 Ascalon 的 Gedrel 对话可获得英雄手册和大师地下城指南。"};
    constexpr TBHint BLACK_WIDOW_CHARM = {0x2000009, L"如果您计划驯服黑寡妇，记得让您的英雄远离，以免它们杀死它。"};
    constexpr TBHint JUNUNDU_HERO_AVOID_COMBAT = {0x200000A, L"您的英雄中有一个或多个被设置为'回避战斗'。在您处于 Junundu 形态时，处于此模式的英雄不会战斗。"};

    HintsModule::Settings settings;
    GW::HookEntry hints_entry;

    void OnEffectAdd_UIMessage(GW::HookStatus*, GW::UI::UIMessage, void* wparam, void*)
    {
        const auto packet = static_cast<GW::UI::UIPacket::kEffectAdd*>(wparam);
        if (!packet || !packet->effect || packet->effect->skill_id != GW::Constants::SkillID::Desert_Wurm_disguise) {
            return;
        }
        const auto me = GW::Agents::GetControlledCharacter();
        if (!me || packet->agent_id != me->agent_id) {
            return;
        }
        const auto world = GW::GetWorldContext();
        if (!world) {
            return;
        }
        for (const auto& flag : world->hero_flags) {
            if (flag.hero_behavior == GW::HeroBehavior::AvoidCombat) {
                HintUIMessage(JUNUNDU_HERO_AVOID_COMBAT).Show();
                return;
            }
        }
    }

    void OnObjectiveComplete_UIMessage(GW::HookStatus*, GW::UI::UIMessage, void* wparam, void*)
    {
        const uint32_t objective_id = *static_cast<uint32_t*>(wparam);
        if (objective_id == 150
            && GW::Map::GetMapID() == GW::Constants::MapID::The_Underworld
            && GetPlayerSkillbarSkill(GW::Constants::SkillID::Charm_Animal)
            && GW::PartyMgr::GetPartyHeroCount()) {
            HintUIMessage(BLACK_WIDOW_CHARM).Show();
        }
    }

    void OnStartMapLoad_UIMessage(GW::HookStatus*, GW::UI::UIMessage, void*, void*)
    {
        if (GW::Map::GetIsInCinematic() && GW::Map::GetMapID() == GW::Constants::MapID::Cinematic_Eye_Vision_A) {
            HintUIMessage(HEROS_HANDBOOK).Delay(1000);
        }
    }

    void OnShowHint_UIMessage(GW::HookStatus* status, GW::UI::UIMessage, void* wparam, void*)
    {
        const auto msg = static_cast<HintUIMessage*>(wparam);
        if (std::ranges::contains(hints_shown, msg->message_id)) {
            if (settings.only_show_hints_once) {
                status->blocked = true;
            }
        }
        else {
            hints_shown.push_back(msg->message_id);
        }
    }

    void OnMapLoaded_UIMessage(GW::HookStatus*, GW::UI::UIMessage, void*, void*)
    {
        uint32_t endgame_msg_idx = 0;
        switch (GW::Map::GetMapID()) {
            case GW::Constants::MapID::Embark_Beach: {
                if (GW::Map::GetIsMapUnlocked(GW::Constants::MapID::Kaineng_Center_outpost)
                    || GW::Map::GetIsMapUnlocked(GW::Constants::MapID::Lions_Arch_outpost)
                    || GW::Map::GetIsMapUnlocked(GW::Constants::MapID::Kamadan_Jewel_of_Istan_outpost)) {
                    break;
                }
                wchar_t out[256];
                const auto campaign = std::to_underlying(GetCharacterCampaign());
                swprintf(out, 256, EMBARK_WITHOUT_HOMELAND.message, embark_beach_campaign_npcs[campaign]);
                HintUIMessage(out, 30000, EMBARK_WITHOUT_HOMELAND.message_id | campaign).Show();
            }
            break;
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

    void OnWriteToChatLog_UIMessage(GW::HookStatus*, GW::UI::UIMessage, void* wparam, void*)
    {
        const auto msg = static_cast<GW::UI::UIChatMessage*>(wparam);
        if (msg->channel == GW::Chat::Channel::CHANNEL_GLOBAL && wcsncmp(msg->message, L"\x8101\x4793\xfda0\xe8e2\x6844", 5) == 0) {
            HintUIMessage(HINT_HERO_EXP).Show();
        }
    }

    void OnShowXunlaiChest_UIMessage(GW::HookStatus*, GW::UI::UIMessage, void*, void*)
    {
        const auto chest = GW::Agents::GetTargetAsAgentLiving();
        const auto me = chest ? GW::Agents::GetControlledCharacter() : nullptr;
        if (me && chest->player_number == 5001 && GetDistance(me->pos, chest->pos) < GW::Constants::Range::Nearby) {
            HintUIMessage(CHEST_CMD).Show();
        }
    }

    void OnQuestAdded_UIMessage(GW::HookStatus*, GW::UI::UIMessage, void* wparam, void*)
    {
        const uint32_t quest_id = *static_cast<uint32_t*>(wparam); // NB: wParam is just a pointer to packet content for QuestAdded
        switch (quest_id) {
            case 56: // Adventure with an ally
                HintUIMessage(QUEST_HINT_ADVENTURE_WITH_AN_ALLY).Show();
                break;
        }
    }

    void OnQuotedItemPrice_UIMessage(GW::HookStatus*, GW::UI::UIMessage, void* wparam, void*)
    {
        const clock_t _now = clock();
        const auto q = static_cast<LastQuote*>(wparam);
        if (last_quote.item_id == q->item_id && _now - last_quoted_item_timestamp < 5 * CLOCKS_PER_SEC) {
            HintUIMessage(BULK_BUY).Show();
        }
        last_quote = *q;
        last_quoted_item_timestamp = _now;
    }

    void OnShowPvpWindowContent_UIMessage(GW::HookStatus*, GW::UI::UIMessage, void*, void*)
    {
        HintUIMessage(HINT_Q9_STR_SHIELDS).Show();
    }
}

//#define PRINT_CHAT_PACKETS
void HintsModule::Initialize()
{
    ToolboxModule::Initialize();
    SettingsRegistry::Register(this, settings);
    RegisterUIMessageCallback(&hints_entry, GW::UI::UIMessage::kEffectAdd, OnEffectAdd_UIMessage);
    RegisterUIMessageCallback(&hints_entry, GW::UI::UIMessage::kObjectiveComplete, OnObjectiveComplete_UIMessage);
    RegisterUIMessageCallback(&hints_entry, GW::UI::UIMessage::kMapChange, OnStartMapLoad_UIMessage);
    RegisterUIMessageCallback(&hints_entry, GW::UI::UIMessage::kShowHint, OnShowHint_UIMessage);
    RegisterUIMessageCallback(&hints_entry, GW::UI::UIMessage::kMapLoaded, OnMapLoaded_UIMessage);
    RegisterUIMessageCallback(&hints_entry, GW::UI::UIMessage::kWriteToChatLog, OnWriteToChatLog_UIMessage);
    RegisterUIMessageCallback(&hints_entry, GW::UI::UIMessage::kShowXunlaiChest, OnShowXunlaiChest_UIMessage);
    RegisterUIMessageCallback(&hints_entry, GW::UI::UIMessage::kQuestAdded, OnQuestAdded_UIMessage);
    RegisterUIMessageCallback(&hints_entry, GW::UI::UIMessage::kVendorQuote, OnQuotedItemPrice_UIMessage);
    RegisterUIMessageCallback(&hints_entry, GW::UI::UIMessage::kPvPWindowContent, OnShowPvpWindowContent_UIMessage);
}

void HintsModule::Update(float)
{
    if (!delayed_hints.empty()
        && GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading
        && GW::Agents::GetControlledCharacter()) {
        const clock_t _now = clock();
        for (auto it = delayed_hints.begin(); it != delayed_hints.end(); ++it) {
            if (it->first < _now) {
                it->second->Show();
                delete it->second;
                delayed_hints.erase(it);
                break; // Skip frame
            }
        }
    }
}

void HintsModule::DrawSettingsInternal()
{
    ImGui::CheckboxWithHelp("每个提示只显示一次", &settings.only_show_hints_once, "工具箱将阻止提示消息（例如\"命令您的角色重复攻击\"）在游戏中多次显示");
    if (settings.only_show_hints_once) {
        ImGui::TextDisabled("已有 %d 条提示在游戏中显示过，将不会再次显示", hints_shown.size());
        if (ImGui::Button("清除缓存的提示")) {
            hints_shown.clear();
        }
    }
}

void HintsModule::SaveSettings(SettingsDoc& doc)
{
    ToolboxModule::SaveSettings(doc);
    doc.SetStruct(Name(), settings);
    doc.Set(Name(), VAR_NAME(hints_shown), hints_shown);
}

void HintsModule::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxModule::LoadSettings(doc, legacy);
    doc.GetStruct(Name(), settings);
    if (!doc.Get(Name(), VAR_NAME(hints_shown), hints_shown) && legacy) {
        const std::string ini_str = legacy->GetValue(Name(), VAR_NAME(hints_shown), "");
        if (!ini_str.empty()) {
            hints_shown.resize((ini_str.size() + 1) / 9);
            ASSERT(GuiUtils::IniToArray(ini_str, hints_shown.data(), hints_shown.size()));
        }
    }
}
