#pragma once

#include <optional>
#include <string>
#include <vector>
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
        ReachTitleRank   = 9,

        // Preset-only triggers (used by built-in elite/dungeon presets; not user-editable)
        ObjectiveDone          = 10, // param1 = objective_id
        DoorOpen               = 11, // param1 = object_id
        DoorClose              = 12, // param1 = object_id
        AgentUpdateAllegiance  = 13, // param1 = player_number, param2 = allegiance_bits
        DoACompleteZone        = 14, // param1 = zone message word
        DungeonReward          = 15, // no params — dungeon chest opened
        ServerMessage          = 16, // pattern = encoded wchar_t prefix to match
        DisplayDialogue        = 17, // pattern = encoded wchar_t prefix to match
        CountdownStart         = 18, // param1 = map_id (ToPK arena countdown)
        ObjectiveStarted       = 19, // param1 = objective_id (ObjectiveUpdateName packet)
    };

    Type                    type      = Type::Manual;
    bool                    hard_mode = false; // MissionComplete/Bonus: require hard mode
    GW::Constants::MapID    map_id    = GW::Constants::MapID::None;
    int                     level     = 0;
    GW::Constants::TitleID  title_id  = GW::Constants::TitleID::None;

    // Preset-only fields (used by ObjectiveDone, DoorOpen/Close, AgentUpdateAllegiance,
    // DoACompleteZone, ServerMessage, DisplayDialogue).
    uint32_t     param1  = 0;
    uint32_t     param2  = 0;
    std::wstring pattern; // for ServerMessage/DisplayDialogue prefix matching
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
// GoalStatus — lifecycle of a goal within a run (mirrors ObjectiveTimerWindow's
// Objective::Status, including the Failed state for aborted runs).
// ---------------------------------------------------------------------------
enum class GoalStatus : uint8_t {
    NotStarted = 0,
    Started    = 1,
    Completed  = 2,
    Failed     = 3,
};

// ---------------------------------------------------------------------------
// GoalEntry — one row in a split list.
// ---------------------------------------------------------------------------
struct GoalEntry {
    std::string    label;
    GoalTrigger    trigger;
    // Optional start trigger: when fired records start_real_time (OT-style per-objective start).
    std::optional<GoalTrigger> start_trigger;
    // If true, start_real_time is set to 0 when the run is attached (fires immediately at t=0).
    bool           starts_immediately = false;
    // < 0 means the start event has not yet fired. Set by GoalEngine.
    double         start_real_time = -1.0;
    double         start_game_time = -1.0;
    // Optional additional triggers with OR semantics (preset-only; any one firing completes this goal).
    std::vector<GoalTrigger> extra_triggers;
    // Preset-only: when this goal starts or completes, also complete the previous N
    // not-yet-completed goals (-1 = all previous). Mirrors OT's
    // starting_completes_n_previous_objectives.
    int            auto_complete_previous = 0;
    // Subsplit grouping: is_header entries have no trigger; their status/timing is derived
    // from all descendants (entries with indent > this entry's indent).
    bool           is_header = false;
    int            indent    = 0; // Visual/logical depth; headers at N own entries with indent > N.
    GoalStatus     status    = GoalStatus::NotStarted;
    CompletedSplit split     = {};
};
