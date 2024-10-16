#include <CharacteristicImpls.h>

#include <enumUtils.h>
#include <InstanceInfo.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Skill.h>
#include <GWCA/GameEntities/Camera.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/CameraMgr.h>

#include <ImGuiCppWrapper.h>

namespace {
    constexpr float eps = 1e-3f;

    int GetHealthRegenPips(const GW::AgentLiving& agent)
    {
        if (agent.GetIsDead()) return 0;
        const float health_regen_per_second = agent.max_hp * agent.hp_pips;
        return (int)std::ceil(health_regen_per_second / 2.f); // 1 pip = 2 health per second
    }
    float angleToAgent(const GW::AgentLiving& centerAgent, const GW::AgentLiving& positionedAgent, ReferenceFrame refFrame)
    {
        if (GW::GetSquareDistance(centerAgent.pos, positionedAgent.pos) < eps) return 0.f;

        const auto angleBetweenNormalizedVectors = [](GW::Vec2f a, GW::Vec2f b){ return std::acos(a.x * b.x + a.y * b.y); };
        constexpr auto radiansToDegree = 180.f / 3.141592741f;

        if (refFrame == ReferenceFrame::Player) {        
            const auto forwards = GW::Normalize(GW::Vec2f{centerAgent.rotation_cos, centerAgent.rotation_sin});
            const auto toTarget = GW::Normalize(GW::Vec2f{positionedAgent.pos.x - centerAgent.pos.x, positionedAgent.pos.y - centerAgent.pos.y});
            return angleBetweenNormalizedVectors(forwards, toTarget) * radiansToDegree;
        }
        else
        {
            const auto camera = GW::CameraMgr::GetCamera();
            if (!camera) return 0.f;
            const auto forwards = GW::Normalize(GW::Vec2f{camera->look_at_target - camera->position});
            const auto toTarget = GW::Normalize(GW::Vec2f{positionedAgent.pos.x - centerAgent.pos.x, positionedAgent.pos.y - centerAgent.pos.y});
            return angleBetweenNormalizedVectors(forwards, toTarget) * radiansToDegree;
        }
    }
}

/// ------------- PositionCharacteristic -------------
PositionCharacteristic::PositionCharacteristic()
{
    if (auto player = GW::Agents::GetControlledCharacter()) 
    {
        position = player->pos;
    }
}
PositionCharacteristic::PositionCharacteristic(InputStream& stream)
{
    stream >> position.x >> position.y >> distance >> comp;
}
void PositionCharacteristic::serialize(OutputStream& stream) const
{
    Characteristic::serialize(stream);

    stream << position.x << position.y << distance << comp;
}
bool PositionCharacteristic::check(const GW::AgentLiving& agent) const
{
    return compare(GW::GetDistance(position, agent.pos), comp, distance);
}
void PositionCharacteristic::drawSettings()
{
    ImGui::PushItemWidth(90.f);
    ImGui::Text("Distance to position");
    ImGui::SameLine();
    ImGui::InputFloat("x", &position.x, 0.0f, 0.0f);
    ImGui::SameLine();
    ImGui::InputFloat("y", &position.y, 0.0f, 0.0f);
    ImGui::SameLine();
    drawEnumButton(ComparisonOperator::Equals, ComparisonOperator::NotEquals, comp, 0, 30.f);
    ImGui::SameLine();
    ImGui::InputFloat("", &distance, 0.0f, 0.0f);
    ImGui::PopItemWidth();
}

/// ------------- PositionPolygonCharacteristic -------------
PositionPolygonCharacteristic::PositionPolygonCharacteristic(InputStream& stream)
{
    polygon = readPositions(stream);
    stream >> comp;
}
void PositionPolygonCharacteristic::serialize(OutputStream& stream) const
{
    Characteristic::serialize(stream);

    writePositions(stream, polygon);
    stream << comp;
}
bool PositionPolygonCharacteristic::check(const GW::AgentLiving& agent) const
{
    if (polygon.size() < 3u) return false;
    const auto isWithin = pointIsInsidePolygon(agent.pos, polygon);
    return (comp == IsIsNot::Is) == isWithin;
}
void PositionPolygonCharacteristic::drawSettings()
{
    drawEnumButton(IsIsNot::Is, IsIsNot::IsNot, comp, 0, 40.f);
    ImGui::SameLine();
    ImGui::Text("within polygon");
    ImGui::SameLine();
    drawPolygonSelector(polygon);
}

/// ------------- DistanceToPlayerCharacteristic -------------
DistanceToPlayerCharacteristic::DistanceToPlayerCharacteristic(InputStream& stream)
{
    stream >> distance >> comp;
}
void DistanceToPlayerCharacteristic::serialize(OutputStream& stream) const
{
    Characteristic::serialize(stream);

    stream << distance << comp;
}
bool DistanceToPlayerCharacteristic::check(const GW::AgentLiving& agent) const
{
    const auto player = GW::Agents::GetControlledCharacter();
    if (!player) return false;

    return compare(GW::GetDistance(player->pos, agent.pos), comp, distance);
}
void DistanceToPlayerCharacteristic::drawSettings()
{
    ImGui::PushItemWidth(90.f);
    ImGui::Text("Distance to player");
    ImGui::SameLine();
    drawEnumButton(ComparisonOperator::Equals, ComparisonOperator::NotEquals, comp, 0, 30.f);
    ImGui::SameLine();
    ImGui::InputFloat("", &distance, 0.0f, 0.0f);
    ImGui::PopItemWidth();
}

/// ------------- DistanceToTargetCharacteristic -------------
DistanceToTargetCharacteristic::DistanceToTargetCharacteristic(InputStream& stream)
{
    stream >> distance >> comp;
}
void DistanceToTargetCharacteristic::serialize(OutputStream& stream) const
{
    Characteristic::serialize(stream);

    stream << distance << comp;
}
bool DistanceToTargetCharacteristic::check(const GW::AgentLiving& agent) const
{
    const auto target = GW::Agents::GetTargetAsAgentLiving();
    if (!target) return false;

    return compare(GW::GetDistance(target->pos, agent.pos), comp, distance);
}
void DistanceToTargetCharacteristic::drawSettings()
{
    ImGui::PushItemWidth(90.f);
    ImGui::Text("Distance to target");
    ImGui::SameLine();
    drawEnumButton(ComparisonOperator::Equals, ComparisonOperator::NotEquals, comp, 0, 30.f);
    ImGui::SameLine();
    ImGui::InputFloat("", &distance, 0.0f, 0.0f);
    ImGui::PopItemWidth();
}

/// ------------- DistanceToModelIdCharacteristic -------------
DistanceToModelIdCharacteristic::DistanceToModelIdCharacteristic(InputStream& stream)
{
    stream >> modelId >> distance >> comp;
}
void DistanceToModelIdCharacteristic::serialize(OutputStream& stream) const
{
    Characteristic::serialize(stream);

    stream << modelId << distance << comp;
}
bool DistanceToModelIdCharacteristic::check(const GW::AgentLiving& reference) const
{
    const auto agents = GW::Agents::GetAgentArray();
    if (!agents) return false;

    auto minDistance = std::numeric_limits<float>::max();
    for (const auto& agent : *agents) 
    {
        if (!agent || !agent->GetIsLivingType()) continue;
        const auto living = agent->GetAsAgentLiving();
        if (living->player_number != (uint16_t)modelId) continue;
        minDistance = std::min(minDistance, GW::GetDistance(reference.pos, living->pos));
    }

    return compare(minDistance, comp, distance);
}
void DistanceToModelIdCharacteristic::drawSettings()
{
    ImGui::PushItemWidth(90.f);
    ImGui::Text("Distance to closest agent of model Id");
    ImGui::SameLine();
    drawModelIDSelector(modelId);
    ImGui::SameLine();
    drawEnumButton(ComparisonOperator::Equals, ComparisonOperator::NotEquals, comp, 0, 30.f);
    ImGui::SameLine();
    ImGui::InputFloat("", &distance, 0.0f, 0.0f);
    ImGui::PopItemWidth();
}

/// ------------- ClassCharacteristic -------------
ClassCharacteristic::ClassCharacteristic(InputStream& stream)
{
    stream >> primary >> secondary >> comp;
}
void ClassCharacteristic::serialize(OutputStream& stream) const
{
    Characteristic::serialize(stream);

    stream << primary << secondary << comp;
}
bool ClassCharacteristic::check(const GW::AgentLiving& agent) const
{
    const auto correctClass = (primary == Class::Any || primary == (Class)agent.primary) && (secondary == Class::Any || secondary == (Class)agent.secondary);
    return (comp == IsIsNot::Is) == correctClass;
}
void ClassCharacteristic::drawSettings()
{
    ImGui::Text("Class");
    ImGui::SameLine();
    drawEnumButton(IsIsNot::Is, IsIsNot::IsNot, comp, 0, 40.f);
    ImGui::SameLine();
    drawEnumButton(Class::Any, Class::Dervish, primary, 1);
    ImGui::SameLine();
    ImGui::Text("/");
    ImGui::SameLine();
    drawEnumButton(Class::Any, Class::Dervish, secondary, 2);
}

/// ------------- NameCharacteristic -------------
NameCharacteristic::NameCharacteristic(InputStream& stream)
{
    name = readStringWithSpaces(stream);
    stream >> comp;
}
void NameCharacteristic::serialize(OutputStream& stream) const
{
    Characteristic::serialize(stream);

    writeStringWithSpaces(stream, name);
    stream << comp;
}
bool NameCharacteristic::check(const GW::AgentLiving& agent) const
{
    const auto correctName = !name.empty() && InstanceInfo::getInstance().getDecodedAgentName(agent.agent_id).contains(name);
    return (comp == IsIsNot::Is) == correctName;
}
void NameCharacteristic::drawSettings()
{
    ImGui::Text("Name");
    ImGui::SameLine();
    drawEnumButton(IsIsNot::Is, IsIsNot::IsNot, comp, 0, 40.f);
    ImGui::SameLine();
    ImGui::InputText("", &name);
}

/// ------------- HPCharacteristic -------------
HPCharacteristic::HPCharacteristic(InputStream& stream)
{
    stream >> hp >> comp;
}
void HPCharacteristic::serialize(OutputStream& stream) const
{
    Characteristic::serialize(stream);

    stream << hp << comp;
}
bool HPCharacteristic::check(const GW::AgentLiving& agent) const
{
    return compare(100.f * agent.hp, comp, hp);
}
void HPCharacteristic::drawSettings()
{
    ImGui::PushItemWidth(50.f);
    ImGui::Text("Hit points");
    ImGui::SameLine();
    drawEnumButton(ComparisonOperator::Equals, ComparisonOperator::NotEquals, comp, 0, 30.f);
    ImGui::SameLine();
    ImGui::InputFloat("%", &hp, 0.0f, 0.0f);
    ImGui::PopItemWidth();
}

/// ------------- HPRegenCharacteristic -------------
HPRegenCharacteristic::HPRegenCharacteristic(InputStream& stream)
{
    stream >> hpRegen >> comp;
}
void HPRegenCharacteristic::serialize(OutputStream& stream) const
{
    Characteristic::serialize(stream);

    stream << hpRegen << comp;
}
bool HPRegenCharacteristic::check(const GW::AgentLiving& agent) const
{
    const auto pips = GetHealthRegenPips(agent);
    return compare(pips, comp, hpRegen);
}
void HPRegenCharacteristic::drawSettings()
{
    ImGui::PushItemWidth(50.f);
    ImGui::Text("HP regeneration");
    ImGui::SameLine();
    drawEnumButton(ComparisonOperator::Equals, ComparisonOperator::NotEquals, comp, 0, 30.f);
    ImGui::SameLine();
    ImGui::InputInt("", &hpRegen, 0);
    ImGui::PopItemWidth();
}

/// ------------- SpeedCharacteristic -------------
SpeedCharacteristic::SpeedCharacteristic(InputStream& stream)
{
    stream >> speed >> comp;
}
void SpeedCharacteristic::serialize(OutputStream& stream) const
{
    Characteristic::serialize(stream);

    stream << speed << comp;
}
bool SpeedCharacteristic::check(const GW::AgentLiving& agent) const
{
    return compare(GW::GetNorm(agent.velocity), comp, speed);
}
void SpeedCharacteristic::drawSettings()
{
    ImGui::PushItemWidth(90.f);
    ImGui::Text("Speed");
    ImGui::SameLine();
    drawEnumButton(ComparisonOperator::Equals, ComparisonOperator::NotEquals, comp, 0, 30.f);
    ImGui::SameLine();
    ImGui::InputFloat("", &speed, 0.0f, 0.0f);
    ImGui::PopItemWidth();
}

/// ------------- WeaponTypeCharacteristic -------------
WeaponTypeCharacteristic::WeaponTypeCharacteristic(InputStream& stream)
{
    stream >> weapon >> comp;
}
void WeaponTypeCharacteristic::serialize(OutputStream& stream) const
{
    Characteristic::serialize(stream);

    stream << weapon << comp;
}
bool WeaponTypeCharacteristic::check(const GW::AgentLiving& agent) const
{
    const auto correctWeapon = checkWeaponType(weapon, agent.weapon_type);
    return (comp == IsIsNot::Is) == correctWeapon;
}
void WeaponTypeCharacteristic::drawSettings()
{
    ImGui::Text("Weapon Type");
    ImGui::SameLine();
    drawEnumButton(IsIsNot::Is, IsIsNot::IsNot, comp, 0, 40.f);
    ImGui::SameLine();
    drawEnumButton(WeaponType::Any, WeaponType::Staff, weapon, 1);
}

/// ------------- ModelCharacteristic -------------
ModelCharacteristic::ModelCharacteristic(InputStream& stream)
{
    stream >> modelId >> comp;
}
void ModelCharacteristic::serialize(OutputStream& stream) const
{
    Characteristic::serialize(stream);

    stream << modelId << comp;
}
bool ModelCharacteristic::check(const GW::AgentLiving& agent) const
{
    return compare(agent.player_number, comp, (uint16_t)modelId);
}
void ModelCharacteristic::drawSettings()
{
    ImGui::Text("Model / player number");
    ImGui::SameLine();
    drawEnumButton(IsIsNot::Is, IsIsNot::IsNot, comp, 0, 40.f);
    ImGui::SameLine();
    drawModelIDSelector(modelId, "");
}

/// ------------- AllegianceCharacteristic -------------
AllegianceCharacteristic::AllegianceCharacteristic(InputStream& stream)
{
    stream >> agentType >> comp;
}
void AllegianceCharacteristic::serialize(OutputStream& stream) const
{
    Characteristic::serialize(stream);

    stream << agentType << comp;
}
bool AllegianceCharacteristic::check(const GW::AgentLiving& agent) const
{
    const auto player = GW::Agents::GetControlledCharacter();
    const auto info = GW::PartyMgr::GetPartyInfo();
    if (!player || !info) return false;
        
    const auto hasCorrectType = [&] {
        switch (agentType) 
        {
            case AgentType::Any:
                return true;
            case AgentType::Self:
                return player && agent.agent_id == player->agent_id;
            case AgentType::PartyMember:
                if (agent.agent_id == player->agent_id) return false;
                if (agent.IsPlayer()) return true;
                for (const auto& hero : info->heroes)
                    if (agent.agent_id == hero.agent_id) return true;
                for (const auto& henchman : info->henchmen)
                    if (agent.agent_id == henchman.agent_id) return true;
                return false;
            case AgentType::Friendly:
                if (agent.agent_id == player->agent_id) return false;
                return agent.allegiance != GW::Constants::Allegiance::Enemy;
            case AgentType::Hostile:
                return agent.allegiance == GW::Constants::Allegiance::Enemy;
            default:
                return false;
        }
    }();

    return hasCorrectType == (comp == IsIsNot::Is);
}
void AllegianceCharacteristic::drawSettings()
{
    ImGui::Text("Allegiance");
    ImGui::SameLine();
    drawEnumButton(IsIsNot::Is, IsIsNot::IsNot, comp, 0, 40.f);
    ImGui::SameLine();
    drawEnumButton(AgentType::Any, AgentType::Hostile, agentType, 1);
}

/// ------------- StatusCharacteristic -------------
StatusCharacteristic::StatusCharacteristic(InputStream& stream)
{
    stream >> status >> comp;
}
void StatusCharacteristic::serialize(OutputStream& stream) const
{
    Characteristic::serialize(stream);

    stream << status << comp;
}
bool StatusCharacteristic::check(const GW::AgentLiving& agent) const
{
    const auto hasCorrectStatus = [&] {
        switch (status) {
            case Status::Enchanted:
                return agent.GetIsEnchanted();
            case Status::WeaponSpelled:
                return agent.GetIsWeaponSpelled();
            case Status::Alive:
                return agent.GetIsAlive();
            case Status::Bleeding:
                return agent.GetIsBleeding();
            case Status::Crippled:
                return agent.GetIsCrippled();
            case Status::DeepWounded:
                return agent.GetIsDeepWounded();
            case Status::Poisoned:
                return agent.GetIsPoisoned();
            case Status::Hexed:
                return agent.GetIsHexed() || agent.GetIsDegenHexed();
            case Status::Idle:
                return agent.GetIsIdle();
            case Status::KnockedDown:
                return agent.GetIsKnockedDown();
            case Status::Moving:
                return agent.GetIsMoving();
            case Status::Attacking:
                return agent.GetIsAttacking();
            case Status::Casting:
                return agent.GetIsCasting();
            default:
                return false;
        }
    }();
    return hasCorrectStatus == (comp == IsIsNot::Is);
}
void StatusCharacteristic::drawSettings()
{
    ImGui::Text("Status");
    ImGui::SameLine();
    drawEnumButton(IsIsNot::Is, IsIsNot::IsNot, comp, 0, 40.f);
    ImGui::SameLine();
    drawEnumButton(Status::Enchanted, Status::Casting, status, 1);
}

/// ------------- SkillCharacteristic -------------
SkillCharacteristic::SkillCharacteristic(InputStream& stream)
{
    stream >> skill >> comp;
}
void SkillCharacteristic::serialize(OutputStream& stream) const
{
    Characteristic::serialize(stream);

    stream << skill << comp;
}
bool SkillCharacteristic::check(const GW::AgentLiving& agent) const
{
    return compare(agent.skill, comp, (uint16_t)skill);
}
void SkillCharacteristic::drawSettings()
{
    drawEnumButton(IsIsNot::Is, IsIsNot::IsNot, comp, 0, 40.f);
    ImGui::SameLine();
    ImGui::Text("using skill");
    ImGui::SameLine();
    drawSkillIDSelector(skill);
}

/// ------------- BondCharacteristic -------------
BondCharacteristic::BondCharacteristic(InputStream& stream)
{
    stream >> skill >> comp;
}
void BondCharacteristic::serialize(OutputStream& stream) const
{
    Characteristic::serialize(stream);

    stream << skill << comp;
}
bool BondCharacteristic::check(const GW::AgentLiving& agent) const
{
    const auto bonds = GW::Effects::GetPlayerBuffs();
    if (!bonds) return false;

    const auto isMaintaining = std::ranges::any_of(*bonds, [&](const auto& bond){ return bond.target_agent_id == agent.agent_id && bond.skill_id == skill; });
    return (comp == IsIsNot::Is) == isMaintaining;
}
void BondCharacteristic::drawSettings()
{
    drawEnumButton(IsIsNot::Is, IsIsNot::IsNot, comp, 0, 40.f);
    ImGui::SameLine();
    ImGui::Text("target of player maintained bond");
    ImGui::SameLine();
    drawSkillIDSelector(skill);
}

/// ------------- AngleToPlayerForwardCharacteristic -------------
AngleToPlayerForwardCharacteristic::AngleToPlayerForwardCharacteristic(InputStream& stream)
{
    stream >> angle >> comp;
}
void AngleToPlayerForwardCharacteristic::serialize(OutputStream& stream) const
{
    Characteristic::serialize(stream);

    stream << angle << comp;
}
bool AngleToPlayerForwardCharacteristic::check(const GW::AgentLiving& agent) const
{
    const auto player = GW::Agents::GetControlledCharacter();
    if (!player) return false;
    return compare(angleToAgent(*player, agent, ReferenceFrame::Player), comp, angle);
}
void AngleToPlayerForwardCharacteristic::drawSettings()
{
    ImGui::PushItemWidth(90.f);
    ImGui::Text("Has angle to player forward");
    ImGui::SameLine();
    drawEnumButton(ComparisonOperator::Equals, ComparisonOperator::NotEquals, comp, 0, 30.f);
    ImGui::SameLine();
    ImGui::InputFloat("", &angle, 0.0f, 0.0f);
    ImGui::PopItemWidth();
}

/// ------------- AngleToCameraForwardCharacteristic -------------
AngleToCameraForwardCharacteristic::AngleToCameraForwardCharacteristic(InputStream& stream)
{
    stream >> angle >> comp;
}
void AngleToCameraForwardCharacteristic::serialize(OutputStream& stream) const
{
    Characteristic::serialize(stream);

    stream << angle << comp;
}
bool AngleToCameraForwardCharacteristic::check(const GW::AgentLiving& agent) const
{
    const auto player = GW::Agents::GetControlledCharacter();
    if (!player) return false;
    return compare(angleToAgent(*player, agent, ReferenceFrame::Camera), comp, angle);
}
void AngleToCameraForwardCharacteristic::drawSettings()
{
    ImGui::PushItemWidth(90.f);
    ImGui::Text("Has angle to camera forward");
    ImGui::SameLine();
    drawEnumButton(ComparisonOperator::Equals, ComparisonOperator::NotEquals, comp, 0, 30.f);
    ImGui::SameLine();
    ImGui::InputFloat("", &angle, 0.0f, 0.0f);
    ImGui::PopItemWidth();
}
/// ------------- IsStoredTargetCharacteristic -------------
IsStoredTargetCharacteristic::IsStoredTargetCharacteristic(InputStream&)
{
}
void IsStoredTargetCharacteristic::serialize(OutputStream& stream) const
{
    Characteristic::serialize(stream);
}
bool IsStoredTargetCharacteristic::check(const GW::AgentLiving& agent) const
{
    return InstanceInfo::getInstance().isStoredTarget(agent);
}
void IsStoredTargetCharacteristic::drawSettings()
{
    ImGui::Text("Is stored target");
}
