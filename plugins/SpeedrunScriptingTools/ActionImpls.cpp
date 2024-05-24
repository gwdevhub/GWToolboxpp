#include <ActionImpls.h>

#include <ConditionIO.h>
#include <ActionIO.h>
#include <InstanceInfo.h>
#include <enumUtils.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <GWCA/Packets/Opcodes.h>
#include <GWCA/Packets/StoC.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Skill.h>

#include <ImGuiCppWrapper.h>
#include <thread>

namespace {
    const std::string missingContentToken = "/";
    const std::string endOfListToken = ">";
    constexpr float indent = 30.f;

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
            GW::GameThread::Enqueue([]{ GW::UI::Keypress(GW::UI::ControlAction_CancelAction); });
            std::this_thread::sleep_for(10ms);
        }
        if (!p->GetIsIdle() && !p->GetIsMoving()) {
            GW::Agents::Move(p->pos);
            std::this_thread::sleep_for(10ms);
        }
        GW::GameThread::Enqueue([item]() {
            GW::Items::EquipItem(item);
        });
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
    float angleToAgent(const GW::AgentLiving* player, const GW::AgentLiving* agent)
    {
        constexpr auto radiansToDegree = 180.f / 3.141592741f;
        const auto angleBetweenNormalizedVectors = [](GW::Vec2f a, GW::Vec2f b) { return std::acos(a.x * b.x + a.y * b.y); };
        const auto forwards = GW::Normalize(GW::Vec2f{player->rotation_cos, player->rotation_sin});
        const auto toTarget = GW::Normalize(GW::Vec2f{agent->pos.x - player->pos.x, agent->pos.y - player->pos.y});
        return angleBetweenNormalizedVectors(forwards, toTarget) * radiansToDegree;
    }

    constexpr double eps = 1e-3;
} // namespace

/// ------------- MoveToAction -------------
MoveToAction::MoveToAction()
{
    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable) {
        if (auto player = GW::Agents::GetPlayerAsAgentLiving()) {
            pos = player->pos;
        }
    }
}
MoveToAction::MoveToAction(InputStream& stream)
{
    stream >> pos.x >> pos.y >> accuracy >> repeatMove;
}
void MoveToAction::serialize(OutputStream& stream) const
{
    Action::serialize(stream);

    stream << pos.x << pos.y << accuracy << repeatMove;
}
void MoveToAction::initialAction()
{
    Action::initialAction();

    hasBegunWalking = false;

    GW::GameThread::Enqueue([pos = this->pos]() -> void {
        GW::Agents::Move(pos);
    });
}
ActionStatus MoveToAction::isComplete() const
{
    const auto player = GW::Agents::GetPlayerAsAgentLiving();
    if (!player) return ActionStatus::Error;

    const auto distance = GW::GetDistance(player->pos, pos);

    if (distance > GW::Constants::Range::Compass) 
    {
        return ActionStatus::Error; // We probably teled
    }

    if (!player->GetIsMoving() && distance > accuracy + eps) 
    {
        if (repeatMove) 
        {
            const auto radius = std::min((int)(accuracy / 4), 10);
            float px = radius > 0 ? pos.x + (rand() % radius - radius / 2) : pos.x;
            float py = radius > 0 ? pos.y + (rand() % radius - radius / 2) : pos.y;

            const auto now = std::chrono::steady_clock::now();
            const auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - lastMovePacketTime).count();
            if (elapsedTime > 50) {
                lastMovePacketTime = now;
                GW::GameThread::Enqueue([pos = GW::GamePos{px, py, pos.zplane}]{
                    GW::Agents::Move(pos);
                });
            }
        }
        else if (hasBegunWalking)
        {
            return ActionStatus::Complete;
        }
    }

    hasBegunWalking |= player->GetIsMoving();

    return distance < accuracy + eps ? ActionStatus::Complete : ActionStatus::Running;
}
void MoveToAction::drawSettings(){
    ImGui::PushID(drawId());

    ImGui::Text("Move to:");
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    ImGui::InputFloat("x", &pos.x, 0.0f, 0.0f);
    ImGui::SameLine();
    ImGui::InputFloat("y", &pos.y, 0.0f, 0.0f);
    ImGui::SameLine();
    ImGui::InputFloat("Accuracy", &accuracy, 0.0f, 0.0f);
    ImGui::SameLine();
    ImGui::Checkbox("Repeat move when idle", &repeatMove);

    ImGui::PopID();
}

/// ------------- CastOnSelfAction -------------
CastOnSelfAction::CastOnSelfAction(InputStream& stream)
{
    stream >> id;
}
void CastOnSelfAction::serialize(OutputStream& stream) const
{
    Action::serialize(stream);

    stream << id;
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
    /*GW::GameThread::Enqueue([skill = (uint32_t)skill, target = target->agent_id]() -> void {
        GW::CtoS::SendPacket(0x14, GAME_CMSG_USE_SKILL, skill, 0, target, 0);
    });*/
}
ActionStatus CastOnSelfAction::isComplete() const
{
    if (!hasSkillReady || id == GW::Constants::SkillID::No_Skill) return ActionStatus::Error;

    const auto player = GW::Agents::GetPlayerAsAgentLiving();
    if (!player) return ActionStatus::Error;

    const auto skillData = GW::SkillbarMgr::GetSkillConstantData(id);
    if (!skillData) return ActionStatus::Error;
    const auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count();
    if (elapsedTime > 2000 * (skillData->activation + skillData->aftercast)) return ActionStatus::Complete;

    hasBegunCasting |= (static_cast<GW::Constants::SkillID>(player->skill) == id);

    return (hasBegunCasting && static_cast<GW::Constants::SkillID>(player->skill) != id) ? ActionStatus::Complete : ActionStatus::Running;
}
void CastOnSelfAction::drawSettings()
{
    ImGui::PushID(drawId());

    ImGui::Text("Force-cast on self:");
    ImGui::SameLine();
    drawSkillIDSelector(id);
    ImGui::SameLine();
    ImGui::ShowHelp("Send a CtoS packet to cast a spell on yourself even if you have another target selected. Only necessary for targeted spells.");

    ImGui::PopID();
}

/// ------------- CastAction -------------
CastAction::CastAction(InputStream& stream)
{
    stream >> id;
}
void CastAction::serialize(OutputStream& stream) const
{
    Action::serialize(stream);

    stream << id;
}
void CastAction::initialAction()
{
    Action::initialAction();

    startTime = std::chrono::steady_clock::now();
    hasBegunCasting = false;
    hasSkillReady = false;

    const auto bar = GW::SkillbarMgr::GetPlayerSkillbar();
    int slot = -1;
    if (bar && bar->IsValid()) {
        for (int i = 0; i < 8; ++i) {
            if (bar->skills[i].skill_id == id) {
                slot = i;
                hasSkillReady = bar->skills[i].GetRecharge() == 0;
            }
        }
    }
    if (slot < 0 || !hasSkillReady) return;

    const auto target = GW::Agents::GetTargetAsAgentLiving();
    GW::GameThread::Enqueue([slot, targetId = target ? target->agent_id : 0]() -> void {
        GW::SkillbarMgr::UseSkill(slot, targetId);
    });
}
ActionStatus CastAction::isComplete() const
{
    if (!hasSkillReady || id == GW::Constants::SkillID::No_Skill) return ActionStatus::Error;

    const auto player = GW::Agents::GetPlayerAsAgentLiving();
    if (!player) return ActionStatus::Error;

    const auto skillData = GW::SkillbarMgr::GetSkillConstantData(id);
    if (!skillData) return ActionStatus::Error;
    if (skillData->activation == 0.f) return ActionStatus::Complete;

    hasBegunCasting |= (static_cast<GW::Constants::SkillID>(player->skill) == id);

    const auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count();
    if (elapsedTime > 2000 * (skillData->activation + skillData->aftercast)) return ActionStatus::Complete;

    return (hasBegunCasting && static_cast<GW::Constants::SkillID>(player->skill) != id) ? ActionStatus::Complete : ActionStatus::Running;
}
void CastAction::drawSettings()
{
    ImGui::PushID(drawId());

    ImGui::Text("Use skill");
    ImGui::SameLine();
    drawSkillIDSelector(id);

    ImGui::PopID();
}

/// ------------- CastBySlotAction -------------
CastBySlotAction::CastBySlotAction(InputStream& stream)
{
    stream >> slot;
}
void CastBySlotAction::serialize(OutputStream& stream) const
{
    Action::serialize(stream);

    stream << slot;
}
void CastBySlotAction::initialAction()
{
    Action::initialAction();
    hasBegunCasting = false;
    startTime = std::chrono::steady_clock::now();

    const auto bar = GW::SkillbarMgr::GetPlayerSkillbar();
    if (!bar || !bar->IsValid()) return;

    hasSkillReady = bar->skills[slot - 1].GetRecharge() == 0;
    id = bar->skills[slot - 1].skill_id;
    if (!hasSkillReady) return;

    const auto target = GW::Agents::GetTargetAsAgentLiving();
    GW::GameThread::Enqueue([this, targetId = target ? target->agent_id : 0]() -> void {
        GW::SkillbarMgr::UseSkill(slot - 1, targetId);
    });
}
ActionStatus CastBySlotAction::isComplete() const
{
    if (!hasSkillReady || id == GW::Constants::SkillID::No_Skill) return ActionStatus::Error;

    const auto player = GW::Agents::GetPlayerAsAgentLiving();
    if (!player) return ActionStatus::Error;

    const auto skillData = GW::SkillbarMgr::GetSkillConstantData(id);
    if (!skillData) return ActionStatus::Error;
    if (skillData->activation == 0.f) return ActionStatus::Complete;

    hasBegunCasting |= (static_cast<GW::Constants::SkillID>(player->skill) == id);

    const auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count();
    if (elapsedTime > 2000 * (skillData->activation+skillData->aftercast)) return ActionStatus::Complete;

    return (hasBegunCasting && static_cast<GW::Constants::SkillID>(player->skill) != id) ? ActionStatus::Complete : ActionStatus::Running;
}
void CastBySlotAction::drawSettings()
{
    ImGui::PushID(drawId());

    ImGui::Text("Use skill");
    ImGui::PushItemWidth(40.f);
    ImGui::SameLine();
    ImGui::InputInt("Slot", reinterpret_cast<int*>(&slot), 0);
    if (slot < 1) slot = 1;
    if (slot > 8) slot = 8;
    
    ImGui::PopID();
}

/// ------------- ChangeTargetAction -------------
ChangeTargetAction::ChangeTargetAction(InputStream& stream)
{
    stream >> agentType >> primary >> secondary >> status >> skill >> sorting >> modelId >> minDistance >> maxDistance >> requireSameModelIdAsTarget >> preferNonHexed >> rotateThroughTargets;
    agentName = readStringWithSpaces(stream);
    polygon = readPositions(stream);
}
void ChangeTargetAction::serialize(OutputStream& stream) const
{
    Action::serialize(stream);

    stream << agentType << primary << secondary << status << skill << sorting << modelId << minDistance << maxDistance << requireSameModelIdAsTarget << preferNonHexed << rotateThroughTargets;
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

    if (agentType == AgentType::Self) 
    {
        GW::GameThread::Enqueue([id = player->agent_id]() -> void {
            GW::Agents::ChangeTarget(id);
        });
        return;
    }

    if (rotateThroughTargets && recentlyTargetedEnemies.empty() && currentTarget)
    {
        recentlyTargetedEnemies.insert(currentTarget->agent_id);
    }

    auto& instanceInfo = InstanceInfo::getInstance();

    const auto fulfillsConditions = [&](const GW::AgentLiving* agent) 
    {
        if (!agent) return false;
        if (agent->agent_id == player->agent_id) return false; // Target self handled seperately
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
        const auto distance = GW::GetDistance(player->pos, agent->pos);
        const auto goodDistance = (minDistance - eps < distance) && (distance < maxDistance + eps);
        const auto goodName = (agentName.empty()) || (instanceInfo.getDecodedName(agent->agent_id) == agentName);
        const auto goodPosition = (polygon.size() < 3u) || pointIsInsidePolygon(agent->pos, polygon);
        const auto goodHp = minHp <= 100.f * agent->hp && 100.f * agent->hp <= maxHp;
        const auto goodAngle = angleToAgent(player, agent) - eps < maxAngle;

        return correctType && correctPrimary && correctSecondary && correctStatus && correctSkill && correctModelId && goodDistance && goodName && goodPosition && goodHp && goodAngle;
    };

    const GW::AgentLiving* currentBestTarget = nullptr;

    const auto isNewBest = [&](const GW::AgentLiving* agent) 
    {
        if(!currentBestTarget)
            return true;
        if (preferNonHexed && !agent->GetIsHexed() && currentBestTarget->GetIsHexed()) 
            return true;
        if (preferNonHexed && agent->GetIsHexed() && !currentBestTarget->GetIsHexed()) 
            return false;

        if (rotateThroughTargets)
        {
            const auto currentBestIsStored = recentlyTargetedEnemies.contains(currentBestTarget->agent_id);
            const auto currentBestIsCurrentTarget = currentTarget && currentBestTarget->agent_id == currentTarget->agent_id;
            const auto agentIsStored = recentlyTargetedEnemies.contains(agent->agent_id);
            const auto agentIsCurrentTarget = currentTarget && agent->agent_id == currentTarget->agent_id;

            if (agentIsStored && !currentBestIsStored) return false;
            if (agentIsCurrentTarget && !currentBestIsCurrentTarget) return false;
            if (!agentIsStored && currentBestIsStored) return true;
            if (!agentIsCurrentTarget && currentBestIsCurrentTarget) return true;
        }

        switch (sorting) {
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
        if (sorting == Sorting::AgentId && !rotateThroughTargets) break;
    }

    if (!currentBestTarget) 
        return;

    if (rotateThroughTargets)
    {
        if (recentlyTargetedEnemies.contains(currentBestTarget->agent_id))
        {
            recentlyTargetedEnemies.clear();
        }
        recentlyTargetedEnemies.insert(currentBestTarget->agent_id);
    }

    GW::GameThread::Enqueue([id = currentBestTarget->agent_id] { GW::Agents::ChangeTarget(id); });
}
void ChangeTargetAction::drawSettings()
{
    ImGui::PushID(drawId());

    ImGui::PushItemWidth(120);

    if (ImGui::TreeNodeEx("Change target to agent with characteristics", ImGuiTreeNodeFlags_FramePadding)) {
        ImGui::BulletText("Type");
        ImGui::SameLine();
        drawEnumButton(AgentType::Any, AgentType::Hostile, agentType, 0);

        if (agentType != AgentType::Self) {
            ImGui::BulletText("Distance to player");
            ImGui::SameLine();
            ImGui::InputFloat("min", &minDistance);
            ImGui::SameLine();
            ImGui::InputFloat("max", &maxDistance);

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
            ImGui::PushID(0);
            drawSkillIDSelector(skill);
            ImGui::PopID();

            ImGui::BulletText("Name");
            ImGui::SameLine();
            ImGui::PushItemWidth(200.f);
            ImGui::InputText("###1", &agentName);

            ImGui::PushItemWidth(80.f);
            ImGui::Bullet();
            ImGui::Text("HP percent");
            ImGui::SameLine();
            ImGui::InputFloat("min###2", &minHp);
            ImGui::SameLine();
            ImGui::InputFloat("max###3", &maxHp);

            ImGui::Bullet();
            ImGui::Checkbox("Prefer enemies that are not hexed", &preferNonHexed);

            ImGui::Bullet();
            ImGui::Checkbox("Rotate through eligible targets", &rotateThroughTargets);

            ImGui::Bullet();
            ImGui::Checkbox("Require same model as current target", &requireSameModelIdAsTarget);

            ImGui::Bullet();
            ImGui::Text(requireSameModelIdAsTarget ? "If no target is selected: Model" : "Model");
            ImGui::SameLine();
            ImGui::InputInt("id (0 for any)###4", &modelId, 0);

            ImGui::Bullet();
            ImGui::Text("Maximum angle to player forward");
            ImGui::SameLine();
            ImGui::InputFloat("degrees", &maxAngle);
            if (maxAngle < 0.f) maxAngle = 0.f;
            if (maxAngle > 180.f) maxAngle = 180.f;

            ImGui::BulletText("Sort candidates by:");
            ImGui::SameLine();
            drawEnumButton(Sorting::AgentId, Sorting::HighestHp, sorting, 4, 150.);

            ImGui::Bullet();
            ImGui::Text("Is within polygon");
            ImGui::SameLine();
            drawPolygonSelector(polygon);
        }

        ImGui::TreePop();
    }

    ImGui::PopID();
}

/// ------------- UseItemAction -------------
UseItemAction::UseItemAction(InputStream& stream)
{
    stream >> id;
}
void UseItemAction::serialize(OutputStream& stream) const
{
    Action::serialize(stream);

    stream << id;
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
    ImGui::PushID(drawId());

    ImGui::Text("Use item:");
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    ImGui::InputInt("model ID", &id, 0);

    ImGui::PopID();
}

/// ------------- EquipItemAction -------------
EquipItemAction::EquipItemAction(InputStream& stream)
{
    stream >> id;
}
void EquipItemAction::serialize(OutputStream& stream) const
{
    Action::serialize(stream);

    stream << id;
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
    ImGui::PushID(drawId());

    ImGui::Text("Equip item:");
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    ImGui::InputInt("model ID", &id, 0);

    ImGui::PopID();
}

/// ------------- SendDialogAction -------------
SendDialogAction::SendDialogAction(InputStream& stream)
{
    stream >> id;
}
void SendDialogAction::serialize(OutputStream& stream) const
{
    Action::serialize(stream);

    stream << id;
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
    ImGui::PushID(drawId());

    ImGui::Text("Send Dialog:");
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    ImGui::InputInt("ID", &id, 0);

    ImGui::PopID();
}

/// ------------- GoToTargetAction -------------
GoToTargetAction::GoToTargetAction(InputStream& stream) : GoToTargetAction()
{
    stream >> finishCondition;
}
void GoToTargetAction::serialize(OutputStream& stream) const
{
    Action::serialize(stream);

    stream << finishCondition;
}
void GoToTargetAction::initialAction()
{
    Action::initialAction();
    
    target = GW::Agents::GetTargetAsAgentLiving();
    if (!target || target->allegiance == GW::Constants::Allegiance::Enemy) return;

    dialogHasPoppedUp = false;
    
    GW::UI::RegisterUIMessageCallback(&hook, GW::UI::UIMessage::kDialogBody, [this, id = target->agent_id](GW::HookStatus*, GW::UI::UIMessage, void* wparam, void*) {
        const auto packet = static_cast<GW::UI::DialogBodyInfo*>(wparam);
        if (packet->agent_id == id)
            dialogHasPoppedUp = true;
    });
    
    GW::GameThread::Enqueue([target = this->target] { GW::Agents::InteractAgent(target); });
}
void GoToTargetAction::finalAction()
{
    Action::finalAction();
    GW::UI::RemoveUIMessageCallback(&hook, GW::UI::UIMessage::kDialogBody);
}
ActionStatus GoToTargetAction::isComplete() const
{
    switch (finishCondition)
    {
        case GoToTargetFinishCondition::StoppedMovingNextToTarget: 
        {
            if (!target || target->allegiance == GW::Constants::Allegiance::Enemy) return ActionStatus::Error;

            const auto player = GW::Agents::GetPlayerAsAgentLiving();
            if (!player) return ActionStatus::Error;

            const auto distance = GW::GetDistance(player->pos, target->pos);
            const auto isMoving = player->GetIsMoving();

            constexpr auto dialogDistance = 101.f;
            return ((!isMoving && distance < dialogDistance) || distance > GW::Constants::Range::Compass) ? ActionStatus::Complete : ActionStatus::Running;
        }
        case GoToTargetFinishCondition::DialogOpen:
            return dialogHasPoppedUp ? ActionStatus::Complete : ActionStatus::Running;
        default:
            return ActionStatus::Complete;
    }
}
void GoToTargetAction::drawSettings()
{
    ImGui::PushID(drawId());

    ImGui::Text("Talk with NPC. Finish action");
    ImGui::SameLine();
    drawEnumButton(GoToTargetFinishCondition::None, GoToTargetFinishCondition::DialogOpen, finishCondition, 0, 250.f);

    ImGui::PopID();
}

/// ------------- WaitAction -------------
WaitAction::WaitAction(InputStream& stream)
{
    stream >> waitTime;
}
void WaitAction::serialize(OutputStream& stream) const
{
    Action::serialize(stream);

    stream << waitTime;
}
void WaitAction::initialAction()
{
    Action::initialAction();

    startTime = std::chrono::steady_clock::now();
}
ActionStatus WaitAction::isComplete() const
{
    const auto now = std::chrono::steady_clock::now();
    const auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
    return (elapsedTime > waitTime) ? ActionStatus::Complete : ActionStatus::Running;
}
void WaitAction::drawSettings()
{
    ImGui::PushID(drawId());

    ImGui::Text("Wait for:");
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    ImGui::InputInt("ms", &waitTime, 0);

    ImGui::PopID();
}

/// ------------- SendChatAction -------------
SendChatAction::SendChatAction(InputStream& stream)
{
    stream >> channel;
    message = readStringWithSpaces(stream);
}
void SendChatAction::serialize(OutputStream& stream) const
{
    Action::serialize(stream);

    stream << channel;
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
    ImGui::PushID(drawId());

    ImGui::Text("Send Chat Message:");
    ImGui::SameLine();
    drawEnumButton(Channel::All, Channel::Emote, channel);
    ImGui::PushItemWidth(300);
    ImGui::SameLine();
    ImGui::InputText("", &message);

    ImGui::PopID();
}

/// ------------- CancelAction -------------
void CancelAction::initialAction()
{
    Action::initialAction();
    GW::GameThread::Enqueue([]{ GW::UI::Keypress(GW::UI::ControlAction_CancelAction); });
}

void CancelAction::drawSettings()
{
    ImGui::PushID(drawId());

    ImGui::Text("Cancel Action");

    ImGui::PopID();
}

/// ------------- DropBuffAction -------------
DropBuffAction::DropBuffAction(InputStream& stream)
{
    stream >> id;
}
void DropBuffAction::serialize(OutputStream& stream) const
{
    Action::serialize(stream);

    stream << id;
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
    ImGui::PushID(drawId());

    ImGui::Text("Drop buff");
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    drawSkillIDSelector(id);

    ImGui::PopID();
}

/// ------------- ConditionedAction -------------
ConditionedAction::ConditionedAction(InputStream& stream)
{
    std::string read;
    stream >> read;
    if (read == missingContentToken)
        cond = nullptr;
    else if (read == "C")
        cond = readCondition(stream);
    else
        return;

    while (stream >> read) {
        if (read == endOfListToken)
            break;
        else if (read == missingContentToken)
            actionsTrue.push_back(nullptr);
        else if (read == "A")
            actionsTrue.push_back(readAction(stream));
        else
            return;
    }
    while (stream >> read) {
        if (read == endOfListToken)
            break;
        else if (read == missingContentToken)
            actionsFalse.push_back(nullptr);
        else if (read == "A")
            actionsFalse.push_back(readAction(stream));
        else
            return;
    }
}
void ConditionedAction::serialize(OutputStream& stream) const
{
    Action::serialize(stream);

    if (cond)
        cond->serialize(stream);
    else
        stream << missingContentToken;

    for (const auto& action : actionsTrue) {
        if (action)
            action->serialize(stream);
        else
            stream << missingContentToken;
    }
    stream << endOfListToken;

    for (const auto& action : actionsFalse) {
        if (action)
            action->serialize(stream);
        else
            stream << missingContentToken;
    }
    stream << endOfListToken;
}
void ConditionedAction::initialAction()
{
    Action::initialAction();

    currentlyExecutedActions = {};

    if (cond && cond->check())
    {
        if (actionsTrue.empty()) return;
        currentlyExecutedActions = actionsTrue;
        if (const auto front = currentlyExecutedActions.front()) front->initialAction();
    }
    else
    {
        if (actionsFalse.empty()) return;
        currentlyExecutedActions = actionsFalse;
        if (const auto front = currentlyExecutedActions.front()) front->initialAction();
    }
}
ActionStatus ConditionedAction::isComplete() const
{
    if (currentlyExecutedActions.empty()) return ActionStatus::Complete;
    const auto first = currentlyExecutedActions.front();

    const auto finishFirstAction = [&] 
    {
        currentlyExecutedActions.erase(currentlyExecutedActions.begin());

        if (!currentlyExecutedActions.empty()) 
        {
            if (const auto second = currentlyExecutedActions.front()) second->initialAction();
        }
        return currentlyExecutedActions.empty() ? ActionStatus::Complete : ActionStatus::Running;
    };

    if (first)
    {
        const auto status = first->isComplete();
        switch (status)
        {
            case ActionStatus::Running:
                return ActionStatus::Running;
            case ActionStatus::Complete:
                return finishFirstAction();
            default:
                currentlyExecutedActions.clear();
                return ActionStatus::Error;
        }
    }
    else
    {
        return finishFirstAction();
    }
}
void ConditionedAction::drawSettings() 
{
    const auto drawActionsSelector = [](auto& actions) {
        ImGui::Indent(indent);

        int rowToDelete = -1;
        for (int i = 0; i < int(actions.size()); ++i) {
            ImGui::PushID(i);

            ImGui::Bullet();
            if (ImGui::Button("X")) {
                if (actions[i])
                    actions[i] = nullptr;
                else
                    rowToDelete = i;
            }

            ImGui::SameLine();
            if (actions[i])
                actions[i]->drawSettings();
            else
                actions[i] = drawActionSelector(100.f);

            ImGui::PopID();
        }
        if (rowToDelete != -1) actions.erase(actions.begin() + rowToDelete);

        ImGui::Bullet();
        if (ImGui::Button("Add row")) {
            actions.push_back(nullptr);
        }

        ImGui::Unindent(indent);
    };

    ImGui::PushID(drawId());

    if (cond)
        cond->drawSettings();
    else    
        cond = drawConditionSelector(120.f);

    ImGui::PushID(0);
    drawActionsSelector(actionsTrue);
    ImGui::PopID();

    if (actionsFalse.empty()) 
    {
        ImGui::SameLine();
        if (ImGui::Button("Add else")) {
            actionsFalse.push_back(nullptr);
        }
    }
    else
    {
        ImGui::Indent(indent);
        ImGui::Text("Else:");
        ImGui::Unindent(indent);

        ImGui::PushID(1);
        drawActionsSelector(actionsFalse);
        ImGui::PopID();
    }

    ImGui::PopID();
}

/// ------------- RepopMinipetAction -------------
RepopMinipetAction::RepopMinipetAction(InputStream& stream)
{
    stream >> itemModelId >> agentModelId;
}
void RepopMinipetAction::serialize(OutputStream& stream) const
{
    Action::serialize(stream);

    stream << itemModelId << agentModelId;
}
void RepopMinipetAction::initialAction()
{
    Action::initialAction();

    agentHasSpawned = false;
    hasUsedItem = false;

    GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::AgentAdd>(&hook,
        [&](GW::HookStatus*, const GW::Packet::StoC::AgentAdd* packet) {
            const auto agent = GW::Agents::GetAgentByID(packet->agent_id);
            if (!agent || !agent->GetIsLivingType()) return;
            if (agent->GetAsAgentLiving()->player_number == agentModelId) agentHasSpawned = true;
        });
}
void RepopMinipetAction::finalAction()
{
    Action::finalAction();
    GW::StoC::RemoveCallback<GW::Packet::StoC::AgentAdd>(&hook);
}

ActionStatus RepopMinipetAction::isComplete() const
{
    if (!hasUsedItem) {
        const auto& instanceInfo = InstanceInfo::getInstance();
        if (!instanceInfo.canPopAgent()) return ActionStatus::Running;

        const auto item = FindMatchingItem(itemModelId);
        if (!item) return ActionStatus::Error;
        const auto needsToUnpop = instanceInfo.hasMinipetPopped();
        GW::GameThread::Enqueue([needsToUnpop, item]() -> void {
            if (needsToUnpop) GW::Items::UseItem(item);
            GW::Items::UseItem(item);
        });
        hasUsedItem = true;
    }

    return (hasUsedItem && agentHasSpawned) ? ActionStatus::Complete : ActionStatus::Running;
}

void RepopMinipetAction::drawSettings()
{
    ImGui::PushID(drawId());

    ImGui::Text("(Unpop and) repop minipet as soon as its available:");
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    ImGui::InputInt("Item model ID", &itemModelId, 0);
    ImGui::SameLine();
    ImGui::InputInt("Agent model ID", &agentModelId, 0);

    ImGui::PopID();
}

/// ------------- PingHardModeAction -------------
void PingHardModeAction::initialAction()
{
    Action::initialAction();

    if (!GW::Effects::GetPlayerEffectBySkillId(GW::Constants::SkillID::Hard_mode)) return;

    const auto player = GW::Agents::GetPlayerAsAgentLiving();
    if (!player) return;

    GW::GameThread::Enqueue([pingId = player->agent_id]() {
        auto packet = GW::UI::UIPacket::kSendCallTarget{GW::CallTargetType::HardMode, pingId};
        GW::UI::SendUIMessage(GW::UI::UIMessage::kSendCallTarget, &packet);
    });
}
void PingHardModeAction::drawSettings()
{
    ImGui::PushID(drawId());

    ImGui::Text("Ping hard mode");

    ImGui::PopID();
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
    
    
    GW::GameThread::Enqueue([id = currentTarget->agent_id]() 
    {
        auto packet = GW::UI::UIPacket::kSendCallTarget{GW::CallTargetType::AttackingOrTargetting, id};
        GW::UI::SendUIMessage(GW::UI::UIMessage::kSendCallTarget, &packet);
    });
}
void PingTargetAction::drawSettings()
{
    ImGui::PushID(drawId());

    ImGui::Text("Ping current target");
    ImGui::SameLine();
    ImGui::Checkbox("Ping same agent only once per instance", &onlyOncePerInstance);

    ImGui::PopID();
}

/// ------------- AutoAttackTargetAction -------------
void AutoAttackTargetAction::initialAction()
{
    Action::initialAction();

    const auto currentTarget = GW::Agents::GetTargetAsAgentLiving();
    if (!currentTarget || currentTarget->allegiance != GW::Constants::Allegiance::Enemy) return;
    
    GW::GameThread::Enqueue([currentTarget]{ GW::Agents::InteractAgent(currentTarget); });
}
void AutoAttackTargetAction::drawSettings()
{
    ImGui::PushID(drawId());

    ImGui::Text("Auto-attack current target");

    ImGui::PopID();
}

/// ------------- ChangeWeaponSetAction -------------
ChangeWeaponSetAction::ChangeWeaponSetAction(InputStream& stream)
{
    stream >> id;
}
void ChangeWeaponSetAction::serialize(OutputStream& stream) const
{
    Action::serialize(stream);

    stream << id;
}
void ChangeWeaponSetAction::initialAction()
{
    Action::initialAction();

    GW::UI::Keypress((GW::UI::ControlAction)((uint32_t)GW::UI::ControlAction_ActivateWeaponSet1 + id - 1));
}

void ChangeWeaponSetAction::drawSettings()
{
    ImGui::PushID(drawId());

    ImGui::Text("Switch to weapon set:");
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    ImGui::InputInt("Slot", &id, 0);
    if (id < 1) id = 1;
    if (id > 4) id = 4;

    ImGui::PopID();
}

/// ------------- StoreTargetAction -------------
StoreTargetAction::StoreTargetAction(InputStream& stream)
{
    stream >> id;
}
void StoreTargetAction::serialize(OutputStream& stream) const
{
    Action::serialize(stream);

    stream << id;
}
void StoreTargetAction::initialAction()
{
    Action::initialAction();
    InstanceInfo::getInstance().storeTarget(GW::Agents::GetTargetAsAgentLiving(), id);
}

void StoreTargetAction::drawSettings()
{
    ImGui::PushID(drawId());

    ImGui::Text("Store current target");

    ImGui::PopID();
}

/// ------------- RestoreTargetAction -------------
RestoreTargetAction::RestoreTargetAction(InputStream& stream)
{
    stream >> id;
}
void RestoreTargetAction::serialize(OutputStream& stream) const
{
    Action::serialize(stream);

    stream << id;
}
void RestoreTargetAction::initialAction()
{
    Action::initialAction();
    const auto agent = InstanceInfo::getInstance().retrieveTarget(id);
    if (!agent) return;

    GW::GameThread::Enqueue([id = agent->agent_id]{ GW::Agents::ChangeTarget(id); });
}

void RestoreTargetAction::drawSettings()
{
    ImGui::PushID(drawId());

    ImGui::Text("Restore target");

    ImGui::PopID();
}

/// ------------- StopScriptAction -------------
void StopScriptAction::drawSettings()
{
    ImGui::PushID(drawId());

    ImGui::Text("Stop script");

    ImGui::PopID();
}
