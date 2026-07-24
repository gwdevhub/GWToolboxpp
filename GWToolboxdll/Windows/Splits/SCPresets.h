#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <GWCA/Constants/Maps.h>
#include <GWCA/GameContainers/GamePos.h>

#include "GoalEntry.h"
#include "GoalList.h"

// ---------------------------------------------------------------------------
// SC preset data — shared by SplitsGoalListWindow's interactive pickers and
// SplitsWindow::ApplySCAutoLoadPreset; always built live, never cached to disk.
// Door/objective ids and dialogue patterns are copied verbatim from
// ObjectiveTimerWindow.cpp's own AddFoWObjectiveSet/AddUWObjectiveSet/
// AddUrgozObjectiveSet/AddDeepObjectiveSet.
// ---------------------------------------------------------------------------
namespace SCPresets {

struct EliteCheckpoint {
    const char*        name;
    GoalTrigger::Type  type;
    uint32_t           param1          = 0;
    const wchar_t*     pattern         = nullptr;
    uint32_t           extra_param1_a  = 0;       // second alternative trigger (0 = none)
    uint32_t           extra_param1_b  = 0;       // third alternative trigger (0 = none)
    const wchar_t*     extra_pattern_a = nullptr; // second alternative pattern (nullptr = none)
    // Uses a real MapEnter start_trigger, not starts_immediately (which fires at list-load time regardless of player location — confirmed live as a bug).
    bool               starts_on_area_entry       = false;
    bool               start_on_objective_started = false; // start_trigger = ObjectiveStarted(param1)
};

struct EliteArea {
    const char*             label;
    const EliteCheckpoint*  checkpoints;
    size_t                  count;
    GW::Constants::MapID    map_id; // stamped on the auto-created header (full-set pick) so ApplyTimerPolicy can auto-start/auto-fail on it, same as OT
};

extern const EliteCheckpoint kFow[11]; // OT's AddQuestObjective: every quest gets ObjectiveStarted (start) and ObjectiveDone (end) off the same objective_id
extern const EliteCheckpoint kUw[11]; // same ObjectiveDone/ObjectiveStarted mechanism as FoW, plus a final Dhuum-kill checkpoint (relay off Pools covers its start)
extern const EliteCheckpoint kUrgoz[11]; // each zone's completion is the door that opens the next (OT's AddObjectiveAfterAll chain); Zone 1 is OT's only explicit SetStarted()
extern const EliteCheckpoint kDeep[13]; // Rooms 1-4 are OT's explicit parallel SetStarted()s; relay covers Room 5 onward as a single-file chain

// Fissure of Woe / Underworld / Urgoz's Warren / The Deep. ToPK deliberately excluded — its arenas are map-based (InstanceLoadInfo/CountdownStart), not an objective/door checklist.
extern const EliteArea kEliteAreas[4];

// Straight from ObjectiveTimerWindow::AddObjectiveSet()'s own AddDungeonObjectiveSet calls. levels[0] names the dungeon for Manual's flat picker, which doesn't break dungeons down by level.
struct Dungeon {
    const GW::Constants::MapID* levels;
    size_t                      level_count;
};
extern const Dungeon kDungeons[20];

// Shared by the interactive picker's Add button and the preset generator so both produce identical goals; area_map_id only matters when starts_on_area_entry is set.
GoalEntry BuildCheckpointGoal(const EliteCheckpoint& c, GW::Constants::MapID area_map_id);

// SC only: one goal per level (matching OT's AddDungeonObjectiveSet — each level completes via the next one's MapEnter, only the final level ends on the real DungeonReward chest).
GoalList BuildDungeonPresetList(const Dungeon& dungeon);

// Header + every checkpoint for a full elite area (mirrors the interactive picker's "select all" path, including the header's map_id for autostart/autofail).
GoalList BuildEliteAreaPresetList(const EliteArea& area);

// Builds the preset covering this map (checks every level of every dungeon, not just the first, so level 2+ still resolves), or std::nullopt if unknown. Always builds fresh, nothing to regenerate.
std::optional<GoalList> BuildPresetForMap(GW::Constants::MapID map_id);

// Domain of Anguish: zone rotation is spawn-dependent, so unlike everything else here it's built fresh from the spawn point (nullopt if it's actually the solo "Mallyx" challenge, same map/file_id).
std::optional<GoalList> BuildDoAPresetList(GW::Vec2f spawn);

// Tomb of the Primeval Kings: fixed order (no rotation) matching OT's AddToPKObjectiveSet. First map's entry is EnterExplorable since its map_id is shared with a non-ToPK outpost use.
extern const GW::Constants::MapID kToPKLevels[4];
GoalList BuildToPKPresetList();

} // namespace SCPresets
