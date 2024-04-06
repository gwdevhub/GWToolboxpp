#include <ConditionImpls.h>

#include <ConditionIO.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Skill.h>
#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/CharContext.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Packets/StoC.h>

#include <unordered_map>
#include <random>
#include "imgui.h"
#include "ImGuiCppWrapper.h"

namespace {
    constexpr double eps = 1e-3;
    const std::string endOfNameSignifier = "ENDOFNAME";
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

    std::string toString(Class c)
    {
        switch (c) {
            case Class::Warrior:
                return "Warrior";
            case Class::Ranger:
                return "Ranger";
            case Class::Monk:
                return "Monk";
            case Class::Necro:
                return "Necro";
            case Class::Mesmer:
                return "Mesmer";
            case Class::Elementalist:
                return "Elementalist";
            case Class::Assassin:
                return "Assassin";
            case Class::Ritualist:
                return "Ritualist";
            case Class::Paragon:
                return "Paragon";
            case Class::Dervish:
                return "Dervish";
            default:
                return "Any";
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
    ImGui::Text("If script has not been triggered in this instance (this condition has to come last)");
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
    std::string word;
    while (!stream.eof()) {
        stream >> word;
        if (word == endOfNameSignifier) break;
        name += word + " ";
    }
    if (!name.empty()) {
        name.erase(name.size() - 1, 1); // last character is space
    }
}
void PlayerHasNameCondition::serialize(std::ostringstream& stream) const
{
    Condition::serialize(stream);

    stream << name << " " << endOfNameSignifier << " ";
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
    std::string word;
    while (!stream.eof()) {
        stream >> word;
        if (word == endOfNameSignifier) break;
        name += word + " ";
    }
    if (!name.empty()) {
        name.erase(name.size() - 1, 1); //last character is space
    }
}
void HasPartyWindowAllyOfNameCondition::serialize(std::ostringstream& stream) const
{
    Condition::serialize(stream);

    stream << name << " " << endOfNameSignifier << " ";
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
    auto& instanceInfo = InstanceInfo::getInstance();
    return instanceInfo.getQuestStatus(id) == status;
}
void QuestHasStateCondition::drawSettings()
{
    ImGui::Text("If the quest has status");
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
}
