#include <ConditionImpls.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/CharContext.h>

bool IsInMapCondition::check() const {
    return GW::Map::GetMapID() == id;
}

bool PartyPlayerCountCondition::check() const
{
    return GW::PartyMgr::GetPartyPlayerCount() == count;
}

bool InstanceProgressCondition::check() const {
    return GW::GetGameContext()->character->progress_bar->progress > requiredProgress;
}

bool PlayerIsNearPositionCondition::check() const
{
    const auto player = GW::Agents::GetPlayerAsAgentLiving(); 
    if (!player) return false;
    return GW::GetSquareDistance(player->pos, pos) < squareAccuracy;
}

bool PlayerHasBuffCondition::check() const
{
    return GW::Effects::GetPlayerBuffBySkillId(id);
}

bool PlayerHasSkillCondition::check() const
{
    return GW::SkillbarMgr::GetSkillSlot(id) >= 0;
}

bool CurrentTargetIsCastingSkillCondition::check() const
{
    const auto target = GW::Agents::GetTargetAsAgentLiving();
    return target && static_cast<GW::Constants::SkillID>(target->skill) == id;
}
