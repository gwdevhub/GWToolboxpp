#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/ItemIDs.h>
#include <GWCA/Constants/Maps.h>
#include <GWCA/Constants/Skills.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Item.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Skill.h>
#include <GWCA/GameEntities/Title.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/MerchantMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <GWCA/Utilities/Hook.h>

#include <ImGuiAddons.h>
#include <Logger.h>
#include <Modules/InventoryItem.h>
#include <Modules/Resources.h>
#include <Utils/SettingsDoc.h>
#include <Utils/SettingsRegistry.h>
#include <Utils/ToolboxUtils.h>
#include <Windows/CompletionWindow.h>
#include <Windows/PlaystyleRestrictionsWindow.h>

using GW::Constants::Campaign;
using GW::Constants::HeroID;
using GW::Constants::MapID;
using GW::Constants::TitleID;
using Profile = PlaystyleRestrictionsWindow::Profile;

namespace {
    constexpr char profile_filename[] = "playstyle_restrictions.json";

    Profile profile;
    PlaystyleRestrictionsWindow::Settings settings;
    GW::HookEntry hook_entry;

    // Recomputed on map load / profile change: the mission-order scan walks every map info,
    // far too heavy for the button-state message that reads it.
    bool mission_entry_blocked = false;

    // Blocking a template load leaves the skill window showing a bar the server never got, so the
    // skillbar rules are reactive: keep the last bar that satisfied them, and put it back on a breach.
    std::unordered_map<uint32_t, GW::SkillbarMgr::SkillTemplate> last_legal_skillbars;
    bool restoring_skillbar = false;

    bool Contains(const std::vector<uint32_t>& haystack, const uint32_t needle)
    {
        return std::ranges::find(haystack, needle) != haystack.end();
    }

    // ==== Static game data ====

    const char* CampaignName(const Campaign campaign)
    {
        switch (campaign) {
            case Campaign::Prophecies: return "Prophecies";
            case Campaign::Factions: return "Factions";
            case Campaign::Nightfall: return "Nightfall";
            case Campaign::EyeOfTheNorth: return "Eye of the North";
            case Campaign::BonusMissionPack: return "Bonus Mission Pack";
            default: return "Core";
        }
    }

    MapID FinalMission(const Campaign campaign)
    {
        switch (campaign) {
            case Campaign::Prophecies: return MapID::Hells_Precipice;
            case Campaign::Factions: return MapID::Imperial_Sanctum_outpost_mission;
            case Campaign::Nightfall: return MapID::Abaddons_Gate;
            case Campaign::EyeOfTheNorth: return MapID::A_Time_for_Heroes_mission;
            default: return MapID::None;
        }
    }

    const std::vector<TitleID>& CampaignTitles(const Campaign campaign)
    {
        static const std::vector<TitleID> none{};
        static const std::vector<TitleID> proph{TitleID::ProtectorTyria, TitleID::GuardianTyria, TitleID::VanquisherTyria, TitleID::SkillHunterTyria, TitleID::TyrianCarto};
        static const std::vector<TitleID> factions{TitleID::ProtectorCantha, TitleID::GuardianCantha, TitleID::VanquisherCantha, TitleID::SkillHunterCantha, TitleID::CanthanCarto};
        static const std::vector<TitleID> nightfall{TitleID::ProtectorElona, TitleID::GuardianElona, TitleID::VanquisherElona, TitleID::SkillHunterElona, TitleID::ElonianCarto, TitleID::Sunspear, TitleID::Lightbringer};
        static const std::vector<TitleID> eotn{TitleID::Asuran, TitleID::Deldrimor, TitleID::Vanguard, TitleID::Norn, TitleID::MasterOfTheNorth};
        switch (campaign) {
            case Campaign::Prophecies: return proph;
            case Campaign::Factions: return factions;
            case Campaign::Nightfall: return nightfall;
            case Campaign::EyeOfTheNorth: return eotn;
            default: return none;
        }
    }

    // The game has no hero->campaign field; heroes not listed here (mercenaries) are core.
    Campaign HeroCampaign(const HeroID hero_id)
    {
        switch (hero_id) {
            case HeroID::Norgu: case HeroID::Goren: case HeroID::Tahlkora: case HeroID::MasterOfWhispers:
            case HeroID::AcolyteJin: case HeroID::Koss: case HeroID::Dunkoro: case HeroID::AcolyteSousuke:
            case HeroID::Melonni: case HeroID::ZhedShadowhoof: case HeroID::GeneralMorgahn: case HeroID::MargridTheSly:
            case HeroID::Zenmai: case HeroID::Olias: case HeroID::Razah:
                return Campaign::Nightfall;
            case HeroID::KeiranThackeray: case HeroID::Jora: case HeroID::PyreFierceshot: case HeroID::Anton:
            case HeroID::Livia: case HeroID::Hayda: case HeroID::Kahmu: case HeroID::Gwen:
            case HeroID::Xandra: case HeroID::Vekk: case HeroID::Ogden:
                return Campaign::EyeOfTheNorth;
            case HeroID::Miku: case HeroID::ZeiRi:
                return Campaign::Factions;
            case HeroID::Devona: case HeroID::GhostOfAlthea:
                return Campaign::Prophecies;
            case HeroID::MOX:
                return Campaign::BonusMissionPack;
            default:
                return Campaign::Core;
        }
    }

    // ==== Progression state ====

    bool IsMissionComplete(const MapID map_id)
    {
        auto check = profile.require_hard_mode ? CompletionCheck::Both : CompletionCheck::NormalMode;
        if (!profile.require_mission_bonuses) {
            check = static_cast<CompletionCheck>(check | CompletionCheck::PrimaryOnly);
        }
        return CompletionWindow::IsAreaComplete(map_id, check);
    }

    bool IsCampaignComplete(const Campaign campaign)
    {
        const auto final_mission = FinalMission(campaign);
        return final_mission != MapID::None && IsMissionComplete(final_mission);
    }

    bool AreCampaignTitlesMaxed(const Campaign campaign)
    {
        for (const auto title_id : CampaignTitles(campaign)) {
            const auto title = GW::PlayerMgr::GetTitleTrack(title_id);
            if (!title || title->current_title_tier_index < title->max_title_tier_index) {
                return false;
            }
        }
        return true;
    }

    bool IsCampaignUnlocked(const Campaign campaign)
    {
        if (campaign == Campaign::Core || campaign == Campaign::BonusMissionPack) {
            return true;
        }
        const auto& order = profile.campaign_order;
        const auto found = std::ranges::find(order, static_cast<uint32_t>(campaign));
        if (found == order.end() || found == order.begin()) {
            return true;
        }
        const auto previous = static_cast<Campaign>(*std::prev(found));
        if (!IsCampaignComplete(previous)) {
            return false;
        }
        return !profile.require_campaign_titles_maxed || AreCampaignTitlesMaxed(previous);
    }

    bool HasRequiredPreSearingTitles()
    {
        return std::ranges::all_of(profile.required_presearing_titles, [](const auto& required) {
            const auto title = GW::PlayerMgr::GetTitleTrack(static_cast<TitleID>(required.title_id));
            return title && title->current_title_tier_index >= required.min_tier;
        });
    }

    // Every mission of this campaign that comes earlier in the story must already be done.
    bool HasSkippedMissions(const GW::AreaInfo* area)
    {
        for (auto map_id = static_cast<MapID>(0); map_id < MapID::Count; map_id = static_cast<MapID>(static_cast<uint32_t>(map_id) + 1)) {
            const auto other = GW::Map::GetMapInfo(map_id);
            if (!other || other->campaign != area->campaign || !other->mission_chronology) {
                continue;
            }
            if (other->mission_chronology < area->mission_chronology && !IsMissionComplete(map_id)) {
                return true;
            }
        }
        return false;
    }

    // Reason the given map is off-limits, or nullptr if it's allowed. Static storage: caller logs it immediately.
    const char* GetMapBlockReason(const MapID map_id)
    {
        static char reason[128];
        const auto area = GW::Map::GetMapInfo(map_id);
        if (!area) {
            return nullptr;
        }
        if (profile.gate_presearing_exit && GW::Map::IsPreSearing() && !GW::Map::IsPreSearing(map_id) && !HasRequiredPreSearingTitles()) {
            return "your profile requires more titles before leaving pre-Searing";
        }
        if (profile.enforce_campaign_order && !IsCampaignUnlocked(area->campaign)) {
            snprintf(reason, sizeof(reason), "%s is not unlocked yet under your profile's campaign order", CampaignName(area->campaign));
            return reason;
        }
        return nullptr;
    }

    // ==== Item helpers ====

    bool IsMajorOrSuperiorRune(const InventoryItem* item)
    {
        // Attribute runes carry their tier in the low byte of mod 0x21e8; vigor has one mod id per tier.
        if (const auto mod = item->GetModifier(0x21e8)) {
            return mod->arg2() >= 2;
        }
        return item->GetModifier(0x27e9) || item->GetModifier(0x27ea);
    }

    bool IsEliteTome(const InventoryItem* item)
    {
        const auto mod = item->GetModifier(0x2788);
        const auto use_id = mod ? mod->arg() : 0;
        return use_id >= 26 && use_id < 36;
    }

    // Reason this item can't be used, or nullptr.
    const char* GetItemUseBlockReason(const GW::Item* raw)
    {
        const auto item = reinterpret_cast<const InventoryItem*>(raw);
        if (Contains(profile.blocked_item_model_ids, item->model_id)) {
            return "that item is on your profile's blocked list";
        }
        if (profile.lockpicks_hard_mode_only && item->model_id == static_cast<uint32_t>(GW::Constants::ItemID::Lockpick) && !GW::PartyMgr::GetIsPartyInHardMode()) {
            return "your profile only allows lockpicks in Hard Mode";
        }
        if (profile.block_elite_tomes && IsEliteTome(item)) {
            return "your profile blocks elite tomes";
        }
        if (profile.block_scrolls && item->type == GW::Constants::ItemType::Scroll) {
            return "your profile blocks scrolls";
        }
        if (profile.block_consumables && item->type == GW::Constants::ItemType::Usable) {
            return "your profile blocks consumables";
        }
        return nullptr;
    }

    // ==== Skill helpers ====

    // Reason this bar isn't allowed for this agent, or nullptr. Static storage: caller uses it immediately.
    const char* GetSkillbarViolation(const uint32_t agent_id, const GW::SkillbarMgr::SkillTemplate& skill_template)
    {
        static char reason[128];
        if (agent_id == GW::Agents::GetControlledCharacterId()) {
            if (profile.lock_skillbar) {
                return "your profile locks the skillbar";
            }
        }
        else if (profile.lock_hero_skillbars) {
            return "your profile locks hero skillbars";
        }
        auto attribute = GW::Constants::AttributeByte::None;
        for (const auto skill_id : skill_template.skills) {
            const auto skill = GW::SkillbarMgr::GetSkillConstantData(skill_id);
            if (!skill) {
                continue;
            }
            if (profile.restrict_skills_to_unlocked_campaigns && !IsCampaignUnlocked(skill->campaign)) {
                snprintf(reason, sizeof(reason), "that bar uses %s skills, which aren't unlocked yet", CampaignName(skill->campaign));
                return reason;
            }
            if (profile.single_attribute_line && skill->attribute != GW::Constants::AttributeByte::None) {
                if (attribute != GW::Constants::AttributeByte::None && attribute != skill->attribute) {
                    return "your profile allows only one attribute line";
                }
                attribute = skill->attribute;
            }
        }
        return nullptr;
    }

    bool AnySkillbarRuleEnabled()
    {
        return settings.enforce && (profile.lock_skillbar || profile.lock_hero_skillbars
            || profile.restrict_skills_to_unlocked_campaigns || profile.single_attribute_line);
    }

    bool SameSkillbar(const GW::SkillbarMgr::SkillTemplate& a, const GW::SkillbarMgr::SkillTemplate& b)
    {
        if (a.primary != b.primary || a.secondary != b.secondary || a.attributes_count != b.attributes_count) {
            return false;
        }
        if (!std::ranges::equal(a.skills, b.skills)) {
            return false;
        }
        for (uint32_t i = 0; i < a.attributes_count && i < _countof(a.attribute_ids); i++) {
            if (a.attribute_ids[i] != b.attribute_ids[i] || a.attribute_values[i] != b.attribute_values[i]) {
                return false;
            }
        }
        return true;
    }

    void RestoreSkillbar(const uint32_t agent_id, const char* reason)
    {
        const auto found = last_legal_skillbars.find(agent_id);
        if (!AnySkillbarRuleEnabled() || found == last_legal_skillbars.end()) {
            return;
        }
        restoring_skillbar = true;
        GW::SkillbarMgr::LoadSkillTemplate(agent_id, found->second);
        restoring_skillbar = false;
        Log::Warning("[Playstyle Restrictions] Reverted: %s", reason);
    }

    // Called once the client has applied a change: a legal bar becomes the new fallback, an
    // illegal one goes back to the last legal bar we saw.
    void EnforceSkillbar(const uint32_t agent_id)
    {
        const auto found = last_legal_skillbars.find(agent_id);
        if (!AnySkillbarRuleEnabled() || found == last_legal_skillbars.end()) {
            return;
        }
        GW::SkillbarMgr::SkillTemplate current;
        if (!GW::SkillbarMgr::GetSkillTemplate(agent_id, current)) {
            return;
        }
        const auto reason = GetSkillbarViolation(agent_id, current);
        if (!reason) {
            found->second = current;
            return;
        }
        // Equal means there's nothing better to go back to - the fallback itself breaks the rule,
        // which happens when a rule is switched on mid-session. Bailing also stops a revert loop.
        if (!SameSkillbar(current, found->second)) {
            RestoreSkillbar(agent_id, reason);
        }
    }

    // ==== Enforcement ====

    void Deny(GW::HookStatus* status, const char* reason)
    {
        status->blocked = true;
        Log::Warning("[Playstyle Restrictions] Blocked: %s", reason);
    }

    bool IsMissionEntryBlocked()
    {
        const auto map_id = GW::Map::GetMapID();
        const auto area = GW::Map::GetMapInfo(map_id);
        if (!area || !area->GetHasEnterButton()) {
            return false;
        }
        const auto target = area->GetHasMissionMapsTo() ? static_cast<MapID>(area->mission_maps_to) : map_id;
        if (GetMapBlockReason(target)) {
            return true;
        }
        if (profile.block_mission_skipping && HasSkippedMissions(area)) {
            return true;
        }
        if (profile.min_level_for_gated_areas && area->min_level) {
            const auto me = GW::Agents::GetControlledCharacter();
            if (me && me->level < profile.min_level_for_gated_areas) {
                return true;
            }
        }
        return false;
    }

    void OnUIMessage(GW::HookStatus* status, const GW::UI::UIMessage message_id, void* wparam, void*)
    {
        if (!settings.enforce || status->blocked) {
            return;
        }
        switch (message_id) {
            case GW::UI::UIMessage::kTravel: {
                const auto destination = static_cast<MapID*>(wparam);
                if (const auto reason = destination ? GetMapBlockReason(*destination) : nullptr) {
                    Deny(status, reason);
                }
            }
            break;
            case GW::UI::UIMessage::kDisableEnterMissionBtn: {
                // The game re-enables the button on every party change; keep it disabled while gated.
                if (wparam == nullptr && mission_entry_blocked) {
                    status->blocked = true;
                }
            }
            break;
            case GW::UI::UIMessage::kShowXunlaiChest: {
                if (profile.block_xunlai_chest) {
                    Deny(status, "your profile blocks the Xunlai Chest");
                }
            }
            break;
            case GW::UI::UIMessage::kPartyAddHero: {
                const auto member = wparam ? static_cast<GW::HeroPartyMember**>(wparam)[1] : nullptr;
                const auto me = GW::Agents::GetControlledCharacter();
                if (!member || !me || member->owner_player_id != me->login_number) {
                    break;
                }
                const auto hero_id = member->hero_id;
                if (Contains(profile.hero_allow_list, static_cast<uint32_t>(hero_id))) {
                    break;
                }
                if (Contains(profile.hero_block_list, static_cast<uint32_t>(hero_id))) {
                    Deny(status, "that hero is on your profile's blocked list");
                    break;
                }
                if (profile.max_party_heroes && GW::PartyMgr::GetPartyHeroCount() >= profile.max_party_heroes) {
                    Deny(status, "your profile caps the hero roster");
                    break;
                }
                if (profile.restrict_heroes_to_unlocked_campaigns && !IsCampaignUnlocked(HeroCampaign(hero_id))) {
                    Deny(status, "that hero's campaign isn't unlocked yet");
                }
            }
            break;
            case GW::UI::UIMessage::kSendUseItem: {
                const auto packet = static_cast<GW::UI::UIPacket::kSendUseItem*>(wparam);
                const auto item = packet ? GW::Items::GetItemById(packet->item_id) : nullptr;
                if (const auto reason = item ? GetItemUseBlockReason(item) : nullptr) {
                    Deny(status, reason);
                }
            }
            break;
            case GW::UI::UIMessage::kEquipItem: {
                const auto item = wparam ? GW::Items::GetItemById(*static_cast<uint32_t*>(wparam)) : nullptr;
                if (item && Contains(profile.blocked_item_model_ids, item->model_id)) {
                    Deny(status, "that item is on your profile's blocked list");
                }
            }
            break;
            case GW::UI::UIMessage::kSendMerchantTransactItem: {
                const auto packet = static_cast<GW::UI::UIPacket::kSendMerchantTransactItem*>(wparam);
                if (!packet || packet->type == GW::Merchant::TransactionType::MerchantSell || packet->type == GW::Merchant::TransactionType::TraderSell) {
                    break;
                }
                for (uint32_t i = 0; i < packet->recv.item_count; i++) {
                    const auto raw = GW::Items::GetItemById(packet->recv.item_ids[i]);
                    if (!raw) {
                        continue;
                    }
                    if (profile.block_zaishen_coin_purchase && raw->GetIsZcoin()) {
                        Deny(status, "your profile blocks buying Zaishen Coins");
                        return;
                    }
                    const auto item = reinterpret_cast<const InventoryItem*>(raw);
                    if (profile.block_purchased_runes && raw->type == GW::Constants::ItemType::Rune_Mod && IsMajorOrSuperiorRune(item)) {
                        Deny(status, "your profile only allows major/superior runes picked up as loot");
                        return;
                    }
                    if (Contains(profile.blocked_item_model_ids, raw->model_id)) {
                        Deny(status, "that item is on your profile's blocked list");
                        return;
                    }
                }
            }
            break;
            case GW::UI::UIMessage::kUpdateSkillbar: {
                const auto agent_id = wparam ? *static_cast<uint32_t*>(wparam) : 0;
                if (agent_id && !restoring_skillbar) {
                    GW::GameThread::Enqueue([agent_id] { EnforceSkillbar(agent_id); });
                }
            }
            break;
            case GW::UI::UIMessage::kSendLoadSkillTemplate: {
                const auto packet = static_cast<GW::UI::UIPacket::kSendLoadSkillTemplate*>(wparam);
                if (!packet || !packet->skill_template || restoring_skillbar) {
                    break;
                }
                const auto found = last_legal_skillbars.find(packet->agent_id);
                if (found == last_legal_skillbars.end() || SameSkillbar(*packet->skill_template, found->second)) {
                    break;
                }
                // The client hasn't applied this yet, so judge the incoming bar rather than the live one;
                // kUpdateSkillbar alone would miss a load that only moves attribute points.
                if (const auto reason = GetSkillbarViolation(packet->agent_id, *packet->skill_template)) {
                    GW::GameThread::Enqueue([agent_id = packet->agent_id, why = std::string(reason)] {
                        RestoreSkillbar(agent_id, why.c_str());
                    });
                }
            }
            break;
            default:
                break;
        }
    }

    void RefreshEnterMissionButton()
    {
        mission_entry_blocked = settings.enforce && IsMissionEntryBlocked();
        if (mission_entry_blocked) {
            GW::UI::SendUIMessage(GW::UI::UIMessage::kDisableEnterMissionBtn, reinterpret_cast<void*>(1));
        }
    }

    // ==== Profile persistence ====

    bool WriteProfile(const std::filesystem::path& path)
    {
        std::string buffer;
        if (glz::write<glz::opts{.prettify = true}>(profile, buffer)) {
            return false;
        }
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file) {
            return false;
        }
        file.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        return true;
    }

    bool ReadProfile(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            return false;
        }
        const std::string buffer{std::istreambuf_iterator(file), {}};
        Profile loaded;
        if (glz::read<glz::opts{.error_on_unknown_keys = false}>(loaded, buffer)) {
            return false;
        }
        profile = std::move(loaded);
        return true;
    }

    // ==== UI ====

    void DrawCampaignOrder()
    {
        for (size_t i = 0; i < profile.campaign_order.size(); i++) {
            const auto campaign = static_cast<Campaign>(profile.campaign_order[i]);
            ImGui::Text("%d. %s", static_cast<int>(i) + 1, CampaignName(campaign));
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60.f);
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::ArrowButton("up", ImGuiDir_Up) && i > 0) {
                std::swap(profile.campaign_order[i], profile.campaign_order[i - 1]);
            }
            ImGui::SameLine();
            if (ImGui::ArrowButton("down", ImGuiDir_Down) && i + 1 < profile.campaign_order.size()) {
                std::swap(profile.campaign_order[i], profile.campaign_order[i + 1]);
            }
            ImGui::PopID();
        }
    }

    void DrawIdList(const char* label, std::vector<uint32_t>& ids, const char* help)
    {
        std::string joined;
        for (const auto id : ids) {
            joined += (joined.empty() ? "" : ",") + std::to_string(id);
        }
        if (ImGui::InputText(label, joined, 256)) {
            ids.clear();
            for (const char* p = joined.c_str(); *p;) {
                char* end = nullptr;
                const auto parsed = strtoul(p, &end, 10);
                if (end == p) {
                    break;
                }
                ids.push_back(static_cast<uint32_t>(parsed));
                p = *end ? end + 1 : end;
            }
        }
        ImGui::ShowHelp(help);
    }
}

void PlaystyleRestrictionsWindow::Initialize()
{
    ToolboxWindow::Initialize();
    SettingsRegistry::Register(this, settings);

    constexpr GW::UI::UIMessage watched[] = {
        GW::UI::UIMessage::kTravel,
        GW::UI::UIMessage::kDisableEnterMissionBtn,
        GW::UI::UIMessage::kShowXunlaiChest,
        GW::UI::UIMessage::kPartyAddHero,
        GW::UI::UIMessage::kSendUseItem,
        GW::UI::UIMessage::kEquipItem,
        GW::UI::UIMessage::kSendMerchantTransactItem,
        GW::UI::UIMessage::kSendLoadSkillTemplate,
        GW::UI::UIMessage::kUpdateSkillbar
    };
    for (const auto message_id : watched) {
        GW::UI::RegisterUIMessageCallback(&hook_entry, message_id, OnUIMessage);
    }
    GW::UI::RegisterUIMessageCallback(&hook_entry, GW::UI::UIMessage::kMapLoaded, [](GW::HookStatus*, GW::UI::UIMessage, void*, void*) {
        last_legal_skillbars.clear();
        RefreshEnterMissionButton();
    }, 0x8000);
}

void PlaystyleRestrictionsWindow::Terminate()
{
    ToolboxWindow::Terminate();
    GW::UI::RemoveUIMessageCallback(&hook_entry);
}

// Tracks the player's and your heroes' bars, so a later change has something to revert to.
void PlaystyleRestrictionsWindow::Update(float)
{
    if (!AnySkillbarRuleEnabled()) {
        last_legal_skillbars.clear();
        return;
    }
    const auto me = GW::Agents::GetControlledCharacter();
    const auto party = GW::PartyMgr::GetPartyInfo();
    if (!me || !party) {
        return;
    }
    std::vector<uint32_t> watched{me->agent_id};
    for (const auto& hero : party->heroes) {
        if (hero.owner_player_id == me->login_number) {
            watched.push_back(hero.agent_id);
        }
    }
    for (const auto agent_id : watched) {
        if (last_legal_skillbars.contains(agent_id)) {
            continue;
        }
        GW::SkillbarMgr::SkillTemplate snapshot;
        // The bar streams in a few frames after the agent itself; an empty first slot means it's not here yet.
        if (GW::SkillbarMgr::GetSkillTemplate(agent_id, snapshot) && snapshot.skills[0] != GW::Constants::SkillID::No_Skill) {
            last_legal_skillbars.emplace(agent_id, snapshot);
        }
    }
    std::erase_if(last_legal_skillbars, [&watched](const auto& entry) { return !Contains(watched, entry.first); });
}

void PlaystyleRestrictionsWindow::Draw(IDirect3DDevice9*)
{
    if (!visible) {
        return;
    }
    ImGui::SetNextWindowSize(ImVec2(480, 640), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        return ImGui::End();
    }

    ImGui::InputText("Profile name", profile.profile_name, 64);
    ImGui::InputText("Author", profile.author, 64);
    ImGui::InputText("Notes", profile.notes, 256);
    if (ImGui::Checkbox("Enforce this profile", &settings.enforce)) {
        GW::GameThread::Enqueue(RefreshEnterMissionButton);
    }
    ImGui::ShowHelp("Turn off to keep the profile but stop blocking anything");
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Campaign progression", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Enforce campaign order", &profile.enforce_campaign_order);
        ImGui::ShowHelp("Block travelling into, and entering missions of, a campaign whose predecessor isn't finished");
        if (profile.enforce_campaign_order) {
            DrawCampaignOrder();
            ImGui::Checkbox("Also require the previous campaign's titles maxed", &profile.require_campaign_titles_maxed);
            ImGui::ShowHelp("Protector, Guardian, Vanquisher, Skill Hunter and Cartographer for that campaign (plus Sunspear/Lightbringer for Nightfall). Account-wide titles are never counted.");
        }
        ImGui::Checkbox("Missions count only when done in Hard Mode too", &profile.require_hard_mode);
        ImGui::ShowHelp("Applies to every mission check on this page");
        ImGui::Checkbox("Missions also need their bonus objective", &profile.require_mission_bonuses);
        ImGui::Checkbox("Missions must be done in story order", &profile.block_mission_skipping);
        ImGui::ShowHelp("Greys out Enter Mission until every earlier mission of that campaign is complete");
        ImGui::Checkbox("Require titles before leaving pre-Searing", &profile.gate_presearing_exit);
        ImGui::ShowHelp("Defaults to Legendary Defender of Ascalon and Survivor rank 1");
        ImGui::InputScalar("Minimum level for gated areas", ImGuiDataType_U32, &profile.min_level_for_gated_areas);
        ImGui::ShowHelp("Blocks Enter Mission for any area with a level requirement until you reach this level. 0 disables.");
    }

    if (ImGui::CollapsingHeader("Heroes")) {
        ImGui::Checkbox("Heroes limited to unlocked campaigns", &profile.restrict_heroes_to_unlocked_campaigns);
        ImGui::ShowHelp("No Olias before Nightfall is unlocked, no Mox in Prophecies, and so on");
        ImGui::InputScalar("Maximum heroes in party", ImGuiDataType_U32, &profile.max_party_heroes);
        ImGui::ShowHelp("0 for no cap");
        ImGui::Checkbox("Lock hero skillbars", &profile.lock_hero_skillbars);
        DrawIdList("Always-allowed heroes", profile.hero_allow_list, "Comma-separated hero ids, exempt from every hero rule (e.g. Reforged Devona)");
        DrawIdList("Never-allowed heroes", profile.hero_block_list, "Comma-separated hero ids");
    }

    if (ImGui::CollapsingHeader("Skills")) {
        ImGui::Checkbox("Skills limited to unlocked campaigns", &profile.restrict_skills_to_unlocked_campaigns);
        ImGui::ShowHelp("Approximates \"what a fresh account would have unlocked\" by the campaign the skill belongs to");
        ImGui::Checkbox("One attribute line only", &profile.single_attribute_line);
        ImGui::Checkbox("Lock the skillbar entirely", &profile.lock_skillbar);
        ImGui::ShowHelp("Skillbar rules put the bar back to the last one that satisfied them instead of refusing the change");
        ImGui::Checkbox("No elite tomes", &profile.block_elite_tomes);
    }

    if (ImGui::CollapsingHeader("Items and equipment")) {
        ImGui::Checkbox("No consumables", &profile.block_consumables);
        ImGui::Checkbox("No scrolls", &profile.block_scrolls);
        ImGui::Checkbox("Lockpicks in Hard Mode only", &profile.lockpicks_hard_mode_only);
        ImGui::ShowHelp("Normal Mode stays regular-keys-only even once Hard Mode is unlocked");
        ImGui::Checkbox("No buying major/superior runes", &profile.block_purchased_runes);
        ImGui::ShowHelp("Only blocks the purchase; the same rune picked up as loot stays legal");
        ImGui::Checkbox("No buying Zaishen Coins", &profile.block_zaishen_coin_purchase);
        DrawIdList("Blocked item model ids", profile.blocked_item_model_ids, "Comma-separated model ids that can't be used, equipped or bought (bonus and holiday weapons, Fire Imp, ...)");
    }

    if (ImGui::CollapsingHeader("Storage")) {
        ImGui::Checkbox("No Xunlai Chest", &profile.block_xunlai_chest);
    }

    ImGui::Separator();
    if (ImGui::Button("Save profile to file...")) {
        Resources::SaveFileDialog([](const char* path) {
            if (!path) {
                return;
            }
            GW::GameThread::Enqueue([chosen = std::filesystem::path(path)] {
                if (!WriteProfile(chosen)) {
                    Log::Error("Failed to write playstyle restriction profile");
                }
            });
        }, "json", Resources::GetPath(profile_filename).string().c_str());
    }
    ImGui::SameLine();
    if (ImGui::Button("Load profile from file...")) {
        Resources::OpenFileDialog([](const char* path) {
            if (!path) {
                return;
            }
            GW::GameThread::Enqueue([chosen = std::filesystem::path(path)] {
                if (!ReadProfile(chosen)) {
                    return Log::Error("Failed to read playstyle restriction profile");
                }
                Log::Info("Loaded playstyle restriction profile \"%s\"", profile.profile_name.c_str());
                RefreshEnterMissionButton();
            });
        }, "json", Resources::GetPath(profile_filename).string().c_str());
    }
    ImGui::End();
}

void PlaystyleRestrictionsWindow::DrawSettingsInternal()
{
    ToolboxWindow::DrawSettingsInternal();
    ImGui::TextDisabled("Gates are enforced where the client routes the action through a UI message toolbox can block or undo; anything else stays on the honour system.");
}

void PlaystyleRestrictionsWindow::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxWindow::LoadSettings(doc, legacy);
    doc.GetStruct(Name(), settings);
    ReadProfile(Resources::GetPath(profile_filename));
}

void PlaystyleRestrictionsWindow::SaveSettings(SettingsDoc& doc)
{
    ToolboxWindow::SaveSettings(doc);
    doc.SetStruct(Name(), settings);
    WriteProfile(Resources::GetPath(profile_filename));
}
