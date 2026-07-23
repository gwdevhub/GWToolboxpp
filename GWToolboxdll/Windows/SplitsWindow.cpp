#include "stdafx.h"

#include <cctype>

#include <Windows/SplitsWindow.h>
#include <Windows/Splits/MapNames.h>
#include <Windows/Splits/SCPresets.h>
#include <Modules/Resources.h>
#include <Utils/EncString.h>
#include <Utils/TextUtils.h>

#include <GWCA/Constants/Constants.h>
#include <GWCA/Context/WorldContext.h>
#include <GWCA/GameEntities/Hero.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Title.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Map.h>

#include <Modules/GWEventBus.h>
#include <Modules/WebSocketModule.h>

// ---------------------------------------------------------------------------
// JSON DTOs for resume.json / per-list run history (glaze reflection requires external linkage).
// ---------------------------------------------------------------------------
namespace SplitsWindowJson {
    struct SerializedSplit {
        double real_time    = 0.0;
        double game_time    = 0.0;
        double segment_real = 0.0;
        double segment_game = 0.0;
    };

    struct SerializedResume {
        std::string list_name;
        double      real_time = 0.0;
        double      game_time = 0.0;
        std::optional<double> total_paused;
        std::vector<std::optional<SerializedSplit>> goals;
    };

    struct SerializedRunSplit {
        double real_time = 0.0;
        double game_time = 0.0;
    };

    struct SerializedRunGoal {
        std::string label;
        std::string status; // "Completed" / "Failed" / "Started" / "NotStarted"
        double real_time = 0.0;
        double game_time = 0.0;
    };

    struct SerializedRun {
        double total_real = 0.0;
        std::vector<SerializedRunSplit> splits;
        std::optional<bool> failed;
        std::optional<int64_t> utc_start;
        std::optional<std::string> character_name;
        std::optional<std::vector<SerializedRunGoal>> goals;
        std::optional<double> total_paused;
    };
}
using namespace SplitsWindowJson;

// Shadow-step skills that should trigger auto-start the same as movement.
// Mirrors TimerLogic.cpp in GWChrono.
static const std::unordered_set<uint32_t> kShadowStepSkills = {
     769,  // Viper's Defense
     770,  // Return
     771,  // Aura of Displacement
     799,  // Beguiling Haze
     815,  // Scorpion Wire
     836,  // Ride the Lightning
     925,  // Recall
     952,  // Death's Charge
    1032,  // Heart of Shadow
    1040,  // Spirit Walk
    1044,  // Dark Prison
    1644,  // Wastrel's Collapse
    1646,  // Augury of Death
    1650,  // Shadow Walk
    1651,  // Death's Retreat
    1652,  // Shadow Prison
    1653,  // Swap
    1654,  // Shadow Meld
    2052,  // Shadow Fang
    2420,  // Ebon Escape
    3428,  // Shadow Theft
};

// Manual: maps with a "Time until mission start" ready-check dialog whose wait should pause
// game time. Vizunah Square / Unwaking Waters lock the party (PartyLock) while waiting; the
// Ascalon Academy queue is solo (no party to lock) and only sends the generic countdown
// packet — both paths funnel into in_mission_queue_, cleared on the next real map change.
static bool IsMissionQueueMap(GW::Constants::MapID map)
{
    static constexpr GW::Constants::MapID kQueueMaps[] = {
        GW::Constants::MapID::Vizunah_Square_Local_Quarter_outpost,
        GW::Constants::MapID::Vizunah_Square_Foreign_Quarter_outpost,
        GW::Constants::MapID::Unwaking_Waters_Luxon_outpost,
        GW::Constants::MapID::Unwaking_Waters_Kurzick_outpost,
        GW::Constants::MapID::Ascalon_City_pre_searing,
    };
    for (const auto m : kQueueMaps) {
        if (map == m) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Initialize / Terminate
// ---------------------------------------------------------------------------
void SplitsWindow::Initialize()
{
    ToolboxWindow::Initialize();
    // MapNames::Init() deferred to first Update() — GW UI context may not be ready yet.

    GWEventBus::Instance().Subscribe(this, [this](const GWEvent& e) {
        using T = GWEvent::Type;
        switch (e.type) {
            case T::MissionComplete:
                engine_.NotifyMissionComplete(static_cast<GW::Constants::MapID>(e.id1));
                break;
            case T::MissionBonus:
                engine_.NotifyMissionBonus(static_cast<GW::Constants::MapID>(e.id1));
                break;
            case T::VanquishComplete:
                engine_.NotifyVanquishComplete(static_cast<GW::Constants::MapID>(e.id1));
                break;
            case T::PartyDefeated:
                // Latched rather than acted on directly — consumed by ApplyTimerPolicy()
                // so all auto-fail/auto-start decisions live in one place.
                pending_party_defeated_ = true;
                break;
            case T::ObjectiveAdd:
                engine_.NotifyObjectiveAdd(e.id1, e.id2);
                PushDbgEvent("ObjAdd", e.id1, e.id2);
                break;
            case T::ObjectiveDone:
                engine_.NotifyEvent(GoalTrigger::Type::ObjectiveDone, e.id1, e.id2); // id2 = map_id
                PushDbgEvent("ObjDone", e.id1, e.id2);
                break;
            case T::ObjectiveStarted:
                engine_.NotifyEvent(GoalTrigger::Type::ObjectiveStarted, e.id1);
                PushDbgEvent("ObjStart", e.id1, 0);
                break;
            case T::DoorOpen:
                engine_.NotifyEvent(GoalTrigger::Type::DoorOpen, e.id1);
                PushDbgEvent("DoorOpen", e.id1, 0);
                break;
            case T::DoorClose:
                engine_.NotifyEvent(GoalTrigger::Type::DoorClose, e.id1);
                PushDbgEvent("DoorClose", e.id1, 0);
                break;
            case T::AgentUpdateAllegiance:
                engine_.NotifyEvent(GoalTrigger::Type::AgentUpdateAllegiance, e.id1, e.id2);
                PushDbgEvent("AgentAllg", e.id1, e.id2);
                break;
            case T::DoACompleteZone:
                engine_.NotifyEvent(GoalTrigger::Type::DoACompleteZone, e.id1);
                PushDbgEvent("DoAZone", e.id1, 0);
                break;
            case T::DungeonReward:
                engine_.NotifyEvent(GoalTrigger::Type::DungeonReward);
                PushDbgEvent("DungeonRwd", 0, 0);
                break;
            case T::ServerMessage:
                engine_.NotifyEvent(GoalTrigger::Type::ServerMessage, 0, 0, e.str, e.str_len);
                // v1/v2 can't hold a full pattern — length + first wchar is just enough to
                // confirm something fired and roughly which one during live testing.
                PushDbgEvent("SrvMsg", static_cast<uint32_t>(e.str_len), e.str_len ? e.str[0] : 0);
                break;
            case T::DisplayDialogue:
                engine_.NotifyEvent(GoalTrigger::Type::DisplayDialogue, 0, 0, e.str, e.str_len);
                PushDbgEvent("DispDlg", static_cast<uint32_t>(e.str_len), e.str_len ? e.str[0] : 0);
                break;
            // ToPK/Ascalon Academy countdown — gated to mission queue maps so unrelated
            // countdowns don't pause Manual's game time.
            case T::CountdownStart:
                if (active_profile_idx_ == 0 && IsMissionQueueMap(static_cast<GW::Constants::MapID>(e.id1)))
                    in_mission_queue_ = true;
                engine_.NotifyEvent(GoalTrigger::Type::CountdownStart, e.id1);
                PushDbgEvent("Countdown", e.id1, 0);
                break;
            // Running: shadow-step auto-start — filter to local player only.
            case T::SkillActivate:
                if (e.id1 == GW::Agents::GetControlledCharacterId())
                    pending_skill_id_ = e.id2;
                break;
            // Manual: party lock signals the mission-start ready-check queue is up/down.
            // Gated to mission queue maps so unrelated party locks don't affect game time.
            case T::PartyLock:
                if (!e.id1) {
                    in_mission_queue_ = false;
                } else if (active_profile_idx_ == 0 && IsMissionQueueMap(last_map_)) {
                    in_mission_queue_ = true;
                }
                break;
            // Zone entry — fires for every new instance including same-map re-entry
            // (district change, new character into same starting map, etc.).
            case T::InstanceLoadInfo:
                pending_came_from_explorable_ = last_was_explorable_;
                last_was_explorable_          = (e.id2 != 0);
                last_map_                     = static_cast<GW::Constants::MapID>(e.id1);
                in_mission_queue_             = false;
                pending_map_enter_            = true;
                NuzlockeOnInstanceLoad();
                break;
            // Transfer packet fires before InstanceLoadInfo — clear queue state immediately
            // on any server transfer (covers district changes where the map doesn't change).
            case T::GameSrvTransfer:
                in_mission_queue_ = false;
                break;
            // Domain of Anguish's zone rotation is spawn-dependent, not a fixed map_id lookup —
            // latched independently of pending_map_enter_ and consumed by
            // ApplySCAutoLoadPreset() (see its own comment for why).
            case T::InstanceLoadFile:
                pending_doa_file_id_ = e.id1;
                pending_doa_spawn_   = e.spawn;
                // v2 = spawn.x only (v1/v2 can't hold both floats) — enough to sanity-check
                // DetectDoAStartingZone's input during live testing without a debugger.
                PushDbgEvent("InstLoadFile", e.id1, static_cast<uint32_t>(static_cast<int32_t>(e.spawn.x)));
                break;

            // --- Challenge/Nuzlocke debug events ---
            case T::PartyPlayerAdd:
            case T::PartyPlayerRemove:
            case T::PartyHeroAdd:
            case T::PartyHeroRemove:
            case T::PartyHenchmanAdd:
            case T::PartyHenchmanRemove:
            case T::AgentDied:
            case T::QuestUpdate:
            case T::QuestRemoved: {
                const char* tag = nullptr;
                switch (e.type) {
                    case T::PartyPlayerAdd:    tag = "PlyAdd";    break;
                    case T::PartyPlayerRemove: tag = "PlyRem";    break;
                    case T::PartyHeroAdd:
                        tag = "HeroAdd";
                        NuzlockeOnHeroAdd(e.id1, static_cast<GW::Constants::HeroID>(e.id2));
                        break;
                    case T::PartyHeroRemove:
                        tag = "HeroRem";
                        NuzlockeOnHeroRemove(e.id1);
                        break;
                    case T::PartyHenchmanAdd:
                        tag = "HenchAdd";
                        NuzlockeOnHenchAdd(e.id1, e.str, e.str_len);
                        break;
                    case T::PartyHenchmanRemove:
                        tag = "HenchRem";
                        NuzlockeOnHenchRemove(e.id1);
                        break;
                    case T::AgentDied:
                        tag = "AgentDied";
                        NuzlockeOnAgentDied(e.id1);
                        // Model ID doubles as living->player_number for non-player agents —
                        // forward it so a MobKill goal (if any is active) can count it.
                        // Players excluded since their player_number is a login/party index,
                        // not a monster model.
                        if (const auto* agent = GW::Agents::GetAgentByID(e.id1)) {
                            if (const auto* living = agent->GetAsAgentLiving(); living && !living->IsPlayer())
                                engine_.NotifyEvent(GoalTrigger::Type::MobKill, living->player_number);
                        }
                        break;
                    case T::QuestUpdate:
                        tag = "QuestUpd";
                        engine_.NotifyEvent(GoalTrigger::Type::QuestPickup, e.id1);
                        break;
                    case T::QuestRemoved:
                        tag = "QuestRem";
                        // Fires on turn-in AND on abandon — GAME_SMSG_QUEST_REMOVE doesn't
                        // distinguish the two, so an abandoned QuestComplete goal will fire
                        // early. Acceptable for run-tracking; revisit if it causes false splits.
                        engine_.NotifyEvent(GoalTrigger::Type::QuestComplete, e.id1);
                        break;
                    default: break;
                }
                if (tag) PushDbgEvent(tag, e.id1, e.id2);
                break;
            }

            default:
                break;
        }
    });

}

// Defaulted here (not in the header) because nuzlocke_pending_hench_names_ holds
// unique_ptr<GuiUtils::EncString>, and EncString is only forward-declared in the header.
SplitsWindow::SplitsWindow()  = default;
SplitsWindow::~SplitsWindow() = default;

void SplitsWindow::Terminate()
{
    GWEventBus::Instance().Unsubscribe(this);
    engine_.Detach();
    ToolboxWindow::Terminate();
}

void SplitsWindow::PushDbgEvent(const char* tag, const uint32_t v1, const uint32_t v2)
{
    if (!debug_log_events_) return;
    if (challenge_dbg_events_.size() >= 200)
        challenge_dbg_events_.erase(challenge_dbg_events_.begin());
    challenge_dbg_events_.push_back({tag, v1, v2});
}


namespace {
    constexpr ImVec4 kNuzlockeAlive     = ImVec4(1.f, 1.f, 1.f, 1.f);
    constexpr ImVec4 kNuzlockeAvailable = ImVec4(0.35f, 1.f, 0.35f, 1.f);
    constexpr ImVec4 kNuzlockeDead      = ImVec4(1.f, 0.35f, 0.35f, 1.f);

    // Henchman names come from the game as "Name [Role Henchman]" — the icon conveys
    // profession now, so the bracket is just noise.
    std::wstring StripHenchBracket(const std::wstring& name)
    {
        std::wstring out = name;
        if (const auto bracket = out.find(L'['); bracket != std::wstring::npos) {
            out.erase(bracket);
            while (!out.empty() && out.back() == L' ') out.pop_back();
        }
        return out;
    }

    // hero_info only lists heroes this account actually owns. Party events fire for
    // every hero in the party including partymates', so without this check a partymate's
    // same-named hero dying would increment our own hero's death count. It also carries
    // primary/secondary profession directly — the agent's AgentLiving::primary byte isn't
    // reliably populated yet at the moment PartyHeroAdd first fires, which is why hero
    // icons were coming up blank.
    const GW::HeroInfo* FindOwnedHeroInfo(const GW::Constants::HeroID hero_id)
    {
        const auto* world = GW::GetWorldContext();
        if (!world) return nullptr;
        for (const auto& hi : world->hero_info) {
            if (hi.hero_id == hero_id) return &hi;
        }
        return nullptr;
    }

    // PartyInfo::henchmen carries profession directly for actual party members, same
    // reasoning as FindOwnedHeroInfo above — AgentLiving::primary isn't reliably populated
    // yet for a henchman NPC the instant PartyHenchmanAdd fires (some Prophecies henchmen's
    // icons were coming up blank because of this race).
    const GW::HenchmanPartyMember* FindPartyHenchman(const uint32_t agent_id)
    {
        const auto* party = GW::PartyMgr::GetPartyInfo();
        if (!party) return nullptr;
        for (const auto& hm : party->henchmen) {
            if (hm.agent_id == agent_id) return &hm;
        }
        return nullptr;
    }
}

// ---------------------------------------------------------------------------
// Nuzlocke: Death Rules (party death tracking)
// ---------------------------------------------------------------------------
void SplitsWindow::NuzlockeOnHeroAdd(const uint32_t agent_id, const GW::Constants::HeroID hero_id)
{
    if (!NuzlockeDeathRulesEnabled()) return;

    const auto* hero_info = FindOwnedHeroInfo(hero_id);
    if (!hero_info) return; // not ours — don't track/conflate partymates' heroes

    nuzlocke_agents_[agent_id] = NuzlockeIdentity{true, hero_id, {}};
    const auto [it, inserted] = nuzlocke_heroes_.try_emplace(hero_id); // first-seen only; leaves existing death count alone
    if (inserted) it->second.profession = hero_info->primary;
}

void SplitsWindow::NuzlockeOnHeroRemove(const uint32_t agent_id)
{
    nuzlocke_agents_.erase(agent_id);
}

void SplitsWindow::NuzlockeOnHenchAdd(const uint32_t agent_id, const wchar_t* name_enc, const size_t name_len)
{
    if (!NuzlockeDeathRulesEnabled()) return;
    // Henchmen carry no owner field (unlike heroes' owner_player_id/FindOwnedHeroInfo above) —
    // they're party-wide slots controlled entirely by whoever's leader, so "not ours" for a
    // henchman means "I'm not the leader," not a per-unit check. Same reasoning as
    // FindOwnedHeroInfo: without this, a partymate's hired henchman dying would increment our
    // own tracked hench death count.
    if (!GW::PartyMgr::GetIsLeader()) return;
    if (!name_enc || !name_len) return;
    nuzlocke_pending_hench_names_.emplace_back(
        agent_id, std::make_unique<GuiUtils::EncString>(std::wstring(name_enc, name_len).c_str()));

    // Stash profession on the identity now so NuzlockeUpdate() can carry it over once the
    // name resolves and the NuzlockeMember entry actually gets created. PartyInfo::henchmen
    // is preferred over AgentLiving::primary — see FindPartyHenchman() for why.
    GW::Constants::Profession profession = GW::Constants::Profession::None;
    if (const auto* hm = FindPartyHenchman(agent_id)) {
        profession = static_cast<GW::Constants::Profession>(hm->profession);
    } else if (const auto* agent = GW::Agents::GetAgentByID(agent_id)) {
        if (const auto* living = agent->GetAsAgentLiving())
            profession = static_cast<GW::Constants::Profession>(living->primary);
    }
    nuzlocke_agents_[agent_id].hench_profession = profession;
}

void SplitsWindow::NuzlockeOnHenchRemove(const uint32_t agent_id)
{
    nuzlocke_agents_.erase(agent_id);
    std::erase_if(nuzlocke_pending_hench_names_, [agent_id](const auto& p) { return p.first == agent_id; });
}

void SplitsWindow::NuzlockeOnAgentDied(const uint32_t agent_id)
{
    if (!NuzlockeDeathRulesEnabled() || !last_was_explorable_) return;
    if (!nuzlocke_dead_agents_.insert(agent_id).second) return; // already counted this death

    const auto it = nuzlocke_agents_.find(agent_id);
    if (it != nuzlocke_agents_.end()) {
        if (it->second.is_hero) {
            nuzlocke_heroes_[it->second.hero_id].deaths++;
        } else {
            const auto hit = nuzlocke_henches_.find(it->second.hench_name);
            if (hit != nuzlocke_henches_.end()) hit->second.deaths++;
        }
        return;
    }

    // Not a pre-registered hero/hench — check whether it's a player we care about.
    const bool is_self = (agent_id == GW::Agents::GetControlledCharacterId());
    if (is_self ? !nuzlocke_track_self_ : !nuzlocke_track_other_players_) return;

    const auto* agent  = GW::Agents::GetAgentByID(agent_id);
    const auto* living = agent ? agent->GetAsAgentLiving() : nullptr;
    if (!living || !living->IsPlayer()) return;

    const wchar_t* raw_name = is_self ? GW::PlayerMgr::GetPlayerName()
                                       : GW::PlayerMgr::GetPlayerName(living->login_number);
    if (!raw_name) return;
    const std::wstring name(raw_name);
    nuzlocke_players_.try_emplace(name, NuzlockeMember{name, 0}).first->second.deaths++;
}

void SplitsWindow::NuzlockeOnInstanceLoad()
{
    if (!NuzlockeDeathRulesEnabled()) return;

    nuzlocke_dead_agents_.clear();
    // agent_ids for hireable town henchmen aren't stable across instances — drop any
    // cached decodes from the previous outpost so a reused id can't show a stale name.
    nuzlocke_city_hench_names_.clear();
    // Same reason: a reused agent_id in the new instance must not be mistaken for "same
    // roster as last frame, already resolved" by NuzlockeUpdate()'s skip check.
    nuzlocke_last_town_hench_ids_.clear();
    nuzlocke_town_hench_all_resolved_ = false;
    // Pre-seed self so they show up in the roster at full lives, same as heroes/henchmen —
    // other players only appear once they've actually died (no PartyPlayerAdd hookup yet).
    if (nuzlocke_track_self_) {
        if (const wchar_t* self_name = GW::PlayerMgr::GetPlayerName()) {
            nuzlocke_players_.try_emplace(self_name, NuzlockeMember{self_name, 0});
        }
    }
}

void SplitsWindow::NuzlockeResetProgress()
{
    nuzlocke_heroes_.clear();
    nuzlocke_henches_.clear();
    nuzlocke_players_.clear();
    nuzlocke_dead_agents_.clear();

    // nuzlocke_agents_ (who's currently a tracked hero/hench in the party) is deliberately
    // left alone, so it can reseed the display rosters below immediately instead of waiting
    // on the next zone transition's PartyHeroAdd/PartyHenchmanAdd events.
    for (const auto& [agent_id, identity] : nuzlocke_agents_) {
        if (identity.is_hero) {
            const auto [it, inserted] = nuzlocke_heroes_.try_emplace(identity.hero_id);
            if (inserted) {
                if (const auto* hero_info = FindOwnedHeroInfo(identity.hero_id))
                    it->second.profession = hero_info->primary;
            }
        } else if (!identity.hench_name.empty()) {
            nuzlocke_henches_.try_emplace(identity.hench_name,
                NuzlockeMember{identity.hench_name, 0, identity.hench_profession});
        }
    }

    if (nuzlocke_track_self_) {
        if (const wchar_t* self_name = GW::PlayerMgr::GetPlayerName()) {
            nuzlocke_players_.try_emplace(self_name, NuzlockeMember{self_name, 0});
        }
    }
}

std::wstring SplitsWindow::NuzlockeHenchKey(const std::wstring& raw_name) const
{
    return nuzlocke_merge_hench_by_name_ ? StripHenchBracket(raw_name) : raw_name;
}

void SplitsWindow::NuzlockeUpdate()
{
    if (!NuzlockeDeathRulesEnabled()) return;

    if (!nuzlocke_pending_hench_names_.empty()) {
        std::erase_if(nuzlocke_pending_hench_names_, [this](auto& p) {
            auto& [agent_id, enc] = p;
            const std::wstring raw_name = enc->wstring();
            if (raw_name.empty()) return false; // not decoded yet

            const std::wstring key = NuzlockeHenchKey(raw_name);
            auto& identity = nuzlocke_agents_[agent_id];
            identity.is_hero    = false;
            identity.hench_name = key;
            nuzlocke_henches_.try_emplace(key, NuzlockeMember{key, 0, identity.hench_profession});
            return true;
        });
    }

    // Hireable roster is a town-only concept — the array is empty/stale in explorables.
    if (last_was_explorable_) {
        nuzlocke_city_hench_available_.clear();
        nuzlocke_last_town_hench_ids_.clear();
        nuzlocke_town_hench_all_resolved_ = false;
        return;
    }
    const auto* world = GW::GetWorldContext();
    if (!world) return;

    // Skip the re-decode/re-hash/re-insert work below entirely once this town's roster hasn't
    // changed and every henchman's profession icon already resolved last pass — but never skip
    // while anything's still pending, so an icon keeps retrying every tick until it actually
    // loads instead of getting stuck blank. nuzlocke_city_hench_available_ is left exactly as
    // it was on a skipped tick (not cleared), since nothing about it could have changed.
    const auto& ids = world->henchmen_agent_ids;
    if (nuzlocke_town_hench_all_resolved_ &&
        std::equal(ids.begin(), ids.end(), nuzlocke_last_town_hench_ids_.begin(), nuzlocke_last_town_hench_ids_.end()))
        return;

    nuzlocke_city_hench_available_.clear();
    bool all_resolved = true;
    for (const uint32_t agent_id : ids) {
        auto& enc = nuzlocke_city_hench_names_[agent_id];
        if (!enc) enc = std::make_unique<GuiUtils::EncString>(GW::Agents::GetAgentEncName(agent_id));
        const std::wstring raw_name = enc->wstring();
        if (raw_name.empty()) { all_resolved = false; continue; } // not decoded yet this frame

        // Seed the roster just from being hireable here, not only from actually being
        // hired — otherwise a henchman never brought into the party never shows up at all.
        const std::wstring key = NuzlockeHenchKey(raw_name);
        const auto it = nuzlocke_henches_.try_emplace(key, NuzlockeMember{key, 0}).first;
        // These NPCs aren't party members, so there's no PartyInfo entry to read profession
        // from reliably (unlike FindPartyHenchman for actual recruits) — AgentLiving::primary
        // can still be unpopulated the first frame a given henchman is seen, so keep retrying
        // every tick until it resolves instead of locking in a blank icon forever.
        if (it->second.profession == GW::Constants::Profession::None) {
            if (const auto* agent = GW::Agents::GetAgentByID(agent_id)) {
                if (const auto* living = agent->GetAsAgentLiving())
                    it->second.profession = static_cast<GW::Constants::Profession>(living->primary);
            }
            if (it->second.profession == GW::Constants::Profession::None) all_resolved = false;
        }
        nuzlocke_city_hench_available_.insert(StripHenchBracket(raw_name));
    }
    nuzlocke_last_town_hench_ids_.assign(ids.begin(), ids.end());
    nuzlocke_town_hench_all_resolved_ = all_resolved;
}

void SplitsWindow::DrawNuzlockeSection()
{
    // Enable/lives settings live in Settings > Splits; this is display-only. Points is a
    // separate module now — its total is drawn in the header clock row instead. Both are
    // Manual-profile only — see NuzlockeDeathRulesEnabled()'s doc comment in the header.
    if (!NuzlockeDeathRulesEnabled()) return;
    if (!ImGui::CollapsingHeader("Death Rules")) return;

    if (nuzlocke_heroes_.empty() && nuzlocke_henches_.empty() && nuzlocke_players_.empty()) {
        ImGui::TextDisabled("Nobody tracked yet this session.");
        return;
    }

    ImGui::TextColored(kNuzlockeAlive, "White");
    ImGui::SameLine(0, 4); ImGui::TextDisabled("alive");
    ImGui::SameLine(0, 12); ImGui::TextColored(kNuzlockeAvailable, "Green");
    ImGui::SameLine(0, 4); ImGui::TextDisabled("henchman hireable here");
    ImGui::SameLine(0, 12); ImGui::TextColored(kNuzlockeDead, "Red");
    ImGui::SameLine(0, 4); ImGui::TextDisabled("out of lives");

    // Players info/lives, centered above the Henchmen/Heroes table. Each player's label is
    // built once (into a small per-player struct) and reused for both the width measurement
    // and the actual draw — instead of concatenating everyone into one throwaway std::string
    // purely to measure it, then reformatting each name a second time to draw it.
    if (!nuzlocke_players_.empty()) {
        struct PlayerLabel { std::string text; int remaining; };
        std::vector<PlayerLabel> labels;
        labels.reserve(nuzlocke_players_.size());
        const float sep_w = ImGui::CalcTextSize("    ").x;
        float textw = 0.f;
        char buf[96];
        for (auto& [name, member] : nuzlocke_players_) {
            const int remaining = nuzlocke_player_lives_ - member.deaths;
            snprintf(buf, sizeof(buf), "%s %d/%d", TextUtils::WStringToString(name).c_str(),
                     remaining > 0 ? remaining : 0, nuzlocke_player_lives_);
            if (!labels.empty()) textw += sep_w;
            textw += ImGui::CalcTextSize(buf).x;
            labels.push_back({buf, remaining});
        }
        const float avail = ImGui::GetContentRegionAvail().x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.f, (avail - textw) * 0.5f));

        bool first = true;
        for (const auto& lbl : labels) {
            if (!first) {
                ImGui::SameLine(0, 0);
                ImGui::TextUnformatted("    ");
                ImGui::SameLine(0, 0);
            }
            first = false;
            ImGui::TextColored(lbl.remaining <= 0 ? kNuzlockeDead : kNuzlockeAlive, "%s", lbl.text.c_str());
        }
    }

    auto icon_size = ImGui::CalcTextSize(" ");
    icon_size.x = icon_size.y;

    if (ImGui::BeginTable("nuzlocke_hench_hero_table", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Henchmen");
        ImGui::TableSetupColumn("Heroes");
        ImGui::TableHeadersRow();
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        for (auto& [name, member] : nuzlocke_henches_) {
            const int remaining = nuzlocke_hench_lives_ - member.deaths;
            const std::wstring display_name = StripHenchBracket(name);

            ImVec4 color = kNuzlockeAlive;
            if (remaining <= 0) color = kNuzlockeDead;
            else if (nuzlocke_city_hench_available_.contains(display_name)) color = kNuzlockeAvailable;

            ImGui::Image(*Resources::GetProfessionIcon(member.profession), icon_size);
            ImGui::SameLine();
            ImGui::TextColored(color, "%s - %d/%d", TextUtils::WStringToString(display_name).c_str(),
                remaining > 0 ? remaining : 0, nuzlocke_hench_lives_);
        }

        ImGui::TableSetColumnIndex(1);
        for (auto& [hero_id, member] : nuzlocke_heroes_) {
            auto* name = Resources::GetHeroName(hero_id);
            const int remaining = nuzlocke_hero_lives_ - member.deaths;
            // Only owned heroes ever make it into nuzlocke_heroes_ (see FindOwnedHeroInfo()
            // in NuzlockeOnHeroAdd), so there's no "not yours" case left to color green here.
            const ImVec4 color = remaining <= 0 ? kNuzlockeDead : kNuzlockeAlive;

            ImGui::Image(*Resources::GetProfessionIcon(member.profession), icon_size);
            ImGui::SameLine();
            ImGui::TextColored(color, "%s - %d/%d", name ? name->string().c_str() : "(hero)",
                remaining > 0 ? remaining : 0, nuzlocke_hero_lives_);
        }

        ImGui::EndTable();
    }
}

// ---------------------------------------------------------------------------
// Nuzlocke: Points
// ---------------------------------------------------------------------------
int SplitsWindow::NuzlockeTotalPoints() const
{
    using T = GoalTrigger::Type;
    int total = 0;
    for (const auto& g : active_list_.goals) {
        if (g.is_header || g.status != GoalStatus::Completed) continue;
        switch (g.trigger.type) {
            case T::Manual:          total += nuzlocke_goal_points_.manual;       break;
            case T::MissionComplete:
            case T::MissionBonus:    total += nuzlocke_goal_points_.missions;     break;
            case T::MapEnter:        total += nuzlocke_goal_points_.explorables;  break;
            case T::EnterExplorable:
            case T::ExitExplorable:
            case T::ExitOutpost:     total += nuzlocke_goal_points_.towns;        break;
            case T::ReachTitleRank:  total += nuzlocke_goal_points_.titles;       break;
            case T::ReachLevel:      total += nuzlocke_goal_points_.reach_level;  break;
            case T::QuestPickup:
            case T::QuestComplete:   total += nuzlocke_goal_points_.quest;        break;
            case T::SkillLearnt:     total += nuzlocke_goal_points_.skill_learnt; break;
            default:                 break; // preset-only triggers (dungeons/elites) aren't scored
        }
    }
    return total;
}

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------
void SplitsWindow::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxWindow::LoadSettings(doc, legacy);

    // Establish data folders — profile subfolders created below
    const auto splits_path = Resources::GetPath(L"splits");
    const auto runs_path   = Resources::GetPath(L"splits\\runs");
    Resources::EnsureFolderExists(splits_path);
    Resources::EnsureFolderExists(runs_path);
    Resources::EnsureFolderExists(splits_path / L"manual");
    Resources::EnsureFolderExists(splits_path / L"running");
    Resources::EnsureFolderExists(splits_path / L"sc");
    Resources::EnsureFolderExists(runs_path   / L"manual");
    Resources::EnsureFolderExists(runs_path   / L"running");
    Resources::EnsureFolderExists(runs_path   / L"sc");
    splits_folder_ = splits_path.wstring() + L"\\";
    runs_folder_   = runs_path.wstring() + L"\\";

    auto get_long = [&](const char* key, long def) -> long {
        long v = def;
        if (!doc.Get(Name(), key, v)) v = legacy->GetLongValue(Name(), key, def);
        return v;
    };
    key_start_ = get_long("key_start", 0);
    key_reset_ = get_long("key_reset", 0);
    key_split_ = get_long("key_split", 0);
    active_profile_idx_ = static_cast<int>(get_long("active_profile", 0));
    if (active_profile_idx_ < 0 || active_profile_idx_ >= kProfileCount) active_profile_idx_ = 0;
    for (int i = 0; i < kProfileCount; ++i)
        profiles_[i].LoadSettings(doc, legacy, kProfileSections[i]);

    doc.Get(Name(), "nuzlocke_death_rules_enabled", nuzlocke_death_rules_enabled_);
    nuzlocke_hero_lives_  = static_cast<int>(get_long("nuzlocke_hero_lives", 1));
    nuzlocke_hench_lives_ = static_cast<int>(get_long("nuzlocke_hench_lives", 1));
    doc.Get(Name(), "nuzlocke_track_self",          nuzlocke_track_self_);
    doc.Get(Name(), "nuzlocke_track_other_players", nuzlocke_track_other_players_);
    nuzlocke_player_lives_ = static_cast<int>(get_long("nuzlocke_player_lives", 1));
    doc.Get(Name(), "nuzlocke_merge_hench_by_name", nuzlocke_merge_hench_by_name_);

    doc.Get(Name(), "nuzlocke_points_enabled", nuzlocke_points_enabled_);
    nuzlocke_goal_points_.manual       = static_cast<int>(get_long("nuzlocke_points_manual", 0));
    nuzlocke_goal_points_.missions     = static_cast<int>(get_long("nuzlocke_points_missions", 0));
    nuzlocke_goal_points_.explorables  = static_cast<int>(get_long("nuzlocke_points_explorables", 0));
    nuzlocke_goal_points_.towns        = static_cast<int>(get_long("nuzlocke_points_towns", 0));
    nuzlocke_goal_points_.titles       = static_cast<int>(get_long("nuzlocke_points_titles", 0));
    nuzlocke_goal_points_.reach_level  = static_cast<int>(get_long("nuzlocke_points_reach_level", 0));
    nuzlocke_goal_points_.quest        = static_cast<int>(get_long("nuzlocke_points_quest", 0));
    nuzlocke_goal_points_.skill_learnt = static_cast<int>(get_long("nuzlocke_points_skill_learnt", 0));

    doc.Get(Name(), "debug_log_events", debug_log_events_);

    engine_.Attach(&active_list_);

    // Crash-protection resume check
    const std::wstring resume_path = splits_folder_ + L"resume.json";
    std::ifstream rf(resume_path);
    if (rf.is_open()) {
        std::stringstream ss;
        ss << rf.rdbuf();
        rf.close();

        SerializedResume j;
        constexpr glz::opts opts{.error_on_unknown_keys = false};
        if (!glz::read<opts>(j, ss.str())) {
            if (!j.list_name.empty()) {
                // active_profile_idx_ is already restored from settings above, so
                // ActiveSplitsFolder() returns the correct profile subfolder.
                const std::wstring list_path = ActiveSplitsFolder() +
                    std::wstring(j.list_name.begin(), j.list_name.end()) + L".json";
                if (std::filesystem::exists(list_path)) {
                    pending_resume_name_ = j.list_name;
                    pending_resume_data_ = ss.str();
                    pending_resume_      = true;
                }
            }
        }
    }
}

void SplitsWindow::SaveSettings(SettingsDoc& doc)
{
    doc.Set(Name(), "key_start", key_start_);
    doc.Set(Name(), "key_reset", key_reset_);
    doc.Set(Name(), "key_split", key_split_);
    doc.Set(Name(), "active_profile", active_profile_idx_);
    for (int i = 0; i < kProfileCount; ++i)
        profiles_[i].SaveSettings(doc, kProfileSections[i]);
    doc.Set(Name(), "nuzlocke_death_rules_enabled", nuzlocke_death_rules_enabled_);
    doc.Set(Name(), "nuzlocke_hero_lives",  nuzlocke_hero_lives_);
    doc.Set(Name(), "nuzlocke_hench_lives", nuzlocke_hench_lives_);
    doc.Set(Name(), "nuzlocke_track_self",          nuzlocke_track_self_);
    doc.Set(Name(), "nuzlocke_track_other_players", nuzlocke_track_other_players_);
    doc.Set(Name(), "nuzlocke_player_lives",        nuzlocke_player_lives_);
    doc.Set(Name(), "nuzlocke_merge_hench_by_name", nuzlocke_merge_hench_by_name_);

    doc.Set(Name(), "nuzlocke_points_enabled",      nuzlocke_points_enabled_);
    doc.Set(Name(), "nuzlocke_points_manual",       nuzlocke_goal_points_.manual);
    doc.Set(Name(), "nuzlocke_points_missions",     nuzlocke_goal_points_.missions);
    doc.Set(Name(), "nuzlocke_points_explorables",  nuzlocke_goal_points_.explorables);
    doc.Set(Name(), "nuzlocke_points_towns",        nuzlocke_goal_points_.towns);
    doc.Set(Name(), "nuzlocke_points_titles",       nuzlocke_goal_points_.titles);
    doc.Set(Name(), "nuzlocke_points_reach_level",  nuzlocke_goal_points_.reach_level);
    doc.Set(Name(), "nuzlocke_points_quest",        nuzlocke_goal_points_.quest);
    doc.Set(Name(), "nuzlocke_points_skill_learnt", nuzlocke_goal_points_.skill_learnt);

    doc.Set(Name(), "debug_log_events", debug_log_events_);
    ToolboxWindow::SaveSettings(doc);
}

// ---------------------------------------------------------------------------
// Goal list management
// ---------------------------------------------------------------------------
void SplitsWindow::NewActiveList(const char* name)
{
    engine_.Detach();
    active_list_ = GoalList{};
    active_list_.name = name ? name : "New List";
    clock_.Reset();
    engine_.Attach(&active_list_);
    cached_saved_lists_dirty_ = true;
}

void SplitsWindow::SaveActiveList()
{
    if (splits_folder_.empty() || active_list_.name.empty()) return;
    // Explicitly saving (even a list that started life as an auto-loaded preset) always
    // produces a normal, user-owned list — presets are never persisted to disk themselves
    // (always rebuilt live from SCPresets, see ApplySCAutoLoadPreset), but the in-memory flag
    // still needs clearing so the saved copy reads as editable, not as a preset.
    active_list_.is_preset = false;
    const std::wstring wname(active_list_.name.begin(), active_list_.name.end());
    active_list_.SaveToFile(ActiveSplitsFolder() + wname + L".json");
    cached_saved_lists_dirty_ = true;
}

void SplitsWindow::LoadActiveList(const std::wstring& path)
{
    DeleteResumeState();
    engine_.Detach();
    active_list_.LoadFromFile(path);
    clock_.Reset();
    engine_.Attach(&active_list_);
    LoadPB();

    run_complete_              = false;
    run_failed_                = false;
    running_awaiting_movement_ = false;
    running_load_paused_       = false;
    pending_skill_id_          = 0;
    in_mission_queue_          = false;
    manually_paused_           = false;
    manual_pause_accum_        = 0.0;
    total_paused_real_         = 0.0;
    pending_map_enter_            = false;
    pending_came_from_explorable_ = false;
}

void SplitsWindow::SetActiveList(GoalList list)
{
    // Same sequencing as LoadActiveList (Detach -> replace -> Attach), just from an
    // already-built list (e.g. SCPresets::BuildDungeonPresetList) instead of a file.
    // Deliberately not NewActiveList() + mutating List()->goals afterward — NewActiveList()
    // attaches the engine against an empty GoalList, so any goal that needs Attach()-time
    // setup (starts_immediately, etc.) would silently never get it if the real goals were
    // only added after the fact.
    DeleteResumeState();
    engine_.Detach();
    active_list_ = std::move(list);
    // User-initiated (picked from the interactive picker), not tool-managed — never leave
    // is_preset set on what's about to become an ordinary editable list.
    active_list_.is_preset = false;
    clock_.Reset();
    engine_.Attach(&active_list_);
    LoadPB();

    run_complete_              = false;
    run_failed_                = false;
    running_awaiting_movement_ = false;
    running_load_paused_       = false;
    pending_skill_id_          = 0;
    in_mission_queue_          = false;
    manually_paused_           = false;
    manual_pause_accum_        = 0.0;
    total_paused_real_         = 0.0;
    pending_map_enter_            = false;
    pending_came_from_explorable_ = false;
}

void SplitsWindow::LoadPB(bool refresh_comparisons)
{
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    pb_splits_.clear();       pb_splits_game_.clear();       pb_total_real_ = nan;
    if (refresh_comparisons) {
        avg_splits_.clear();      avg_splits_game_.clear();
        last_splits_.clear();     last_splits_game_.clear();
    }

    if (runs_folder_.empty() || active_list_.name.empty()) return;

    std::wstring safe_name(active_list_.name.begin(), active_list_.name.end());
    for (auto& c : safe_name) {
        if (c == L' ') c = L'_';
        else if (c == L'/' || c == L'\\' || c == L':' || c == L'*' ||
                 c == L'?' || c == L'"'  || c == L'<' || c == L'>' || c == L'|')
            c = L'-';
    }
    const std::wstring runs_path = ActiveRunsFolder() + safe_name + L".json";
    std::ifstream rf(runs_path);
    if (!rf.is_open()) return;

    std::stringstream ss;
    ss << rf.rdbuf();

    std::vector<SerializedRun> runs;
    constexpr glz::opts opts{.error_on_unknown_keys = false};
    if (glz::read<opts>(runs, ss.str())) return;

    // Arrays are indexed by non-header goals only (headers carry no split data).
    int goal_count = 0;
    for (const auto& g : active_list_.goals) if (!g.is_header) ++goal_count;
    if (goal_count == 0) return;

    // PB: fastest non-failed run that reached every goal.
    double best = std::numeric_limits<double>::infinity();
    const SerializedRun* best_run = nullptr;
    for (const auto& run : runs) {
        if (run.failed.value_or(false)) continue;
        if (static_cast<int>(run.splits.size()) < goal_count) continue;
        if (run.total_real < best) { best = run.total_real; best_run = &run; }
    }
    if (best_run) {
        pb_total_real_ = best;
        pb_splits_.resize(static_cast<size_t>(goal_count), nan);
        pb_splits_game_.resize(static_cast<size_t>(goal_count), nan);
        for (int i = 0; i < goal_count && i < static_cast<int>(best_run->splits.size()); ++i) {
            pb_splits_[static_cast<size_t>(i)]      = best_run->splits[static_cast<size_t>(i)].real_time;
            pb_splits_game_[static_cast<size_t>(i)] = best_run->splits[static_cast<size_t>(i)].game_time;
        }
    }

    if (!refresh_comparisons) return;

    // Average: mean of each goal index's time across every non-failed run that reached it —
    // a run that only got to goal 3 of 5 still contributes to goals 0-2's average.
    {
        std::vector<double> sum_real(static_cast<size_t>(goal_count), 0.0);
        std::vector<double> sum_game(static_cast<size_t>(goal_count), 0.0);
        std::vector<int>    count(static_cast<size_t>(goal_count), 0);
        for (const auto& run : runs) {
            if (run.failed.value_or(false)) continue;
            for (int i = 0; i < goal_count && i < static_cast<int>(run.splits.size()); ++i) {
                sum_real[static_cast<size_t>(i)] += run.splits[static_cast<size_t>(i)].real_time;
                sum_game[static_cast<size_t>(i)] += run.splits[static_cast<size_t>(i)].game_time;
                ++count[static_cast<size_t>(i)];
            }
        }
        avg_splits_.resize(static_cast<size_t>(goal_count), nan);
        avg_splits_game_.resize(static_cast<size_t>(goal_count), nan);
        for (int i = 0; i < goal_count; ++i) {
            if (count[static_cast<size_t>(i)] > 0) {
                avg_splits_[static_cast<size_t>(i)]      = sum_real[static_cast<size_t>(i)] / count[static_cast<size_t>(i)];
                avg_splits_game_[static_cast<size_t>(i)] = sum_game[static_cast<size_t>(i)] / count[static_cast<size_t>(i)];
            }
        }
    }

    // Last Run: most recent attempt in history, failed or not — whatever splits it reached.
    if (!runs.empty()) {
        const SerializedRun& last = runs.back();
        last_splits_.resize(static_cast<size_t>(goal_count), nan);
        last_splits_game_.resize(static_cast<size_t>(goal_count), nan);
        for (int i = 0; i < goal_count && i < static_cast<int>(last.splits.size()); ++i) {
            last_splits_[static_cast<size_t>(i)]      = last.splits[static_cast<size_t>(i)].real_time;
            last_splits_game_[static_cast<size_t>(i)] = last.splits[static_cast<size_t>(i)].game_time;
        }
    }
}

const std::vector<double>& SplitsWindow::CompareSplits() const
{
    using CM = SplitsProfile::ComparisonMode;
    switch (ActiveProfile().comparison_mode) {
        case CM::Average: return avg_splits_;
        case CM::LastRun: return last_splits_;
        default:          return pb_splits_;
    }
}

const std::vector<double>& SplitsWindow::CompareSplitsGame() const
{
    using CM = SplitsProfile::ComparisonMode;
    switch (ActiveProfile().comparison_mode) {
        case CM::Average: return avg_splits_game_;
        case CM::LastRun: return last_splits_game_;
        default:          return pb_splits_game_;
    }
}

std::vector<std::pair<std::string, std::wstring>> SplitsWindow::GetSavedLists() const
{
    if (splits_folder_.empty()) return {};
    const std::wstring folder = ActiveSplitsFolder();
    if (cached_saved_lists_dirty_ || folder != cached_saved_lists_folder_) {
        cached_saved_lists_        = GoalList::ListSaved(folder);
        cached_saved_lists_folder_ = folder;
        cached_saved_lists_dirty_  = false;
    }
    return cached_saved_lists_;
}

// ---------------------------------------------------------------------------
// Crash protection
// ---------------------------------------------------------------------------
void SplitsWindow::SaveResumeState()
{
    if (splits_folder_.empty() || !clock_.IsRunning() || active_list_.name.empty()) return;

    SerializedResume j;
    j.list_name    = active_list_.name;
    j.real_time    = clock_.RealTime();
    j.game_time    = clock_.GameTime();
    j.total_paused = total_paused_real_;

    j.goals.reserve(active_list_.goals.size());
    for (const auto& g : active_list_.goals) {
        if (g.status != GoalStatus::Completed) {
            j.goals.emplace_back(std::nullopt);
        } else {
            j.goals.push_back(SerializedSplit{
                g.split.real_time, g.split.game_time, g.split.segment_real, g.split.segment_game});
        }
    }

    std::ofstream f(splits_folder_ + L"resume.json");
    if (f.is_open()) f << glz::write<glz::opts{.prettify = true}>(j).value_or(std::string{});
}

void SplitsWindow::DeleteResumeState()
{
    pending_resume_ = false;
    pending_resume_name_.clear();
    pending_resume_data_.clear();
    if (splits_folder_.empty()) return;
    std::error_code ec;
    std::filesystem::remove(splits_folder_ + L"resume.json", ec);
}

void SplitsWindow::ApplyResume()
{
    if (!pending_resume_) return;
    pending_resume_ = false;

    SerializedResume j;
    constexpr glz::opts opts{.error_on_unknown_keys = false};
    const bool parse_failed = static_cast<bool>(glz::read<opts>(j, pending_resume_data_));
    pending_resume_data_.clear();
    if (parse_failed) return;

    if (j.list_name.empty()) return;
    const double real_time = j.real_time;
    const double game_time = j.game_time;

    const std::wstring list_path = ActiveSplitsFolder() +
        std::wstring(j.list_name.begin(), j.list_name.end()) + L".json";

    engine_.Detach();
    active_list_.LoadFromFile(list_path);
    engine_.Attach(&active_list_);
    LoadPB();

    for (size_t i = 0; i < j.goals.size() && i < active_list_.goals.size(); ++i) {
        const auto& jg = j.goals[i];
        if (!jg.has_value()) continue;
        auto& g              = active_list_.goals[i];
        g.status             = GoalStatus::Completed;
        g.split.real_time    = jg->real_time;
        g.split.game_time    = jg->game_time;
        g.split.segment_real = jg->segment_real;
        g.split.segment_game = jg->segment_game;
    }

    clock_.Restore(real_time, game_time);
    engine_.ForceStarted();
    total_paused_real_   = j.total_paused.value_or(0.0);
    pending_map_enter_            = false;
    pending_came_from_explorable_ = false;
}

void SplitsWindow::DiscardResume()
{
    pending_resume_ = false;
    pending_resume_data_.clear();
    DeleteResumeState();
}

// ---------------------------------------------------------------------------
// SaveCompletedRun / FailRun / SaveRunToHistory
// ---------------------------------------------------------------------------
void SplitsWindow::SaveCompletedRun()
{
    run_complete_ = true;
    clock_.Pause();
    SaveRunToHistory(/*failed=*/false);
    LoadPB(/*refresh_comparisons=*/false);
    UpdateReferenceIfPB();
    DeleteResumeState();
    if (ActiveProfile().auto_send_age)
        GW::Chat::SendChat('/', L"age");
}

void SplitsWindow::FailRun(const char* reason)
{
    if (run_complete_ || run_failed_ || !clock_.IsRunning()) return;
    engine_.FailRun(clock_);
    clock_.Pause();
    run_failed_ = true;
    WebSocketModule::Instance().Send("reset", (std::string("Splits: Reset - ") + reason).c_str());
    SaveRunToHistory(/*failed=*/true);
    DeleteResumeState();
}

void SplitsWindow::SaveRunToHistory(bool failed)
{
    if (runs_folder_.empty() || active_list_.name.empty()) return;

    std::wstring safe_name(active_list_.name.begin(), active_list_.name.end());
    for (auto& c : safe_name) {
        if (c == L' ') c = L'_';
        else if (c == L'/' || c == L'\\' || c == L':' || c == L'*' ||
                 c == L'?' || c == L'"'  || c == L'<' || c == L'>' || c == L'|')
            c = L'-';
    }
    const std::wstring runs_path = ActiveRunsFolder() + safe_name + L".json";

    std::vector<SerializedRun> runs;
    {
        std::ifstream rf(runs_path);
        if (rf.is_open()) {
            std::stringstream ss;
            ss << rf.rdbuf();
            constexpr glz::opts opts{.error_on_unknown_keys = false};
            if (glz::read<opts>(runs, ss.str())) runs.clear();
        }
    }

    auto status_name = [](GoalStatus s) -> std::string {
        switch (s) {
            case GoalStatus::Started:   return "Started";
            case GoalStatus::Completed: return "Completed";
            case GoalStatus::Failed:    return "Failed";
            default:                    return "NotStarted";
        }
    };

    SerializedRun run;
    run.total_real      = clock_.RealTime();
    run.failed          = failed;
    run.utc_start       = run_start_unix_;
    run.character_name  = run_char_name_;
    run.total_paused    = total_paused_real_;

    // Headers carry no split data — PB/history arrays index only non-header goals.
    std::vector<SerializedRunGoal> rgoals;
    for (const auto& g : active_list_.goals) {
        if (g.is_header) continue;
        run.splits.push_back(SerializedRunSplit{g.split.real_time, g.split.game_time});
        rgoals.push_back(SerializedRunGoal{g.label, status_name(g.status),
                                            g.split.real_time, g.split.game_time});
    }
    run.goals = std::move(rgoals);

    runs.push_back(std::move(run));
    constexpr size_t kMaxRuns = 200;
    if (runs.size() > kMaxRuns)
        runs.erase(runs.begin(), runs.end() - static_cast<long>(kMaxRuns));

    std::ofstream f(runs_path);
    if (f.is_open())
        f << glz::write<glz::opts{.prettify = true}>(runs).value_or(std::string{});
}

// ---------------------------------------------------------------------------
// Profile folder routing
// ---------------------------------------------------------------------------
std::wstring SplitsWindow::ActiveSplitsFolder() const
{
    if (active_profile_idx_ == 0) return splits_folder_ + L"manual\\";
    if (active_profile_idx_ == 1) return splits_folder_ + L"running\\";
    if (active_profile_idx_ == 2) return splits_folder_ + L"sc\\";
    return splits_folder_;
}

std::wstring SplitsWindow::ActiveRunsFolder() const
{
    if (active_profile_idx_ == 0) return runs_folder_ + L"manual\\";
    if (active_profile_idx_ == 1) return runs_folder_ + L"running\\";
    if (active_profile_idx_ == 2) return runs_folder_ + L"sc\\";
    return runs_folder_;
}

void SplitsWindow::UpdateReferenceIfPB()
{
    if (std::isnan(pb_total_real_) || pb_splits_.empty()) return;

    double ref_total = std::numeric_limits<double>::infinity();
    if (active_list_.reference.has_value() && !active_list_.reference->splits.empty())
        ref_total = active_list_.reference->splits.back();

    if (pb_total_real_ >= ref_total) return;

    GoalReference& ref = active_list_.reference.emplace();
    ref.name   = "Personal Best";
    ref.splits = pb_splits_;

    tm tm_buf{};
    const time_t t = time(nullptr);
    localtime_s(&tm_buf, &t);
    char date_buf[16];
    snprintf(date_buf, sizeof(date_buf), "%04d-%02d-%02d",
             tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday);
    ref.date = date_buf;

    SaveActiveList();
}

// ---------------------------------------------------------------------------
// Update — called every frame
// ---------------------------------------------------------------------------
void SplitsWindow::Update(float delta)
{
    MapNames::Init(); // no-op after first successful call; deferred here so GW UI is ready
    NuzlockeUpdate();

    const auto instance_type   = GW::Map::GetInstanceType();
    const bool is_explorable   = (instance_type == GW::Constants::InstanceType::Explorable);
    const bool is_loading      = (instance_type == GW::Constants::InstanceType::Loading);
    const bool in_cinematic    = GW::Map::GetIsInCinematic();

    // Consume bus-sourced map-entry flags (set by InstanceLoadInfo / GameSrvTransfer callbacks).
    const bool just_entered_map     = pending_map_enter_;
    const bool came_from_explorable = pending_came_from_explorable_;
    pending_map_enter_            = false;
    pending_came_from_explorable_ = false;

    // Before engine_.Update() below, so a freshly-swapped-in preset is already attached in
    // time for this same tick's Pass 1/autostart checks.
    ApplySCAutoLoadPreset(just_entered_map);

    // Accumulate wall-clock time while manually paused (clock_.RealTime() is frozen during
    // a manual pause, so it can't be used to measure how long the pause lasted).
    if (manually_paused_)
        manual_pause_accum_ += static_cast<double>(delta);
    // Manual (idx 0): pause game time during loading, cinematics, and mission-start queues.
    // Running (idx 1): game time is controlled entirely by the clock pause below (explorable only).
    const bool time_paused = is_loading || in_cinematic
        || (active_profile_idx_ == 0 && in_mission_queue_);

    clock_.AddRealTime(static_cast<double>(delta));

    if (!time_paused)
        clock_.AddGameTime(static_cast<double>(delta));

    const GW::Agent* controlled = GW::Agents::GetControlledCharacter();
    const GW::AgentLiving* controlled_living = controlled ? controlled->GetAsAgentLiving() : nullptr;
    int player_level = 0;
    if (controlled_living)
        player_level = static_cast<int>(controlled_living->level);

    // Running: clock only ticks in explorable areas (no loading, no town time).
    if (active_profile_idx_ == 1) {
        if (!is_explorable && clock_.IsRunning() && !running_load_paused_) {
            clock_.Pause();
            running_load_paused_ = true;
        }
        // Arm movement detector whenever in explorable and clock isn't active — matches
        // GWChrono's continuous check so the player doesn't need to re-zone to arm.
        if (is_explorable && !clock_.IsRunning() && !run_complete_ && !run_failed_)
            running_awaiting_movement_ = true;
    }

    // Running: movement or shadow step in an explorable starts or resumes the clock.
    if (active_profile_idx_ == 1 && !run_complete_ && !run_failed_) {
        if (running_awaiting_movement_ && is_explorable) {
            const uint32_t skill = pending_skill_id_;
            pending_skill_id_    = 0;

            bool triggered = skill != 0 && kShadowStepSkills.count(skill) != 0;
            if (!triggered) {
                triggered = controlled_living &&
                    (controlled_living->GetIsMoving() ||
                     controlled_living->model_state == 204 ||
                     controlled_living->move_x != 0.f ||
                     controlled_living->move_y != 0.f);
            }

            if (triggered) {
                running_awaiting_movement_ = false;
                if (running_load_paused_) {
                    // Mid-run resume after a town or loading screen.
                    clock_.Resume();
                    running_load_paused_ = false;
                } else {
                    // Initial run start.
                    run_complete_   = false;
                    run_failed_     = false;
                    run_char_name_.clear();
                    run_char_level_ = player_level;
                    run_start_unix_ = static_cast<int64_t>(time(nullptr));
                    if (const wchar_t* wname = GW::PlayerMgr::GetPlayerName()) {
                        const int sz = WideCharToMultiByte(CP_UTF8, 0, wname, -1, nullptr, 0, nullptr, nullptr);
                        if (sz > 1) {
                            run_char_name_.resize(static_cast<size_t>(sz) - 1);
                            WideCharToMultiByte(CP_UTF8, 0, wname, -1, run_char_name_.data(), sz, nullptr, nullptr);
                        }
                    }
                    WebSocketModule::Instance().Send("reset", "Splits: Reset - run starting");
                    WebSocketModule::Instance().Send("start", "Splits: Start - movement detected");
                    clock_.Start();
                    engine_.ForceStarted();
                }
            }
        } else {
            pending_skill_id_ = 0;
        }
    }

    // Running (sequential): split on every zone transition after the clock has started.
    // No zone-type filter — routes can pass through towns/outposts as checkpoints.
    // Manual: fire on any map entry.
    const bool fire_map_enter = active_profile_idx_ != 1
        ? just_entered_map
        : (just_entered_map && clock_.RealTime() > 0.0);

    const int fired = engine_.Update(clock_, last_map_, fire_map_enter,
                                     came_from_explorable, instance_type,
                                     player_level,
                                     /*sequential=*/(active_profile_idx_ == 1));

    if (clock_.IsRunning()) {
        resume_save_timer_ += static_cast<float>(delta);
        if (fired > 0 || resume_save_timer_ >= 1.0f) {
            SaveResumeState();
            resume_save_timer_ = 0.f;
        }
        if (fired > 0)
            WebSocketModule::Instance().Send("split", "Splits: Split - goal complete");
    }

    ApplyTimerPolicy(just_entered_map, player_level);

    // Running: also check completion when a split just fired with the clock paused (final outpost entry).
    if (!run_complete_ && !run_failed_ && !active_list_.goals.empty() &&
        (clock_.IsRunning() || (active_profile_idx_ == 1 && fired > 0))) {
        bool all_done = true;
        for (const auto& g : active_list_.goals) {
            if (g.is_header) continue;
            if (g.status != GoalStatus::Completed) { all_done = false; break; }
        }
        if (all_done) SaveCompletedRun();
    }

    // Keybind edge detection
    auto poll_key = [](int vk, bool& prev) -> bool {
        if (vk <= 0) { prev = false; return false; }
        const bool held  = (GetAsyncKeyState(vk) & 0x8000) != 0;
        const bool fired2 = held && !prev;
        prev = held;
        return fired2;
    };
    if (poll_key(key_start_, key_start_prev_)) StartRun();
    if (poll_key(key_reset_, key_reset_prev_)) ResetRun();
    if (poll_key(key_split_, key_split_prev_)) TriggerManualSplit();
}

// ---------------------------------------------------------------------------
// Timer policy: auto-fail (party wipe, incomplete rezone) and Manual profile auto-start.
// See the header doc comment on ApplyTimerPolicy() for why this is deliberately separate
// from GoalEngine's own fire/complete switch rather than sharing a dispatch table with it.
// ---------------------------------------------------------------------------
void SplitsWindow::ApplyTimerPolicy(const bool just_entered_map, const int player_level)
{
    // ---- Auto-fail conditions ----
    const bool party_defeated = pending_party_defeated_;
    pending_party_defeated_    = false;
    if (party_defeated && ActiveProfile().stop_on_party_defeated && clock_.IsRunning())
        FailRun(); // default reason: "party defeated"

    // A VQ/Mission/Bonus goal was attempted and abandoned (rezoned out of its target map
    // without completing it). Always drained so the flag can't go stale across ticks even
    // when the profile has this behavior turned off.
    if (engine_.ConsumeIncompleteRezone() && ActiveProfile().auto_fail_on_rezone && clock_.IsRunning())
        FailRun("left the area without finishing the objective");

    // ---- Manual/SC profiles: auto-start the clock ----
    // Auto-start when the first goal fires (any trigger, including a manual Split).
    // For Mission/MissionBonus/VanquishComplete/DungeonReward/Titles/MobKill first goals,
    // also start on the earliest sign the attempt has actually begun — not just full
    // completion — so the clock covers the whole attempt instead of only the instant it
    // finishes. Excludes manually_paused_ so a user-initiated pause doesn't get immediately
    // undone.
    //
    // SC=OT here: OT's own run timer auto-starts the instant you load into the relevant
    // explorable (ObjectiveSet::run_start_time_point is set at map-load, not gated on any
    // specific objective/door/quest firing first) — same MapEnter-based early-start rule as
    // Manual's Mission/Bonus/Vanquish, just matched against either the goal's own
    // trigger.map_id (Dungeons) or its owning header's map_id (Elite Areas — see below).
    //
    // NOTE: this progress-detection switch is intentionally separate from GoalEngine's
    // fire/complete switch (GoalEngine.cpp, Pass 2) — Splits (trigger firing) and Timer
    // (clock policy) are deliberately two independent pieces, so some structural
    // duplication here is expected. If you add a trigger type in GoalEntry.h that has a
    // real time gap between "attempt begins" and "attempt completes," add its progress
    // rule here too (see the matching note on GoalTrigger::Type).
    if ((active_profile_idx_ == 0 || active_profile_idx_ == 2) &&
        !clock_.IsRunning() && !run_complete_ && !run_failed_ && !manually_paused_) {
        bool should_start = false;
        // Elite Areas checkpoints (ObjectiveDone/DoorOpen/DisplayDialogue/ServerMessage) carry
        // no map_id of their own — only the auto-created area header does (set when every
        // checkpoint was added via "select all"). Remembered as we walk past each header so
        // the first non-header goal can fall back to it.
        GW::Constants::MapID owning_header_map = GW::Constants::MapID::None;
        for (const auto& g : active_list_.goals) {
            if (g.is_header) {
                owning_header_map = g.trigger.map_id;
                continue;
            }
            if (g.status == GoalStatus::Started || g.status == GoalStatus::Completed) {
                should_start = true;
            } else if (just_entered_map && last_was_explorable_) {
                // last_was_explorable_ (set synchronously from the InstanceLoadInfo packet's
                // is_explorable field) rather than the polled is_explorable/GetInstanceType() —
                // that poll can still reflect the previous instance for a frame right at the
                // transition boundary, which was silently failing this check every time.
                // Some missions' map_id can also be entered as an outpost/staging instance
                // (same MapID, different instance_type) before the explorable unlocks — this
                // still correctly excludes that case.
                using TT = GoalTrigger::Type;
                const auto tt = g.trigger.type;
                if (tt == TT::MissionComplete || tt == TT::MissionBonus || tt == TT::VanquishComplete ||
                    tt == TT::DungeonReward) {
                    if (last_map_ == g.trigger.map_id) should_start = true;
                } else if (owning_header_map != GW::Constants::MapID::None && last_map_ == owning_header_map) {
                    should_start = true;
                }
            } else if (g.trigger.type == GoalTrigger::Type::ReachTitleRank) {
                // No map/zone signal to key off — start the instant any progress toward the
                // title is detected, rather than waiting for the full rank to complete.
                const GW::Title* title = GW::PlayerMgr::GetTitleTrack(g.trigger.title_id);
                if (title && title->current_points > 0) should_start = true;
            } else if (g.trigger.type == GoalTrigger::Type::MobKill) {
                // Same reasoning as Mission/Bonus/Vanquish above: start on the first kill,
                // not after the full trigger.param2 count completes.
                if (g.trigger_progress > 0) should_start = true;
            }
            break; // only check the first non-header goal
        }
        if (should_start) {
            run_char_name_.clear();
            run_char_level_ = player_level;
            run_start_unix_ = static_cast<int64_t>(time(nullptr));
            if (const wchar_t* wname = GW::PlayerMgr::GetPlayerName()) {
                const int sz = WideCharToMultiByte(CP_UTF8, 0, wname, -1, nullptr, 0, nullptr, nullptr);
                if (sz > 1) {
                    run_char_name_.resize(static_cast<size_t>(sz) - 1);
                    WideCharToMultiByte(CP_UTF8, 0, wname, -1, run_char_name_.data(), sz, nullptr, nullptr);
                }
            }
            WebSocketModule::Instance().Send("reset", "Splits: Reset - run starting");
            WebSocketModule::Instance().Send("start", "Splits: Start - first goal fired");
            clock_.Start();
        }
    }
}

void SplitsWindow::ApplySCAutoLoadPreset(const bool just_entered_map)
{
    if (active_profile_idx_ != 2) return;

    // Domain of Anguish, keyed off InstanceLoadFile's file_id (219215 — same magic number
    // ObjectiveTimerWindow.cpp's own CheckIsMapLoaded checks), not the map_id path below: DoA's
    // zone rotation is spawn-dependent, so it can't be a static per-map lookup. Checked
    // independently of just_entered_map since InstanceLoadFile can arrive on a different
    // Update() tick than the InstanceLoadInfo that sets it (server packet order isn't
    // guaranteed) — this is its own one-shot latch instead.
    if (pending_doa_file_id_ == 219215) {
        const GW::Vec2f spawn = pending_doa_spawn_;
        pending_doa_file_id_  = 0; // consume regardless of outcome (incl. Mallyx/no swap)
        if (!clock_.IsRunning()) {
            if (auto doa_preset = SCPresets::BuildDoAPresetList(spawn)) {
                if (active_list_.name != doa_preset->name &&
                    (active_list_.name.empty() || active_list_.is_preset)) {
                    SetActiveList(std::move(*doa_preset));
                    active_list_.is_preset = true;
                }
            }
        }
        return;
    }

    if (!just_entered_map || clock_.IsRunning()) return;

    auto preset = SCPresets::BuildPresetForMap(last_map_);
    if (!preset) return; // not a tracked dungeon/elite area

    if (active_list_.name == preset->name) return; // already the right list loaded

    // Never override a genuine user-authored list — only auto-swap when nothing's loaded yet,
    // or what's loaded is itself just a preset (e.g. left over from an earlier auto-load).
    if (!active_list_.name.empty() && !active_list_.is_preset) return;

    SetActiveList(std::move(*preset));
    // SetActiveList always clears is_preset (a manually-picked list is user-owned) — but this
    // path is auto-detection, not a user pick, so it must stay swappable for the next dungeon/
    // area the player walks into.
    active_list_.is_preset = true;
}

// ---------------------------------------------------------------------------
// Draw
// ---------------------------------------------------------------------------
void SplitsWindow::Draw(IDirect3DDevice9*)
{
    if (!visible) return;
    window_.Draw(*this);
}

// ---------------------------------------------------------------------------
// Settings UI
// ---------------------------------------------------------------------------
void SplitsWindow::DrawSettingsInternal()
{
    window_.DrawSettings(*this);

    // Manual-only feature start to finish — hidden entirely outside Manual rather than shown
    // disabled/inert, so there's no confusion about whether it's doing anything for
    // Running/SC (it never was, but a visible-but-inert checkbox invited exactly that doubt).
    if (active_profile_idx_ == 0) {
        ImGui::Separator();
        ImGui::TextUnformatted("Nuzlocke");
        ImGui::Indent();

        // Death Rules and Points are independent modules — enabling one doesn't require the
        // other. Death Rules draws its roster in its own section below the goal list; Points
        // has no section of its own, its total is shown left-aligned in the header clock row.
        ImGui::Checkbox("Death Rules", &nuzlocke_death_rules_enabled_);
        if (nuzlocke_death_rules_enabled_) {
            ImGui::Indent();
            ImGui::SetNextItemWidth(120.f);
            ImGui::InputInt("Hero lives", &nuzlocke_hero_lives_);
            ImGui::SetNextItemWidth(120.f);
            ImGui::InputInt("Henchman lives", &nuzlocke_hench_lives_);
            if (nuzlocke_hero_lives_  < 1) nuzlocke_hero_lives_  = 1;
            if (nuzlocke_hench_lives_ < 1) nuzlocke_hench_lives_ = 1;

            ImGui::Checkbox("Track own deaths", &nuzlocke_track_self_);
            ImGui::Checkbox("Track other players' deaths", &nuzlocke_track_other_players_);
            if (nuzlocke_track_self_ || nuzlocke_track_other_players_) {
                ImGui::SetNextItemWidth(120.f);
                ImGui::InputInt("Player lives", &nuzlocke_player_lives_);
                if (nuzlocke_player_lives_ < 1) nuzlocke_player_lives_ = 1;
            }

            ImGui::Checkbox("Merge same-named henchmen across campaigns", &nuzlocke_merge_hench_by_name_);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(
                    "Off: a henchman name reused by a different NPC/build in another campaign\n"
                    "or outpost (e.g. two different \"Eve\"s) tracks as a separate entry.\n"
                    "On: any henchman sharing that display name is folded into one entry,\n"
                    "sharing the same life count.\n"
                    "Only affects henchmen tracked from here on, not ones already seen this session.");
            }
            ImGui::Unindent();
        }

        ImGui::Checkbox("Points", &nuzlocke_points_enabled_);
        if (nuzlocke_points_enabled_) {
            ImGui::Indent();
            ImGui::TextDisabled("Leave at 0 for goal types you don't want scored.");
            ImGui::SetNextItemWidth(100.f); ImGui::InputInt("Manual",       &nuzlocke_goal_points_.manual);
            ImGui::SetNextItemWidth(100.f); ImGui::InputInt("Missions",     &nuzlocke_goal_points_.missions);
            ImGui::SetNextItemWidth(100.f); ImGui::InputInt("Explorables",  &nuzlocke_goal_points_.explorables);
            ImGui::SetNextItemWidth(100.f); ImGui::InputInt("Towns",       &nuzlocke_goal_points_.towns);
            ImGui::SetNextItemWidth(100.f); ImGui::InputInt("Titles",      &nuzlocke_goal_points_.titles);
            ImGui::SetNextItemWidth(100.f); ImGui::InputInt("Reach Level", &nuzlocke_goal_points_.reach_level);
            ImGui::SetNextItemWidth(100.f); ImGui::InputInt("Quest",       &nuzlocke_goal_points_.quest);
            ImGui::SetNextItemWidth(100.f); ImGui::InputInt("Skill Learnt", &nuzlocke_goal_points_.skill_learnt);
            ImGui::Unindent();
        }

        ImGui::Unindent();
    }

    ImGui::Separator();
    if (ImGui::CollapsingHeader("Party / Quest Debug Log")) {
        // Off by default — same idea as OT's own "Debug: log events" checkbox
        // (ObjectiveTimerWindow::show_debug_events), just an in-UI list here instead of
        // routed through Log::Info. Nothing is captured into challenge_dbg_events_ at all
        // while this is off (see PushDbgEvent), not just hidden.
        ImGui::Checkbox("Log events", &debug_log_events_);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Records every party/quest/preset event below as it fires.\nUse for debugging and to confirm hooks are actually firing during live testing.");
        if (ImGui::Button("Clear##cdbg")) challenge_dbg_events_.clear();
        ImGui::SameLine();
        ImGui::TextDisabled("(%zu events)", challenge_dbg_events_.size());
        ImGui::TextDisabled("PlyAdd/Rem: v1=player_number  |  HeroAdd: v1=agent_id v2=hero_id  |  HenchAdd: v1=agent_id v2=profession  |  AgentDied: v1=agent_id v2=state  |  QuestUpd: v1=quest_id v2=log_state");
        ImGui::TextDisabled("ObjAdd: v1=objective_id v2=type_flags(0x1=bullet)  |  ObjDone: v1=objective_id v2=map_id  |  ObjStart: v1=objective_id");
        ImGui::TextDisabled("DoorOpen/Close: v1=object_id  |  AgentAllg: v1=player_number v2=allegiance_bits  |  DungeonRwd: (no params)");
        ImGui::TextDisabled("DoAZone: v1=zone message word  |  Countdown: v1=map_id  |  InstLoadFile: v1=file_id v2=spawn.x");
        ImGui::TextDisabled("SrvMsg/DispDlg: v1=pattern length v2=first wchar (not the full pattern)");
        ImGui::BeginChild("##cdbglog", {0, 160}, true, ImGuiWindowFlags_HorizontalScrollbar);
        for (const auto& e : challenge_dbg_events_) {
            ImGui::Text("%-10s  v1=%-6u (0x%04X)  v2=%-6u (0x%04X)",
                e.tag, e.v1, e.v1, e.v2, e.v2);
        }
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.f)
            ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();
    }
}

// ---------------------------------------------------------------------------
// Controls
// ---------------------------------------------------------------------------
void SplitsWindow::StartRun()
{
    if (clock_.IsRunning()) {
        // Pause — leave all run/goal state untouched.
        clock_.Pause();
        manually_paused_    = true;
        manual_pause_accum_ = 0.0;
        return;
    }

    if (manually_paused_) {
        // Resume — keep all progress; just fold the pause into the running total and continue.
        total_paused_real_ += manual_pause_accum_;
        manually_paused_ = false;
        clock_.Resume();
        return;
    }

    // Fresh start.
    engine_.Attach(&active_list_);
    auto wide_to_utf8 = [](const wchar_t* ws) -> std::string {
        if (!ws || !*ws) return {};
        const int sz = WideCharToMultiByte(CP_UTF8, 0, ws, -1, nullptr, 0, nullptr, nullptr);
        if (sz <= 1) return {};
        std::string s(static_cast<size_t>(sz) - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, ws, -1, s.data(), sz, nullptr, nullptr);
        return s;
    };
    run_char_name_.clear();
    run_char_level_ = 0;
    if (const wchar_t* wname = GW::PlayerMgr::GetPlayerName())
        run_char_name_ = wide_to_utf8(wname);
    if (const auto* agent = GW::Agents::GetControlledCharacter())
        run_char_level_ = static_cast<int>(agent->level);
    run_start_unix_ = static_cast<int64_t>(time(nullptr));
    // Full refresh (not just the PB-only rescan SaveCompletedRun() does) so Average/Last Run
    // pick up whatever run just finished — the run about to start should compare against it,
    // even though that run's own "Run complete!" screen deliberately didn't.
    LoadPB();
    WebSocketModule::Instance().Send("reset", "Splits: Reset - run starting");
    WebSocketModule::Instance().Send("start", "Splits: Start - manually started");
    clock_.Start();
    engine_.ForceStarted();
}

void SplitsWindow::ResetRun()
{
    if (clock_.IsRunning() || run_complete_ || run_failed_)
        WebSocketModule::Instance().Send("reset", "Splits: Reset - run reset");
    DeleteResumeState();
    run_complete_              = false;
    run_failed_                = false;
    running_awaiting_movement_ = false;
    running_load_paused_       = false;
    pending_skill_id_          = 0;
    in_mission_queue_          = false;
    manually_paused_           = false;
    manual_pause_accum_        = 0.0;
    total_paused_real_         = 0.0;
    engine_.Reset();
    clock_.Reset();
    pending_map_enter_            = false;
    pending_came_from_explorable_ = false;
    // last_map_ deliberately left untouched: re-entry only fires when InstanceLoadInfo arrives,
    // so the player must leave and re-enter the zone — no spurious MapEnter re-trigger on reset.
    if (NuzlockeDeathRulesEnabled()) NuzlockeResetProgress();
}

void SplitsWindow::TriggerManualSplit()
{
    engine_.TriggerManual(clock_);
}

void SplitsWindow::SwitchProfile(int idx)
{
    if (idx < 0 || idx >= kProfileCount || idx == active_profile_idx_) return;

    if (!active_list_.name.empty())
        profiles_[active_profile_idx_].last_list_name = active_list_.name;

    active_profile_idx_ = idx;

    // Reset run state without clearing last_map_ — resetting it to None would trigger
    // a spurious just_entered_map on the next Update tick, immediately re-loading presets.
    DeleteResumeState();
    run_complete_       = false;
    run_failed_         = false;
    manually_paused_    = false;
    manual_pause_accum_ = 0.0;
    total_paused_real_  = 0.0;
    engine_.Detach();
    engine_.Reset();
    clock_.Reset();

    active_list_ = GoalList{};

    const SplitsProfile& p = ActiveProfile();
    if (!p.last_list_name.empty() && !splits_folder_.empty()) {
        const std::wstring path = ActiveSplitsFolder() +
            std::wstring(p.last_list_name.begin(), p.last_list_name.end()) + L".json";
        if (std::filesystem::exists(path))
            LoadActiveList(path);
    }
}
