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
#include <GWCA/Managers/PartyMgr.h>

#include <GWCA/Packets/Opcodes.h>
#include <GWCA/Packets/StoC.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Skill.h>
#include <GWCA/GameEntities/Party.h>

#include <ImGuiCppWrapper.h>
#include <thread>

namespace {
    const std::string missingContentToken = "/";
    const std::string endOfListToken = ">";
    constexpr float indent = 25.f;
    constexpr double eps = 1e-3;

    GW::Item* FindMatchingItem(GW::Constants::Bag _bag_idx, uint32_t model_id, std::optional<uint32_t> modstruct = std::nullopt)
    {
        GW::Bag* bag = GW::Items::GetBag(_bag_idx);
        if (!bag) return nullptr;
        for (auto item : bag->items) {
            if (!item) continue;

            auto modStructFound = !modstruct.has_value();
            for (size_t i = 0; !modStructFound && i < item->mod_struct_size; i++)
                modStructFound = *modstruct == item->mod_struct[i].mod;
            
            if (modStructFound && item->model_id == model_id)
                return item;
        }
        return nullptr;
    }
    GW::Item* FindMatchingItem(uint32_t model_id, std::optional<uint32_t> modstruct = std::nullopt)
    {
        constexpr size_t bags_size = static_cast<size_t>(GW::Constants::Bag::Equipment_Pack) + 1;

        GW::Item* item = nullptr;
        for (size_t i = static_cast<size_t>(GW::Constants::Bag::Backpack); i < bags_size && !item; i++)
            item = FindMatchingItem(static_cast<GW::Constants::Bag>(i), model_id, modstruct);
        if (!item) 
            item = FindMatchingItem(GW::Constants::Bag::Equipped_Items, model_id, modstruct);
        return item;
    }
    std::optional<std::pair<GW::Constants::Bag, uint32_t>> findEmptyItemSlot()
    {
        using GW::Constants::Bag;
        for (const auto bagSlot : {Bag::Backpack, Bag::Belt_Pouch, Bag::Bag_1, Bag::Bag_2, Bag::Equipment_Pack})
        {
            const auto bag = GW::Items::GetBag(bagSlot);
            if (!bag) continue;

            for (uint32_t itemSlot = 0; itemSlot < bag->items.size(); ++itemSlot) 
            {
                if (!bag->items[itemSlot]) return std::pair{bagSlot, itemSlot};
            }
        }
        return std::nullopt;
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
        if (GW::GetSquareDistance(player->pos, agent->pos) < eps) return 0.f;
        constexpr auto radiansToDegree = 180.f / 3.141592741f;
        const auto angleBetweenNormalizedVectors = [](GW::Vec2f a, GW::Vec2f b) {
            return std::acos(a.x * b.x + a.y * b.y);
        };
        const auto forwards = GW::Normalize(GW::Vec2f{player->rotation_cos, player->rotation_sin});
        const auto toTarget = GW::Normalize(GW::Vec2f{agent->pos.x - player->pos.x, agent->pos.y - player->pos.y});
        return angleBetweenNormalizedVectors(forwards, toTarget) * radiansToDegree;
    }

    std::optional<std::pair<size_t, size_t>> getHeroWithSkill(GW::Constants::SkillID skillId, std::optional<GW::Constants::HeroID> heroId)
    {
        const auto heroSkillSlot = [&](GW::AgentID agentId) -> std::optional<size_t>
        {
            const auto skillBarArray = GW::SkillbarMgr::GetSkillbarArray();
            if (!skillBarArray) return {};
            const auto agentSkillBar = std::ranges::find_if(*skillBarArray, [&](const auto& skillbar){ return skillbar.agent_id == agentId; });
            if (agentSkillBar == skillBarArray->end()) return {};
            if (const auto skillBarSkill = agentSkillBar->GetSkillById(skillId); skillBarSkill && skillBarSkill->GetRecharge() == 0) 
                return skillBarSkill - &agentSkillBar->skills[0];
            return {};
        };

        const auto partyInfo = GW::PartyMgr::GetPartyInfo();
        if (!partyInfo) 
            return {};
        const auto& party_heros = partyInfo->heroes;
        if (!party_heros.valid()) 
            return {};

        const auto player = GW::Agents::GetPlayerAsAgentLiving();
        if (!player) 
            return {};
        
        for (auto heroIt = party_heros.begin(); heroIt != party_heros.end(); ++heroIt)
        {
            if (heroIt->owner_player_id != player->login_number || (heroId && heroIt->hero_id != heroId.value())) continue;
            if (const auto skillSlot = heroSkillSlot(heroIt->agent_id)) 
                return std::pair{heroIt - party_heros.begin(), *skillSlot};
        }
        return {};
    }
} // namespace

/// ------------- MoveToAction -------------
MoveToAction::MoveToAction()
{
    if (auto player = GW::Agents::GetPlayerAsAgentLiving()) 
    {
        pos = player->pos;
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
    stream >> agentType >> primary >> secondary >> alive >> skill >> sorting >> modelId >> minDistance >> maxDistance >> requireSameModelIdAsTarget >> preferNonHexed >> rotateThroughTargets;
    agentName = readStringWithSpaces(stream);
    polygon = readPositions(stream);
    stream >> minAngle >> maxAngle >> enchanted >> weaponspelled >> poisoned >> bleeding >> hexed;
}
void ChangeTargetAction::serialize(OutputStream& stream) const
{
    Action::serialize(stream);

    stream << agentType << primary << secondary << alive << skill << sorting << modelId << minDistance << maxDistance << requireSameModelIdAsTarget << preferNonHexed << rotateThroughTargets;
    writeStringWithSpaces(stream, agentName);
    writePositions(stream, polygon);
    stream << minAngle << maxAngle << enchanted << weaponspelled << poisoned << bleeding << hexed;
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
                case AgentType::PartyMember:
                    return true;
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
        const auto correctStatus = (alive == AnyNoYes::Any) || ((alive == AnyNoYes::Yes) == agent->GetIsAlive());
        const auto correctEnch = (enchanted == AnyNoYes::Any) || ((enchanted == AnyNoYes::Yes) == agent->GetIsEnchanted());
        const auto correctWeaponSpell = (weaponspelled == AnyNoYes::Any) || ((weaponspelled == AnyNoYes::Yes) == agent->GetIsWeaponSpelled());
        const auto correctBleed = (bleeding == AnyNoYes::Any) || ((bleeding == AnyNoYes::Yes) == agent->GetIsBleeding());
        const auto correctPoison = (poisoned == AnyNoYes::Any) || ((poisoned == AnyNoYes::Yes) == agent->GetIsPoisoned());
        const auto correctSkill = (skill == GW::Constants::SkillID::No_Skill) || (skill == (GW::Constants::SkillID)agent->skill);
        const auto correctModelId = (requireSameModelIdAsTarget && currentTarget) 
            ? (currentTarget->player_number == agent->player_number) 
            : ((modelId == 0) || (agent->player_number == modelId));
        const auto distance = GW::GetDistance(player->pos, agent->pos);
        const auto goodDistance = (minDistance - eps < distance) && (distance < maxDistance + eps);
        const auto goodName = (agentName.empty()) || (instanceInfo.getDecodedAgentName(agent->agent_id) == agentName);
        const auto goodPosition = (polygon.size() < 3u) || pointIsInsidePolygon(agent->pos, polygon);
        const auto goodHp = minHp <= 100.f * agent->hp && 100.f * agent->hp <= maxHp;
        const auto goodAngle = angleToAgent(player, agent) - eps < maxAngle;

        return correctType && correctPrimary && correctSecondary && correctStatus && correctEnch && correctWeaponSpell && correctBleed && correctPoison && correctSkill && 
                correctModelId && goodDistance && goodName && goodPosition && goodHp && goodAngle;
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
            case Sorting::ModelID:
                return agent->player_number < currentBestTarget->player_number;
            default:
                return false;
        }
    };

    const auto testAgent = [&](const GW::Agent* agent) 
    {
        if (!agent) return;

        const auto living = agent->GetAsAgentLiving();
        if (!fulfillsConditions(living)) return;
        if (isNewBest(living)) currentBestTarget = living;
    };



    if (agentType == AgentType::PartyMember) 
    {
        const auto info = GW::PartyMgr::GetPartyInfo();
        if (!info) return;

        for (const auto& partyMember : info->players)
            testAgent(GW::Agents::GetAgentByID(GW::Agents::GetAgentIdByLoginNumber(partyMember.login_number)));
        for (const auto& hero : info->heroes)
            testAgent(GW::Agents::GetAgentByID(GW::Agents::GetAgentIdByLoginNumber(hero.agent_id)));
        for (const auto& henchman : info->henchmen)
            testAgent(GW::Agents::GetAgentByID(GW::Agents::GetAgentIdByLoginNumber(henchman.agent_id)));
    }
    else 
    {
        for (const auto* agent : *agents) 
        {
            testAgent(agent);
            if (currentBestTarget && sorting == Sorting::AgentId && !rotateThroughTargets) break;
        }
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
        ImGui::BulletText("Allegiance");
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
            ImGui::PushID(0);
            drawSkillIDSelector(skill);
            ImGui::PopID();

            ImGui::BulletText("Name");
            ImGui::SameLine();
            ImGui::PushItemWidth(200.f);
            ImGui::InputText("###9", &agentName);

            ImGui::PushItemWidth(80.f);
            ImGui::Bullet();
            ImGui::Text("HP percent");
            ImGui::SameLine();
            ImGui::InputFloat("min###10", &minHp);
            ImGui::SameLine();
            ImGui::InputFloat("max###11", &maxHp);

            ImGui::Bullet();
            ImGui::Checkbox("Prefer enemies that are not hexed", &preferNonHexed);

            ImGui::Bullet();
            ImGui::Checkbox("Rotate through eligible targets", &rotateThroughTargets);

            ImGui::Bullet();
            ImGui::Checkbox("Require same model as current target", &requireSameModelIdAsTarget);

            ImGui::Bullet();
            ImGui::Text(requireSameModelIdAsTarget ? "If no target is selected: Model" : "Model");
            ImGui::SameLine();
            drawModelIDSelector(modelId, "id (0 for any)###12");

            ImGui::Bullet();
            ImGui::Text("Angle to player forward (degrees)");
            ImGui::SameLine();
            ImGui::InputFloat("min###13", &minAngle);
            ImGui::SameLine();
            ImGui::InputFloat("max###14", &maxAngle);
            if (minAngle < 0.f) minAngle = 0.f;
            if (minAngle > 180.f) minAngle = 180.f;
            if (maxAngle < 0.f) maxAngle = 0.f;
            if (maxAngle > 180.f) maxAngle = 180.f;

            ImGui::BulletText("Sort candidates by:");
            ImGui::SameLine();
            drawEnumButton(Sorting::AgentId, Sorting::ModelID, sorting, 15, 150.);

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
    const auto item = FindMatchingItem(id);
    const auto itemName = item ? InstanceInfo::getInstance().getDecodedItemName(item->item_id) : "";

    ImGui::PushID(drawId());

    ImGui::Text("Use item:");
    ImGui::SameLine();
    ImGui::Text("%s", itemName.c_str());
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    ImGui::InputInt("model ID", &id, 0);

    ImGui::PopID();
}

/// ------------- EquipItemAction -------------
EquipItemAction::EquipItemAction(InputStream& stream)
{
    stream >> id >> modstruct >> hasModstruct;
}
void EquipItemAction::serialize(OutputStream& stream) const
{
    Action::serialize(stream);

    stream << id << modstruct << hasModstruct;
}
void EquipItemAction::initialAction()
{
    Action::initialAction();

    const auto item = FindMatchingItem(id, hasModstruct ? std::optional{modstruct} : std::nullopt);
    if (!item) return;

    GW::GameThread::Enqueue([item] { SafeEquip(item); });
}
void EquipItemAction::drawSettings()
{
    const auto item = FindMatchingItem(id, hasModstruct ? std::optional{modstruct} : std::nullopt);
    const auto itemName = item ? InstanceInfo::getInstance().getDecodedItemName(item->item_id) : "";
    ImGui::PushID(drawId());

    ImGui::Text("Equip item:");
    ImGui::SameLine();
    ImGui::Text("%s", itemName.c_str());
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    ImGui::InputInt("model ID", &id, 0);
    ImGui::SameLine();
    if (hasModstruct)
    {
        ImGui::Text("0x");
        ImGui::SameLine();
        ImGui::InputInt("Modstruct", &modstruct, 0, 0, ImGuiInputTextFlags_CharsHexadecimal);
        ImGui::SameLine();
        if (ImGui::Button("X"))
        {
            hasModstruct = false;
            modstruct = 0;
        }
    }
    else 
    {
        if (ImGui::Button("Add modstruct")) 
        {
            hasModstruct = true;
        }
    }

    ImGui::PopID();
}

/// ------------- UnequipItemAction -------------
UnequipItemAction::UnequipItemAction(InputStream& stream)
{
    stream >> slot;
}
void UnequipItemAction::serialize(OutputStream& stream) const
{
    Action::serialize(stream);

    stream << slot;
}
void UnequipItemAction::initialAction()
{
    Action::initialAction();

    const auto equippedItemsBag = GW::Items::GetBag(GW::Constants::Bag::Equipped_Items);
    if (!equippedItemsBag || equippedItemsBag->items.size() < (uint32_t)slot + 1) return;
    const auto equippedItem = equippedItemsBag->items[(uint32_t)slot];

    const auto emptySlot = findEmptyItemSlot();
    if (!emptySlot) return;

    GW::GameThread::Enqueue([equippedItem, bag = emptySlot->first, itemSlot = emptySlot->second]{ GW::Items::MoveItem(equippedItem, bag, itemSlot); });
}
void UnequipItemAction::drawSettings()
{
    ImGui::PushID(drawId());

    ImGui::Text("Unequip item in slot");
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    drawEnumButton(EquippedItemSlot::Mainhand, EquippedItemSlot::Hands, slot);

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

    if (channel == Channel::Log) 
    {
        logMessage(message);
        return;
    }

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

    GW::GameThread::Enqueue([channelId, message = this->message] { GW::Chat::SendChat(channelId, message.c_str()); });
}

void SendChatAction::drawSettings()
{
    ImGui::PushID(drawId());

    ImGui::Text("Send Chat Message:");
    ImGui::SameLine();
    drawEnumButton(Channel::All, Channel::Log, channel);
    ImGui::PushItemWidth(300);
    ImGui::SameLine();
    ImGui::InputText("", &message);

    ImGui::PopID();
}

ActionBehaviourFlags SendChatAction::behaviour() const
{
    if (channel == Channel::Emote || channel == Channel::Log)
    {
        return ActionBehaviourFlag::ImmediateFinish | ActionBehaviourFlag::CanBeRunInOutpost;
    }
    return ActionBehaviourFlag::ImmediateFinish;
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
    const auto readCond = [&](auto& condition) 
    {
        stream >> read;
        if (read == missingContentToken)
            condition = nullptr;
        else if (read == "C")
            condition = readCondition(stream);
        else
            throw std::runtime_error("Invalid string");

        stream.proceedPastSeparator(2);
    };
    const auto readActionSequence = [&](auto& sequence) {
        size_t size;
        stream >> size;
        for (size_t i = 0; i < size; ++i) {
            if (stream.isAtSeparator() || !(stream >> read)) continue;

            if (read == missingContentToken)
                sequence.push_back(nullptr);
            else if (read == "A")
                sequence.push_back(readAction(stream));
            else
                throw std::runtime_error("Invalid string");

            stream.proceedPastSeparator(2);
        }
    };
    
    try 
    {
        readCond(cond);
        readActionSequence(actionsIf);
        readActionSequence(actionsElse);

        size_t actionsElseIfSize;
        stream >> actionsElseIfSize;

        for (size_t i = 0; i < actionsElseIfSize; ++i) {
            actionsElseIf.push_back({});
            readCond(actionsElseIf.back().first);
            readActionSequence(actionsElseIf.back().second);
        }
    }
    catch(...)
    {
    }
}
void ConditionedAction::serialize(OutputStream& stream) const
{
    Action::serialize(stream);

    const auto writeCondition = [&](const auto& condition) 
    {
        if (condition)
            condition->serialize(stream);
        else
            stream << missingContentToken;
        stream.writeSeparator(2);
    };
    const auto writeActionSequence = [&](const auto& sequence) 
    {
        stream << sequence.size();
        for (const auto& action : sequence) 
        {
            if (action)
                action->serialize(stream);
            else
                stream << missingContentToken;
            stream.writeSeparator(2);
        }
    };

    writeCondition(cond);
    writeActionSequence(actionsIf);
    writeActionSequence(actionsElse);

    stream << actionsElseIf.size();
    for (const auto& [condEI, actionsEI] : actionsElseIf) 
    {
        writeCondition(condEI);
        writeActionSequence(actionsEI);
    }
}
void ConditionedAction::initialAction()
{
    Action::initialAction();

    currentlyExecutedActions = {};

    bool foundActions = false;
    if (cond && cond->check())
    {
        foundActions = true;
        currentlyExecutedActions = actionsIf;
    }
    else
    {
        for (const auto& [condEI, actionsEI] : actionsElseIf)
        {
            if (condEI && condEI->check()) 
            {
                foundActions = true;
                currentlyExecutedActions = actionsEI;
                break;
            }
        }
    }
    if (!foundActions)
    {
        currentlyExecutedActions = actionsElse;
    }

    while (!currentlyExecutedActions.empty())
    {
        auto& front = currentlyExecutedActions.front();
        if (!front) 
            currentlyExecutedActions.erase(currentlyExecutedActions.begin(), currentlyExecutedActions.begin() + 1);
        else if (front->behaviour().test(ActionBehaviourFlag::ImmediateFinish))
        {
            front->initialAction();
            front->finalAction();
            currentlyExecutedActions.erase(currentlyExecutedActions.begin(), currentlyExecutedActions.begin() + 1);
        }
        else 
        {
            front->initialAction();
            break;
        }
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
    const auto drawActionsSelector = [](auto& actions) 
    {
        ImGui::Indent(indent);

        std::optional<int> rowToDelete;
        std::optional<std::pair<int, int>> rowsToSwap;

        for (int i = 0; i < int(actions.size()); ++i) 
        {
            ImGui::PushID(i);

            ImGui::Bullet();
            if (ImGui::Button("X", ImVec2(20, 0))) 
            {
                if (actions[i])
                    actions[i] = nullptr;
                else
                    rowToDelete = i;
            }
            ImGui::SameLine();
            if (ImGui::Button("^", ImVec2(20, 0)) && i > 0) 
                rowsToSwap = {i - 1, i};
            ImGui::SameLine();
            if (ImGui::Button("v", ImVec2(20, 0)) && i + 1 < int(actions.size())) rowsToSwap = {i, i + 1};

            ImGui::SameLine();
            if (actions[i])
                actions[i]->drawSettings();
            else
                actions[i] = drawActionSelector(100.f);

            ImGui::PopID();
        }
        if (rowToDelete) actions.erase(actions.begin() + *rowToDelete);
        if (rowsToSwap) std::swap(*(actions.begin() + rowsToSwap->first), *(actions.begin() + rowsToSwap->second));

        ImGui::Bullet();
        if (ImGui::Button("Add row")) 
        {
            actions.push_back(nullptr);
        }

        ImGui::Unindent(indent);
    };

    ImGui::PushID(drawId());

    ImGui::PushID(0);
    if (cond) 
    {
        if (ImGui::Button("Remove")) 
            cond = nullptr;
        else 
        {
            ImGui::SameLine();
            cond->drawSettings();
        }
    }
    else    
        cond = drawConditionSelector(120.f);
    drawActionsSelector(actionsIf);
    ImGui::PopID();

    ImGui::Indent(indent);

    int elseIfIndex = 0;
    std::optional<int> elseIfToDelete;
    std::optional<std::pair<int, int>> elseIfToSwap;

    for (auto& [condEI, actionsEI] : actionsElseIf) {
        ImGui::PushID(1 + elseIfIndex);
        if (ImGui::Button("X", ImVec2(20, 0))) elseIfToDelete = elseIfIndex;
        ImGui::SameLine();
        if (ImGui::Button("^", ImVec2(20, 0)) && elseIfIndex > 0) elseIfToSwap = {elseIfIndex - 1, elseIfIndex};
        ImGui::SameLine();
        if (ImGui::Button("v", ImVec2(20, 0)) && elseIfIndex + 1 < int(actionsElseIf.size())) elseIfToSwap = {elseIfIndex, elseIfIndex + 1};
        ImGui::SameLine();
        if (condEI) 
        {
            if (ImGui::Button("Remove")) 
                condEI = nullptr;
            else 
            {
                ImGui::SameLine();
                ImGui::Text("Else");
                ImGui::SameLine();
                condEI->drawSettings();
            }
        }
        else 
        {
            ImGui::Text("Else If");
            ImGui::SameLine();
            condEI = drawConditionSelector(120.f);
        }
        
        drawActionsSelector(actionsEI);
        ImGui::PopID();

        ++elseIfIndex;
    }
    if (elseIfToDelete) actionsElseIf.erase(actionsElseIf.begin() + elseIfToDelete.value());
    if (elseIfToSwap) std::swap(*(actionsElseIf.begin() + elseIfToSwap->first), *(actionsElseIf.begin() + elseIfToSwap->second));
    
    if (!actionsElse.empty())
    {
        ImGui::PushID(1 + actionsElseIf.size());
        if (ImGui::Button("X")) actionsElse.clear();
        ImGui::SameLine();
        ImGui::Text("Else");

        drawActionsSelector(actionsElse);
        ImGui::PopID();
    }

    if (ImGui::Button("Add else if"))
        actionsElseIf.push_back({nullptr, {}});
    
    if (actionsElse.empty())
    {
        ImGui::SameLine();
        if (ImGui::Button("Add else"))
            actionsElse.push_back(nullptr);
    }
    
    ImGui::Unindent(indent);

    ImGui::PopID();
}
ActionBehaviourFlags ConditionedAction::behaviour() const
{
    ActionBehaviourFlags flags = ActionBehaviourFlag::All;
    for (const auto& action : actionsIf) 
    {
        if (action) flags &= action->behaviour();
    }
    for (const auto& action : actionsElse) 
    {
        if (action) flags &= action->behaviour();
    }
    return flags;
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
    const auto item = FindMatchingItem(itemModelId);
    const auto itemName = item ? InstanceInfo::getInstance().getDecodedItemName(item->item_id) : "";

    ImGui::PushID(drawId());

    ImGui::Text("(Unpop and) repop minipet as soon as its available:");
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    ImGui::Text("%s", itemName.c_str());
    ImGui::SameLine();
    ImGui::InputInt("Item model ID", &itemModelId, 0);
    ImGui::SameLine();
    drawModelIDSelector(agentModelId, "Agent model ID");

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

/// ------------- LogOutAction -------------
void LogOutAction::initialAction()
{
    Action::initialAction();

    auto packet = GW::UI::UIPacket::kLogout{.unknown = 0, .character_select = 0};
    SendUIMessage(GW::UI::UIMessage::kLogout, &packet);
}

void LogOutAction::drawSettings()
{
    ImGui::PushID(drawId());

    ImGui::Text("Log out");

    ImGui::PopID();
}

/// ------------- UseHeroSkillAction -------------
UseHeroSkillAction::UseHeroSkillAction(InputStream& stream)
{
    stream >> hero >> skill;
}
void UseHeroSkillAction::serialize(OutputStream& stream) const
{
    Action::serialize(stream);

    stream << hero << skill;
}
void UseHeroSkillAction::initialAction()
{
    Action::initialAction();

    const auto heroAndSkillSlot = getHeroWithSkill(skill, hero != GW::Constants::HeroID::NoHero ? std::optional{hero} : std::nullopt);
    if (!heroAndSkillSlot) return;
    const auto& [heroSlot, skillSlot] = heroAndSkillSlot.value();

    const auto controlKey = [&] {
        switch (heroSlot)
        {
            case 0:
                return GW::UI::ControlAction_Hero1Skill1 + skillSlot;
            case 1:
                return GW::UI::ControlAction_Hero2Skill1 + skillSlot;
            case 2:
                return GW::UI::ControlAction_Hero3Skill1 + skillSlot;
            case 3:
                return GW::UI::ControlAction_Hero4Skill1 + skillSlot;
            case 4:
                return GW::UI::ControlAction_Hero5Skill1 + skillSlot;
            case 5:
                return GW::UI::ControlAction_Hero6Skill1 + skillSlot;
            default:
                return GW::UI::ControlAction_Hero7Skill1 + skillSlot;
        }
    }();

    GW::GameThread::Enqueue([controlKey = static_cast<GW::UI::ControlAction>(controlKey)] { GW::UI::Keypress(controlKey); });
}

void UseHeroSkillAction::drawSettings()
{
    ImGui::PushID(drawId());

    ImGui::Text("Use skill");
    ImGui::SameLine();
    drawSkillIDSelector(skill);
    ImGui::SameLine();
    ImGui::Text("on hero");
    ImGui::SameLine();
    drawEnumButton(GW::Constants::HeroID::NoHero, GW::Constants::HeroID::ZeiRi, hero);

    ImGui::PopID();
}

/// ------------- ClearTargetAction -------------
void ClearTargetAction::initialAction()
{
    Action::initialAction();
    GW::GameThread::Enqueue([] {
        GW::UI::Keypress(GW::UI::ControlAction_ClearTarget);
    });
}

void ClearTargetAction::drawSettings()
{
    ImGui::PushID(drawId());

    ImGui::Text("Clear Target");

    ImGui::PopID();
}

/// ------------- WaitUntilAction -------------
WaitUntilAction::WaitUntilAction(InputStream& stream)
{
    std::string read;
    stream >> read;
    if (read == missingContentToken)
        condition = nullptr;
    else if (read == "C")
        condition = readCondition(stream);
}
void WaitUntilAction::serialize(OutputStream& stream) const
{
    Action::serialize(stream);

    if (condition)
        condition->serialize(stream);
    else
        stream << missingContentToken;
}

void WaitUntilAction::drawSettings()
{
    ImGui::PushID(drawId());

    ImGui::Text("Wait until is fulfilled: ");
    ImGui::SameLine();
    if (condition)
        condition->drawSettings();
    else
        condition = drawConditionSelector(120.f);

    ImGui::PopID();
}
ActionStatus WaitUntilAction::isComplete() const 
{
    if (!condition || condition->check()) 
        return ActionStatus::Complete;
    return ActionStatus::Running;
}
