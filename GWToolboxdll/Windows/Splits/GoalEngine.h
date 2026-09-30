#pragma once

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Maps.h>

#include <Windows/Splits/GoalClock.h>
#include <Windows/Splits/GoalList.h>

#include <set>
#include <vector>

class GoalEngine {
public:
    void Attach(GoalList* list);
    void Detach();

    // Returns goals fired this tick. Ordered types block later goals; unordered never block.
    // is_explorable must come with just_entered_map, not GetInstanceType() poll (lag one frame, miss tick).
    int Update(const GoalClock& clock,
               GW::Constants::MapID current_map,
               bool just_entered_map,
               bool came_from_explorable,
               bool is_explorable,
               int  player_level,
               float delta);

    void TriggerManual(const GoalClock& clock);

    // Arm bonus poll, not read now: read now give false positive.
    void NotifyMissionComplete(GW::Constants::MapID map);
    void NotifyVanquishComplete(GW::Constants::MapID map);
    // Primary objective (no BULLET bit). Its ObjectiveDone give real map_id.
    void NotifyObjectiveAdd(uint32_t obj_id, uint32_t type_flags);

    // str only for ServerMessage/DisplayDialogue. Copied.
    void NotifyEvent(GoalTrigger::Type type, uint32_t id1 = 0, uint32_t id2 = 0,
                     const wchar_t* str = nullptr, size_t str_len = 0);

    void Reset();
    void ForceStarted();
    // Resume: Attach/Reset zero baseline, put back.
    void RestoreLastSplit(double real, double game) { last_real_ = real; last_game_ = game; }

    void FailRun(const GoalClock& clock);

    // Started VQ/Mission/Bonus map left unfinished. Caller pick policy. Clear on read.
    [[nodiscard]] bool ConsumeIncompleteRezone();

    [[nodiscard]] bool ConsumeWrongMapEntered();

private:
    void FireGoal(int index, const GoalClock& clock);
    void StampSplit(GoalEntry& g, const GoalClock& clock) const;
    // Poll once/sec: CompletionWindow scan not free.
    void CheckPendingMissionBonus(float delta);
    // Live complete events flaky some maps. Poll, only while in goal map.
    void CheckPendingCompletions(float delta, GW::Constants::MapID current_map);
    bool TrustedCompletionCheck(int goal_index, const wchar_t* player_name, const GoalTrigger& t);
    void CompletePreviousGoals(int index, const GoalClock& clock);

    struct PendingEvent {
        GoalTrigger::Type type;
        uint32_t          id1;
        uint32_t          id2;
        std::wstring      str;
    };

    GoalList* list_    = nullptr;
    bool      started_ = false;

    GW::Constants::MapID prev_map_ = GW::Constants::MapID::None;

    double last_real_ = 0.0;
    double last_game_ = 0.0;

    GW::Constants::MapID mission_complete_map_  = GW::Constants::MapID::None;
    GW::Constants::MapID mission_bonus_map_     = GW::Constants::MapID::None;
    GW::Constants::MapID vanquish_complete_map_ = GW::Constants::MapID::None;
    // No timeout: unearned bonus stay pending, harmless.
    GW::Constants::MapID pending_bonus_check_map_ = GW::Constants::MapID::None;
    bool                 pending_bonus_hm_        = false;
    float                bonus_check_timer_        = 0.f;
    float                completion_check_timer_   = 0.f;
    // Goals seen incomplete this run. Only trust "complete" for these, so old beaten mission not insta-done.
    std::set<int>        completion_confirmed_incomplete_;
    uint32_t             primary_obj_id_        = 0;
    bool                 pending_incomplete_rezone_ = false;
    bool                 pending_wrong_map_entered_ = false;
    // Already on first goal map at start: no zone edge will come, fake one.
    bool                 pending_run_start_ = false;

    std::vector<PendingEvent> pending_events_;
};
