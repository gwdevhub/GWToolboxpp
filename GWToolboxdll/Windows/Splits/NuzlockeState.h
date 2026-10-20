#pragma once

#include <GWCA/Constants/Constants.h>

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace GuiUtils {
    class EncString;
}

struct GoalList;

// Roster built from what seen this session. Deaths count in explorables only.
struct NuzlockeMember {
    std::wstring name;
    int          deaths = 0;
    // Heroes/henches only. Profession ready at once, names decode later.
    GW::Constants::Profession profession = GW::Constants::Profession::None;
};

struct NuzlockeIdentity {
    bool                      is_hero = false;
    GW::Constants::HeroID     hero_id{};
    std::wstring              hench_name;
    GW::Constants::Profession hench_profession = GW::Constants::Profession::None;
};

// Flat so the settings registry indexes every field (search, /tb_setting). Points: global, 0 = no score.
struct NuzlockeSettings {
    bool death_tracker_enabled = false;
    int  hero_lives          = 1;
    int  hench_lives         = 1;
    int  player_lives        = 1;
    // Same-name hench in other campaign has other agent_id.
    bool merge_hench_by_name = false;

    bool points_enabled      = false;
    int  points_manual       = 0;
    int  points_missions     = 0; // MissionComplete + MissionBonus
    int  points_explorables  = 0; // Enter/Exit Explorable, VQ, old MapEnter
    int  points_towns        = 0; // Enter/Exit Outpost
    int  points_titles       = 0;
    int  points_reach_level  = 0;
    int  points_quest        = 0; // QuestPickup + QuestComplete
    int  points_skill_learnt = 0;
};

struct NuzlockeState {
    // Out-of-line: EncString only forward-declared here.
    NuzlockeState();
    ~NuzlockeState();

    NuzlockeSettings settings;

    std::map<GW::Constants::HeroID, NuzlockeMember> heroes;
    std::map<std::wstring, NuzlockeMember>          henches;
    // Player names not encoded: read from agent at death.
    std::map<std::wstring, NuzlockeMember>          players;
    std::unordered_map<uint32_t, NuzlockeIdentity>  agents;
    // Counted dead agents. Leave on rezz so next death cost life. Clear on load.
    std::unordered_set<uint32_t>                    dead_agents;
    // Member so buckets reused each frame.
    std::unordered_set<uint32_t>                    live_agents_scratch;
    std::vector<std::pair<uint32_t, std::unique_ptr<GuiUtils::EncString>>> pending_hench_names;
    // agent_ids not stable across instances: clear on load.
    std::unordered_map<uint32_t, std::unique_ptr<GuiUtils::EncString>> city_hench_names;
    // Town only. Recompute when ids change or something unresolved.
    std::unordered_set<std::wstring> city_hench_available;
    // Never skip while unresolved, so icon keep retry till primary set.
    std::vector<uint32_t> last_town_hench_ids;
    bool                  town_hench_all_resolved = false;

    void OnInstanceLoad();
    // Pre-mark as counted before poll see resign deaths.
    void OnPartyResigned();
    // Keep agents so rosters reseed now, not next zone.
    void ResetProgress();
    // last_was_explorable must be CURRENT map.
    void Update(bool last_was_explorable);
    void Draw();
    [[nodiscard]] int TotalPoints(const GoalList& list) const;

private:
    [[nodiscard]] std::wstring HenchKey(const std::wstring& raw_name) const;
};
