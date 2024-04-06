#include <ConditionImpls.h>

#include <ConditionIO.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/CharContext.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Packets/StoC.h>

#include <unordered_map>
#include <random>
#include "imgui.h"

namespace {
    constexpr double eps = 1e-3;
    const std::string missingContentToken = "/";
    static std::random_device rd;
    static std::mt19937 generator(rd());
    static std::uniform_int_distribution<> getRandomInt(1, std::numeric_limits<int>::max());
    static int instanceUniqueId{getRandomInt(generator)};

    struct QuestObserver {
        std::unordered_map<GW::Constants::QuestID, QuestStatus> questStatus;
        GW::HookEntry ObjectiveUpdateName_Entry;
        GW::HookEntry ObjectiveDone_Entry;
        GW::HookEntry InstanceLoadFile_Entry;

        QuestObserver()
        {
            GW::StoC::RegisterPacketCallback<GW::Packet::StoC::ObjectiveUpdateName>(&ObjectiveUpdateName_Entry, [this](GW::HookStatus*, const GW::Packet::StoC::ObjectiveUpdateName* packet) {
                this->questStatus[(GW::Constants::QuestID)packet->objective_id] = QuestStatus::Started;
            });
            GW::StoC::RegisterPacketCallback<GW::Packet::StoC::ObjectiveDone>(&ObjectiveDone_Entry, [this](GW::HookStatus*, const GW::Packet::StoC::ObjectiveDone* packet) {
                this->questStatus[(GW::Constants::QuestID)packet->objective_id] = QuestStatus::Completed;
            });
            GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::InstanceLoadFile>(&InstanceLoadFile_Entry, [this](GW::HookStatus*, const GW::Packet::StoC::InstanceLoadFile*) {
                this->questStatus.clear();
                instanceUniqueId = getRandomInt(generator);
            });
        }
    };
    static std::unique_ptr<QuestObserver> questObserver = nullptr;

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
    return GW::SkillbarMgr::GetSkillSlot(id) >= 0;
}
void PlayerHasSkillCondition::drawSettings()
{
    ImGui::Text("If the player has the skill");
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    ImGui::InputInt("id", reinterpret_cast<int*>(&id), 0);
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
    if (!questObserver) questObserver = std::make_unique<QuestObserver>();
    return questObserver->questStatus[id] == status;
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
