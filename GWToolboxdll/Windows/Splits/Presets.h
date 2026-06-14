#pragma once

#include "GoalList.h"

#include <optional>
#include <vector>

#include <GWCA/Constants/Maps.h>
#include <GWCA/GameContainers/GamePos.h>

// ---------------------------------------------------------------------------
// Presets — builds hardcoded GoalList objects mirroring the objective sets
// from ObjectiveTimerWindow (FoW, UW, Deep, Urgoz, DoA, dungeons).
// ---------------------------------------------------------------------------
namespace Presets {
    // Returns a ready-to-use GoalList for the given map, or nullopt if the
    // map has no preset.  For Domain_of_Anguish, 'spawn' determines which
    // area starts first (Mallyx spawn returns nullopt).
    std::optional<GoalList> GetPresetForMap(GW::Constants::MapID map_id, GW::Vec2f spawn);
} // namespace Presets
