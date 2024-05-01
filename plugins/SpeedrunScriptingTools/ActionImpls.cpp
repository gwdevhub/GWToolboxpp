#include <ActionImpls.h>

#include <ConditionIO.h>
#include <ActionIO.h>
#include <InstanceInfo.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/CtoSMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/EffectMgr.h>

#include <GWCA/Packets/Opcodes.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Skill.h>

#include "imgui.h"
#include <ImGuiCppWrapper.h>
#include <thread>

namespace {
    const std::string missingContentToken = "/";

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
        return item;
    }
    void SafeEquip(GW::Item* item)
    {
        using namespace std::chrono_literals;
        GW::AgentLiving* p = GW::Agents::GetCharacter();
        const GW::Skillbar* s = GW::SkillbarMgr::GetPlayerSkillbar();
        if (!item || !p || p->GetIsDead() || p->GetIsKnockedDown() || (s && s->casting)) return;
        if (p->skill) {
            GW::CtoS::SendPacket(0x4, GAME_CMSG_CANCEL_MOVEMENT);
            std::this_thread::sleep_for(10ms);
        }
        if (!p->GetIsIdle() && !p->GetIsMoving()) {
            GW::Agents::Move(p->pos);
            std::this_thread::sleep_for(10ms);
        }
        GW::Items::EquipItem(item);
    }
    const GW::AgentLiving* findAgentWithId(uint32_t id)
    {
        const auto agents = GW::Agents::GetAgentArray();
        if (!agents) return nullptr;
        for (const auto* agent : *agents) {
            if (!agent) continue;
            if (const auto living = agent->GetAsAgentLiving(); living && living->player_number == id) return living;
        }
        return nullptr;
    }
    std::string_view toString(SendChatAction::Channel channel) {
        switch (channel) {
            case SendChatAction::Channel::All:
                return "All";
            case SendChatAction::Channel::Guild:
                return "Guild";
            case SendChatAction::Channel::Team:
                return "Team";
            case SendChatAction::Channel::Trade:
                return "Trade";
            case SendChatAction::Channel::Alliance:
                return "Alliance";
            case SendChatAction::Channel::Whisper:
                return "Whisper";
            case SendChatAction::Channel::Emote:
                return "Emote";
            default:
                return "Unknown";
        }
    }

    template <typename T>
    void drawEnumButton(T firstValue, T lastValue, T& currentValue, int id = 0, float width = 100.)
    {
        ImGui::PushID(id);

        if (ImGui::Button(toString(currentValue).data(), ImVec2(width, 0))) {
            ImGui::OpenPopup("Enum popup");
        }
        if (ImGui::BeginPopup("Enum popup")) {
            for (auto i = (int)firstValue; i <= (int)lastValue; ++i) {
                if (ImGui::Selectable(toString((T)i).data())) {
                    currentValue = static_cast<T>(i);
                }
            }
            ImGui::EndPopup();
        }

        ImGui::PopID();
    }

    void ctosUseSkill(GW::Constants::SkillID skill, GW::AgentLiving* target) {
        GW::GameThread::Enqueue([skill = (uint32_t)skill, target = target->agent_id]() -> void {
            GW::CtoS::SendPacket(0x14, GAME_CMSG_USE_SKILL, skill, 0, target, 0);
        });
    }

    constexpr double eps = 1e-3;
} // namespace

/// ------------- MoveToAction -------------
MoveToAction::MoveToAction(std::istringstream& stream)
{
    stream >> pos.x >> pos.y >> accuracy >> radius;
}
void MoveToAction::serialize(std::ostringstream& stream) const
{
    Action::serialize(stream);

    stream << pos.x << " " << pos.y << " " << accuracy << " " << radius << " ";
}
void MoveToAction::initialAction()
{
    Action::initialAction();

    GW::GameThread::Enqueue([pos = this->pos]() -> void {
        GW::Agents::Move(pos);
    });
}
bool MoveToAction::isComplete() const
{
    const auto player = GW::Agents::GetPlayerAsAgentLiving();
    if (!player) return true;

    const auto distance = GW::GetDistance(player->pos, pos);

    if (distance > GW::Constants::Range::Compass) {
        return true; // We probably teled
    }

    if (!player->GetIsMoving()) {
        float px = radius > 0 ? pos.x + (rand() % radius - radius / 2) : pos.x;
        float py = radius > 0 ? pos.y + (rand() % radius - radius / 2) : pos.y;

        const auto now = std::chrono::steady_clock::now();
        const auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - lastMovePacketTime).count();
        if (elapsedTime > 50) {
            lastMovePacketTime = now;
            GW::Agents::Move(GW::GamePos{px, py, pos.zplane});
        }
    }

    return distance < accuracy + eps;
}
void MoveToAction::drawSettings(){
    ImGui::Text("Move to:");
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    ImGui::InputFloat("x", &pos.x, 0.0f, 0.0f);
    ImGui::SameLine();
    ImGui::InputFloat("y", &pos.y, 0.0f, 0.0f);
    ImGui::SameLine();
    ImGui::InputFloat("Accuracy", &accuracy, 0.0f, 0.0f);
    ImGui::SameLine();
    ImGui::InputInt("Target radius", &radius, 0);
    ImGui::SameLine();
    ShowHelp("If radius is greater than 0, moves to a slightly different position within the specified radius every time the player stops moving. Useful for walking through gates");
}

/// ------------- CastOnSelfAction -------------
CastOnSelfAction::CastOnSelfAction(std::istringstream& stream)
{
    stream >> id;
}
void CastOnSelfAction::serialize(std::ostringstream& stream) const
{
    Action::serialize(stream);

    stream << id << " ";
}
void CastOnSelfAction::initialAction()
{
    Action::initialAction();

    GW::Skillbar* bar = GW::SkillbarMgr::GetPlayerSkillbar();
    
    int slot = -1;
    hasSkillReady = false;
    if (bar && bar->IsValid())
    for (int i = 0; i < 8; ++i) {
        if (bar->skills[i].skill_id == id) {
            slot = i;
            hasSkillReady = bar->skills[i].GetRecharge() == 0;
        }
    }
    if (slot < 0 || !hasSkillReady) return;

    startTime = std::chrono::steady_clock::now();

    hasBegunCasting = false;
    ctosUseSkill(id, GW::Agents::GetPlayerAsAgentLiving());
}
bool CastOnSelfAction::isComplete() const
{
    if (!hasSkillReady || id == GW::Constants::SkillID::No_Skill) return true;

    const auto player = GW::Agents::GetPlayerAsAgentLiving();
    if (!player) return true;

    const auto skillData = GW::SkillbarMgr::GetSkillConstantData(id);
    if (!skillData) return true;
    const auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count();
    if (elapsedTime > 2000 * skillData->activation) return true;

    hasBegunCasting |= (static_cast<GW::Constants::SkillID>(player->skill) == id);
    

    return hasBegunCasting && static_cast<GW::Constants::SkillID>(player->skill) != id;
}
void CastOnSelfAction::drawSettings()
{
    ImGui::Text("Force-cast on self:");
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    ImGui::InputInt("Skill ID", reinterpret_cast<int*>(&id), 0);
    ImGui::SameLine();
    ShowHelp("Send a CtoS packet to cast a spell on yourself even if you have another target selected. Only necessary for targeted spells.");
}

/// ------------- CastAction -------------
CastAction::CastAction(std::istringstream& stream)
{
    stream >> id;
}
void CastAction::serialize(std::ostringstream& stream) const
{
    Action::serialize(stream);

    stream << id << " ";
}
void CastAction::initialAction()
{
    Action::initialAction();
    

    GW::Skillbar* bar = GW::SkillbarMgr::GetPlayerSkillbar();

    int slot = -1;
    hasSkillReady = false;
    if (bar && bar->IsValid())
    for (int i = 0; i < 8; ++i) {
        if (bar->skills[i].skill_id == id) {
            slot = i;
            hasSkillReady = bar->skills[i].GetRecharge() == 0;
        }
    }
    if (slot < 0 || !hasSkillReady) return;

    startTime = std::chrono::steady_clock::now();

    hasBegunCasting = false;

    const auto target = GW::Agents::GetTargetAsAgentLiving();

    GW::GameThread::Enqueue([slot, targetId = target ? target->agent_id : 0]() -> void {
        GW::SkillbarMgr::UseSkill(slot, targetId);
    });
}
bool CastAction::isComplete() const
{
    if (!hasSkillReady || id == GW::Constants::SkillID::No_Skill) return true;

    const auto player = GW::Agents::GetPlayerAsAgentLiving();
    if (!player) return true;

    const auto skillData = GW::SkillbarMgr::GetSkillConstantData(id);
    if (!skillData) return true;
    const auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count();
    if (elapsedTime > 2000 * skillData->activation) return true;

    hasBegunCasting |= (static_cast<GW::Constants::SkillID>(player->skill) == id);

    return hasBegunCasting && static_cast<GW::Constants::SkillID>(player->skill) != id;
}
void CastAction::drawSettings()
{
    ImGui::Text("Cast on target:");
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    ImGui::InputInt("Skill ID", reinterpret_cast<int*>(&id), 0);
}

/// ------------- ChangeTargetAction -------------
ChangeTargetAction::ChangeTargetAction(std::istringstream& stream)
{
    stream >> agentType >> primary >> secondary >> status >> skill >> sorting >> modelId >> minDistance >> maxDistance >> mayBeCurrentTarget >> requireSameModelIdAsTarget >> preferNonHexed;
    agentName = readStringWithSpaces(stream);
    polygon = readPositions(stream);
}
void ChangeTargetAction::serialize(std::ostringstream& stream) const
{
    Action::serialize(stream);

    stream << agentType << " " << primary << " " << secondary << " " << status << " " << skill << " " << sorting << " " << modelId << " " << minDistance << " " << maxDistance << " " << mayBeCurrentTarget << " " << requireSameModelIdAsTarget << " "
           << preferNonHexed << " ";
    writeStringWithSpaces(stream, agentName);
    writePositions(stream, polygon);
}
void ChangeTargetAction::initialAction()
{
    Action::initialAction();

    const auto player = GW::Agents::GetPlayerAsAgentLiving();
    const auto currentTarget = GW::Agents::GetTargetAsAgentLiving();
    const auto agents = GW::Agents::GetAgentArray();
    if (!player || !agents) return;

    auto& instanceInfo = InstanceInfo::getInstance();

    const auto fulfillsConditions = [&](const GW::AgentLiving* agent) 
    {
        if (!agent) return false;
        const auto correctType = [&]() -> bool {
            switch (agentType) {
                case AgentType::Any:
                    return true;
                case AgentType::PartyMember: //optimize this? Dont need to check all agents
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
        const auto correctSkill = (skill == GW::Constants::SkillID::No_Skill) || (skill == (GW::Constants::SkillID)agent->skill);
        const auto correctModelId = (requireSameModelIdAsTarget && currentTarget) 
            ? (currentTarget->player_number == agent->player_number) 
            : ((modelId == 0) || (agent->player_number == modelId));
        const auto acceptableWithCurrentTarget = mayBeCurrentTarget || !currentTarget || (agent->agent_id != currentTarget->agent_id);
        const auto distance = GW::GetDistance(player->pos, agent->pos);
        const auto goodDistance = (minDistance < distance) && (distance < maxDistance);
        const auto goodName = (agentName.empty()) || (instanceInfo.getDecodedName(agent->agent_id) == agentName);
        const auto goodPosition = (polygon.size() < 3u) || pointIsInsidePolygon(agent->pos, polygon);
        return correctType && correctPrimary && correctSecondary && correctStatus && correctSkill && correctModelId && acceptableWithCurrentTarget && goodDistance && goodName && goodPosition;
    };

    GW::AgentLiving const * currentBestTarget = nullptr;

    const auto isNewBest = [&](const GW::AgentLiving* agent) 
    {
        if(!currentBestTarget)
            return true;
        if (preferNonHexed && !agent->GetIsHexed() && currentBestTarget->GetIsHexed()) 
            return true;
        if (preferNonHexed && agent->GetIsHexed() && !currentBestTarget->GetIsHexed()) 
            return false;
        switch (sorting) {
            case Sorting::AgentId:
                return true;
            case Sorting::ClosestToPlayer:
                return GW::GetSquareDistance(player->pos, agent->pos) < GW::GetSquareDistance(player->pos, currentBestTarget->pos);
            case Sorting::FurthestFromPlayer:
                return GW::GetSquareDistance(player->pos, agent->pos) > GW::GetSquareDistance(player->pos, currentBestTarget->pos);
            case Sorting::ClosestToTarget:
                return currentTarget
                    ? GW::GetSquareDistance(currentTarget->pos, agent->pos) < GW::GetSquareDistance(currentTarget->pos, currentBestTarget->pos) 
                    : GW::GetSquareDistance(player->pos, agent->pos) < GW::GetSquareDistance(player->pos, currentBestTarget->pos); // Fallback: Closest to player
            case Sorting::FurthestFromTarget:
                return currentTarget 
                    ? GW::GetSquareDistance(currentTarget->pos, agent->pos) > GW::GetSquareDistance(currentTarget->pos, currentBestTarget->pos) 
                    : GW::GetSquareDistance(player->pos, agent->pos) < GW::GetSquareDistance(player->pos, currentBestTarget->pos); // Fallback: Closest to player
            case Sorting::LowestHp:
                return agent->hp < currentBestTarget->hp;
            case Sorting::HighestHp:
                return agent->hp > currentBestTarget->hp;
            default:
                return false;
        }
    };

    for (const auto* agent : *agents) {
        if (!agent) continue;
        auto living = agent->GetAsAgentLiving();
        if (!fulfillsConditions(living)) continue;
        if (isNewBest(living)) currentBestTarget = living;
        if (sorting == Sorting::AgentId) break;
    }

    if (currentBestTarget) 
    {
        GW::GameThread::Enqueue([id = currentBestTarget->agent_id]() -> void {
            GW::Agents::ChangeTarget(id);
        });
    }
}
void ChangeTargetAction::drawSettings()
{
    ImGui::PushItemWidth(120);

    if (ImGui::TreeNodeEx("Change target to agent with characteristics", ImGuiTreeNodeFlags_FramePadding)) {
        ImGui::BulletText("Distance to player");
        ImGui::SameLine();
        ImGui::InputFloat("min", &minDistance);
        ImGui::SameLine();
        ImGui::InputFloat("max", &maxDistance);

        ImGui::BulletText("Allegiance");
        ImGui::SameLine();
        drawEnumButton(AgentType::Any, AgentType::Hostile, agentType, 0);

        ImGui::BulletText("Class");
        ImGui::SameLine();
        drawEnumButton(Class::Any, Class::Dervish, primary, 1);

        ImGui::SameLine();
        ImGui::Text("/");
        ImGui::SameLine();
        drawEnumButton(Class::Any, Class::Dervish, secondary, 2);

        ImGui::BulletText("Dead or alive");
        ImGui::SameLine();
        drawEnumButton(Status::Any, Status::Alive, status, 3);

        ImGui::BulletText("Uses skill");
        ImGui::SameLine();
        ImGui::InputInt("id (0 for any)###0", reinterpret_cast<int*>(&skill), 0);

        ImGui::BulletText("Has name");
        ImGui::SameLine();
        ImGui::InputText("###1", &agentName);

        ImGui::Bullet();
        ImGui::Checkbox("Prefer enemies that are not hexed", &preferNonHexed);

        ImGui::Bullet();
        ImGui::Checkbox("May choose current target again", &mayBeCurrentTarget);

        ImGui::Bullet();
        ImGui::Checkbox("Require same model as current target", &requireSameModelIdAsTarget);

        ImGui::Bullet();
        ImGui::Text(requireSameModelIdAsTarget ? "If no target is selected, pick agent with model" : "Pick agent with model");
        ImGui::SameLine();
        ImGui::InputInt("id (0 for any)###2", &modelId, 0);

        ImGui::BulletText("Sort candidates by:");
        ImGui::SameLine();
        drawEnumButton(Sorting::AgentId, Sorting::HighestHp, sorting, 4, 150.);

        ImGui::Bullet();
        ImGui::Text("Is within polygon");
        drawPolygonSelector(polygon);

        ImGui::TreePop();
    }
}

/// ------------- UseItemAction -------------
UseItemAction::UseItemAction(std::istringstream& stream)
{
    stream >> id;
}
void UseItemAction::serialize(std::ostringstream& stream) const
{
    Action::serialize(stream);

    stream << id << " ";
}
void UseItemAction::initialAction()
{
    Action::initialAction();

    const auto item = FindMatchingItem(id);
    if (!item) return;

    GW::GameThread::Enqueue([item]() -> void {
        GW::Items::UseItem(item);
    });
}
void UseItemAction::drawSettings()
{
    ImGui::Text("Use item:");
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    ImGui::InputInt("ID", &id, 0);
}

/// ------------- EquipItemAction -------------
EquipItemAction::EquipItemAction(std::istringstream& stream)
{
    stream >> id;
}
void EquipItemAction::serialize(std::ostringstream& stream) const
{
    Action::serialize(stream);

    stream << id << " ";
}
void EquipItemAction::initialAction()
{
    Action::initialAction();

    const auto item = FindMatchingItem(id);
    if (!item) return;

    GW::GameThread::Enqueue([item]() -> void {
        SafeEquip(item);
    });
}
void EquipItemAction::drawSettings()
{
    ImGui::Text("Use item:");
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    ImGui::InputInt("ID", &id, 0);
}

/// ------------- SendDialogAction -------------
SendDialogAction::SendDialogAction(std::istringstream& stream)
{
    stream >> id;
}
void SendDialogAction::serialize(std::ostringstream& stream) const
{
    Action::serialize(stream);

    stream << id << " ";
}
void SendDialogAction::initialAction()
{
    Action::initialAction();

    GW::GameThread::Enqueue([id = this->id]() -> void {
        GW::Agents::SendDialog(id);
    });
}
void SendDialogAction::drawSettings()
{
    ImGui::Text("Send Dialog:");
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    ImGui::InputInt("ID", &id, 0);
}

/// ------------- GoToTargetAction -------------
GoToTargetAction::GoToTargetAction(std::istringstream& stream)
{
    stream >> accuracy;
}
void GoToTargetAction::serialize(std::ostringstream& stream) const
{
    Action::serialize(stream);

    stream << accuracy << " ";
}
void GoToTargetAction::initialAction()
{
    Action::initialAction();
    
    target = GW::Agents::GetTargetAsAgentLiving();
    if (!target) return;

    GW::GameThread::Enqueue([target = this->target]() -> void {
        GW::Agents::InteractAgent(target);
    });
}
bool GoToTargetAction::isComplete() const
{
    if (!target) return true;
    const auto player = GW::Agents::GetPlayerAsAgentLiving();
    if (!player) return true;

    const auto distance = GW::GetDistance(player->pos, target->pos);

    return distance < accuracy + eps || distance > GW::Constants::Range::Compass;
}
void GoToTargetAction::drawSettings()
{
    ImGui::Text("Go to target:");
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    ImGui::InputFloat("Accuracy", &accuracy, 0.0f, 0.0f);
}

/// ------------- WaitAction -------------
WaitAction::WaitAction(std::istringstream& stream)
{
    stream >> waitTime;
}
void WaitAction::serialize(std::ostringstream& stream) const
{
    Action::serialize(stream);

    stream << waitTime << " ";
}
void WaitAction::initialAction()
{
    Action::initialAction();

    startTime = std::chrono::steady_clock::now();
}
bool WaitAction::isComplete() const
{
    const auto now = std::chrono::steady_clock::now();
    const auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
    return elapsedTime > waitTime;
}
void WaitAction::drawSettings()
{
    ImGui::Text("Wait for:");
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    ImGui::InputInt("ms", &waitTime, 0);
}

/// ------------- SendChatAction -------------
SendChatAction::SendChatAction(std::istringstream& stream)
{
    stream >> channel;
    message = readStringWithSpaces(stream);
}
void SendChatAction::serialize(std::ostringstream& stream) const
{
    Action::serialize(stream);

    stream << channel << " ";
    writeStringWithSpaces(stream, message);
}
void SendChatAction::initialAction()
{
    Action::initialAction();

    const auto channelId = [&]() -> char {
        switch (channel) {
            case Channel::All:
                return '!';
            case Channel::Guild:
                return '@';
            case Channel::Team:
                return '#';
            case Channel::Trade:
                return '$';
            case Channel::Alliance:
                return '%';
            case Channel::Whisper:
                return '"';
            case Channel::Emote:
                return '/';
            default:
                return '#';
        }
        }();
    
    GW::GameThread::Enqueue([channelId, message = this->message]() -> void {
        GW::Chat::SendChat(channelId, message.c_str());
    });
}

void SendChatAction::drawSettings()
{
    ImGui::Text("Send Chat Message:");
    ImGui::SameLine();
    drawEnumButton(Channel::All, Channel::Emote, channel);
    ImGui::PushItemWidth(300);
    ImGui::SameLine();
    ImGui::InputText("", &message);
}

/// ------------- CancelAction -------------
void CancelAction::initialAction()
{
    Action::initialAction();
    GW::GameThread::Enqueue([]() -> void {
        GW::CtoS::SendPacket(0x4, GAME_CMSG_CANCEL_MOVEMENT);
    });
}

void CancelAction::drawSettings()
{
    ImGui::Text("Cancel Action");
}

/// ------------- DropBuffAction -------------
DropBuffAction::DropBuffAction(std::istringstream& stream)
{
    stream >> id;
}
void DropBuffAction::serialize(std::ostringstream& stream) const
{
    Action::serialize(stream);

    stream << id << " ";
}
void DropBuffAction::initialAction()
{
    Action::initialAction();

    const auto buff = GW::Effects::GetPlayerBuffBySkillId(id);
    if (!buff) return;

    GW::GameThread::Enqueue([buffId = buff->buff_id]() -> void {
        GW::Effects::DropBuff(buffId);
    });
}
void DropBuffAction::drawSettings()
{
    ImGui::Text("Drop buff");
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    ImGui::InputInt("Skill ID", reinterpret_cast<int*>(&id), 0);
}

/// ------------- ConditionedAction -------------
ConditionedAction::ConditionedAction(std::istringstream& stream)
{
    std::string read;
    stream >> read;
    if (read == missingContentToken) {
        cond = nullptr;
    }
    else if (read == "C") {
        cond = readCondition(stream);
    }
    else {
        assert(false);
    }
    stream >> read;
    if (read == missingContentToken) {
        act = nullptr;
    }
    else if (read == "A") {
        act = readAction(stream);
    }
    else {
        assert(false);
    }
}
void ConditionedAction::serialize(std::ostringstream& stream) const
{
    Action::serialize(stream);

    if (cond)
        cond->serialize(stream);
    else
        stream << missingContentToken << " ";

    if (act)
        act->serialize(stream);
    else
        stream << missingContentToken << " ";
}
void ConditionedAction::initialAction()
{
    Action::initialAction();

    if (cond && act && cond->check()) 
        act->initialAction();
}
bool ConditionedAction::isComplete() const
{
    if (cond && act) 
        return act->isComplete();
    return true;
}
void ConditionedAction::drawSettings() 
{
    if (cond)
        cond->drawSettings();
    else    
        cond = drawConditionSelector(120.f);
    ImGui::Indent(120.f);
    if (act)
        act->drawSettings();
    else
        act = drawActionSelector(120.f);
    ImGui::Unindent(120.f);
}

/// ------------- RepopMinipetAction -------------
RepopMinipetAction::RepopMinipetAction(std::istringstream& stream)
{
    stream >> id;
}
void RepopMinipetAction::serialize(std::ostringstream& stream) const
{
    Action::serialize(stream);

    stream << id << " ";
}
void RepopMinipetAction::initialAction()
{
    Action::initialAction();
}

bool RepopMinipetAction::isComplete() const
{
    const auto& instanceInfo = InstanceInfo::getInstance();
    if (!instanceInfo.canPopAgent()) return false;

    const auto item = FindMatchingItem(id);
    if (!item) return true;
    const auto needsToUnpop = instanceInfo.hasMinipetPopped();
    GW::GameThread::Enqueue([needsToUnpop, item]() -> void {
        if (needsToUnpop) 
            GW::Items::UseItem(item);
        GW::Items::UseItem(item);
    });
    return true;
}

void RepopMinipetAction::drawSettings()
{
    ImGui::Text("(Unpop and) repop minipet as soon as its available:");
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    ImGui::InputInt("Item ID", &id, 0);
}

/// ------------- PingHardModeAction -------------
void PingHardModeAction::initialAction()
{
    Action::initialAction();

    if (!GW::Effects::GetPlayerEffectBySkillId(GW::Constants::SkillID::Hard_mode)) return;

    // Read from the packet logger. This is probably in the map information somewhere, but I could not find it.
    const auto pingId = [] {
        switch (GW::Map::GetMapID()) {
            case GW::Constants::MapID::The_Underworld:
                return 0x14;
            case GW::Constants::MapID::Domain_of_Anguish:
                return 0x27;
            case GW::Constants::MapID::The_Fissure_of_Woe:
                return 0xA;
            default:
                return 0;
        }
    }();
    
    if (pingId) {
        GW::GameThread::Enqueue([pingId]() {
            GW::CtoS::SendPacket(0xC, GAME_CMSG_TARGET_CALL, 0x4, pingId);
        });
    }
    
}
void PingHardModeAction::drawSettings()
{
    ImGui::Text("Ping hard mode");
    ShowHelp("Currently only works in UW, DoA and FoW because I don't understand the information sent. Let me know if you need any others.");
}

/// ------------- PingTargetAction -------------
void PingTargetAction::initialAction()
{
    Action::initialAction();

    const auto currentTarget = GW::Agents::GetTargetAsAgentLiving();
    if (!currentTarget) return;

    if (const auto currentInstanceId = InstanceInfo::getInstance().getInstanceId(); currentInstanceId != instanceId) {
        instanceId = currentInstanceId;
        pingedTargets.clear();
    }

    if (onlyOncePerInstance && pingedTargets.contains(currentTarget->agent_id)) return;

    pingedTargets.insert(currentTarget->agent_id);
    GW::GameThread::Enqueue([id = currentTarget->agent_id]() {
        GW::CtoS::SendPacket(0xC, GAME_CMSG_TARGET_CALL, 0xA, id);
    });
}
void PingTargetAction::drawSettings()
{
    ImGui::Text("Ping current target");
    ImGui::SameLine();
    ImGui::Checkbox("Ping same agent only once per instance", &onlyOncePerInstance);
}

/// ------------- AutoAttackTargetAction -------------
void AutoAttackTargetAction::initialAction()
{
    Action::initialAction();

    const auto currentTarget = GW::Agents::GetTargetAsAgentLiving();
    if (!currentTarget || currentTarget->allegiance != GW::Constants::Allegiance::Enemy) return;
    
    GW::GameThread::Enqueue([id = currentTarget->agent_id]() {
        GW::CtoS::SendPacket(0xC, GAME_CMSG_ATTACK_AGENT, id, 0, 0);
    });
}
void AutoAttackTargetAction::drawSettings()
{
    ImGui::Text("Autoattack current target");
}
