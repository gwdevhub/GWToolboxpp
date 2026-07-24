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
    // NOTE: a new type with a real start-to-complete gap (e.g. MissionComplete, MobKill) also needs a progress rule in SplitsWindow::ApplyTimerPolicy() — not compiler-checked.
    enum class Type : uint8_t {
        Manual           = 0,
        // No explorable/outpost distinction; Manual's Explorables/Towns pickers use EnterExplorable/EnterOutpost instead for that.
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
        // param1 = model_id (visible via Info window, no wiki needed), param2 = kill count (0->1); event-driven off AgentDied.
        MobKill                 = 23,
        // Outpost-specific MapEnter — some zones share a map_id between an Outpost and a later Explorable instance (e.g. GNW).
        EnterOutpost            = 24,
    };

    Type                    type      = Type::Manual;
    bool                    hard_mode = false; // MissionComplete/Bonus: require hard mode
    GW::Constants::MapID    map_id    = GW::Constants::MapID::None;
    int                     level     = 0;
    GW::Constants::TitleID  title_id  = GW::Constants::TitleID::None;

    // Preset-only fields (ObjectiveDone, DoorOpen/Close, AgentUpdateAllegiance, DoACompleteZone, ServerMessage, DisplayDialogue).
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
// GoalStatus — lifecycle of a goal within a run (mirrors ObjectiveTimerWindow's Objective::Status, including Failed for aborted runs).
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
    // Dynamic = Start/End/Duration alone, no PB/AVG/Last-Run comparison — for goals with an independent start that doesn't fit relay-chain segment math (parallel triggers, Quest pickup/complete).
    enum class DisplayStyle : uint8_t { Splits = 0, Dynamic = 1 };

    std::string    label;
    GoalTrigger    trigger;
    DisplayStyle   display_style = DisplayStyle::Splits;
    std::optional<GoalTrigger> start_trigger; // records start_real_time when fired (OT-style per-objective start)
    // OR-semantics alternates for start_trigger (preset-only) — e.g. DoA's "360" room, reachable through any of 3 doors.
    std::vector<GoalTrigger> extra_start_triggers;
    bool           starts_immediately = false; // start_real_time = 0 when the run is attached (fires at t=0)
    double         start_real_time = -1.0; // < 0 = not yet fired. Set by GoalEngine.
    double         start_game_time = -1.0;
    std::vector<GoalTrigger> extra_triggers; // OR-semantics alternates for trigger (preset-only)
    int            auto_complete_previous = 0; // preset-only: also complete the previous N goals (-1 = all) on start/complete, mirrors OT's starting_completes_n_previous_objectives
    bool           is_header = false; // headers have no trigger; status/timing derives from descendants (indent > this entry's)
    int            indent    = 0; // Visual/logical depth; headers at N own entries with indent > N.
    GoalStatus     status    = GoalStatus::NotStarted;
    CompletedSplit split     = {};
    int            trigger_progress = 0; // MobKill only: kill count toward trigger.param2, runtime-only, reset by ResetRunState()
};
