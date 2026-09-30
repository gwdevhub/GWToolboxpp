#include "stdafx.h"

#include "GoalEngine.h"

#include <GWCA/Context/CharContext.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/GameContainers/Array.h>

#include <GWCA/GameEntities/Title.h>

#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>

#include <Utils/TextUtils.h>
#include <Windows/CompletionWindow.h>
#include <Windows/Splits/SCPresets.h>

void GoalEngine::Attach(GoalList* list)
{
    list_ = list;
    Reset();
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
    pending_bonus_check_map_ = GW::Constants::MapID::None;
    bonus_check_timer_      = 0.f;
    completion_check_timer_ = 0.f;
    completion_confirmed_incomplete_.clear();
    primary_obj_id_         = 0;
    pending_incomplete_rezone_ = false;
    pending_wrong_map_entered_ = false;
    pending_run_start_         = false;
    if (list_) list_->ResetRunState();
}

bool GoalEngine::ConsumeWrongMapEntered()
{
    const bool v = pending_wrong_map_entered_;
    pending_wrong_map_entered_ = false;
    return v;
}

bool GoalEngine::ConsumeIncompleteRezone()
{
    const bool v = pending_incomplete_rezone_;
    pending_incomplete_rezone_ = false;
    return v;
}

void GoalEngine::NotifyMissionComplete(GW::Constants::MapID map)
{
    mission_complete_map_   = map;
    pending_bonus_check_map_ = map;
    // HM clear only set HM bits: poll read that mode.
    pending_bonus_hm_        = GW::PartyMgr::GetIsPartyInHardMode();
}

// CompletionWindow, not raw WorldContext: raw give false positive right after kMissionComplete.
void GoalEngine::CheckPendingMissionBonus(const float delta)
{
    if (pending_bonus_check_map_ == GW::Constants::MapID::None) return;
    bonus_check_timer_ += delta;
    if (bonus_check_timer_ < 1.0f) return;
    bonus_check_timer_ = 0.f;
    // GetCharContext name, not PlayerMgr: must match CompletionWindow key.
    const auto* char_context = GW::GetCharContext();
    if (!char_context) return;
    const auto mode = pending_bonus_hm_ ? CompletionCheck::HardMode : CompletionCheck::NormalMode;
    if (CompletionWindow::IsAreaComplete(char_context->player_name, pending_bonus_check_map_, mode)) {
        mission_bonus_map_       = pending_bonus_check_map_;
        pending_bonus_check_map_ = GW::Constants::MapID::None;
    }
}

void GoalEngine::CheckPendingCompletions(const float delta, const GW::Constants::MapID current_map)
{
    if (!list_ || current_map == GW::Constants::MapID::None) return;
    completion_check_timer_ += delta;
    if (completion_check_timer_ < 1.0f) return;
    completion_check_timer_ = 0.f;
    // GetCharContext name, not PlayerMgr: must match CompletionWindow key.
    const auto* char_context = GW::GetCharContext();
    if (!char_context) return;
    for (int i = 0; i < static_cast<int>(list_->goals.size()); ++i) {
        const auto& g = list_->goals[static_cast<size_t>(i)];
        if (g.is_header || g.status == GoalStatus::Completed) continue;
        // Only in goal map: bits are historical, old beaten mission would insta-done.
        if (g.trigger.map_id != current_map) continue;
        // Bonus stay on own arm-then-poll.
        if (g.trigger.type == GoalTrigger::Type::MissionComplete) {
            if (TrustedCompletionCheck(i, char_context->player_name, g.trigger))
                NotifyMissionComplete(g.trigger.map_id);
        } else if (g.trigger.type == GoalTrigger::Type::VanquishComplete) {
            if (TrustedCompletionCheck(i, char_context->player_name, g.trigger))
                NotifyVanquishComplete(g.trigger.map_id);
        }
    }
}

bool GoalEngine::TrustedCompletionCheck(const int goal_index, const wchar_t* player_name, const GoalTrigger& t)
{
    // Untimed: rezone check need answer now, not after throttled poll. PrimaryOnly: Mission not need Bonus bit.
    bool complete = false;
    if (t.type == GoalTrigger::Type::MissionComplete) {
        const uint32_t mode = t.hard_mode ? CompletionCheck::HardMode : CompletionCheck::NormalMode;
        complete = CompletionWindow::IsAreaComplete(player_name, t.map_id, static_cast<CompletionCheck>(mode | CompletionCheck::PrimaryOnly));
    }
    else if (t.type == GoalTrigger::Type::VanquishComplete) {
        complete = CompletionWindow::IsAreaComplete(player_name, t.map_id, CompletionCheck::NormalMode);
    }
    if (!complete) {
        completion_confirmed_incomplete_.insert(goal_index);
        return false;
    }
    return completion_confirmed_incomplete_.contains(goal_index);
}

void GoalEngine::NotifyObjectiveAdd(uint32_t obj_id, uint32_t type_flags)
{
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
                       float delta)
{
    if (!list_ || list_->goals.empty()) {
        prev_map_                = current_map;
        mission_complete_map_    = GW::Constants::MapID::None;
        mission_bonus_map_       = GW::Constants::MapID::None;
        pending_bonus_check_map_ = GW::Constants::MapID::None;
        pending_events_.clear();
        return 0;
    }

    if (!started_ && just_entered_map)
        started_ = true;

    int fired = 0;

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

    // Shared by both wrong-turn checks so they not drift.
    auto is_map_enter = [](GoalTrigger::Type type) -> bool {
        return type == GoalTrigger::Type::EnterExplorable || type == GoalTrigger::Type::EnterOutpost;
    };
    auto is_map_enter_or_exit = [&](GoalTrigger::Type type) -> bool {
        return is_map_enter(type) ||
               type == GoalTrigger::Type::ExitExplorable || type == GoalTrigger::Type::ExitOutpost;
    };

    auto matches_pending_trigger = [&](const GoalTrigger& tr) -> bool {
        for (const auto& ev : pending_events_) {
            if (ev.type != tr.type) continue;
            switch (tr.type) {
                case GoalTrigger::Type::DungeonReward: {
                    // Chest event carries the map it opened in; old goals outside the table keep matching any chest.
                    const auto* dungeon = SCPresets::FindDungeon(tr.map_id);
                    if (!dungeon || dungeon == SCPresets::FindDungeon(static_cast<GW::Constants::MapID>(ev.id1))) return true;
                    break;
                }
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

    // Same trigger on two goals (hub MapEnter): only arm earliest per tick.
    auto triggers_equal = [](const GoalTrigger& a, const GoalTrigger& b) {
        return a.type == b.type && a.map_id == b.map_id && a.param1 == b.param1 &&
               a.param2 == b.param2 && a.level == b.level && a.title_id == b.title_id &&
               a.hard_mode == b.hard_mode && a.pattern == b.pattern;
    };

    if (started_) {
        CheckPendingCompletions(delta, current_map);
        CheckPendingMissionBonus(delta);

        const bool effective_just_entered = just_entered_map || pending_run_start_;
        pending_run_start_ = false;

        // Primary ObjectiveDone give real map_id where kMissionComplete wrong (GNW).
        if (primary_obj_id_ != 0) {
            for (const auto& ev : pending_events_) {
                if (ev.type == GoalTrigger::Type::ObjectiveDone &&
                    ev.id1 == primary_obj_id_ && ev.id2 != 0)
                    NotifyMissionComplete(static_cast<GW::Constants::MapID>(ev.id2));
            }
        }

        std::vector<const GoalTrigger*> claimed_this_tick;
        for (int i = 0; i < static_cast<int>(list_->goals.size()); ++i) {
            GoalEntry& g = list_->goals[i];
            if (g.is_header)                       continue;
            if (g.status == GoalStatus::Completed) continue;
            if (g.start_real_time >= 0.0)          continue;
            if (!g.start_trigger.has_value())      continue;
            const GoalTrigger& st = g.start_trigger.value();
            bool start_fire = false;
            const GoalTrigger* fired_trigger = &st;
            switch (st.type) {
                case GoalTrigger::Type::MapEnter:
                    start_fire = effective_just_entered && (current_map == st.map_id);
                    break;
                // Same map_id outpost or explorable (ToPK). Match OT explorable gate.
                case GoalTrigger::Type::EnterExplorable:
                    start_fire = effective_just_entered && is_explorable && (current_map == st.map_id);
                    break;
                case GoalTrigger::Type::EnterOutpost:
                    start_fire = effective_just_entered && !is_explorable && (current_map == st.map_id);
                    break;
                default:
                    start_fire = matches_pending_trigger(st);
                    if (!start_fire) {
                        for (const auto& est : g.extra_start_triggers) {
                            if (matches_pending_trigger(est)) { start_fire = true; fired_trigger = &est; break; }
                        }
                    }
                    break;
            }
            if (start_fire) {
                bool already_claimed = false;
                for (const GoalTrigger* claimed : claimed_this_tick) {
                    if (triggers_equal(*claimed, *fired_trigger)) { already_claimed = true; break; }
                }
                if (already_claimed) continue;
                claimed_this_tick.push_back(fired_trigger);

                g.start_real_time = clock.RealTime();
                g.start_game_time = clock.GameTime();
                g.status          = GoalStatus::Started;
                CompletePreviousGoals(i, clock);
            }
        }

        for (int i = 0; i < static_cast<int>(list_->goals.size()); ++i) {
            GoalEntry& g = list_->goals[i];
            if (g.is_header)                       continue;
            if (g.status == GoalStatus::Completed) continue;
            // Must be Started first, so start/end not fire same tick.
            if (g.start_trigger.has_value() && g.status == GoalStatus::NotStarted) {
                if (is_ordered(g.trigger.type)) break;
                continue;
            }

            bool fire = false;
            const GoalTrigger& t = g.trigger;

            switch (t.type) {
                case GoalTrigger::Type::MapEnter:
                    fire = effective_just_entered && current_map == t.map_id;
                    break;

                case GoalTrigger::Type::EnterExplorable:
                    fire = effective_just_entered && is_explorable && (current_map == t.map_id);
                    break;

                // Same map_id outpost or explorable (GNW): town entry only.
                case GoalTrigger::Type::EnterOutpost:
                    fire = effective_just_entered && !is_explorable && (current_map == t.map_id);
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
                    // Left a town (not a same-id explorable, GNW), and not just a district change.
                    fire = just_entered_map && !came_from_explorable && (prev_map_ == t.map_id) &&
                           (is_explorable || current_map != t.map_id);
                    break;

                case GoalTrigger::Type::ReachTitleRank: {
                    // t.level = 1-based rank. Tier anchor only exist once title has progress: resolve live.
                    const GW::Title* title = GW::PlayerMgr::GetTitleTrack(t.title_id);
                    if (title && t.level > 0 && title->current_title_tier_index != 0) {
                        const uint32_t target_tier_idx = title->max_title_tier_index + static_cast<uint32_t>(t.level - 1);
                        fire = title->current_title_tier_index >= target_tier_idx;
                    }
                    break;
                }

                case GoalTrigger::Type::Manual:
                    break;

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
                    fire = matches_pending_trigger(t);
                    if (!fire) {
                        for (const auto& et : g.extra_triggers) {
                            if ((fire = matches_pending_trigger(et))) break;
                        }
                    }
                    break;
                }

                // No event: poll.
                case GoalTrigger::Type::SkillLearnt:
                    fire = GW::SkillbarMgr::GetIsSkillLearnt(static_cast<GW::Constants::SkillID>(t.param1));
                    break;

                // Count, not first match: AoE wipe queue many kills in one tick. param1 = old lists only.
                case GoalTrigger::Type::MobKill: {
                    const bool by_name = !t.pattern.empty();
                    std::wstring pattern_lower;
                    int kills_this_tick = 0;
                    for (const auto& ev : pending_events_) {
                        if (ev.type != GoalTrigger::Type::MobKill) continue;
                        if (by_name && pattern_lower.empty()) pattern_lower = TextUtils::ToLower(t.pattern);
                        const bool matches = by_name
                            ? (!ev.str.empty() && TextUtils::ToLower(ev.str) == pattern_lower)
                            : (ev.id1 == t.param1);
                        if (matches) ++kills_this_tick;
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
                // Close previous legs first so segments use pre-arrival last_real_.
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
                // Wrong turn. start_real_time check skip goal whose start fired this tick.
                if (just_entered_map && g.start_real_time != clock.RealTime() &&
                    is_map_enter_or_exit(g.trigger.type))
                    pending_wrong_map_entered_ = true;
                // Started Manual goal not block later auto goals.
                if (g.trigger.type == GoalTrigger::Type::Manual &&
                    g.status == GoalStatus::Started)
                    continue;
                break;
            }
        }

        // After Pass 2: current leg exit must get chance to complete first.
        if (just_entered_map) {
            for (const auto& g : list_->goals) {
                if (g.is_header)                       continue;
                if (g.status == GoalStatus::Completed) continue;
                if (g.status != GoalStatus::NotStarted) break;
                if (!g.start_trigger.has_value())      break;
                if (is_map_enter(g.start_trigger->type))
                    pending_wrong_map_entered_ = true;
                break;
            }
        }

        // Like OT StopObjectives. After Pass 2 to avoid same-tick false positive.
        if (just_entered_map && came_from_explorable) {
            using TT = GoalTrigger::Type;
            GW::Constants::MapID owning_header_map = GW::Constants::MapID::None;
            // Advance on each done MapEnter so level change (CoF 1->2) not read as abandon.
            GW::Constants::MapID segment_start_map = GW::Constants::MapID::None;
            for (int gi = 0; gi < static_cast<int>(list_->goals.size()); ++gi) {
                const auto& g = list_->goals[static_cast<size_t>(gi)];
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
                if (g.trigger.type == TT::DungeonReward && SCPresets::FindDungeon(g.trigger.map_id)) {
                    // Whole dungeon is the goal's area: a level change inside it isn't leaving.
                    const auto* dungeon = SCPresets::FindDungeon(g.trigger.map_id);
                    map_matches = SCPresets::FindDungeon(prev_map_) == dungeon && SCPresets::FindDungeon(current_map) != dungeon;
                } else if (g.trigger.type == TT::VanquishComplete || g.trigger.type == TT::MissionComplete ||
                    g.trigger.type == TT::DungeonReward || g.trigger.type == TT::CountdownStart) {
                    // Bonus excluded: own background poll finish it.
                    map_matches = (g.trigger.map_id == prev_map_);
                } else if (g.trigger.type == TT::ObjectiveDone || g.trigger.type == TT::DoorOpen ||
                           g.trigger.type == TT::DisplayDialogue || g.trigger.type == TT::ServerMessage ||
                           g.trigger.type == TT::DoACompleteZone || g.trigger.type == TT::AgentUpdateAllegiance) {
                    // DoA: one map_id for all 4 zones, use header map.
                    map_matches = (owning_header_map != GW::Constants::MapID::None &&
                                    owning_header_map == prev_map_);
                } else if (g.trigger.type == TT::MapEnter) {
                    // Dungeon level goal map_id = NEXT level: use segment_start_map.
                    map_matches = (segment_start_map != GW::Constants::MapID::None &&
                                    segment_start_map == prev_map_);
                }
                if (map_matches) {
                    // Poll throttled 1/sec, may lag. Recheck now; real abandon still fail.
                    const bool completion_backed = g.trigger.type == TT::VanquishComplete || g.trigger.type == TT::MissionComplete;
                    const auto* char_context = completion_backed ? GW::GetCharContext() : nullptr;
                    if (!(char_context && TrustedCompletionCheck(gi, char_context->player_name, g.trigger)))
                        pending_incomplete_rezone_ = true;
                }
                break;
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
    started_          = true;
    pending_run_start_ = true;
}

void GoalEngine::TriggerManual(const GoalClock& clock)
{
    if (!list_) return;
    for (int i = 0; i < static_cast<int>(list_->goals.size()); ++i) {
        GoalEntry& g = list_->goals[i];
        if (g.is_header) continue;
        if (g.status != GoalStatus::Completed && g.trigger.type == GoalTrigger::Type::Manual) {
            started_ = true;
            FireGoal(i, clock);
            CompletePreviousGoals(i, clock);
            return;
        }
    }
}

void GoalEngine::StampSplit(GoalEntry& g, const GoalClock& clock) const
{
    g.split.real_time    = clock.RealTime();
    g.split.game_time    = clock.GameTime();
    g.split.segment_real = clock.RealTime() - last_real_;
    g.split.segment_game = clock.GameTime() - last_game_;
}

void GoalEngine::FireGoal(int index, const GoalClock& clock)
{
    GoalEntry& g = list_->goals[index];
    g.status = GoalStatus::Completed;
    StampSplit(g, clock);
    last_real_ = clock.RealTime();
    last_game_ = clock.GameTime();

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
        g.status = GoalStatus::Failed;
        StampSplit(g, clock);
    }
}
