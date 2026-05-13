#pragma once

#include "GoalList.h"
#include "GoalClock.h"

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
               bool vq_complete,
               int  player_level);

    void TriggerManual(const GoalClock& clock);

    void NotifyMissionComplete(GW::Constants::MapID map);
    void NotifyMissionBonus(GW::Constants::MapID map);

    void Reset();
    void ForceStarted();

    [[nodiscard]] bool HasList()   const { return list_ != nullptr; }
    [[nodiscard]] bool IsStarted() const { return started_; }
    [[nodiscard]] GoalList* List() const { return list_; }

private:
    void FireGoal(int index, const GoalClock& clock);

    GoalList* list_    = nullptr;
    bool      started_ = false;

    GW::Constants::MapID prev_map_ = GW::Constants::MapID::None;

    double last_real_ = 0.0;
    double last_game_ = 0.0;

    GW::Constants::MapID mission_complete_map_ = GW::Constants::MapID::None;
    GW::Constants::MapID mission_bonus_map_    = GW::Constants::MapID::None;
};
