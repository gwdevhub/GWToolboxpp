#include "stdafx.h"
#include "GoalEngine.h"

#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameEntities/Title.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/PlayerMgr.h>

void GoalEngine::Attach(GoalList* list)
{
    list_ = list;
    Reset();
    // Goals marked starts_immediately begin at t=0 of the run (like OT's SetStarted() at set creation).
    if (list_) {
        for (auto& g : list_->goals) {
            if (g.is_header) continue;
            if (g.starts_immediately) {
                g.start_real_time = 0.0;
                g.start_game_time = 0.0;
                g.status          = GoalStatus::Started;
            }
        }
    }
}

void GoalEngine::Detach()
{
    list_    = nullptr;
    started_ = false;
}

void GoalEngine::Reset()
{
    started_              = false;
    prev_map_             = GW::Constants::MapID::None;
    last_real_            = 0.0;
    last_game_            = 0.0;
    mission_complete_map_ = GW::Constants::MapID::None;
    mission_bonus_map_    = GW::Constants::MapID::None;
    if (list_) list_->ResetRunState();
}

void GoalEngine::NotifyMissionComplete(GW::Constants::MapID map)
{
    mission_complete_map_ = map;
}

void GoalEngine::NotifyMissionBonus(GW::Constants::MapID map)
{
    mission_bonus_map_ = map;
}

void GoalEngine::NotifyVanquishComplete(GW::Constants::MapID map)
{
    vanquish_complete_map_ = map;
}

void GoalEngine::NotifyEvent(GoalTrigger::Type type, uint32_t id1, uint32_t id2,
                             const wchar_t* str, size_t str_len)
{
    PendingEvent ev;
    ev.type = type;
    ev.id1  = id1;
    ev.id2  = id2;
    if (str && str_len > 0)
        ev.str.assign(str, str_len);
    pending_events_.push_back(std::move(ev));
}

int GoalEngine::Update(const GoalClock& clock,
                       GW::Constants::MapID current_map,
                       bool just_entered_map,
                       bool came_from_explorable,
                       GW::Constants::InstanceType instance_type,
                       int  player_level,
                       bool simple_order)
{
    const bool is_explorable = (instance_type == GW::Constants::InstanceType::Explorable);

    if (!list_ || list_->goals.empty()) {
        prev_map_             = current_map;
        mission_complete_map_ = GW::Constants::MapID::None;
        mission_bonus_map_    = GW::Constants::MapID::None;
        pending_events_.clear();
        return -1;
    }

    if (!started_ && just_entered_map)
        started_ = true;

    int fired = -1;

    // Returns true when trigger 't' matches any event currently in pending_events_.
    auto matchesPendingTrigger = [&](const GoalTrigger& tr) -> bool {
        for (const auto& ev : pending_events_) {
            if (ev.type != tr.type) continue;
            switch (tr.type) {
                case GoalTrigger::Type::DungeonReward:
                    return true;
                case GoalTrigger::Type::AgentUpdateAllegiance:
                    if (ev.id1 == tr.param1 && ev.id2 == tr.param2) return true;
                    break;
                case GoalTrigger::Type::ServerMessage:
                case GoalTrigger::Type::DisplayDialogue:
                    if (!tr.pattern.empty() && ev.str.size() >= tr.pattern.size() &&
                        ev.str.compare(0, tr.pattern.size(), tr.pattern) == 0)
                        return true;
                    break;
                default:
                    if (ev.id1 == tr.param1) return true;
                    break;
            }
        }
        return false;
    };

    // Two start triggers are "the same value" (e.g. two legs both starting on MapEnter into
    // the same hub map visited more than once in a run) — used below so a repeated trigger
    // only arms the earliest pending goal per tick instead of all of them at once.
    auto triggersEqual = [](const GoalTrigger& a, const GoalTrigger& b) {
        return a.type == b.type && a.map_id == b.map_id && a.param1 == b.param1 &&
               a.param2 == b.param2 && a.level == b.level && a.title_id == b.title_id &&
               a.hard_mode == b.hard_mode && a.pattern == b.pattern;
    };

    if (started_) {
        // Pass 1: check start triggers — records when each objective begins.
        // Checked for ALL non-completed goals (supports parallel objectives like Deep rooms 1-4).
        std::vector<const GoalTrigger*> claimed_this_tick;
        for (int i = 0; i < static_cast<int>(list_->goals.size()); ++i) {
            GoalEntry& g = list_->goals[i];
            if (g.is_header)                       continue;
            if (g.status == GoalStatus::Completed) continue;
            if (g.start_real_time >= 0.0)          continue; // already started
            if (!g.start_trigger.has_value())      continue;
            const GoalTrigger& st = g.start_trigger.value();
            bool start_fire = false;
            switch (st.type) {
                case GoalTrigger::Type::MapEnter:
                    start_fire = just_entered_map && (current_map == st.map_id);
                    break;
                default:
                    start_fire = matchesPendingTrigger(st);
                    break;
            }
            if (start_fire) {
                bool already_claimed = false;
                for (const GoalTrigger* claimed : claimed_this_tick) {
                    if (triggersEqual(*claimed, st)) { already_claimed = true; break; }
                }
                if (already_claimed) continue;
                claimed_this_tick.push_back(&st);

                g.start_real_time = clock.RealTime();
                g.start_game_time = clock.GameTime();
                g.status          = GoalStatus::Started;
                CompletePreviousGoals(i, clock);
            }
        }

        // Pass 2: check end triggers — fires completion for the first matching goal.
        for (int i = 0; i < static_cast<int>(list_->goals.size()); ++i) {
            GoalEntry& g = list_->goals[i];
            if (g.is_header)                       continue;
            if (g.status == GoalStatus::Completed) continue;
            // A goal with a start_trigger must be Started before it can complete,
            // preventing its start and end from firing on the same map-entry tick.
            if (g.start_trigger.has_value() && g.status == GoalStatus::NotStarted) {
                if (simple_order) break; // strict order: this goal blocks all later ones too
                continue;
            }

            bool fire = false;
            const GoalTrigger& t = g.trigger;

            switch (t.type) {
                case GoalTrigger::Type::MapEnter:
                    fire = just_entered_map && (current_map == t.map_id);
                    break;

                case GoalTrigger::Type::EnterExplorable:
                    fire = just_entered_map && is_explorable && (current_map == t.map_id);
                    break;

                case GoalTrigger::Type::ExitExplorable:
                    fire = just_entered_map && came_from_explorable && (prev_map_ == t.map_id);
                    break;

                case GoalTrigger::Type::VanquishComplete:
                    fire = (vanquish_complete_map_ == t.map_id);
                    break;

                case GoalTrigger::Type::MissionComplete:
                    fire = (mission_complete_map_ == t.map_id) &&
                           (!t.hard_mode || GW::PartyMgr::GetIsPartyInHardMode());
                    break;

                case GoalTrigger::Type::MissionBonus:
                    fire = (mission_bonus_map_ == t.map_id) &&
                           (!t.hard_mode || GW::PartyMgr::GetIsPartyInHardMode());
                    break;

                case GoalTrigger::Type::ReachLevel:
                    fire = (player_level >= t.level);
                    break;

                case GoalTrigger::Type::ExitOutpost:
                    fire = just_entered_map && (prev_map_ == t.map_id);
                    break;

                case GoalTrigger::Type::ReachTitleRank: {
                    const GW::Title* title = GW::PlayerMgr::GetTitleTrack(t.title_id);
                    // Require non-null title data, a non-zero current tier (0 = no progress loaded),
                    // and a non-zero target (level=0 would fire spuriously on untracked titles).
                    fire = title &&
                           title->current_title_tier_index != 0 &&
                           t.level > 0 &&
                           title->current_title_tier_index >= static_cast<uint32_t>(t.level);
                    break;
                }

                case GoalTrigger::Type::Manual:
                    break;

                // Preset-only triggers — matched against pending_events_ queue.
                // Also checks extra_triggers (OR semantics).
                case GoalTrigger::Type::ObjectiveDone:
                case GoalTrigger::Type::DoorOpen:
                case GoalTrigger::Type::DoorClose:
                case GoalTrigger::Type::AgentUpdateAllegiance:
                case GoalTrigger::Type::DoACompleteZone:
                case GoalTrigger::Type::DungeonReward:
                case GoalTrigger::Type::ServerMessage:
                case GoalTrigger::Type::DisplayDialogue:
                case GoalTrigger::Type::CountdownStart: {
                    fire = matchesPendingTrigger(t);
                    if (!fire) {
                        for (const auto& et : g.extra_triggers) {
                            if ((fire = matchesPendingTrigger(et))) break;
                        }
                    }
                    break;
                }

                default:
                    break;
            }

            if (fire) {
                // End goals that cascade (auto_complete_previous, no start_trigger) must
                // close previous legs before firing themselves, so their segments use the
                // pre-arrival last_real_ rather than the one updated by this goal.
                if (g.auto_complete_previous != 0 && !g.start_trigger.has_value()) {
                    CompletePreviousGoals(i, clock);
                    FireGoal(i, clock);
                } else {
                    FireGoal(i, clock);
                    CompletePreviousGoals(i, clock);
                }
                fired = i;
                break;
            }
            if (simple_order) break; // strict order: stop at the earliest pending goal
        } // end Pass 2
    }

    mission_complete_map_  = GW::Constants::MapID::None;
    mission_bonus_map_     = GW::Constants::MapID::None;
    vanquish_complete_map_ = GW::Constants::MapID::None;
    pending_events_.clear();
    if (current_map != GW::Constants::MapID::None)
        prev_map_ = current_map;

    return fired;
}

void GoalEngine::ForceStarted()
{
    started_ = true;
}

void GoalEngine::TriggerManual(const GoalClock& clock)
{
    if (!list_) return;
    for (int i = 0; i < static_cast<int>(list_->goals.size()); ++i) {
        GoalEntry& g = list_->goals[i];
        if (g.is_header) continue;
        if (g.status != GoalStatus::Completed && g.trigger.type == GoalTrigger::Type::Manual) {
            if (!started_) started_ = true;
            FireGoal(i, clock);
            CompletePreviousGoals(i, clock);
            return;
        }
    }
}

void GoalEngine::FireGoal(int index, const GoalClock& clock)
{
    GoalEntry& g = list_->goals[index];
    g.status             = GoalStatus::Completed;
    g.split.real_time    = clock.RealTime();
    g.split.game_time    = clock.GameTime();
    g.split.segment_real = clock.RealTime() - last_real_;
    g.split.segment_game = clock.GameTime() - last_game_;
    last_real_ = clock.RealTime();
    last_game_ = clock.GameTime();

    // OT-style relay: next sequential non-header goal without an explicit start_trigger auto-starts now.
    int next_i = index + 1;
    while (next_i < static_cast<int>(list_->goals.size()) && list_->goals[next_i].is_header)
        ++next_i;
    if (next_i < static_cast<int>(list_->goals.size())) {
        GoalEntry& nxt = list_->goals[next_i];
        if (nxt.start_real_time < 0.0 && !nxt.start_trigger.has_value()) {
            nxt.start_real_time = g.split.real_time;
            nxt.start_game_time = g.split.game_time;
            if (nxt.status == GoalStatus::NotStarted)
                nxt.status = GoalStatus::Started;
        }
    }
}

void GoalEngine::CompletePreviousGoals(int index, const GoalClock& clock)
{
    const GoalEntry& g = list_->goals[index];
    if (g.auto_complete_previous == 0) return;

    const int from = (g.auto_complete_previous < 0)
        ? 0
        : std::max(0, index - g.auto_complete_previous);

    for (int j = from; j < index; ++j) {
        if (list_->goals[j].is_header) continue;
        if (list_->goals[j].status != GoalStatus::Completed)
            FireGoal(j, clock);
    }
}

void GoalEngine::FailRun(const GoalClock& clock)
{
    if (!list_) return;
    for (auto& g : list_->goals) {
        if (g.is_header) continue;
        if (g.status != GoalStatus::Started) continue;
        g.status             = GoalStatus::Failed;
        g.split.real_time    = clock.RealTime();
        g.split.game_time    = clock.GameTime();
        g.split.segment_real = clock.RealTime() - last_real_;
        g.split.segment_game = clock.GameTime() - last_game_;
    }
}
