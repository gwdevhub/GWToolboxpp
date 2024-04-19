#include <ConditionImpls.h>

#include <ConditionIO.h>
#include <utils.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Player.h>
#include <GWCA/GameEntities/Skill.h>
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

#include <Keys.h>

#include "windows.h"
#include "imgui.h"
#include "ImGuiCppWrapper.h"

#include <algorithm>
#include <optional>

namespace {
    constexpr double eps = 1e-3;
    const std::string missingContentToken = "/";

    std::string_view toString(QuestStatus status)
    {
        switch (status) {
            case QuestStatus::NotStarted:
                return "Not started";
            case QuestStatus::Started:
                return "Started";
            case QuestStatus::Completed:
                return "Completed";
            case QuestStatus::Failed:
                return "Failed";
        }
        return "";
    }

    template <typename T>
    void drawEnumButton(T firstValue, T lastValue, T& currentValue, int id = 0, float width = 100.)
    {
        const auto popupName = std::string{"p"} + "###" + std::to_string(id);
        const auto buttonText = std::string{toString(currentValue)} + "###" + std::to_string(id);
        if (ImGui::Button(buttonText.c_str(), ImVec2(width, 0))) {
            ImGui::OpenPopup(popupName.c_str());
        }
        if (ImGui::BeginPopup(popupName.c_str())) {
            for (auto i = (int)firstValue; i <= (int)lastValue; ++i) {
                if (ImGui::Selectable(toString((T)i).data())) {
                    currentValue = static_cast<T>(i);
                }
            }
            ImGui::EndPopup();
        }
    }

}

/// ------------- InvertedCondition -------------
NegatedCondition::NegatedCondition(std::istringstream& stream)
{
    std::string read;
    stream >> read;
    if (read == missingContentToken) {
        cond = nullptr;
    }
    else if (read == "C"){
        cond = readCondition(stream);
    }
    else
    {
        assert(false);
    }
}
void NegatedCondition::serialize(std::ostringstream& stream) const
{
    Condition::serialize(stream);
    if (cond)
        cond->serialize(stream);
    else
        stream << missingContentToken << " ";
}
bool NegatedCondition::check() const
{
    return cond && !cond->check();
}
void NegatedCondition::drawSettings()
{
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
}

/// ------------- DisjunctionCondition -------------
DisjunctionCondition::DisjunctionCondition(std::istringstream& stream) 
{
    std::string read;
    stream >> read;
    if (read == missingContentToken) {
        first = nullptr;
    }
    else if (read == "C") {
        first = readCondition(stream);
    }
    else {
        assert(false);
    }
    stream >> read;
    if (read == missingContentToken) {
        second = nullptr;
    }
    else if (read == "C") {
        second = readCondition(stream);
    }
    else {
        assert(false);
    }
}
void DisjunctionCondition::serialize(std::ostringstream& stream) const 
{
    Condition::serialize(stream);

    if (first)
        first->serialize(stream);
    else
        stream << missingContentToken << " ";
    
    if (second)
        second->serialize(stream);
    else
        stream << missingContentToken << " ";
}
bool DisjunctionCondition::check() const
{
    return first && second && (first->check() || second->check());
}
void DisjunctionCondition::drawSettings()
{
    ImGui::Text("(");
    ImGui::SameLine();
    if (first) {
        first->drawSettings();
    }
    else {
        ImGui::PushID(1923512);
        first = drawConditionSelector(100.f);
        ImGui::PopID();
    }
    ImGui::SameLine();
    ImGui::Text(") OR (");
    ImGui::SameLine();
    if (second) {
        second->drawSettings();
    }
    else {
        ImGui::PushID(656123);
        second = drawConditionSelector(100.f);
        ImGui::PopID();
    }
    ImGui::SameLine();
    ImGui::Text(")");
}

/// ------------- IsInMapCondition -------------
IsInMapCondition::IsInMapCondition(std::istringstream& stream)
{
    int read;
    stream >> read;
    id = (GW::Constants::MapID)read;
}
void IsInMapCondition::serialize(std::ostringstream& stream) const
{
    Condition::serialize(stream);

    stream << (int)id << " ";
}
bool IsInMapCondition::check() const {
    return GW::Map::GetMapID() == id;
}
void IsInMapCondition::drawSettings() {
    ImGui::Text("If player is in map");
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    ImGui::InputInt("id", reinterpret_cast<int*>(&id), 0);
}

/// ------------- PartyPlayerCountCondition -------------
PartyPlayerCountCondition::PartyPlayerCountCondition(std::istringstream& stream)
{
    stream >> count;
}
void PartyPlayerCountCondition::serialize(std::ostringstream& stream) const
{
    Condition::serialize(stream);

    stream << count << " ";
}
bool PartyPlayerCountCondition::check() const
{
    return GW::PartyMgr::GetPartyPlayerCount() == uint32_t(count);
}
void PartyPlayerCountCondition::drawSettings()
{
    ImGui::Text("If the number of players in the party is");
    ImGui::PushItemWidth(30);
    ImGui::SameLine();
    ImGui::InputInt("", &count, 0);
}

/// ------------- InstanceProgressCondition -------------
InstanceProgressCondition::InstanceProgressCondition(std::istringstream& stream)
{
    stream >> requiredProgress;
}
void InstanceProgressCondition::serialize(std::ostringstream& stream) const
{
    Condition::serialize(stream);

    stream << requiredProgress << " ";
}
bool InstanceProgressCondition::check() const {
    return GW::GetGameContext()->character->progress_bar->progress > requiredProgress;
}
void InstanceProgressCondition::drawSettings()
{
    ImGui::Text("If the instance progress (0..1) is greater than");
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    ImGui::InputFloat("", &requiredProgress, 0);
}

/// ------------- OnlyTriggerOnceCondition -------------
OnlyTriggerOnceCondition::OnlyTriggerOnceCondition(std::istringstream&)
{
}
bool OnlyTriggerOnceCondition::check() const
{
    const auto currentInstanceId = InstanceInfo::getInstance().getInstanceId();
    if (triggeredLastInInstanceId == currentInstanceId) return false;
    
    triggeredLastInInstanceId = currentInstanceId;
    return true;
}
void OnlyTriggerOnceCondition::drawSettings()
{
    ImGui::Text("If script has not been triggered in this instance");
}

/// ------------- PlayerIsNearPositionCondition -------------
PlayerIsNearPositionCondition::PlayerIsNearPositionCondition(std::istringstream& stream)
{
    stream >> pos.x >> pos.y >> accuracy;
}
void PlayerIsNearPositionCondition::serialize(std::ostringstream& stream) const
{
    Condition::serialize(stream);

    stream << pos.x << " " << pos.y << " " << accuracy << " ";
}
bool PlayerIsNearPositionCondition::check() const
{
    const auto player = GW::Agents::GetPlayerAsAgentLiving(); 
    if (!player) return false;
    printf("distance %f", GW::GetDistance(player->pos, pos));
    return GW::GetDistance(player->pos, pos) < accuracy + eps;
}
void PlayerIsNearPositionCondition::drawSettings()
{
    ImGui::Text("If the player is near position");
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    ImGui::InputFloat("x", &pos.x, 0.0f, 0.0f);
    ImGui::SameLine();
    ImGui::InputFloat("y", &pos.y, 0.0f, 0.0f);
    ImGui::SameLine();
    ImGui::InputFloat("Accuracy", &accuracy, 0.0f, 0.0f);
}

/// ------------- PlayerHasBuffCondition -------------
PlayerHasBuffCondition::PlayerHasBuffCondition(std::istringstream& stream)
{
    int read;
    stream >> read;
    id = (GW::Constants::SkillID)read;
}
void PlayerHasBuffCondition::serialize(std::ostringstream& stream) const
{
    Condition::serialize(stream);

    stream << (int)id << " ";
}
bool PlayerHasBuffCondition::check() const
{
    return GW::Effects::GetPlayerBuffBySkillId(id);
}
void PlayerHasBuffCondition::drawSettings()
{
    ImGui::Text("If the player is affected by the skill");
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    ImGui::InputInt("id", reinterpret_cast<int*>(&id), 0);
}

/// ------------- PlayerHasSkillCondition -------------
PlayerHasSkillCondition::PlayerHasSkillCondition(std::istringstream& stream)
{
    int read;
    stream >> read;
    id = (GW::Constants::SkillID)read;
}
void PlayerHasSkillCondition::serialize(std::ostringstream& stream) const
{
    Condition::serialize(stream);

    stream << (int)id << " ";
}
bool PlayerHasSkillCondition::check() const
{
    GW::Skillbar* bar = GW::SkillbarMgr::GetPlayerSkillbar();
    if (!bar || !bar->IsValid()) return false;
    for (int i = 0; i < 8; ++i) {
        if (bar->skills[i].skill_id == id) {
            return bar->skills[i].GetRecharge() == 0;
        }
    }
    return false;
}
void PlayerHasSkillCondition::drawSettings()
{
    ImGui::Text("If the player has the skill");
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    ImGui::InputInt("id", reinterpret_cast<int*>(&id), 0);
    ImGui::SameLine();
    ImGui::Text("off cooldown");
}

/// ------------- PlayerHasClassCondition -------------
PlayerHasClassCondition::PlayerHasClassCondition(std::istringstream& stream)
{
    int read;

    stream >> read;
    primary = (Class)read;

    stream >> read;
    secondary = (Class)read;
}
void PlayerHasClassCondition::serialize(std::ostringstream& stream) const
{
    Condition::serialize(stream);

    stream << (int)primary << " " << (int)secondary << " ";
}
bool PlayerHasClassCondition::check() const
{
    const auto player = GW::Agents::GetPlayerAsAgentLiving();
    if (!player) return false;
    return (primary == Class::Any || primary == (Class)player->primary) && (secondary == Class::Any || secondary == (Class)player->secondary);
}
void PlayerHasClassCondition::drawSettings()
{
    const auto drawClassSelector = [&](Class& c, bool primary) {
        ImGui::SameLine();
        const auto id = std::string{"###"} + (primary ? "1" : "2");
        if (ImGui::Button((toString(c) + id).data(), ImVec2(100, 0))) {
            ImGui::OpenPopup((std::string{"Pick class"} + id).c_str());
        }
        if (ImGui::BeginPopup((std::string{"Pick class"} + id).c_str())) {
            for (auto i = 0; i <= (int)Class::Dervish; ++i) {
                if (ImGui::Selectable(toString((Class)i).data())) {
                    c = (Class)i;
                }
            }
            ImGui::EndPopup();
        }
    };
    ImGui::Text("If the player has primary");
    drawClassSelector(primary, true);
    ImGui::SameLine();
    ImGui::Text("and secondary");
    drawClassSelector(secondary, false);
}

/// ------------- PlayerHasNameCondition -------------
PlayerHasNameCondition::PlayerHasNameCondition(std::istringstream& stream)
{
    name = readStringWithSpaces(stream);
}
void PlayerHasNameCondition::serialize(std::ostringstream& stream) const
{
    Condition::serialize(stream);

    writeStringWithSpaces(stream, name);
}
bool PlayerHasNameCondition::check() const
{
    const auto player = GW::Agents::GetPlayerAsAgentLiving();
    if (!player) return false;

    auto& instanceInfo = InstanceInfo::getInstance();
    return instanceInfo.getDecodedName(player->agent_id) == name;
}
void PlayerHasNameCondition::drawSettings()
{
    ImGui::Text("If player character has name");
    ImGui::SameLine();
    ImGui::PushItemWidth(300);
    ImGui::InputText("name", &name);
}

/// ------------- PlayerHasEnergyCondition -------------
PlayerHasEnergyCondition::PlayerHasEnergyCondition(std::istringstream& stream)
{
    stream >> minEnergy;
}
void PlayerHasEnergyCondition::serialize(std::ostringstream& stream) const
{
    Condition::serialize(stream);

    stream << minEnergy << " ";
}
bool PlayerHasEnergyCondition::check() const
{
    const auto player = GW::Agents::GetPlayerAsAgentLiving();
    if (!player) return false;

    return player->energy * player->max_energy >= minEnergy;
}
void PlayerHasEnergyCondition::drawSettings()
{
    ImGui::Text("If player has at least");
    ImGui::SameLine();
    ImGui::PushItemWidth(90);
    ImGui::InputInt("energy", &minEnergy, 0);
}

/// ------------- CurrentTargetIsCastingSkillCondition -------------
CurrentTargetIsCastingSkillCondition::CurrentTargetIsCastingSkillCondition(std::istringstream& stream)
{
    int read;
    stream >> read;
    id = (GW::Constants::SkillID)read;
}
void CurrentTargetIsCastingSkillCondition::serialize(std::ostringstream& stream) const
{
    Condition::serialize(stream);

    stream << (int)id << " ";
}
bool CurrentTargetIsCastingSkillCondition::check() const
{
    const auto target = GW::Agents::GetTargetAsAgentLiving();
    return target && static_cast<GW::Constants::SkillID>(target->skill) == id;
}
void CurrentTargetIsCastingSkillCondition::drawSettings()
{
    ImGui::Text("If the target is casting the skill");
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    ImGui::InputInt("id", reinterpret_cast<int*>(&id), 0);
}

/// ------------- CurrentTargetHasHpBelowCondition -------------
CurrentTargetHasHpBelowCondition::CurrentTargetHasHpBelowCondition(std::istringstream& stream)
{
    stream >> hp;
}
void CurrentTargetHasHpBelowCondition::serialize(std::ostringstream& stream) const
{
    Condition::serialize(stream);

    stream << hp << " ";
}
bool CurrentTargetHasHpBelowCondition::check() const
{
    const auto target = GW::Agents::GetTargetAsAgentLiving();
    return target && target->hp < hp;
}
void CurrentTargetHasHpBelowCondition::drawSettings()
{
    ImGui::Text("If the target has HP below");
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    ImGui::InputFloat("%", &hp, 0);
}

/// ------------- HasPartyWindowAllyOfNameCondition -------------
HasPartyWindowAllyOfNameCondition::HasPartyWindowAllyOfNameCondition(std::istringstream& stream)
{
    name = readStringWithSpaces(stream);
}
void HasPartyWindowAllyOfNameCondition::serialize(std::ostringstream& stream) const
{
    Condition::serialize(stream);

    writeStringWithSpaces(stream, name);
}
bool HasPartyWindowAllyOfNameCondition::check() const
{
    const auto info = GW::PartyMgr::GetPartyInfo();
    const auto agentArray = GW::Agents::GetAgentArray();
    if (!info || !agentArray) return false;

    auto& instanceInfo = InstanceInfo::getInstance();
    return std::ranges::any_of(info->others, [&](const auto& allyId) {
        return instanceInfo.getDecodedName(allyId) == name;
    });
}
void HasPartyWindowAllyOfNameCondition::drawSettings()
{
    ImGui::Text("Has party window ally of name");
    ImGui::SameLine();
    ImGui::PushItemWidth(300);
    ImGui::InputText("Ally name", &name);
}

/// ------------- PartyMemberStatusCondition -------------
PartyMemberStatusCondition::PartyMemberStatusCondition(std::istringstream& stream)
{
    int read;
    stream >> read;
    status = (Status)read;
    name = readStringWithSpaces(stream);
}
void PartyMemberStatusCondition::serialize(std::ostringstream& stream) const
{
    Condition::serialize(stream);

    stream << (int)status;
    writeStringWithSpaces(stream, name);
}
bool PartyMemberStatusCondition::check() const
{
    const auto info = GW::PartyMgr::GetPartyInfo();
    const auto agentArray = GW::Agents::GetAgentArray();
    if (!info || !agentArray) return false;

    auto& instanceInfo = InstanceInfo::getInstance();

    const auto agentID = [&] {
        for (const auto& player : info->players) {
            auto candidate = GW::Agents::GetAgentIdByLoginNumber(player.login_number);
            if (instanceInfo.getDecodedName(candidate) == name) {
                return std::optional{candidate};
            }
        }
        return std::optional<GW::AgentID>{};
    }();

    if (!agentID) return false;
    
    const auto agent = GW::Agents::GetAgentByID(agentID.value());
    if (!agent) return false;
    const auto living = agent->GetAsAgentLiving();

    bool shouldBeAlive = status == Status::Alive;
    return living && living->GetIsAlive() == shouldBeAlive;
}
void PartyMemberStatusCondition::drawSettings()
{
    ImGui::Text("Party window ally status");
    ImGui::SameLine();
    if (ImGui::Button(toString(status).data(), ImVec2(100, 0))) {
        ImGui::OpenPopup("Pick status");
    }
    if (ImGui::BeginPopup("Pick status")) {
        for (auto i = 0; i <= (int)Status::Alive; ++i) {
            if (ImGui::Selectable(toString((Status)i).data())) {
                status = (Status)i;
            }
        }
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    ImGui::PushItemWidth(300);
    ImGui::InputText("Ally name", &name);
}

/// ------------- QuestHasStateCondition -------------

QuestHasStateCondition::QuestHasStateCondition(std::istringstream& stream)
{
    int read;

    stream >> read;
    id = (GW::Constants::QuestID)read;

    stream >> read;
    status = (QuestStatus)read;
}
void QuestHasStateCondition::serialize(std::ostringstream& stream) const
{
    Condition::serialize(stream);

    stream << (int)id << " " << (int)status << " ";
}
bool QuestHasStateCondition::check() const
{
    return InstanceInfo::getInstance().getQuestStatus(id) == status;
}
void QuestHasStateCondition::drawSettings()
{
    ImGui::Text("If the quest objective has status");
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    ImGui::InputInt("id", reinterpret_cast<int*>(&id), 0);
    ImGui::SameLine();

    if (ImGui::Button(toString(status).data(), ImVec2(100, 0))) {
        ImGui::OpenPopup("Pick status");
    }
    if (ImGui::BeginPopup("Pick status")) {
        for (auto i = 0; i <= (int)QuestStatus::Failed; ++i) {
            if (ImGui::Selectable(toString((QuestStatus)i).data())) {
                status = (QuestStatus)i;
            }
        }
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    ShowHelp("Objective ID, NOT quest ID!\nUW: Chamber 146, Restore 147, UWG 149, Vale 150, Waste 151, Pits 152, Planes 153, Mnts 154, Pools 155");
}

/// ------------- KeyIsPressedCondition -------------
KeyIsPressedCondition::KeyIsPressedCondition(std::istringstream& stream)
{
    stream >> shortcutKey >> shortcutMod;
    description = makeHotkeyDescription(shortcutKey, shortcutMod);
}
void KeyIsPressedCondition::serialize(std::ostringstream& stream) const
{
    Condition::serialize(stream);

    stream << shortcutKey << " " << shortcutMod << " ";
}
bool KeyIsPressedCondition::check() const
{
    bool keyIsPressed = GetAsyncKeyState(shortcutKey) & (1 << 15);
    if (shortcutMod & ModKey_Control) keyIsPressed &= ImGui::IsKeyDown(ImGuiKey_ModCtrl);
    if (shortcutMod & ModKey_Shift) keyIsPressed &= ImGui::IsKeyDown(ImGuiKey_ModShift);
    if (shortcutMod & ModKey_Alt) keyIsPressed &= ImGui::IsKeyDown(ImGuiKey_ModAlt);

    return keyIsPressed;
}
void KeyIsPressedCondition::drawSettings()
{
    ImGui::Text("If key is pressed:");
    ImGui::SameLine();
    drawHotkeySelector(shortcutKey, shortcutMod, description, 100.f);
}

/// ------------- InstanceTimeCondition -------------

InstanceTimeCondition::InstanceTimeCondition(std::istringstream& stream)
{
    stream >> timeInSeconds;
}
void InstanceTimeCondition::serialize(std::ostringstream& stream) const
{
    Condition::serialize(stream);

    stream << timeInSeconds << " ";
}
bool InstanceTimeCondition::check() const
{
    return (int)(GW::Map::GetInstanceTime() / 1000) >= timeInSeconds;
}
void InstanceTimeCondition::drawSettings()
{
    ImGui::Text("If the instance is older than");
    ImGui::SameLine();
    ImGui::InputInt("s", &timeInSeconds, 0);
}

/// ------------- NearbyAgentCondition -------------

NearbyAgentCondition::NearbyAgentCondition(std::istringstream& stream)
{
    stream >> agentType >> primary >> secondary >> status >> hexed >> skill >> modelId >> minDistance >> maxDistance;
    agentName = readStringWithSpaces(stream);
}
void NearbyAgentCondition::serialize(std::ostringstream& stream) const
{
    Condition::serialize(stream);

    stream << agentType << " " << primary << " " << secondary << " " << status << " " << hexed << " " << skill << " "
           << " " << modelId << " " << minDistance << " " << maxDistance << " ";
    writeStringWithSpaces(stream, agentName);
}
bool NearbyAgentCondition::check() const
{
    const auto player = GW::Agents::GetPlayerAsAgentLiving();
    const auto agents = GW::Agents::GetAgentArray();
    if (!player || !agents) return false;

    auto& instanceInfo = InstanceInfo::getInstance();

    const auto fulfillsConditions = [&](const GW::AgentLiving* agent) {
        if (!agent) return false;
        const auto correctType = [&]() -> bool {
            switch (agentType) {
                case AgentType::Any:
                    return true;
                case AgentType::PartyMember: // optimize this? Dont need to check all agents
                    return agent->IsPlayer();
                case AgentType::Friendly:
                    return agent->allegiance != GW::Constants::Allegiance::Enemy;
                case AgentType::Hostile:
                    return agent->allegiance == GW::Constants::Allegiance::Enemy;
                default:
                    return false;
            }
        }();
        const auto correctPrimary = (primary == Class::Any) || primary == (Class)agent->primary;
        const auto correctSecondary = (secondary == Class::Any) || secondary == (Class)agent->secondary;
        const auto correctStatus = (status == Status::Any) || ((status == Status::Alive) == agent->GetIsAlive());
        const auto hexedCorrectly = (hexed == HexedStatus::Any) || ((hexed == HexedStatus::Hexed) == agent->GetIsHexed());
        const auto correctSkill = (skill == GW::Constants::SkillID::No_Skill) || (skill == (GW::Constants::SkillID)agent->skill);
        const auto correctModelId = (modelId == 0) || (agent->player_number == modelId);
        const auto distance = GW::GetDistance(player->pos, agent->pos);
        const auto goodDistance = (minDistance < distance) && (distance < maxDistance);
        const auto goodName = (agentName.empty()) || (instanceInfo.getDecodedName(agent->agent_id) == agentName);
        return correctType && correctPrimary && correctSecondary && correctStatus && hexedCorrectly && correctSkill && correctModelId && goodDistance && goodName;
    };
    if (agentType == AgentType::PartyMember) {
        const auto info = GW::PartyMgr::GetPartyInfo();
        if (!info) return false;
        for (const auto& partyMember : info->players) {
            const auto agent = GW::Agents::GetAgentByID(GW::Agents::GetAgentIdByLoginNumber(partyMember.login_number));
            if (!agent) continue;
            if (fulfillsConditions(agent->GetAsAgentLiving())) return true;
        }
    }
    else {
        for (const auto* agent : *agents) {
            if (!agent) continue;
            if (fulfillsConditions(agent->GetAsAgentLiving())) return true;
        }
    }
    return false;
}
void NearbyAgentCondition::drawSettings()
{
    int drawId = 7122;
    ImGui::PushItemWidth(120);

    if (ImGui::TreeNodeEx("If there exists an agent with characteristics", ImGuiTreeNodeFlags_FramePadding)) {
        ImGui::BulletText("Distance to player");
        ImGui::SameLine();
        ImGui::InputFloat("min", &minDistance);
        ImGui::SameLine();
        ImGui::InputFloat("max", &maxDistance);

        ImGui::BulletText("Allegiance");
        ImGui::SameLine();
        drawEnumButton(AgentType::Any, AgentType::Hostile, agentType, ++drawId);

        ImGui::BulletText("Class");
        ImGui::SameLine();
        drawEnumButton(Class::Any, Class::Dervish, primary, ++drawId);
        ImGui::SameLine();
        ImGui::Text("/");
        ImGui::SameLine();
        drawEnumButton(Class::Any, Class::Dervish, secondary, ++drawId);

        ImGui::BulletText("Dear or alive");
        ImGui::SameLine();
        drawEnumButton(Status::Any, Status::Alive, status, ++drawId);

        ImGui::BulletText("Hexed");
        ImGui::SameLine();
        drawEnumButton(HexedStatus::Any, HexedStatus::Hexed, hexed, ++drawId);

        ImGui::BulletText("Uses skill");
        ImGui::SameLine();
        ImGui::InputInt("id (0 for any)###2", reinterpret_cast<int*>(&skill), 0);

        ImGui::BulletText("Has name");
        ImGui::SameLine();
        ImGui::InputText((std::string{"###"} + std::to_string(++drawId)).c_str(), &agentName);

        ImGui::Bullet();
        ImGui::Text("Has model");
        ImGui::SameLine();
        ImGui::InputInt("id (0 for any)###1", &modelId, 0);

        ImGui::TreePop();
    }
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
