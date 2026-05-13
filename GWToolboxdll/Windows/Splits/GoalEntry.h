#pragma once

#include <string>
#include <GWCA/Constants/Maps.h>
#include <GWCA/Constants/Constants.h>

// ---------------------------------------------------------------------------
// GoalTrigger — describes the condition that completes a split.
// ---------------------------------------------------------------------------
struct GoalTrigger {
    enum class Type : uint8_t {
        Manual           = 0,
        MapEnter         = 1,
        EnterExplorable  = 2,
        ExitExplorable   = 3,
        VanquishComplete = 4,
        MissionComplete  = 5,
        MissionBonus     = 6,
        ReachLevel       = 7,
        ExitOutpost      = 8,
        ReachTitleRank   = 9
    };

    Type                    type     = Type::Manual;
    bool                    hard_mode = false; // MissionComplete/Bonus: require hard mode
    GW::Constants::MapID    map_id   = GW::Constants::MapID::None;
    int                     level    = 0;
    GW::Constants::TitleID  title_id = GW::Constants::TitleID::None;
};

// ---------------------------------------------------------------------------
// CompletedSplit — time data recorded when a goal fires.
// ---------------------------------------------------------------------------
struct CompletedSplit {
    double real_time    = 0.0;
    double game_time    = 0.0;
    double segment_real = 0.0;
    double segment_game = 0.0;
};

// ---------------------------------------------------------------------------
// GoalEntry — one row in a split list.
// ---------------------------------------------------------------------------
struct GoalEntry {
    std::string    label;
    GoalTrigger    trigger;
    bool           completed = false;
    CompletedSplit split     = {};
};
