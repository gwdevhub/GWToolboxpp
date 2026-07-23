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
    // NOTE: adding a type here only teaches GoalEngine.cpp's Pass 2 switch how to *complete*
    // it. If the new type also has a real time gap between "the attempt begins" and "it
    // completes" (like MissionComplete or MobKill), the Manual profile's clock won't
    // auto-start until full completion unless you also add a progress rule to
    // SplitsWindow::ApplyTimerPolicy()'s should_start switch. These two switches are
    // deliberately separate (Splits = trigger firing, Timer = clock policy) rather than
    // sharing one dispatch table, so this won't be caught by the compiler — update both.
    enum class Type : uint8_t {
        Manual           = 0,
        // Enter this map_id, any instance type — no explorable/outpost distinction. Sequential
        // mode (Running profile) ignores map_id entirely and fires on any transition, so this
        // ambiguity doesn't matter there; it's what DrawRunningLegPicker still produces. The
        // Manual profile's Explorables/Towns pickers use EnterExplorable/EnterOutpost instead,
        // which do check instance type.
        MapEnter         = 1,
        EnterExplorable  = 2, // enter this map_id specifically as an Explorable instance — produced by the "Explorables" batch picker
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

        // User-editable, manual ID entry (no master list exists for either).
        QuestPickup            = 20, // param1 = quest_id; fires once off QuestAdd (event-driven)
        QuestComplete          = 21, // param1 = quest_id; fires off QuestRemove (event-driven, also fires on abandon)
        SkillLearnt            = 22, // param1 = skill_id; polled via GetIsSkillLearnt
        // param1 = model_id (AgentLiving::player_number for an NPC — visible live via the
        // Info window's "Model ID" field, no wiki lookup needed). param2 = required kill
        // count (0 treated as 1, so "boss kill" is just this trigger with an implicit count
        // of 1). Event-driven off AgentDied; accumulates in GoalEntry::trigger_progress
        // across ticks/runs until it reaches param2.
        MobKill                 = 23,
        // Enter this map_id specifically as an Outpost instance (not Explorable). Completes
        // the Enter/Exit x Explorable/Outpost set — some zones share the same map_id between
        // a town/mission-staging Outpost instance and a later Explorable instance (e.g. The
        // Great Northern Wall), so plain MapEnter (no instance-type check) can't tell them
        // apart. Produced by the "Towns" batch picker instead of MapEnter.
        EnterOutpost            = 24,
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
    // Splits = cumulative time + segment, each compared against PB/Average/Last Run
    // (SplitsProfile::comparison_mode) — the normal speedrun-split look. Dynamic = Start/End/
    // Duration for this goal specifically (start(pickup)/end(complete)/split); no history
    // comparison, since a goal with its own independent start (parallel start_trigger, e.g.
    // Deep's simultaneous rooms; or a Quest's Pickup/Complete pair) doesn't fit the "vs
    // previous row" segment math a relay-chain list relies on. Per-goal rather than
    // per-list/per-profile so this can sit inside an otherwise normal Manual list — used by
    // Quest goals today, and by imported OT objective sets once that exists.
    enum class DisplayStyle : uint8_t { Splits = 0, Dynamic = 1 };

    std::string    label;
    GoalTrigger    trigger;
    DisplayStyle   display_style = DisplayStyle::Splits;
    // Optional start trigger: when fired records start_real_time (OT-style per-objective start).
    std::optional<GoalTrigger> start_trigger;
    // Optional additional start triggers with OR semantics (preset-only; any one firing starts
    // this goal, same idea as extra_triggers below but for start_trigger — e.g. Domain of
    // Anguish's "360" room, reachable through any of 3 doors).
    std::vector<GoalTrigger> extra_start_triggers;
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
    // MobKill only: cumulative kill count toward trigger.param2. Runtime-only (not
    // serialized to the list file, not restored on resume) — reset by ResetRunState().
    int            trigger_progress = 0;
};
