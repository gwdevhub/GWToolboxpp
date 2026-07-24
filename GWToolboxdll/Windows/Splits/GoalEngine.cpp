#include "stdafx.h"
#include "GoalEngine.h"

#include <GWCA/Context/WorldContext.h>

#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameEntities/Title.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>

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
    mission_complete_map_   = GW::Constants::MapID::None;
    mission_bonus_map_      = GW::Constants::MapID::None;
    vanquish_complete_map_  = GW::Constants::MapID::None;
    primary_obj_id_         = 0;
    incomplete_rezone_pending_ = false;
    if (list_) list_->ResetRunState();
}

bool GoalEngine::ConsumeIncompleteRezone()
{
    const bool v = incomplete_rezone_pending_;
    incomplete_rezone_pending_ = false;
    return v;
}

bool GoalEngine::NotifyMissionComplete(GW::Constants::MapID map)
{
    mission_complete_map_ = map;

    // missions_bonus bitmask is historical (never clears) — reads true on every later attempt regardless of re-earning; accepted limitation, see the Settings warning.
    const auto* w = GW::GetWorldContext();
    if (!w) return false;
    const uint32_t idx  = static_cast<uint32_t>(map);
    const uint32_t word = idx >> 5;
    const uint32_t bit  = 1u << (idx & 31);
    if (word < w->missions_bonus.size() && (w->missions_bonus[word] & bit)) {
        mission_bonus_map_ = map;
        return true;
    }
    return false;
}

void GoalEngine::NotifyObjectiveAdd(uint32_t obj_id, uint32_t type_flags)
{
    // No BULLET bit (0x1) = base/primary objective; its ObjectiveDone synthesizes MissionComplete/Bonus in Update() (Prophecies path).
    if (!(type_flags & 0x1))
        primary_obj_id_ = obj_id;
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
                       bool is_explorable,
                       int  player_level,
                       bool sequential)
{
    if (!list_ || list_->goals.empty()) {
        prev_map_             = current_map;
        mission_complete_map_ = GW::Constants::MapID::None;
        mission_bonus_map_    = GW::Constants::MapID::None;
        pending_events_.clear();
        return 0;
    }

    if (!started_ && just_entered_map)
        started_ = true;

    int fired = 0;

    // True for trigger types that barrier Pass 2 (an unmet ordered goal stops later goals firing out of sequence); mission/bonus/title/objective-event types never block.
    auto is_ordered = [](GoalTrigger::Type type) -> bool {
        switch (type) {
            case GoalTrigger::Type::MissionComplete:
            case GoalTrigger::Type::MissionBonus:
            case GoalTrigger::Type::ReachTitleRank:
            case GoalTrigger::Type::ObjectiveDone:
            case GoalTrigger::Type::DoorOpen:
            case GoalTrigger::Type::DoorClose:
            case GoalTrigger::Type::AgentUpdateAllegiance:
            case GoalTrigger::Type::DoACompleteZone:
            case GoalTrigger::Type::DungeonReward:
            case GoalTrigger::Type::ServerMessage:
            case GoalTrigger::Type::DisplayDialogue:
            case GoalTrigger::Type::ObjectiveStarted:
            case GoalTrigger::Type::QuestPickup:
            case GoalTrigger::Type::QuestComplete:
            case GoalTrigger::Type::MobKill:
                return false;
            default:
                return true;
        }
    };

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

    // Same trigger value (e.g. two legs both starting on the same hub MapEnter) — used so a repeated trigger only arms the earliest pending goal per tick.
    auto triggersEqual = [](const GoalTrigger& a, const GoalTrigger& b) {
        return a.type == b.type && a.map_id == b.map_id && a.param1 == b.param1 &&
               a.param2 == b.param2 && a.level == b.level && a.title_id == b.title_id &&
               a.hard_mode == b.hard_mode && a.pattern == b.pattern;
    };

    if (started_) {
        // Synthesizes MissionComplete/Bonus off the primary objective's ObjectiveDone (reliable server map_id) for missions where kMissionComplete reports it wrong/missing, e.g. GNW.
        if (primary_obj_id_ != 0) {
            for (const auto& ev : pending_events_) {
                if (ev.type == GoalTrigger::Type::ObjectiveDone &&
                    ev.id1 == primary_obj_id_ && ev.id2 != 0)
                    (void)NotifyMissionComplete(static_cast<GW::Constants::MapID>(ev.id2));
            }
        }

        // Pass 1: records when each objective begins; checked for all non-completed goals (supports parallel objectives like Deep rooms 1-4).
        std::vector<const GoalTrigger*> claimed_this_tick;
        for (int i = 0; i < static_cast<int>(list_->goals.size()); ++i) {
            GoalEntry& g = list_->goals[i];
            if (g.is_header)                       continue;
            if (g.status == GoalStatus::Completed) continue;
            if (g.start_real_time >= 0.0)          continue; // already started
            if (!g.start_trigger.has_value())      continue;
            const GoalTrigger& st = g.start_trigger.value();
            bool start_fire = false;
            const GoalTrigger* fired_trigger = &st;
            switch (st.type) {
                case GoalTrigger::Type::MapEnter:
                    start_fire = just_entered_map && (current_map == st.map_id);
                    break;
                // Same map_id can be Outpost or Explorable at different points (e.g. ToPK's The_Underworld_PvP) — matches OT's own explorable-only gate before AddToPKObjectiveSet().
                case GoalTrigger::Type::EnterExplorable:
                    start_fire = just_entered_map && is_explorable && (current_map == st.map_id);
                    break;
                default:
                    start_fire = matchesPendingTrigger(st);
                    // OR alternates (e.g. DoA's "360" room, reachable through any of 3 doors) — same idea as extra_triggers but for starting.
                    if (!start_fire) {
                        for (const auto& est : g.extra_start_triggers) {
                            if (matchesPendingTrigger(est)) { start_fire = true; fired_trigger = &est; break; }
                        }
                    }
                    break;
            }
            if (start_fire) {
                bool already_claimed = false;
                for (const GoalTrigger* claimed : claimed_this_tick) {
                    if (triggersEqual(*claimed, *fired_trigger)) { already_claimed = true; break; }
                }
                if (already_claimed) continue;
                claimed_this_tick.push_back(fired_trigger);

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
            // A goal with a start_trigger must be Started before it can complete, so start/end can't both fire on the same tick.
            if (g.start_trigger.has_value() && g.status == GoalStatus::NotStarted) {
                if (is_ordered(g.trigger.type)) break;
                continue;
            }

            bool fire = false;
            const GoalTrigger& t = g.trigger;

            switch (t.type) {
                case GoalTrigger::Type::MapEnter:
                    // Sequential (Running): any transition fires the next split; non-sequential (Manual): requires the specific map_id.
                    fire = sequential ? just_entered_map : (just_entered_map && current_map == t.map_id);
                    break;

                case GoalTrigger::Type::EnterExplorable:
                    fire = just_entered_map && is_explorable && (current_map == t.map_id);
                    break;

                // Same map_id can be Outpost or Explorable (e.g. GNW); !is_explorable so this only fires for the town/staging entry, not the mission.
                case GoalTrigger::Type::EnterOutpost:
                    fire = just_entered_map && !is_explorable && (current_map == t.map_id);
                    break;

                case GoalTrigger::Type::ExitExplorable:
                    fire = just_entered_map && came_from_explorable && (prev_map_ == t.map_id);
                    break;

                case GoalTrigger::Type::VanquishComplete:
                    fire = (vanquish_complete_map_ == t.map_id);
                    break;

                case GoalTrigger::Type::MissionComplete: {
                    const bool hm_ok = !t.hard_mode || GW::PartyMgr::GetIsPartyInHardMode();
                    fire = (mission_complete_map_ == t.map_id) && hm_ok;
                    break;
                }

                case GoalTrigger::Type::MissionBonus: {
                    const bool hm_ok = !t.hard_mode || GW::PartyMgr::GetIsPartyInHardMode();
                    fire = (mission_bonus_map_ == t.map_id) && hm_ok;
                    break;
                }

                case GoalTrigger::Type::ReachLevel:
                    fire = (player_level >= t.level);
                    break;

                case GoalTrigger::Type::ExitOutpost:
                    fire = just_entered_map && (prev_map_ == t.map_id);
                    break;

                case GoalTrigger::Type::ReachTitleRank: {
                    // t.level is the target RANK (1-based), not a stored tier index — the tier-index anchor (max_title_tier_index) only exists once the title has any progress at all, so it's resolved here against the live Title* instead of at goal-creation time. This is what lets a goal be added for a title still at zero progress.
                    const GW::Title* title = GW::PlayerMgr::GetTitleTrack(t.title_id);
                    if (title && t.level > 0 && title->current_title_tier_index != 0) {
                        const uint32_t target_tier_idx = title->max_title_tier_index + static_cast<uint32_t>(t.level - 1);
                        fire = title->current_title_tier_index >= target_tier_idx;
                    }
                    break;
                }

                case GoalTrigger::Type::Manual:
                    break;

                // Preset-only triggers, matched against pending_events_ (also checks extra_triggers, OR semantics).
                case GoalTrigger::Type::ObjectiveDone:
                case GoalTrigger::Type::DoorOpen:
                case GoalTrigger::Type::DoorClose:
                case GoalTrigger::Type::AgentUpdateAllegiance:
                case GoalTrigger::Type::DoACompleteZone:
                case GoalTrigger::Type::DungeonReward:
                case GoalTrigger::Type::ServerMessage:
                case GoalTrigger::Type::DisplayDialogue:
                case GoalTrigger::Type::CountdownStart:
                case GoalTrigger::Type::QuestPickup:
                case GoalTrigger::Type::QuestComplete: {
                    fire = matchesPendingTrigger(t);
                    if (!fire) {
                        for (const auto& et : g.extra_triggers) {
                            if ((fire = matchesPendingTrigger(et))) break;
                        }
                    }
                    break;
                }

                // SkillLearnt has no event at all, it's pure state — must be polled.
                case GoalTrigger::Type::SkillLearnt:
                    fire = GW::SkillbarMgr::GetIsSkillLearnt(static_cast<GW::Constants::SkillID>(t.param1));
                    break;

                // Counts toward param2 rather than first-match, since an AoE wipe can add multiple matching events to pending_events_ in one tick.
                case GoalTrigger::Type::MobKill: {
                    int kills_this_tick = 0;
                    for (const auto& ev : pending_events_) {
                        if (ev.type == GoalTrigger::Type::MobKill && ev.id1 == t.param1)
                            ++kills_this_tick;
                    }
                    g.trigger_progress += kills_this_tick;
                    const uint32_t target = t.param2 > 0 ? t.param2 : 1;
                    fire = g.trigger_progress >= static_cast<int>(target);
                    break;
                }

                default:
                    break;
            }

            if (fire) {
                // Cascading end goals (auto_complete_previous, no start_trigger) must close previous legs first, so their segments use the pre-arrival last_real_.
                if (g.auto_complete_previous != 0 && !g.start_trigger.has_value()) {
                    CompletePreviousGoals(i, clock);
                    FireGoal(i, clock);
                } else {
                    FireGoal(i, clock);
                    CompletePreviousGoals(i, clock);
                }
                fired++;
                if (is_ordered(t.type)) break;
            }
            if (!fire && is_ordered(g.trigger.type)) {
                // Running: skip non-matching goals so earlier list entries don't block later ones once the start zone is behind the runner.
                if (sequential) continue;
                // A Started Manual goal shouldn't block subsequent auto-completing goals (e.g. an Add-End MapEnter with auto_complete_previous).
                if (g.trigger.type == GoalTrigger::Type::Manual &&
                    g.status == GoalStatus::Started)
                    continue;
                break;
            }
        } // end Pass 2

        // Auto-fail: mirrors OT's StopObjectives — checks the first incomplete (not Started, since editor-built goals never reach Started before completing) goal; runs after Pass 2 to avoid a same-tick false positive.
        if (just_entered_map && came_from_explorable) {
            using TT = GoalTrigger::Type;
            GW::Constants::MapID owning_header_map = GW::Constants::MapID::None;
            // Map the current goal is active on: starts at the header's map, advances to each completed MapEnter goal's own target, so a normal level transition (e.g. CoF 1->2) isn't misread as abandonment.
            GW::Constants::MapID segment_start_map = GW::Constants::MapID::None;
            for (const auto& g : list_->goals) {
                if (g.is_header) {
                    owning_header_map = g.trigger.map_id;
                    segment_start_map = g.trigger.map_id;
                    continue;
                }
                if (g.status == GoalStatus::Completed || g.status == GoalStatus::Failed) {
                    if (g.trigger.type == TT::MapEnter) segment_start_map = g.trigger.map_id;
                    continue;
                }
                bool map_matches = false;
                if (g.trigger.type == TT::VanquishComplete || g.trigger.type == TT::MissionComplete ||
                    g.trigger.type == TT::MissionBonus || g.trigger.type == TT::DungeonReward ||
                    g.trigger.type == TT::CountdownStart) {
                    // CountdownStart (ToPK) carries its own level's real map_id directly, same as Mission/Bonus/VQ/DungeonReward.
                    map_matches = (g.trigger.map_id == prev_map_);
                } else if (g.trigger.type == TT::ObjectiveDone || g.trigger.type == TT::DoorOpen ||
                           g.trigger.type == TT::DisplayDialogue || g.trigger.type == TT::ServerMessage ||
                           g.trigger.type == TT::DoACompleteZone || g.trigger.type == TT::AgentUpdateAllegiance) {
                    // DoACompleteZone/AgentUpdateAllegiance (DoA) use the header-map fallback too, since DoA's whole run shares one map_id across all 4 rotated zones.
                    map_matches = (owning_header_map != GW::Constants::MapID::None &&
                                    owning_header_map == prev_map_);
                } else if (g.trigger.type == TT::MapEnter) {
                    // SC's per-level dungeon goals: this goal's own map_id is the *next* level, so segment_start_map (not owning_header_map) is the map it's active on.
                    map_matches = (segment_start_map != GW::Constants::MapID::None &&
                                    segment_start_map == prev_map_);
                }
                if (map_matches) incomplete_rezone_pending_ = true;
                break; // only the current goal can be the one just abandoned
            }
        }
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
