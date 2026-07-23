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
    // Ordered trigger types (explorable/town/level/manual) act as barriers: an unmet
    // ordered goal blocks all subsequent goals in the pass. Unordered types (mission/bonus/
    // title/objective events) never block each other, so e.g. MissionComplete + MissionBonus
    // can both fire in the same tick.
    // When sequential=true, MapEnter goals fire on any explorable entry (map_id ignored) —
    // used by the Running profile to advance splits in order without map-ID matching.
    // is_explorable must be synchronous with just_entered_map, not a live GetInstanceType() poll (can lag a frame and miss the one-shot tick).
    int Update(const GoalClock& clock,
               GW::Constants::MapID current_map,
               bool just_entered_map,
               bool came_from_explorable,
               bool is_explorable,
               int  player_level,
               bool sequential = false);

    void TriggerManual(const GoalClock& clock);

    // Returns true if the bonus bit was also set (WorldContext bitmask check) — the only source for bonus completion; no separate server event exists for it.
    bool NotifyMissionComplete(GW::Constants::MapID map);
    void NotifyVanquishComplete(GW::Constants::MapID map);
    // Tracks which objective is the base/primary (type_flags without BULLET bit).
    // When that objective fires ObjectiveDone, GoalEngine synthesizes both
    // MissionComplete and MissionBonus with the correct server-side map_id.
    void NotifyObjectiveAdd(uint32_t obj_id, uint32_t type_flags);

    // Generic event notification for preset-only triggers (DoorOpen, ObjectiveDone, etc.)
    // str is only needed for ServerMessage/DisplayDialogue and must remain valid until Update() runs.
    void NotifyEvent(GoalTrigger::Type type, uint32_t id1 = 0, uint32_t id2 = 0,
                     const wchar_t* str = nullptr, size_t str_len = 0);

    void Reset();
    void ForceStarted();

    // Marks every goal currently Started as Failed and records its split times.
    // Goals that never started (NotStarted) and goals already Completed are untouched.
    void FailRun(const GoalClock& clock);

    // True if, during the most recent Update(), a Started VanquishComplete/MissionComplete/
    // MissionBonus goal's target map was just left (rezoned out of) without completing it.
    // Detection only — GoalEngine doesn't know about SplitsProfile's auto_fail_on_rezone
    // toggle, so acting on this (calling FailRun) is the caller's policy decision. Clears
    // the flag on read so it's only ever consumed once per occurrence.
    [[nodiscard]] bool ConsumeIncompleteRezone();

    [[nodiscard]] bool     HasList()       const { return list_ != nullptr; }
    [[nodiscard]] bool     IsStarted()     const { return started_; }
    [[nodiscard]] GoalList* List()         const { return list_; }
    [[nodiscard]] uint32_t PrimaryObjId() const { return primary_obj_id_; }

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
    // Base/primary objective id (type_flags & 0x1 == 0, no BULLET bit).
    // When this objective fires ObjectiveDone, both mission_complete_map_ and
    // mission_bonus_map_ are set using the reliable server-side map_id (Prophecies path).
    uint32_t             primary_obj_id_        = 0;
    // See ConsumeIncompleteRezone().
    bool                 incomplete_rezone_pending_ = false;

    std::vector<PendingEvent> pending_events_;
};
