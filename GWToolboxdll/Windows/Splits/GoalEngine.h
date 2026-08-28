#pragma once

#include "GoalList.h"
#include "GoalClock.h"

#include <vector>
#include <GWCA/Constants/Maps.h>
#include <GWCA/Constants/Constants.h>

// ---------------------------------------------------------------------------
// GoalEngine — checks conditions each frame, fires splits, tracks state.
// ---------------------------------------------------------------------------
class GoalEngine {
public:
    void Attach(GoalList* list);
    void Detach();

    // Returns the count of goals that fired this tick (0 = none).
    // Ordered types block all subsequent goals until met; unordered types (mission/bonus/title/objective) never block, so several can fire in one tick.
    // is_explorable must be synchronous with just_entered_map, not a live GetInstanceType() poll (can lag a frame and miss the one-shot tick).
    int Update(const GoalClock& clock,
               GW::Constants::MapID current_map,
               bool just_entered_map,
               bool came_from_explorable,
               bool is_explorable,
               int  player_level);

    void TriggerManual(const GoalClock& clock);

    // Arms a pending bonus check (see CheckPendingMissionBonus) instead of reading it synchronously, which produces false positives.
    void NotifyMissionComplete(GW::Constants::MapID map);
    void NotifyVanquishComplete(GW::Constants::MapID map);
    // Tracks the base/primary objective (no BULLET bit); its ObjectiveDone synthesizes MissionComplete+MissionBonus with the real server map_id.
    void NotifyObjectiveAdd(uint32_t obj_id, uint32_t type_flags);

    // Generic event notification for preset-only triggers (DoorOpen, ObjectiveDone, etc.)
    // str is only needed for ServerMessage/DisplayDialogue and must remain valid until Update() runs.
    void NotifyEvent(GoalTrigger::Type type, uint32_t id1 = 0, uint32_t id2 = 0,
                     const wchar_t* str = nullptr, size_t str_len = 0);

    void Reset();
    void ForceStarted();

    // Marks Started goals Failed with a split time; NotStarted/Completed goals are untouched.
    void FailRun(const GoalClock& clock);

    // True if a Started Vanquish/Mission/Bonus goal's map was just left unfinished; detection only, caller decides policy since GoalEngine doesn't know auto_fail_on_rezone. Clears on read.
    [[nodiscard]] bool ConsumeIncompleteRezone();

    // Fires for any profile; only Running's caller-side policy acts on it.
    [[nodiscard]] bool ConsumeWrongMapEntered();

    // TEMPORARY diagnostic for the MissionComplete-not-firing investigation: Pass 2 appends here whenever it evaluates a MissionComplete goal against a non-None mission_complete_map_. Drained (and cleared) by SplitsWindow::Update() into PushDbgEvent right after calling Update() here. Remove once resolved.
    struct DebugNote { const char* tag; uint32_t v1; uint32_t v2; };
    std::vector<DebugNote> debug_notes_;

private:
    void FireGoal(int index, const GoalClock& clock);
    void CheckPendingMissionBonus();
    // Completes any not-yet-completed goals before `index` per its auto_complete_previous.
    void CompletePreviousGoals(int index, const GoalClock& clock);

    struct PendingEvent {
        GoalTrigger::Type type;
        uint32_t          id1;
        uint32_t          id2;
        std::wstring      str; // copy of string data for ServerMessage/DisplayDialogue
    };

    GoalList* list_    = nullptr;
    bool      started_ = false;

    GW::Constants::MapID prev_map_ = GW::Constants::MapID::None;

    double last_real_ = 0.0;
    double last_game_ = 0.0;

    GW::Constants::MapID mission_complete_map_  = GW::Constants::MapID::None;
    GW::Constants::MapID mission_bonus_map_     = GW::Constants::MapID::None;
    GW::Constants::MapID vanquish_complete_map_ = GW::Constants::MapID::None;
    // No timeout — a genuinely-unearned bonus just stays pending harmlessly for the rest of the run.
    GW::Constants::MapID pending_bonus_check_map_ = GW::Constants::MapID::None;
    // Base/primary objective id (no BULLET bit); its ObjectiveDone sets both mission_complete_map_/mission_bonus_map_ via the real server map_id.
    uint32_t             primary_obj_id_        = 0;
    // See ConsumeIncompleteRezone().
    bool                 pending_incomplete_rezone_ = false;
    // See ConsumeWrongMapEntered().
    bool                 pending_wrong_map_entered_ = false;
    // One-shot: lets the first Enter-type goal fire if you're already standing on its map when the run starts/resumes, since a real zone-transition edge will never come.
    bool                 pending_run_start_ = false;

    std::vector<PendingEvent> pending_events_;
};
