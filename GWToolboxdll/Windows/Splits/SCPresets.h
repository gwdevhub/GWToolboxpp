#pragma once

#include <GWCA/Constants/Maps.h>

#include <GWCA/GameContainers/GamePos.h>

#include <Windows/Splits/GoalEntry.h>
#include <Windows/Splits/GoalList.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace SCPresets {

struct EliteCheckpoint {
    const char*        name;
    GoalTrigger::Type  type;
    uint32_t           param1          = 0;
    const wchar_t*     pattern         = nullptr;
    uint32_t           extra_param1_a  = 0;       // 0 = none
    uint32_t           extra_param1_b  = 0;       // 0 = none
    const wchar_t*     extra_pattern_a = nullptr; // nullptr = none
    // Real MapEnter start, not starts_immediately: that fire at list load wherever player is.
    bool               starts_on_area_entry       = false;
    bool               start_on_objective_started = false;
};

struct EliteArea {
    const char*             label;
    const EliteCheckpoint*  checkpoints;
    size_t                  count;
    GW::Constants::MapID    map_id; // on header: ApplyTimerPolicy autostart/autofail
};

extern const EliteCheckpoint kFow[11];
extern const EliteCheckpoint kUw[11];
extern const EliteCheckpoint kUrgoz[11]; // Zone 1 only explicit start; rest chain off doors
extern const EliteCheckpoint kDeep[13]; // Rooms 1-4 parallel start; 5+ relay

// ToPK not here: map-based arenas, not objective checklist.
extern const EliteArea kEliteAreas[4];

struct Dungeon {
    // Static, not GetMapName: async (often "" here) + language dependent, and name key history file.
    const char*                 name;
    const GW::Constants::MapID* levels;
    size_t                      level_count;
};
extern const Dungeon kDungeons[20];
// Dungeon owning map_id (any level), or nullptr.
[[nodiscard]] const Dungeon* FindDungeon(GW::Constants::MapID map_id);

GoalEntry BuildCheckpointGoal(const EliteCheckpoint& c, GW::Constants::MapID area_map_id);

// Each level done on next level MapEnter; last on DungeonReward.
GoalList BuildDungeonPresetList(const Dungeon& dungeon);

GoalList BuildEliteAreaPresetList(const EliteArea& area);

// Check every level, so level 2+ resolve.
std::optional<GoalList> BuildPresetForMap(GW::Constants::MapID map_id);

// -1 = Mallyx (not DoA), else 0-3 (Foundry, City, Veil, Gloom).
int DetectDoAStartingZone(GW::Vec2f spawn);
// Fixed zone order whatever the rotation; the starting zone is started at runtime (DoAZoneFirstGoal).
GoalList BuildDoAPresetList();
// First non-header goal under that zone's header, or -1 (e.g. a variant with the zone removed).
int DoAZoneFirstGoal(const GoalList& list, int zone);
// "Foundry"/"City"/"Veil"/"Gloom", or "" for -1/out of range.
const char* DoAZoneName(int zone);

// First map shared with non-ToPK outpost: EnterExplorable.
extern const GW::Constants::MapID kToPKLevels[4];
GoalList BuildToPKPresetList();

// Header map_id (or lone goal's). None = not preset shape.
GW::Constants::MapID AnchorMapId(const GoalList& list);

} // namespace SCPresets
