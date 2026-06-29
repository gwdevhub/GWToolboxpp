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

    // Returns the index of a goal that just completed this frame, or -1.
    int Update(const GoalClock& clock,
               GW::Constants::MapID current_map,
               bool just_entered_map,
               bool came_from_explorable,
               GW::Constants::InstanceType instance_type,
               int  player_level);

    void TriggerManual(const GoalClock& clock);

    void NotifyMissionComplete(GW::Constants::MapID map);
    void NotifyMissionBonus(GW::Constants::MapID map);
    void NotifyVanquishComplete(GW::Constants::MapID map);

    // Generic event notification for preset-only triggers (DoorOpen, ObjectiveDone, etc.)
    // str is only needed for ServerMessage/DisplayDialogue and must remain valid until Update() runs.
    void NotifyEvent(GoalTrigger::Type type, uint32_t id1 = 0, uint32_t id2 = 0,
                     const wchar_t* str = nullptr, size_t str_len = 0);

    void Reset();
    void ForceStarted();

    // Marks every goal currently Started as Failed and records its split times.
    // Goals that never started (NotStarted) and goals already Completed are untouched.
    void FailRun(const GoalClock& clock);

    [[nodiscard]] bool HasList()   const { return list_ != nullptr; }
    [[nodiscard]] bool IsStarted() const { return started_; }
    [[nodiscard]] GoalList* List() const { return list_; }

private:
    void FireGoal(int index, const GoalClock& clock);
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

    std::vector<PendingEvent> pending_events_;
};
