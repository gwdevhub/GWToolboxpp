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

int GoalEngine::Update(const GoalClock& clock,
                       GW::Constants::MapID current_map,
                       bool just_entered_map,
                       bool came_from_explorable,
                       GW::Constants::InstanceType instance_type,
                       bool vq_complete,
                       int  player_level)
{
    const bool is_explorable = (instance_type == GW::Constants::InstanceType::Explorable);

    if (!list_ || list_->goals.empty()) {
        prev_map_             = current_map;
        mission_complete_map_ = GW::Constants::MapID::None;
        mission_bonus_map_    = GW::Constants::MapID::None;
        return -1;
    }

    if (!started_ && just_entered_map)
        started_ = true;

    int fired = -1;

    if (started_) {
        for (int i = 0; i < static_cast<int>(list_->goals.size()); ++i) {
            GoalEntry& g = list_->goals[i];
            if (g.completed) continue;

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
                    fire = vq_complete && (current_map == t.map_id);
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
                    fire = title && title->current_title_tier_index >= static_cast<uint32_t>(t.level);
                    break;
                }

                case GoalTrigger::Type::Manual:
                    break;

                default:
                    break;
            }

            if (fire) {
                FireGoal(i, clock);
                fired = i;
                break;
            }
        }
    }

    mission_complete_map_ = GW::Constants::MapID::None;
    mission_bonus_map_    = GW::Constants::MapID::None;
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
        if (!g.completed && g.trigger.type == GoalTrigger::Type::Manual) {
            if (!started_) started_ = true;
            FireGoal(i, clock);
            return;
        }
    }
}

void GoalEngine::FireGoal(int index, const GoalClock& clock)
{
    GoalEntry& g = list_->goals[index];
    g.completed          = true;
    g.split.real_time    = clock.RealTime();
    g.split.game_time    = clock.GameTime();
    g.split.segment_real = clock.RealTime() - last_real_;
    g.split.segment_game = clock.GameTime() - last_game_;
    last_real_ = clock.RealTime();
    last_game_ = clock.GameTime();
}
