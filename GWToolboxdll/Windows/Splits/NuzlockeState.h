#pragma once

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <GWCA/Constants/Constants.h>

namespace GuiUtils {
    class EncString;
}

// v1: roster is built lazily from whichever heroes/henchmen we've actually seen this session; no pre-seeded campaign roster. Deaths only count in explorables.
struct NuzlockeMember {
    std::wstring name;
    int          deaths = 0;
    // Heroes and henchmen only (real players don't get an icon); read once at NuzlockeOnHenchAdd/NuzlockeOnHeroAdd time since the agent's profession byte is available immediately even though henchman names decode async.
    GW::Constants::Profession profession = GW::Constants::Profession::None;
};

struct NuzlockeIdentity {
    bool                      is_hero = false;
    GW::Constants::HeroID     hero_id{};
    std::wstring              hench_name;
    GW::Constants::Profession hench_profession = GW::Constants::Profession::None;
};

// Point value per goal category, applied when a goal of that category completes. Global (not per-list) — a non-Nuzlocke list just scores 0 since these all default to 0. Fields mirror the Add Goal trigger dropdown.
struct NuzlockePointValues {
    int manual       = 0;
    int missions     = 0; // MissionComplete + MissionBonus
    int explorables  = 0; // MapEnter
    int towns        = 0; // EnterExplorable, ExitExplorable, ExitOutpost
    int titles       = 0; // ReachTitleRank
    int reach_level  = 0;
    int quest        = 0; // QuestPickup + QuestComplete
    int skill_learnt = 0;
};

// All Death Rules + Points settings and runtime tracking for SplitsWindow's Nuzlocke feature.
struct NuzlockeState {
    // Out-of-line (SplitsWindow.cpp): pending_hench_names/city_hench_names hold unique_ptr<GuiUtils::EncString>, only forward-declared here.
    NuzlockeState();
    ~NuzlockeState();

    // ---- Death Rules settings ----
    bool death_rules_enabled = false;
    int  hero_lives          = 1;
    int  hench_lives         = 1;
    bool track_self          = false;
    bool track_other_players = false;
    int  player_lives        = 1;
    // Same real-world henchman name (e.g. "Eve") can be a different NPC/build with a different bracket role-text elsewhere. Off keeps each raw name+role distinct ("campaign separation"); on folds them into one tracked entry by display name ("same character").
    bool merge_hench_by_name = false;

    // ---- Death Rules runtime state ----
    std::map<GW::Constants::HeroID, NuzlockeMember> heroes;
    std::map<std::wstring, NuzlockeMember>          henches;
    // Real players (self and/or others), keyed by character name; resolved directly from the agent at time of death (player names aren't encoded/localized like hero names, so no pre-registration or async decode needed).
    std::map<std::wstring, NuzlockeMember>          players;
    // agent_id -> identity, only while that agent is actually in the party.
    std::unordered_map<uint32_t, NuzlockeIdentity>  agents;
    // agent_ids already counted as dead — guards against AgentState re-firing the dead bit more than once for the same death. Cleared on new instance load.
    std::unordered_set<uint32_t>                    dead_agents;
    // Henchman names decode asynchronously; polled from Update() until ready.
    std::vector<std::pair<uint32_t, std::unique_ptr<GuiUtils::EncString>>> pending_hench_names;
    // Hireable henchmen in the current outpost, keyed by agent_id so we don't re-issue a decode request for one already resolved. Cleared on every instance load since agent_ids aren't stable across instances.
    std::unordered_map<uint32_t, std::unique_ptr<GuiUtils::EncString>> city_hench_names;
    // Stripped display-names of henchmen hireable in THIS outpost right now — recomputed only when the id list changes or something's still unresolved, not unconditionally every tick. Empty in explorables (hireable roster is a town-only concept).
    std::unordered_set<std::wstring> city_hench_available;
    // Skip-check for the above: last frame's raw henchmen_agent_ids plus whether every one had a resolved profession icon. Never skips while anything's unresolved, so a hireable henchman's icon keeps retrying until AgentLiving::primary populates rather than getting stuck blank. Reset on instance load since agent_ids aren't stable across instances.
    std::vector<uint32_t> last_town_hench_ids;
    bool                  town_hench_all_resolved = false;

    // ---- Points ----
    bool                 points_enabled = false;
    NuzlockePointValues  goal_points;
};
