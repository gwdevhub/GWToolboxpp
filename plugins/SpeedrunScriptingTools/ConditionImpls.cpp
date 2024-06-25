#include <ConditionImpls.h>

#include <ConditionIO.h>
#include <enumUtils.h>
#include <InstanceInfo.h>

#include <GWCA/Constants/Constants.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Player.h>
#include <GWCA/GameEntities/Skill.h>
#include <GWCA/GameEntities/Attribute.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/CharContext.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/ChatMgr.h>

#include <Keys.h>
#include "ImGuiCppWrapper.h"
#include <algorithm>

namespace {
    constexpr double eps = 1e-3;
    constexpr float indent = 30.f;
    const std::string missingContentToken = "/";
    const std::string endOfListToken = ">";

    GW::Item* FindMatchingItem(GW::Constants::Bag _bag_idx, uint32_t model_id)
    {
        GW::Bag* bag = GW::Items::GetBag(_bag_idx);
        if (!bag) return nullptr;
        GW::ItemArray& items = bag->items;
        for (auto _item : items) {
            if (_item && _item->model_id == model_id) return _item;
        }
        return nullptr;
    }
    GW::Item* FindMatchingItem(uint32_t model_id)
    {
        constexpr size_t bags_size = static_cast<size_t>(GW::Constants::Bag::Equipment_Pack) + 1;

        GW::Item* item = nullptr;
        for (size_t i = static_cast<size_t>(GW::Constants::Bag::Backpack); i < bags_size && !item; i++)
            item = FindMatchingItem(static_cast<GW::Constants::Bag>(i), model_id);
        if (!item) 
            item = FindMatchingItem(GW::Constants::Bag::Equipped_Items, model_id);
        return item;
    }

    // Returns angle in degrees (0-180) between player forwards direction and agent
    float angleToAgent(const GW::AgentLiving* player, const GW::AgentLiving* agent) 
    {
        if (GW::GetSquareDistance(player->pos,agent->pos) < eps)
            return 0.f;
        constexpr auto radiansToDegree = 180.f / 3.141592741f;
        const auto angleBetweenNormalizedVectors = [](GW::Vec2f a, GW::Vec2f b) {
            return std::acos(a.x * b.x + a.y * b.y);
        };
        const auto forwards = GW::Normalize(GW::Vec2f{player->rotation_cos, player->rotation_sin});
        const auto toTarget = GW::Normalize(GW::Vec2f{agent->pos.x - player->pos.x, agent->pos.y - player->pos.y});
        return angleBetweenNormalizedVectors(forwards, toTarget) * radiansToDegree;
    }

    enum class WeaponRequirement : uint32_t { Axe = 1, Bow = 2, Dagger = 8, Hammer = 16, Scythe = 32, Spear = 64, Ranged = 70, Sword = 128 };
    enum class EquippedWeaponType : uint16_t { Bow = 1, Axe = 2, Hammer = 3, Daggers = 4, Scythe = 5, Spear = 6, Sword = 7, Wand = 10, StaffA = 12, StaffB = 14 };

    bool weaponFulfillsRequirement(EquippedWeaponType weapon, WeaponRequirement req, GW::Constants::SkillType type) 
    {
        const auto isRanged = [](EquippedWeaponType type) {
            switch (type) {
                case EquippedWeaponType::Bow:
                case EquippedWeaponType::Spear:
                case EquippedWeaponType::Wand:
                case EquippedWeaponType::StaffA:
                case EquippedWeaponType::StaffB:
                    return true;
                case EquippedWeaponType::Axe:
                case EquippedWeaponType::Hammer:
                case EquippedWeaponType::Daggers:
                case EquippedWeaponType::Scythe:
                case EquippedWeaponType::Sword:
                    return false;
            }
            return true;
        };

        if (type != GW::Constants::SkillType::Attack) return true;
        
        switch (req) 
        {
            case WeaponRequirement::Axe:
                return weapon == EquippedWeaponType::Axe;
            case WeaponRequirement::Bow:
                return weapon == EquippedWeaponType::Bow;
            case WeaponRequirement::Dagger:
                return weapon == EquippedWeaponType::Daggers;
            case WeaponRequirement::Hammer:
                return weapon == EquippedWeaponType::Hammer;
            case WeaponRequirement::Scythe:
                return weapon == EquippedWeaponType::Scythe;
            case WeaponRequirement::Spear:
                return weapon == EquippedWeaponType::Spear;
            case WeaponRequirement::Sword:
                return weapon == EquippedWeaponType::Sword;
            case WeaponRequirement::Ranged:
                return isRanged(weapon);
            default: // Melee Attack
                return !isRanged(weapon);
        }
    }

    uint32_t getEnergyCost(const GW::Skill& skill)
    {
        double cost = skill.GetEnergyCost();
        if (GW::Effects::GetPlayerEffectBySkillId(GW::Constants::SkillID::Quickening_Zephyr)) 
        {
            cost *= 2;
        }

        const auto reduceCostBasedOnAttribute = [&](GW::Constants::Attribute attribute) 
        {
            const auto player = GW::Agents::GetPlayerAsAgentLiving();
            if (!player) return;
            const auto playerAttributes = GW::PartyMgr::GetAgentAttributes(player->agent_id);
            if (!playerAttributes) return;
            const auto attributeLevel = playerAttributes[(uint32_t)attribute].level;
            cost *= (1. - attributeLevel * 0.04);
        };

        if (skill.profession == GW::Constants::ProfessionByte::Ranger || skill.type == GW::Constants::SkillType::Ritual) 
        {
            reduceCostBasedOnAttribute(GW::Constants::Attribute::Expertise);
        }
        else if (skill.profession == GW::Constants::ProfessionByte::Dervish && skill.type == GW::Constants::SkillType::Enchantment)
        {
            reduceCostBasedOnAttribute(GW::Constants::Attribute::Mysticism);
        }

        return (uint32_t)std::round(cost);
    }
}

/// ------------- InvertedCondition -------------
NegatedCondition::NegatedCondition(InputStream& stream)
{
    std::string read;
    stream >> read;
    if (read == missingContentToken)
        cond = nullptr;
    else if (read == "C")
        cond = readCondition(stream);
    else
        return;
}
void NegatedCondition::serialize(OutputStream& stream) const
{
    Condition::serialize(stream);
    if (cond)
        cond->serialize(stream);
    else
        stream << missingContentToken;
}
bool NegatedCondition::check() const
{
    return cond && !cond->check();
}
void NegatedCondition::drawSettings()
{
    ImGui::PushID(drawId());
    ImGui::Text("NOT (");
    ImGui::SameLine();
    if (cond) {
        cond->drawSettings();
    }
    else {
        cond = drawConditionSelector(100.f);
    }
    ImGui::SameLine();
    ImGui::Text(")");
    ImGui::PopID();
}

/// ------------- DisjunctionCondition -------------
DisjunctionCondition::DisjunctionCondition(InputStream& stream) 
{
    int count;
    std::string token;
    stream >> count;

    for (int i = 0; i < count; ++i)
    {
        if (stream.isAtSeparator() || !(stream >> token)) continue;
        
        if (token == missingContentToken) 
            conditions.push_back(nullptr);
        else if (token == "C") 
        {
            if (auto read = readCondition(stream)) 
                conditions.push_back(std::move(read));
        }
        else 
        {
            return;
        }

        stream.proceedPastSeparator(2);
    }
}
void DisjunctionCondition::serialize(OutputStream& stream) const 
{
    Condition::serialize(stream);

    stream << conditions.size();

    for (const auto& condition : conditions) 
    {
        if (condition)
            condition->serialize(stream);
        else
            stream << missingContentToken;

        stream.writeSeparator(2);
    }
}
bool DisjunctionCondition::check() const
{
    return std::ranges::any_of(conditions, [](const auto& condition) {return condition && condition->check();});
}
void DisjunctionCondition::drawSettings()
{
    ImGui::PushID(drawId());
    ImGui::Text("If ONE of the following is true:");
    ImGui::Indent(indent);

    int rowToDelete = -1;
    for (int i = 0; i < int(conditions.size()); ++i) {
        ImGui::PushID(i);

        ImGui::Bullet();
        if (ImGui::Button("X")) {
            if (conditions[i])
                conditions[i] = nullptr;
            else
                rowToDelete = i;
        }

        ImGui::SameLine();
        if (conditions[i])
            conditions[i]->drawSettings();
        else
            conditions[i] = drawConditionSelector(100.f);

        ImGui::PopID();
    }
    if (rowToDelete != -1) conditions.erase(conditions.begin() + rowToDelete);

    ImGui::Bullet();
    if (ImGui::Button("+")) {
        conditions.push_back(nullptr);
    }

    ImGui::Unindent(indent);
    ImGui::PopID();
}

/// ------------- ConjunctionCondition -------------
ConjunctionCondition::ConjunctionCondition(InputStream& stream)
{
    int count;
    std::string token;
    stream >> count;

    for (int i = 0; i < count; ++i) 
    {
        if (stream.isAtSeparator() || !(stream >> token)) continue;

        if (token == missingContentToken)
            conditions.push_back(nullptr);
        else if (token == "C") 
        {
            if (auto read = readCondition(stream)) 
                conditions.push_back(std::move(read));
        }
        else {
            return;
        }

        stream.proceedPastSeparator(2);
    }
}
void ConjunctionCondition::serialize(OutputStream& stream) const
{
    Condition::serialize(stream);

    stream << conditions.size();

    for (const auto& condition : conditions) 
    {
        if (condition)
            condition->serialize(stream);
        else
            stream << missingContentToken;

        stream.writeSeparator(2);
    }
}
bool ConjunctionCondition::check() const
{
    return std::ranges::all_of(conditions, [](const auto& condition) {return !condition || condition->check();});
}
void ConjunctionCondition::drawSettings()
{
    ImGui::PushID(drawId());
    ImGui::Text("If ALL of the following is true:");
    ImGui::Indent(indent);

    int rowToDelete = -1;
    for (int i = 0; i < int(conditions.size()); ++i) {
        ImGui::PushID(i);

        ImGui::Bullet();
        if (ImGui::Button("X")) {
            if (conditions[i])
                conditions[i] = nullptr;
            else
                rowToDelete = i;
        }

        ImGui::SameLine();
        if (conditions[i])
            conditions[i]->drawSettings();
        else
            conditions[i] = drawConditionSelector(100.f);

        ImGui::PopID();
    }
    if (rowToDelete != -1) conditions.erase(conditions.begin() + rowToDelete);

    ImGui::Bullet();
    if (ImGui::Button("+")) {
        conditions.push_back(nullptr);
    }

    ImGui::Unindent(indent);
    ImGui::PopID();
}

/// ------------- IsInMapCondition -------------
IsInMapCondition::IsInMapCondition()
{
    id = GW::Map::GetMapID();
}
IsInMapCondition::IsInMapCondition(InputStream& stream)
{
    stream >> id;
}
void IsInMapCondition::serialize(OutputStream& stream) const
{
    Condition::serialize(stream);

    stream << id;
}
bool IsInMapCondition::check() const {
    return GW::Map::GetMapID() == id;
}
void IsInMapCondition::drawSettings() {
    ImGui::PushID(drawId());
    ImGui::Text("If player is in map");
    ImGui::SameLine();
    drawMapIDSelector(id);
    ImGui::PopID();
}

/// ------------- PartyPlayerCountCondition -------------
PartyPlayerCountCondition::PartyPlayerCountCondition(InputStream& stream)
{
    stream >> count;
}
void PartyPlayerCountCondition::serialize(OutputStream& stream) const
{
    Condition::serialize(stream);

    stream << count;
}
bool PartyPlayerCountCondition::check() const
{
    return GW::PartyMgr::GetPartySize() == uint32_t(count);
}
void PartyPlayerCountCondition::drawSettings()
{
    ImGui::PushID(drawId());
    ImGui::Text("If the party size is");
    ImGui::PushItemWidth(30);
    ImGui::SameLine();
    ImGui::InputInt("", &count, 0);
    ImGui::PopID();
}

/// ------------- PartyHasLoadedInCondition -------------
PartyHasLoadedInCondition::PartyHasLoadedInCondition(InputStream& stream)
{
    stream >> req >> slot;
}
void PartyHasLoadedInCondition::serialize(OutputStream& stream) const
{
    Condition::serialize(stream);

    stream << req << slot;
}
bool PartyHasLoadedInCondition::check() const
{
    const auto partyInfo = GW::PartyMgr::GetPartyInfo();
    if (!partyInfo) return false;

    if (req == PlayerConnectednessRequirement::All)
        return std::ranges::all_of(partyInfo->players, [](const auto& player) { return player.connected(); });
    else
        return slot - 1 < (int)partyInfo->players.size() && partyInfo->players[slot - 1].connected();
}
void PartyHasLoadedInCondition::drawSettings()
{
    ImGui::PushID(drawId());
    ImGui::Text("If");
    ImGui::SameLine();
    drawEnumButton(PlayerConnectednessRequirement::All, PlayerConnectednessRequirement::Individual, req);
    if (req == PlayerConnectednessRequirement::Individual)
    {
        ImGui::SameLine();
        ImGui::Text("with player number");
        ImGui::SameLine();
        ImGui::PushItemWidth(50.f);
        ImGui::InputInt("", &slot, 0);
        if (slot < 1) slot = 1;
        if (slot > 12) slot = 12;
    }
    else
    {
        slot = 1;
    }
    ImGui::SameLine();
    ImGui::Text("has finished loading");
    
    
    ImGui::PopID();
}

/// ------------- InstanceProgressCondition -------------
InstanceProgressCondition::InstanceProgressCondition(InputStream& stream)
{
    stream >> requiredProgress;
}
void InstanceProgressCondition::serialize(OutputStream& stream) const
{
    Condition::serialize(stream);

    stream << requiredProgress;
}
bool InstanceProgressCondition::check() const {
    return GW::GetGameContext()->character->progress_bar->progress * 100.f + eps > requiredProgress;
}
void InstanceProgressCondition::drawSettings()
{
    ImGui::PushID(drawId());
    ImGui::Text("If the instance progress is greater than or equal");
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    ImGui::InputFloat("%", &requiredProgress, 0);
    ImGui::PopID();
}

/// ------------- OnlyTriggerOnceCondition -------------
bool OnlyTriggerOnceCondition::check() const
{
    const auto currentInstanceId = InstanceInfo::getInstance().getInstanceId();
    if (triggeredLastInInstanceId == currentInstanceId) return false;
    
    triggeredLastInInstanceId = currentInstanceId;
    return true;
}
void OnlyTriggerOnceCondition::drawSettings()
{
    ImGui::PushID(drawId());
    ImGui::Text("If script has not been triggered in this instance");
    ImGui::PopID();
}

/// ------------- PlayerIsNearPositionCondition -------------
PlayerIsNearPositionCondition::PlayerIsNearPositionCondition()
{
    if (auto player = GW::Agents::GetPlayerAsAgentLiving()) {
        pos = player->pos;
    }
}
PlayerIsNearPositionCondition::PlayerIsNearPositionCondition(InputStream& stream) 
{
    stream >> pos.x >> pos.y >> accuracy;
}
void PlayerIsNearPositionCondition::serialize(OutputStream& stream) const
{
    Condition::serialize(stream);

    stream << pos.x << pos.y << accuracy;
}
bool PlayerIsNearPositionCondition::check() const
{
    const auto player = GW::Agents::GetPlayerAsAgentLiving(); 
    if (!player) return false;
    return GW::GetDistance(player->pos, pos) < accuracy + eps;
}
void PlayerIsNearPositionCondition::drawSettings()
{
    ImGui::PushID(drawId());
    ImGui::Text("If the player is near position");
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    ImGui::InputFloat("x", &pos.x, 0.0f, 0.0f);
    ImGui::SameLine();
    ImGui::InputFloat("y", &pos.y, 0.0f, 0.0f);
    ImGui::SameLine();
    ImGui::InputFloat("Accuracy", &accuracy, 0.0f, 0.0f);
    ImGui::PopID();
}

/// ------------- PlayerHasBuffCondition -------------
PlayerHasBuffCondition::PlayerHasBuffCondition(InputStream& stream)
{
    stream >> id >> minimumDuration >> maximumDuration >> hasMinimumDuration >> hasMaximumDuration;

    // Compatability with old scripts
    if (minimumDuration > 0) hasMinimumDuration = true;
    if (maximumDuration > 0) hasMaximumDuration = true;
}
void PlayerHasBuffCondition::serialize(OutputStream& stream) const
{
    Condition::serialize(stream);

    stream << id << minimumDuration << maximumDuration << hasMinimumDuration << hasMaximumDuration;
}
bool PlayerHasBuffCondition::check() const
{
    const auto effect = GW::Effects::GetPlayerEffectBySkillId(id);
    if (!effect) return false;
    if (hasMinimumDuration && effect->GetTimeRemaining() < DWORD(minimumDuration)) return false;
    if (hasMaximumDuration && effect->GetTimeRemaining() > DWORD(maximumDuration)) return false;
    return true;
}
void PlayerHasBuffCondition::drawSettings()
{
    ImGui::PushID(drawId());
    ImGui::Text("If the player is affected by the skill");
    ImGui::SameLine();
    drawSkillIDSelector(id);
    ImGui::SameLine();
    ImGui::Text("Remaining duration (ms):");
    ImGui::PushItemWidth(50.f);
    ImGui::SameLine();
    

    if (hasMinimumDuration)
    {
        ImGui::InputInt("min", &minimumDuration, 0);
        ImGui::SameLine();
        if (ImGui::Button("X###0")) 
        {
            hasMinimumDuration = false;
            minimumDuration = 0;
        }
    }
    else 
    {
        if (ImGui::Button("Add min")) hasMinimumDuration = true;
    }

    ImGui::SameLine();
    if (hasMaximumDuration) 
    {
        ImGui::InputInt("max", &maximumDuration, 0);
        ImGui::SameLine();
        if (ImGui::Button("X###1")) {
            hasMaximumDuration = false;
            maximumDuration = 0;
        }
    }
    else 
    {
        if (ImGui::Button("Add max")) hasMaximumDuration = true;
    }

    ImGui::PopID();
}

/// ------------- PlayerHasSkillCondition -------------
PlayerHasSkillCondition::PlayerHasSkillCondition(InputStream& stream)
{
    stream >> id >> requirement;
}
void PlayerHasSkillCondition::serialize(OutputStream& stream) const
{
    Condition::serialize(stream);

    stream << id << requirement;
}
bool PlayerHasSkillCondition::check() const
{
    const auto player = GW::Agents::GetPlayerAsAgentLiving();
    const auto bar = GW::SkillbarMgr::GetPlayerSkillbar();
    if (!player || !bar || !bar->IsValid()) return false;

    return std::ranges::any_of(bar->skills, [&](const auto& skill) {
        if (skill.skill_id != id) return false;
        if (skill.skill_id == GW::Constants::SkillID::No_Skill || (uint32_t)skill.skill_id >= (uint32_t)GW::Constants::SkillID::Count) return false;
        switch (requirement) {
            case HasSkillRequirement::OnBar:
                return true;
            case HasSkillRequirement::OffCooldown:
                return skill.GetRecharge() == 0;
            case HasSkillRequirement::ReadyToUse:
                if (player->skill) return false;
                const auto& skilldata = *GW::SkillbarMgr::GetSkillConstantData(skill.skill_id);
                if (skill.GetRecharge() > 0) return false;
                if (skill.adrenaline_a < skilldata.adrenaline) return false;
                if (getEnergyCost(skilldata) > player->energy * player->max_energy) return false;
                return weaponFulfillsRequirement((EquippedWeaponType)player->weapon_type, (WeaponRequirement)skilldata.weapon_req, skilldata.type);
        }
        return false;
    });
}
void PlayerHasSkillCondition::drawSettings()
{
    ImGui::PushID(drawId());
    
    ImGui::Text("If the player has skill");
    ImGui::SameLine();
    drawSkillIDSelector(id);
    ImGui::SameLine();
    drawEnumButton(HasSkillRequirement::OnBar, HasSkillRequirement::ReadyToUse, requirement);
    ImGui::SameLine();
    ImGui::ShowHelp("'Ready to use' checks energy requirement, cooldown, adrenaline and weapon type.\r\nEnergy requirement only takes into account base cost, QZ, expertise and mysticism.");

    ImGui::PopID();
}

/// ------------- PlayerHasClassCondition -------------
PlayerHasClassCondition::PlayerHasClassCondition(InputStream& stream)
{
    stream >> primary >> secondary;
}
void PlayerHasClassCondition::serialize(OutputStream& stream) const
{
    Condition::serialize(stream);

    stream << primary << secondary;
}
bool PlayerHasClassCondition::check() const
{
    const auto player = GW::Agents::GetPlayerAsAgentLiving();
    if (!player) return false;
    return (primary == Class::Any || primary == (Class)player->primary) && (secondary == Class::Any || secondary == (Class)player->secondary);
}
void PlayerHasClassCondition::drawSettings()
{
    ImGui::PushID(drawId());
    
    ImGui::PushID(0);
    ImGui::Text("If the player has primary");
    ImGui::SameLine();
    drawEnumButton(Class::Any, Class::Dervish, primary);
    ImGui::PopID();

    ImGui::SameLine();

    ImGui::PushID(1);
    ImGui::Text("and secondary");
    ImGui::SameLine();
    drawEnumButton(Class::Any, Class::Dervish, secondary);
    ImGui::PopID();

    ImGui::PopID();
}

/// ------------- PlayerHasNameCondition -------------
PlayerHasNameCondition::PlayerHasNameCondition(InputStream& stream)
{
    name = readStringWithSpaces(stream);
}
void PlayerHasNameCondition::serialize(OutputStream& stream) const
{
    Condition::serialize(stream);

    writeStringWithSpaces(stream, name);
}
bool PlayerHasNameCondition::check() const
{
    if (name.empty()) return false;
    const auto player = GW::Agents::GetPlayerAsAgentLiving();
    if (!player) return false;

    auto& instanceInfo = InstanceInfo::getInstance();
    return instanceInfo.getDecodedAgentName(player->agent_id) == name;
}
void PlayerHasNameCondition::drawSettings()
{
    ImGui::PushID(drawId());
    ImGui::Text("If player character has name");
    ImGui::SameLine();
    ImGui::PushItemWidth(300);
    ImGui::InputText("name", &name);
    ImGui::PopID();
}

/// ------------- PlayerHasEnergyCondition -------------
PlayerHasEnergyCondition::PlayerHasEnergyCondition(InputStream& stream)
{
    stream >> minEnergy;
}
void PlayerHasEnergyCondition::serialize(OutputStream& stream) const
{
    Condition::serialize(stream);

    stream << minEnergy;
}
bool PlayerHasEnergyCondition::check() const
{
    const auto player = GW::Agents::GetPlayerAsAgentLiving();
    if (!player) return false;

    return player->energy * player->max_energy >= minEnergy;
}
void PlayerHasEnergyCondition::drawSettings()
{
    ImGui::PushID(drawId());
    ImGui::Text("If player has at least");
    ImGui::SameLine();
    ImGui::PushItemWidth(90);
    ImGui::InputInt("energy", &minEnergy, 0);
    ImGui::PopID();
}

/// ------------- CurrentTargetIsCastingSkillCondition -------------
CurrentTargetIsCastingSkillCondition::CurrentTargetIsCastingSkillCondition(InputStream& stream)
{
    stream >> id;
}
void CurrentTargetIsCastingSkillCondition::serialize(OutputStream& stream) const
{
    Condition::serialize(stream);

    stream << id;
}
bool CurrentTargetIsCastingSkillCondition::check() const
{
    const auto target = GW::Agents::GetTargetAsAgentLiving();
    return target && static_cast<GW::Constants::SkillID>(target->skill) == id;
}
void CurrentTargetIsCastingSkillCondition::drawSettings()
{
    ImGui::PushID(drawId());

    ImGui::Text("If the target is casting the skill");
    ImGui::SameLine();
    drawSkillIDSelector(id);
    ImGui::SameLine();
    ImGui::Text("(0 for none)");

    ImGui::PopID();
}

/// ------------- CurrentTargetDistanceCondition -------------
CurrentTargetDistanceCondition::CurrentTargetDistanceCondition(InputStream& stream)
{
    stream >> minDistance >> maxDistance;
}
void CurrentTargetDistanceCondition::serialize(OutputStream& stream) const
{
    Condition::serialize(stream);

    stream << minDistance << maxDistance;
}
bool CurrentTargetDistanceCondition::check() const
{
    const auto player = GW::Agents::GetPlayerAsAgentLiving();
    const auto target = GW::Agents::GetTargetAsAgentLiving();
    if (!player || !target) return false;

    const auto distance = GW::GetDistance(player->pos, target->pos);
    return minDistance <= distance + eps && distance - eps <= maxDistance;
}
void CurrentTargetDistanceCondition::drawSettings()
{
    ImGui::PushID(drawId());
    
    ImGui::Text("If the target has distance to player");
    ImGui::SameLine();
    ImGui::PushItemWidth(80.f);
    ImGui::InputFloat("min", &minDistance, 0);
    ImGui::SameLine();
    ImGui::InputFloat("max", &maxDistance, 0);
    
    ImGui::PopID();
}

/// ------------- CurrentTargetHasHpBelowCondition -------------
CurrentTargetHasHpBelowCondition::CurrentTargetHasHpBelowCondition(InputStream& stream)
{
    stream >> hp;
}
void CurrentTargetHasHpBelowCondition::serialize(OutputStream& stream) const
{
    Condition::serialize(stream);

    stream << hp;
}
bool CurrentTargetHasHpBelowCondition::check() const
{
    const auto target = GW::Agents::GetTargetAsAgentLiving();
    return target && target->hp * 100.f < hp;
}
void CurrentTargetHasHpBelowCondition::drawSettings()
{
    ImGui::PushID(drawId());
    ImGui::Text("If the target has HP below");
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    ImGui::InputFloat("%", &hp, 0);
    ImGui::PopID();
}

/// ------------- CurrentTargetAllegianceCondition -------------
CurrentTargetAllegianceCondition::CurrentTargetAllegianceCondition(InputStream& stream)
{
    stream >> agentType;
}
void CurrentTargetAllegianceCondition::serialize(OutputStream& stream) const
{
    Condition::serialize(stream);

    stream << agentType;
}
bool CurrentTargetAllegianceCondition::check() const
{
    const auto target = GW::Agents::GetTargetAsAgentLiving();
    if (!target) return false;
    switch (agentType) {
        case AgentType::Any:
            return true;
        case AgentType::PartyMember:
            return target->IsPlayer();
        case AgentType::Friendly:
            return target->allegiance != GW::Constants::Allegiance::Enemy;
        case AgentType::Hostile:
            return target->allegiance == GW::Constants::Allegiance::Enemy;
        default:
            return false;
    }
}
void CurrentTargetAllegianceCondition::drawSettings()
{
    ImGui::PushID(drawId());
    ImGui::Text("If the target has type");
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    drawEnumButton(AgentType::Any, AgentType::Hostile, agentType);
    ImGui::PopID();
}

/// ------------- CurrentTargetModelCondition -------------
CurrentTargetModelCondition::CurrentTargetModelCondition(InputStream& stream)
{
    stream >> modelId;
}
void CurrentTargetModelCondition::serialize(OutputStream& stream) const
{
    Condition::serialize(stream);

    stream << modelId;
}
bool CurrentTargetModelCondition::check() const
{
    const auto target = GW::Agents::GetTargetAsAgentLiving();
    return target && target->player_number == modelId;
}
void CurrentTargetModelCondition::drawSettings()
{
    ImGui::PushID(drawId());
    ImGui::Text("If the target has model");
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    drawModelIDSelector(modelId);
    ImGui::PopID();
}

/// ------------- HasPartyWindowAllyOfNameCondition -------------
HasPartyWindowAllyOfNameCondition::HasPartyWindowAllyOfNameCondition(InputStream& stream)
{
    name = readStringWithSpaces(stream);
}
void HasPartyWindowAllyOfNameCondition::serialize(OutputStream& stream) const
{
    Condition::serialize(stream);

    writeStringWithSpaces(stream, name);
}
bool HasPartyWindowAllyOfNameCondition::check() const
{
    if (name.empty()) return false;
    const auto info = GW::PartyMgr::GetPartyInfo();
    const auto agentArray = GW::Agents::GetAgentArray();
    if (!info || !agentArray) return false;

    auto& instanceInfo = InstanceInfo::getInstance();

    for (const auto& player : info->players) {
        auto candidate = GW::Agents::GetAgentIdByLoginNumber(player.login_number);
        if (instanceInfo.getDecodedAgentName(candidate) == name) { return true; }
    }

    return std::ranges::any_of(info->others, [&](const auto& allyId) 
    {
        return instanceInfo.getDecodedAgentName(allyId) == name;
    });
}
void HasPartyWindowAllyOfNameCondition::drawSettings()
{
    ImGui::PushID(drawId());
    ImGui::Text("If party window ally of name");
    ImGui::SameLine();
    ImGui::PushItemWidth(300);
    ImGui::InputText("Ally name", &name);
    ImGui::SameLine();
    ImGui::Text("exists");
    ImGui::PopID();
}

/// ------------- PartyMemberStatusCondition -------------
PartyMemberStatusCondition::PartyMemberStatusCondition(InputStream& stream)
{
    stream >> alive;
    name = readStringWithSpaces(stream);
}
void PartyMemberStatusCondition::serialize(OutputStream& stream) const
{
    Condition::serialize(stream);

    stream << alive;
    writeStringWithSpaces(stream, name);
}
bool PartyMemberStatusCondition::check() const
{
    if (name.empty()) return false;
    const auto info = GW::PartyMgr::GetPartyInfo();
    if (!info) return false;

    auto& instanceInfo = InstanceInfo::getInstance();

    for (const auto& player : info->players)
    {
        const auto agentId = GW::Agents::GetAgentIdByLoginNumber(player.login_number);
        const auto decodedName = instanceInfo.getDecodedAgentName(agentId);
        if (decodedName != name) continue;

        if (alive == AnyNoYes::Any) return true; // Player is in the instance

        const auto mapAgent = GW::Agents::GetMapAgentByID(agentId);
        if (!mapAgent) continue;
        return (mapAgent->GetIsDead()) == (alive == AnyNoYes::No);
    }

    return false;
}
void PartyMemberStatusCondition::drawSettings()
{
    ImGui::PushID(drawId());
    ImGui::Text("If party window ally is alive");
    ImGui::SameLine();
    drawEnumButton(AnyNoYes::Any, AnyNoYes::Yes, alive);
    ImGui::SameLine();
    ImGui::PushItemWidth(300);
    ImGui::InputText("Ally name", &name);
    ImGui::PopID();
}

/// ------------- QuestHasStateCondition -------------

QuestHasStateCondition::QuestHasStateCondition(InputStream& stream)
{
    stream >> id >> status;
}
void QuestHasStateCondition::serialize(OutputStream& stream) const
{
    Condition::serialize(stream);

    stream << id << status;
}
bool QuestHasStateCondition::check() const
{
    return InstanceInfo::getInstance().getObjectiveStatus(id) == status;
}
void QuestHasStateCondition::drawSettings()
{
    ImGui::PushID(drawId());
    ImGui::Text("If the quest objective has status");
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    ImGui::InputInt("id", reinterpret_cast<int*>(&id), 0);
    ImGui::SameLine();
    drawEnumButton(QuestStatus::NotStarted, QuestStatus::Failed, status);
    ImGui::SameLine();
    ImGui::ShowHelp("Objective ID, NOT quest ID!\nUW: Chamber 146, Restore 147, Escort 148, UWG 149, Vale 150, Waste 151, Pits 152, Planes 153, Mnts 154, Pools 155, Dhuum 157");
    ImGui::PopID();
}

/// ------------- KeyIsPressedCondition -------------
KeyIsPressedCondition::KeyIsPressedCondition(InputStream& stream)
{
    stream >> shortcutKey >> shortcutMod >> blockKey;
    description = makeHotkeyDescription(shortcutKey, shortcutMod);
    if (shortcutKey && blockKey) 
    {
        InstanceInfo::getInstance().requestDisableKey({shortcutKey, shortcutMod});
    }
}
void KeyIsPressedCondition::serialize(OutputStream& stream) const
{
    Condition::serialize(stream);

    stream << shortcutKey << shortcutMod << blockKey;
}
KeyIsPressedCondition::~KeyIsPressedCondition() 
{
    if (blockKey) InstanceInfo::getInstance().requestEnableKey({shortcutKey, shortcutMod});
}
bool KeyIsPressedCondition::check() const
{
    if (GW::Chat::GetIsTyping()) return false;
    bool keyIsPressed = GetAsyncKeyState(shortcutKey) & (1 << 15);
    if (shortcutMod & ModKey_Control) keyIsPressed &= ImGui::IsKeyDown(ImGuiKey_ModCtrl);
    if (shortcutMod & ModKey_Shift) keyIsPressed &= ImGui::IsKeyDown(ImGuiKey_ModShift);
    if (shortcutMod & ModKey_Alt) keyIsPressed &= ImGui::IsKeyDown(ImGuiKey_ModAlt);

    return keyIsPressed;
}
void KeyIsPressedCondition::drawSettings()
{
    ImGui::PushID(drawId());
    ImGui::Text("If key is held down:");
    ImGui::SameLine();
    const auto oldKey = std::pair{shortcutKey, shortcutMod};
    drawHotkeySelector(shortcutKey, shortcutMod, description, 100.f);
    if (const auto newKey = std::pair{shortcutKey, shortcutMod}; blockKey && newKey != oldKey)
    {
        InstanceInfo::getInstance().requestEnableKey(oldKey);
        InstanceInfo::getInstance().requestDisableKey(newKey);
    }
    ImGui::SameLine();
    
    bool wasBlocking = blockKey;
    ImGui::Checkbox("Block key", &blockKey);
    if (wasBlocking && !blockKey) 
    {
        InstanceInfo::getInstance().requestEnableKey({shortcutKey, shortcutMod});
    }
    else if (!wasBlocking && blockKey)
    {
        InstanceInfo::getInstance().requestDisableKey({shortcutKey, shortcutMod});
    }

    ImGui::PopID();
}

/// ------------- InstanceTimeCondition -------------

InstanceTimeCondition::InstanceTimeCondition(InputStream& stream)
{
    stream >> timeInSeconds;
}
void InstanceTimeCondition::serialize(OutputStream& stream) const
{
    Condition::serialize(stream);

    stream << timeInSeconds;
}
bool InstanceTimeCondition::check() const
{
    return (int)(GW::Map::GetInstanceTime() / 1000) >= timeInSeconds;
}
void InstanceTimeCondition::drawSettings()
{
    ImGui::PushID(drawId());
    ImGui::Text("If the instance is older than");
    ImGui::SameLine();
    ImGui::PushItemWidth(90);
    ImGui::InputInt("seconds", &timeInSeconds, 0);
    ImGui::PopID();
}

/// ------------- NearbyAgentCondition -------------

NearbyAgentCondition::NearbyAgentCondition(InputStream& stream)
{
    stream >> agentType >> primary >> secondary >> alive >> hexed >> skill >> modelId >> minDistance >> maxDistance >> minHp >> maxHp;
    agentName = readStringWithSpaces(stream);
    polygon = readPositions(stream);
    stream >> minAngle >> maxAngle >> enchanted >> weaponspelled >> poisoned >> bleeding;
}
void NearbyAgentCondition::serialize(OutputStream& stream) const
{
    Condition::serialize(stream);

    stream << agentType << primary << secondary << alive << hexed << skill << modelId << minDistance << maxDistance << minHp << maxHp;
    writeStringWithSpaces(stream, agentName);
    writePositions(stream, polygon);
    stream << minAngle << maxAngle << enchanted << weaponspelled << poisoned << bleeding;
}
bool NearbyAgentCondition::check() const
{
    const auto player = GW::Agents::GetPlayerAsAgentLiving();
    const auto agents = GW::Agents::GetAgentArray();
    if (!player || !agents) return false;

    auto& instanceInfo = InstanceInfo::getInstance();

    const auto fulfillsConditions = [&](const GW::Agent* agent) {
        if (!agent || !agent->GetIsLivingType() || agent->agent_id == player->agent_id) return false;
        const auto living = agent->GetAsAgentLiving();

        const auto correctType = [&]() -> bool {
            switch (agentType) {
                case AgentType::Any:
                    return true;
                case AgentType::PartyMember:
                    return true;
                case AgentType::Friendly:
                    return living->allegiance != GW::Constants::Allegiance::Enemy;
                case AgentType::Hostile:
                    return living->allegiance == GW::Constants::Allegiance::Enemy;
                default:
                    return false;
            }
        }();

        const auto correctPrimary = (primary == Class::Any) || primary == (Class)living->primary;
        const auto correctSecondary = (secondary == Class::Any) || secondary == (Class)living->secondary;
        const auto correctStatus = (alive == AnyNoYes::Any) || ((alive == AnyNoYes::Yes) == living->GetIsAlive());
        const auto correctHex = (hexed == AnyNoYes::Any) || ((hexed == AnyNoYes::Yes) == living->GetIsHexed());
        const auto correctEnch = (enchanted == AnyNoYes::Any) || ((enchanted == AnyNoYes::Yes) == living->GetIsEnchanted());
        const auto correctWeaponSpell = (weaponspelled == AnyNoYes::Any) || ((weaponspelled == AnyNoYes::Yes) == living->GetIsWeaponSpelled());
        const auto correctBleed = (bleeding == AnyNoYes::Any) || ((bleeding == AnyNoYes::Yes) == living->GetIsBleeding());
        const auto correctPoison = (poisoned == AnyNoYes::Any) || ((poisoned == AnyNoYes::Yes) == living->GetIsPoisoned());

        const auto correctSkill = (skill == GW::Constants::SkillID::No_Skill) || (skill == (GW::Constants::SkillID)living->skill);
        const auto correctModelId = (modelId == 0) || (living->player_number == modelId);
        const auto distance = GW::GetDistance(player->pos, living->pos);
        const auto goodDistance = (minDistance <= distance) && (distance <= maxDistance);
        const auto goodName = (agentName.empty()) || (instanceInfo.getDecodedAgentName(living->agent_id) == agentName);
        const auto goodPosition = (polygon.size() < 3u) || pointIsInsidePolygon(living->pos, polygon);
        const auto goodHp = minHp <= 100.f * living->hp && 100.f * living->hp <= maxHp;
        const auto angle = angleToAgent(player, living);
        const auto goodAngle = minAngle - eps < angle && angle < maxAngle + eps;

        return correctType && correctPrimary && correctSecondary && correctStatus && correctHex && correctEnch && correctWeaponSpell && correctBleed && correctPoison 
                && correctSkill && correctModelId && goodDistance && goodName && goodPosition && goodHp && goodAngle;
    };
    if (agentType == AgentType::PartyMember) 
    {
        const auto info = GW::PartyMgr::GetPartyInfo();
        if (!info) return false;

        for (const auto& partyMember : info->players) 
            if (fulfillsConditions(GW::Agents::GetAgentByID(GW::Agents::GetAgentIdByLoginNumber(partyMember.login_number)))) return true;
        for (const auto& hero : info->heroes)
            if (fulfillsConditions(GW::Agents::GetAgentByID(hero.agent_id))) return true;
        for (const auto& henchman : info->henchmen)
            if (fulfillsConditions(GW::Agents::GetAgentByID(henchman.agent_id))) return true;
    }
    else 
    {
        for (const auto* agent : *agents) 
        {
            if (fulfillsConditions(agent)) 
                return true;
        }
    }
    return false;
}
void NearbyAgentCondition::drawSettings()
{
    ImGui::PushID(drawId());
    ImGui::PushItemWidth(120);

    if (ImGui::TreeNodeEx("If there exists an agent with characteristics", ImGuiTreeNodeFlags_FramePadding)) {
        ImGui::BulletText("Allegiance");
        ImGui::SameLine();
        drawEnumButton(AgentType::Any, AgentType::Hostile, agentType, 0, 100.f, std::nullopt, std::optional{AgentType::Self});

        ImGui::BulletText("Distance to player");
        ImGui::SameLine();
        ImGui::InputFloat("min###0", &minDistance);
        ImGui::SameLine();
        ImGui::InputFloat("max###1", &maxDistance);

        ImGui::BulletText("Class");
        ImGui::SameLine();
        drawEnumButton(Class::Any, Class::Dervish, primary, 1);
        ImGui::SameLine();
        ImGui::Text("/");
        ImGui::SameLine();
        drawEnumButton(Class::Any, Class::Dervish, secondary, 2);

        ImGui::BulletText("Is alive");
        ImGui::SameLine();
        drawEnumButton(AnyNoYes::Any, AnyNoYes::Yes, alive, 3);

        ImGui::BulletText("Is enchanted");
        ImGui::SameLine();
        drawEnumButton(AnyNoYes::Any, AnyNoYes::Yes, enchanted, 4);

        ImGui::BulletText("Is affected by weapon spell");
        ImGui::SameLine();
        drawEnumButton(AnyNoYes::Any, AnyNoYes::Yes, weaponspelled, 5);

        ImGui::BulletText("Is hexed");
        ImGui::SameLine();
        drawEnumButton(AnyNoYes::Any, AnyNoYes::Yes, hexed, 6);

        ImGui::BulletText("Is poisoned");
        ImGui::SameLine();
        drawEnumButton(AnyNoYes::Any, AnyNoYes::Yes, poisoned, 7);

        ImGui::BulletText("Is bleeding");
        ImGui::SameLine();
        drawEnumButton(AnyNoYes::Any, AnyNoYes::Yes, bleeding, 8);

        ImGui::BulletText("Uses skill");
        ImGui::SameLine();
        ImGui::PushID(2);
        drawSkillIDSelector(skill);
        ImGui::PopID();
        ImGui::SameLine();
        ImGui::Text("(0 for any)");

        ImGui::PushItemWidth(200.f);
        ImGui::BulletText("Has name");
        ImGui::SameLine();
        ImGui::InputText("name (empty for any)", &agentName);

        ImGui::PushItemWidth(80.f);
        ImGui::Bullet();
        ImGui::Text("HP percent");
        ImGui::SameLine();
        ImGui::InputFloat("min###9", &minHp);
        ImGui::SameLine();
        ImGui::InputFloat("max###10", &maxHp);

        ImGui::Bullet();
        ImGui::Text("Has model");
        ImGui::SameLine();
        drawModelIDSelector(modelId, "id (0 for any)###11");

        ImGui::Bullet();
        ImGui::Text("Angle to player forward (degrees)");
        ImGui::SameLine();
        ImGui::InputFloat("min###12", &minAngle);
        ImGui::SameLine();
        ImGui::InputFloat("max###13", &maxAngle);
        if (minAngle < 0.f) minAngle = 0.f;
        if (minAngle > 180.f) minAngle = 180.f;
        if (maxAngle < 0.f) maxAngle = 0.f;
        if (maxAngle > 180.f) maxAngle = 180.f;

        ImGui::Bullet();
        ImGui::Text("Is within polygon");
        ImGui::SameLine();
        drawPolygonSelector(polygon);

        ImGui::TreePop();
    }
    ImGui::PopID();
}

/// ------------- CanPopAgentCondition -------------

bool CanPopAgentCondition::check() const
{
    return InstanceInfo::getInstance().canPopAgent();
}
void CanPopAgentCondition::drawSettings()
{
    ImGui::Text("If player can pop minipet or ghost in the box");
}

/// ------------- PlayerIsIdleCondition -------------

bool PlayerIsIdleCondition::check() const
{
    const auto player = GW::Agents::GetPlayerAsAgentLiving();
    if (!player) return false;
    return player->GetIsIdle();
}
void PlayerIsIdleCondition::drawSettings()
{
    ImGui::Text("If player is idle");
}

/// ------------- PlayerHasItemEquippedCondition -------------
PlayerHasItemEquippedCondition::PlayerHasItemEquippedCondition(InputStream& stream)
{
    stream >> modelId;
}
void PlayerHasItemEquippedCondition::serialize(OutputStream& stream) const
{
    Condition::serialize(stream);

    stream << modelId;
}
bool PlayerHasItemEquippedCondition::check() const
{
    return FindMatchingItem(GW::Constants::Bag::Equipped_Items, modelId) != nullptr;
}
void PlayerHasItemEquippedCondition::drawSettings()
{
    const auto item = FindMatchingItem(modelId);
    const auto itemName = item ? InstanceInfo::getInstance().getDecodedItemName(item->item_id) : "";

    ImGui::PushID(drawId());

    ImGui::Text("If the player has item equipped");
    ImGui::SameLine();
    ImGui::Text("%s", itemName.c_str());
    ImGui::SameLine();
    ImGui::PushItemWidth(90);
    ImGui::InputInt("model id", &modelId, 0);

    ImGui::PopID();
}

/// ------------- PlayerHasHpBelowCondition -------------
PlayerHasHpBelowCondition::PlayerHasHpBelowCondition(InputStream& stream)
{
    stream >> hp;
}
void PlayerHasHpBelowCondition::serialize(OutputStream& stream) const
{
    Condition::serialize(stream);

    stream << hp;
}
bool PlayerHasHpBelowCondition::check() const
{
    const auto player = GW::Agents::GetPlayerAsAgentLiving();
    return player && player->hp * 100.f < hp;
}
void PlayerHasHpBelowCondition::drawSettings()
{
    ImGui::PushID(drawId());
    ImGui::Text("If the player has HP below");
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    ImGui::InputFloat("%", &hp, 0);
    ImGui::PopID();
}

/// ------------- ItemInInventoryCondition -------------
ItemInInventoryCondition::ItemInInventoryCondition(InputStream& stream)
{
    stream >> modelId;
}
void ItemInInventoryCondition::serialize(OutputStream& stream) const
{
    Condition::serialize(stream);

    stream << modelId;
}
bool ItemInInventoryCondition::check() const
{
    const auto item = FindMatchingItem(modelId);
    return item && item->bag->bag_type == GW::Constants::BagType::Inventory;
}
void ItemInInventoryCondition::drawSettings()
{
    const auto item = FindMatchingItem(modelId);
    const auto itemName = item ? InstanceInfo::getInstance().getDecodedItemName(item->item_id) : "";

    ImGui::PushID(drawId());

    ImGui::Text("If the player has item in inventory");
    ImGui::SameLine();
    ImGui::Text("%s", itemName.c_str());
    ImGui::SameLine();
    ImGui::PushItemWidth(90);
    ImGui::InputInt("model id", &modelId, 0);

    ImGui::PopID();
}

/// ------------- PlayerStatusCondition -------------
PlayerStatusCondition::PlayerStatusCondition(InputStream& stream)
{
    stream >> status;
}
void PlayerStatusCondition::serialize(OutputStream& stream) const
{
    Condition::serialize(stream);

    stream << status;
}
bool PlayerStatusCondition::check() const
{
    const auto player = GW::Agents::GetPlayerAsAgentLiving();
    if (!player) return false;

    switch (status) 
    {
        case Status::Enchanted:
            return player->GetIsEnchanted();
        case Status::WeaponSpelled:
            return player->GetIsWeaponSpelled();
        case Status::Alive:
            return player->GetIsAlive();
        case Status::Bleeding:
            return player->GetIsBleeding();
        case Status::Crippled:
            return player->GetIsCrippled();
        case Status::DeepWounded:
            return player->GetIsDeepWounded();
        case Status::Poisoned:
            return player->GetIsPoisoned();
        case Status::Hexed:
            return player->GetIsHexed() || player->GetIsDegenHexed();
        default:
            return false;
    }
}
void PlayerStatusCondition::drawSettings()
{
    ImGui::PushID(drawId());

    ImGui::Text("If player is");
    ImGui::SameLine();
    drawEnumButton(Status::Enchanted, Status::Hexed, status);

    ImGui::PopID();
}

/// ------------- CurrentTargetStatusCondition -------------
CurrentTargetStatusCondition::CurrentTargetStatusCondition(InputStream& stream)
{
    stream >> status;
}
void CurrentTargetStatusCondition::serialize(OutputStream& stream) const
{
    Condition::serialize(stream);

    stream << status;
}
bool CurrentTargetStatusCondition::check() const
{
    const auto target = GW::Agents::GetTargetAsAgentLiving();
    if (!target) return false;

    switch (status) {
        case Status::Enchanted:
            return target->GetIsEnchanted();
        case Status::WeaponSpelled:
            return target->GetIsWeaponSpelled();
        case Status::Alive:
            return target->GetIsAlive();
        case Status::Bleeding:
            return target->GetIsBleeding();
        case Status::Crippled:
            return target->GetIsCrippled();
        case Status::DeepWounded:
            return target->GetIsDeepWounded();
        case Status::Poisoned:
            return target->GetIsPoisoned();
        case Status::Hexed:
            return target->GetIsHexed() || target->GetIsDegenHexed();
        default:
            return false;
    }
}
void CurrentTargetStatusCondition::drawSettings()
{
    ImGui::PushID(drawId());

    ImGui::Text("If current target is");
    ImGui::SameLine();
    drawEnumButton(Status::Enchanted, Status::Hexed, status);

    ImGui::PopID();
}

/// ------------- PlayerInPolygonCondition -------------
PlayerInPolygonCondition::PlayerInPolygonCondition(InputStream& stream)
{
    polygon = readPositions(stream);
}
void PlayerInPolygonCondition::serialize(OutputStream& stream) const
{
    Condition::serialize(stream);

    writePositions(stream, polygon);
}
bool PlayerInPolygonCondition::check() const
{
    const auto player = GW::Agents::GetPlayerAsAgentLiving();
    if (!player || polygon.size() < 3) return false;
    return pointIsInsidePolygon(player->pos, polygon);
}
void PlayerInPolygonCondition::drawSettings()
{
    ImGui::PushID(drawId());

    ImGui::Text("If player is inside polygon");
    ImGui::SameLine();
    drawPolygonSelector(polygon);

    ImGui::PopID();
}

/// ------------- InstanceTypeCondition -------------
InstanceTypeCondition::InstanceTypeCondition(InputStream& stream)
{
    stream >> instanceType;
}
void InstanceTypeCondition::serialize(OutputStream& stream) const
{
    Condition::serialize(stream);

    stream << instanceType;
}
bool InstanceTypeCondition::check() const
{
    return GW::Map::GetInstanceType() == instanceType;
}
void InstanceTypeCondition::drawSettings()
{
    ImGui::PushID(drawId());

    ImGui::Text("If player is in");
    ImGui::SameLine();
    drawEnumButton(GW::Constants::InstanceType::Outpost, GW::Constants::InstanceType::Explorable, instanceType);

    ImGui::PopID();
}

/// ------------- RemainingCooldownCondition -------------
RemainingCooldownCondition::RemainingCooldownCondition(InputStream& stream)
{
    stream >> id >> hasMinimumCooldown >> hasMaximumCooldown >> minimumCooldown >> maximumCooldown;
}
void RemainingCooldownCondition::serialize(OutputStream& stream) const
{
    Condition::serialize(stream);

    stream << id << hasMinimumCooldown << hasMaximumCooldown << minimumCooldown << maximumCooldown;
}
bool RemainingCooldownCondition::check() const
{
    const auto bar = GW::SkillbarMgr::GetPlayerSkillbar();
    if (!bar || !bar->IsValid()) return false;

    for (int i = 0; i < 8; ++i) 
    {
        if (bar->skills[i].skill_id == id) 
        {
            const auto cooldown = (int)bar->skills[i].GetRecharge();
            return (!hasMinimumCooldown || cooldown >= minimumCooldown) && (!hasMaximumCooldown || cooldown <= maximumCooldown);
        }
            
    }
    return false;
}
void RemainingCooldownCondition::drawSettings()
{
    ImGui::PushID(drawId());

    ImGui::Text("If skill");
    ImGui::SameLine();
    drawSkillIDSelector(id);
    ImGui::SameLine();
    ImGui::Text("has remaining cooldown (in ms)");
    ImGui::SameLine();
    
    if (hasMinimumCooldown) 
    {
        ImGui::InputInt("min", &minimumCooldown, 0);
        ImGui::SameLine();
        if (ImGui::Button("X###0")) {
            hasMinimumCooldown = false;
            minimumCooldown = 0;
        }
    }
    else 
    {
        if (ImGui::Button("Add min")) hasMinimumCooldown = true;
    }

    ImGui::SameLine();
    if (hasMaximumCooldown) 
    {
        ImGui::InputInt("max", &maximumCooldown, 0);
        ImGui::SameLine();
        if (ImGui::Button("X###1")) {
            hasMaximumCooldown = false;
            maximumCooldown = 0;
        }
    }
    else {
        if (ImGui::Button("Add max")) hasMaximumCooldown = true;
    }


    ImGui::PopID();
}

/// ------------- FoeCountCondition -------------
FoeCountCondition::FoeCountCondition(InputStream& stream)
{
    stream >> maximum;
}
void FoeCountCondition::serialize(OutputStream& stream) const
{
    Condition::serialize(stream);

    stream << maximum;
}
bool FoeCountCondition::check() const
{
    return (int)GW::Map::GetFoesToKill() <= maximum;
}
void FoeCountCondition::drawSettings()
{
    ImGui::PushID(drawId());

    ImGui::Text("If there are at most");
    ImGui::SameLine();
    ImGui::PushItemWidth(80);
    ImGui::InputInt("enemies left in the instance", &maximum, 0);

    ImGui::PopID();
}

/// ------------- MoraleCondition -------------
MoraleCondition::MoraleCondition(InputStream& stream)
{
    stream >> minimumMorale;
}
void MoraleCondition::serialize(OutputStream& stream) const
{
    Condition::serialize(stream);

    stream << minimumMorale;
}
bool MoraleCondition::check() const
{
    const auto worldContext = GW::GetWorldContext();
    if (!worldContext) return false;
    return int(worldContext->morale) - 100 >= minimumMorale;
}
void MoraleCondition::drawSettings()
{
    ImGui::PushID(drawId());

    ImGui::Text("If morale is greater than or equal to");
    ImGui::SameLine();
    ImGui::PushItemWidth(80);
    ImGui::InputInt("% (negative values for DP)", &minimumMorale, 0);

    ImGui::PopID();
}

/// ------------- FalseCondition -------------
bool FalseCondition::check() const
{
    return false;
}
void FalseCondition::drawSettings()
{
    ImGui::PushID(drawId());

    ImGui::Text("False");

    ImGui::PopID();
}

/// ------------- TrueCondition -------------
bool TrueCondition::check() const
{
    return true;
}
void TrueCondition::drawSettings()
{
    ImGui::PushID(drawId());

    ImGui::Text("True");

    ImGui::PopID();
}

/// ------------- OnceCondition -------------
OnceCondition::OnceCondition(InputStream& stream)
{
    std::string read;
    stream >> read;
    if (read == missingContentToken)
        cond = nullptr;
    else if (read == "C")
        cond = readCondition(stream);
    else
        return;
}
void OnceCondition::serialize(OutputStream& stream) const
{
    Condition::serialize(stream);
    if (cond)
        cond->serialize(stream);
    else
        stream << missingContentToken;
}
bool OnceCondition::check() const
{
    const auto currentInstanceId = InstanceInfo::getInstance().getInstanceId();
    if (triggeredLastInInstanceId == currentInstanceId) return false;
    if (cond && !cond->check()) return false;

    triggeredLastInInstanceId = currentInstanceId;
    return true;
}
void OnceCondition::drawSettings()
{
    ImGui::PushID(drawId());
    ImGui::Text("ONCE (");
    ImGui::SameLine();
    if (cond) {
        cond->drawSettings();
    }
    else {
        cond = drawConditionSelector(100.f);
    }
    ImGui::SameLine();
    ImGui::Text(")");
    ImGui::SameLine();
    ImGui::ShowHelp("Is true exactly once the first time the condition is met");
    ImGui::PopID();
}

/// ------------- UntilCondition -------------
UntilCondition::UntilCondition(InputStream& stream)
{
    std::string read;
    stream >> read;
    if (read == missingContentToken)
        cond = nullptr;
    else if (read == "C")
        cond = readCondition(stream);
    else
        return;
}
void UntilCondition::serialize(OutputStream& stream) const
{
    Condition::serialize(stream);
    if (cond)
        cond->serialize(stream);
    else
        stream << missingContentToken;
}
bool UntilCondition::check() const
{
    const auto currentInstanceId = InstanceInfo::getInstance().getInstanceId();
    if (conditionLastTrueInInstanceId != currentInstanceId) 
    {
        currentState = true;
        conditionLastTrueInInstanceId = currentInstanceId;
    }
    if (currentState && cond && cond->check()) 
    {
        currentState = false;
    }

    return currentState;
}
void UntilCondition::drawSettings()
{
    ImGui::PushID(drawId());
    ImGui::Text("UNTIL (");
    ImGui::SameLine();
    if (cond) {
        cond->drawSettings();
    }
    else {
        cond = drawConditionSelector(100.f);
    }
    ImGui::SameLine();
    ImGui::Text(")");
    ImGui::SameLine();
    ImGui::ShowHelp("Is true until the condition is met the first time in each instance. False after that.");
    ImGui::PopID();
}

/// ------------- AfterCondition -------------
AfterCondition::AfterCondition(InputStream& stream)
{
    std::string read;
    stream >> read;
    if (read == missingContentToken)
        cond = nullptr;
    else if (read == "C")
        cond = readCondition(stream);
    else
        return;
}
void AfterCondition::serialize(OutputStream& stream) const
{
    Condition::serialize(stream);
    if (cond)
        cond->serialize(stream);
    else
        stream << missingContentToken;
}
bool AfterCondition::check() const
{
    const auto currentInstanceId = InstanceInfo::getInstance().getInstanceId();
    if (conditionLastTrueInInstanceId != currentInstanceId) {
        currentState = false;
        conditionLastTrueInInstanceId = currentInstanceId;
    }
    if (!currentState && cond && cond->check()) 
    {
        currentState = true;
    }

    return currentState;
}
void AfterCondition::drawSettings()
{
    ImGui::PushID(drawId());
    ImGui::Text("AFTER (");
    ImGui::SameLine();
    if (cond) {
        cond->drawSettings();
    }
    else {
        cond = drawConditionSelector(100.f);
    }
    ImGui::SameLine();
    ImGui::Text(")");
    ImGui::SameLine();
    ImGui::ShowHelp("Is false until the condition is met the first time in each instance. True after that.");
    ImGui::PopID();
}

/// ------------- ToggleCondition -------------
ToggleCondition::ToggleCondition(InputStream& stream)
{
    stream >> defaultState;

    std::string read;

    stream >> read;
    if (read == missingContentToken)
        toggleOnCond = nullptr;
    else if (read == "C") 
    {
        toggleOnCond = readCondition(stream);
        stream.proceedPastSeparator(2);
    }
    else
        return;

    stream >> read;
    if (read == missingContentToken)
        toggleOffCond = nullptr;
    else if (read == "C") 
    {
        toggleOffCond = readCondition(stream);
        stream.proceedPastSeparator(2);
    }
}
void ToggleCondition::serialize(OutputStream& stream) const
{
    Condition::serialize(stream);
    
    stream << defaultState;

    if (toggleOnCond) 
    {
        toggleOnCond->serialize(stream);
        stream.writeSeparator(2);
    }
    else
        stream << missingContentToken;

    if (toggleOffCond) 
    {
        toggleOffCond->serialize(stream);
        stream.writeSeparator(2);
    }
    else
        stream << missingContentToken;
}
bool ToggleCondition::check() const
{
    const auto currentInstanceId = InstanceInfo::getInstance().getInstanceId();
    if (lastResetInInstanceId != currentInstanceId) 
    {
        currentState = defaultState == TrueFalse::True;
        lastResetInInstanceId = currentInstanceId;
    }
    if (!currentState && toggleOnCond && toggleOnCond->check())
    {
        currentState = true;
    }
    if (currentState && toggleOffCond && toggleOffCond->check()) 
    {
        currentState = false;
    }

    return currentState;
}
void ToggleCondition::drawSettings()
{
    const auto drawToggle = [&](auto& cond, const auto text, const auto id)
    {
        ImGui::PushID(id);
        
        bool deleteCondition = false;
        
        ImGui::Bullet();
        ImGui::Text(text);
        
        ImGui::SameLine();
        if (cond) 
        {
            if (ImGui::Button("X")) deleteCondition = true;
            ImGui::SameLine();
            cond->drawSettings();
        }
        else
            cond = drawConditionSelector(100.f);

        if (deleteCondition) 
            cond = nullptr;

        ImGui::PopID();
    };
    ImGui::PushID(drawId());
    
    ImGui::Bullet();
    ImGui::Text("Initially:");
    ImGui::SameLine();
    drawEnumButton(TrueFalse::True, TrueFalse::False, defaultState);

    ImGui::Indent(84.f);
    drawToggle(toggleOnCond, "Toggle on", 0);
    drawToggle(toggleOffCond, "Toggle off", 1);
    ImGui::Unindent(84.f);

    ImGui::PopID();
}

/// ------------- CurrentTargetNameCondition -------------
CurrentTargetNameCondition::CurrentTargetNameCondition(InputStream& stream) 
{
    name = readStringWithSpaces(stream);
}
void CurrentTargetNameCondition::serialize(OutputStream& stream) const
{
    Condition::serialize(stream);

    writeStringWithSpaces(stream, name);
}
bool CurrentTargetNameCondition::check() const
{
    if (name.empty()) return false;
    const auto target = GW::Agents::GetTargetAsAgentLiving();
    if (!target) return false;

    auto& instanceInfo = InstanceInfo::getInstance();
    return instanceInfo.getDecodedAgentName(target->agent_id) == name;
}
void CurrentTargetNameCondition::drawSettings()
{
    ImGui::PushID(drawId());

    ImGui::Text("If current target has name");
    ImGui::SameLine();
    ImGui::PushItemWidth(300);
    ImGui::InputText("name", &name);

    ImGui::PopID();
}

/// ------------- ThrottleCondition -------------
ThrottleCondition::ThrottleCondition(InputStream& stream)
{
    stream >> delayInMs;
}
void ThrottleCondition::serialize(OutputStream& stream) const
{
    Condition::serialize(stream);

    stream << delayInMs;
}
bool ThrottleCondition::check() const
{
    const auto now = std::chrono::steady_clock::now();
    
    const auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTimeReturnedTrue).count();
    
    if (elapsedTime < delayInMs) return false;
    lastTimeReturnedTrue = now;
    return true;
}
void ThrottleCondition::drawSettings()
{
    ImGui::PushID(drawId());

    ImGui::Text("If script has not run in ");
    ImGui::SameLine();
    ImGui::PushItemWidth(80);
    ImGui::InputInt("ms", &delayInMs, 0);

    ImGui::PopID();
}
