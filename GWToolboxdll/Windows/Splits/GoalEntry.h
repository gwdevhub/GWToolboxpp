#pragma once

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Maps.h>

#include <optional>
#include <string>
#include <vector>

struct GoalTrigger {
    // New type with start->done gap? Add progress rule in ApplyTimerPolicy too.
    enum class Type : uint8_t {
        Manual           = 0,
        // Any instance type. Pickers use Enter* instead.
        MapEnter         = 1,
        EnterExplorable  = 2,
        ExitExplorable   = 3,
        VanquishComplete = 4,
        MissionComplete  = 5,
        MissionBonus     = 6,
        ReachLevel       = 7,
        ExitOutpost      = 8,
        ReachTitleRank   = 9,

        // Preset only.
        ObjectiveDone          = 10, // param1 = objective_id
        DoorOpen               = 11, // param1 = object_id
        DoorClose              = 12, // param1 = object_id
        AgentUpdateAllegiance  = 13, // param1 = player_number, param2 = allegiance_bits
        DoACompleteZone        = 14, // param1 = zone message word
        DungeonReward          = 15, // map_id = any level of the dungeon; chest event carries the map it opened in
        ServerMessage          = 16, // pattern = encoded wchar_t prefix to match
        DisplayDialogue        = 17, // pattern = encoded wchar_t prefix to match
        CountdownStart         = 18, // param1 = map_id (ToPK arena countdown)
        ObjectiveStarted       = 19, // param1 = objective_id (kObjectiveUpdated UI message)

        // User enter by ID.
        QuestPickup            = 20, // param1 = quest_id
        QuestComplete          = 21, // param1 = quest_id. Abandon fire too.
        SkillLearnt            = 22, // param1 = skill_id. Polled.
        // pattern = mob name (any case). param1 = model_id (old lists). param2 = kills.
        MobKill                 = 23,
        // Same map_id can be outpost then explorable (GNW).
        EnterOutpost            = 24,
    };

    Type                    type      = Type::Manual;
    bool                    hard_mode = false; // Mission/Bonus: need HM
    GW::Constants::MapID    map_id    = GW::Constants::MapID::None;
    int                     level     = 0;
    GW::Constants::TitleID  title_id  = GW::Constants::TitleID::None;

    // Preset only.
    uint32_t     param1  = 0;
    uint32_t     param2  = 0;
    std::wstring pattern;
};

struct CompletedSplit {
    double real_time    = 0.0;
    double game_time    = 0.0;
    double segment_real = 0.0;
    double segment_game = 0.0;
};

enum class GoalStatus : uint8_t {
    NotStarted = 0,
    Started    = 1,
    Completed  = 2,
    Failed     = 3,
};

struct GoalEntry {
    // Dynamic: Start/End/Duration, no PB compare. For goals with own start.
    enum class DisplayStyle : uint8_t { Splits = 0, Dynamic = 1 };

    std::string    label;
    GoalTrigger    trigger;
    DisplayStyle   display_style = DisplayStyle::Splits;
    std::optional<GoalTrigger> start_trigger;
    // OR alternates, preset only (DoA 360: 3 doors).
    std::vector<GoalTrigger> extra_start_triggers;
    bool           starts_immediately = false; // start t=0 on attach. Not saved.
    double         start_real_time = -1.0; // < 0 = not fired
    double         start_game_time = -1.0;
    std::vector<GoalTrigger> extra_triggers; // OR alternates, preset only
    int            auto_complete_previous = 0; // preset only: also finish N previous (-1 = all)
    bool           is_header = false; // no trigger. Status from children.
    int            indent    = 0; // header at N owns children > N
    GoalStatus     status    = GoalStatus::NotStarted;
    CompletedSplit split     = {};
    int            trigger_progress = 0; // MobKill kills so far. Runtime only.
};
