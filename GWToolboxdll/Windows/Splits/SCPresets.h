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
// SC preset data — one source of truth shared by SplitsGoalListWindow's
// interactive Dungeons/Elite Areas pickers and SplitsWindow::ApplySCAutoLoadPreset,
// so both build identical goals from the same table rather than keeping two
// copies in sync by hand. Presets are always built live from this data, never
// cached to disk — a fix here takes effect on the very next run, with nothing
// to regenerate.
//
// Elite Area checkpoint data is reduced from ObjectiveTimerWindow's own preset
// definitions down to "what fires when this segment is done" (not replicating
// OT's own parallel-start tracking) — see SplitsGoalListWindow.cpp's original
// DrawEliteAreaBatchPicker banner comment for the full rationale. Door ids,
// objective ids, and encoded dialogue/message patterns are copied verbatim
// from ObjectiveTimerWindow.cpp's AddFoWObjectiveSet/AddUWObjectiveSet/
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
    // Without one of these two, start_real_time never gets set at all for this checkpoint
    // (relay only sets the *next* goal's start when *this* one completes) — Dynamic display
    // would show "--:--" forever even once the checkpoint completes. OT itself always gives
    // every quest objective its own ObjectiveStarted start event (AddQuestObjective) and only
    // creates a zone/room's parallel-start Objectives once its ObjectiveSet is created — which
    // itself only happens on actually loading into that map. starts_on_area_entry mirrors that:
    // start_trigger = MapEnter(the owning area's own map_id), NOT an immediate/unconditional
    // start — a plain "starts_immediately" flag would mark this Started the instant the list is
    // merely *loaded* (GoalEngine::Attach()), regardless of where the player actually is, which
    // is wrong for anything gated on being in the right place (confirmed live: a dungeon/area
    // preset sitting loaded while still in town was firing its autostart from town).
    bool               starts_on_area_entry       = false;
    bool               start_on_objective_started = false; // start_trigger = ObjectiveStarted(param1)
};

struct EliteArea {
    const char*             label;
    const EliteCheckpoint*  checkpoints;
    size_t                  count;
    GW::Constants::MapID    map_id; // stamped onto the auto-created header (full-set pick) so
                                     // ApplyTimerPolicy can auto-start/auto-fail on it, same as OT
};

// FoW: OT's AddQuestObjective gives every quest both ObjectiveStarted (start) and
// ObjectiveDone (end) off the same objective_id.
extern const EliteCheckpoint kFow[11];
// Same ObjectiveDone/ObjectiveStarted mechanism as FoW, plus a final Dhuum-kill checkpoint
// (no separate start event for Dhuum in OT either — relay off Pools completing covers it).
extern const EliteCheckpoint kUw[11];
// Each zone's "completion" is the door that opens the next one (OT chains these via
// AddObjectiveAfterAll, which auto-completes everything before it when the next zone's start
// event fires). Zone 1 is OT's only explicit SetStarted(); every zone after relays off the
// previous zone's own completion — Urgoz's zones are a genuine single-file chain.
extern const EliteCheckpoint kUrgoz[11];
// Each room's "completion" is whatever starts the next one. Rooms 1-4 are OT's explicit
// SetStarted()s (parallel — all four are live simultaneously); relay covers everything from
// Room 5 onward, a genuine single-file chain from there.
extern const EliteCheckpoint kDeep[13];

// Fissure of Woe / Underworld / Urgoz's Warren / The Deep. ToPK deliberately excluded — its
// arenas are map-based (InstanceLoadInfo/CountdownStart per map), not an objective/door
// checklist, so it doesn't fit this shape; needs its own approach.
extern const EliteArea kEliteAreas[4];

// Every level's map_id, in order, straight from ObjectiveTimerWindow::AddObjectiveSet()'s own
// AddDungeonObjectiveSet({...}) calls. levels[0] is used purely to name/identify the dungeon
// for Manual's flat picker (which — deliberately, unlike SC — doesn't break dungeons down by
// level at all; see DrawDungeonBatchPicker).
struct Dungeon {
    const GW::Constants::MapID* levels;
    size_t                      level_count;
};
extern const Dungeon kDungeons[20];

// Builds one GoalEntry from a checkpoint definition — shared by the interactive picker's Add
// button and the preset generator so both produce byte-identical goals. area_map_id is only
// used when the checkpoint has starts_on_area_entry set (the area's own map_id becomes that
// checkpoint's start_trigger); harmless to pass for any other checkpoint.
GoalEntry BuildCheckpointGoal(const EliteCheckpoint& c, GW::Constants::MapID area_map_id);

// SC only: one goal per level (header + children for multi-level dungeons, matching OT's own
// AddDungeonObjectiveSet exactly — each level completes the moment the next one starts via
// MapEnter, only the final level's completion is the actual DungeonReward chest). Single-level
// dungeons collapse to one flat DungeonReward goal, same as Manual's picker, since there's
// nothing to break down.
GoalList BuildDungeonPresetList(const Dungeon& dungeon);

// Header + every checkpoint preset list for a full elite area (mirrors the interactive
// picker's "select all" path exactly, including the header's map_id for autostart/autofail).
GoalList BuildEliteAreaPresetList(const EliteArea& area);

// Builds the preset covering this map (matching against every level of every dungeon, not just
// each one's first, so this still resolves correctly on level 2+ of a multi-level dungeon; then
// every elite area's own map_id), or std::nullopt if map_id isn't part of any known dungeon/
// elite area. Used by SplitsWindow::ApplySCAutoLoadPreset — always builds fresh from the tables
// above, so a logic fix here applies immediately with no stale cached list to regenerate.
std::optional<GoalList> BuildPresetForMap(GW::Constants::MapID map_id);

// Domain of Anguish: zone rotation is spawn-dependent (varies by which of 4 area entrances you
// land nearest — Foundry/City/Veil/Gloom in some rotated order), so unlike everything else here
// it can't be a static table. Built fresh from the InstanceLoadFile spawn point every time (same
// nearest-neighbor check OT's own AddDoAObjectiveSet does), full room-level detail matching OT's
// AddDoAObjectiveSet exactly (not reduced to top-level zones only). Returns std::nullopt if the
// spawn is nearest the separate solo "Mallyx the Unfathomable" challenge instead (shares the
// same map/file_id as DoA, not DoA itself).
std::optional<GoalList> BuildDoAPresetList(GW::Vec2f spawn);

// Tomb of the Primeval Kings: fixed order (unlike DoA, no rotation) — The Underworld (PvP) ->
// Scarred Earth -> The Courtyard -> The Hall of Heroes, each completing on that same map's own
// CountdownStart (matches OT's own AddToPKObjectiveSet exactly). The first map's entry is
// EnterExplorable rather than plain MapEnter since it's shared with a non-ToPK outpost use
// elsewhere (same reason OT itself gates AddToPKObjectiveSet behind an explorable-only check).
extern const GW::Constants::MapID kToPKLevels[4];
GoalList BuildToPKPresetList();

} // namespace SCPresets
