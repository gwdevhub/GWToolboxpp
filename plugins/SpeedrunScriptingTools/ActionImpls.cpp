#include <ActionImpls.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/GameEntities/Agent.h>

namespace {
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
}

/// ------------- MoveToAction -------------
void MoveToAction::initialAction()
{
    Action::initialAction();

    GW::Agents::Move(pos);
}
bool MoveToAction::isComplete() const {
    const auto player = GW::Agents::GetPlayerAsAgentLiving();
    if (!player) return true;

    if (!player->GetIsMoving()) GW::Agents::Move(pos);

    return GW::GetSquareDistance(player->pos, pos) < squareAccuracy;
}

/// ------------- CastOnSelfAction -------------
void CastOnSelfAction::initialAction()
{
    Action::initialAction();

    const auto slot = GW::SkillbarMgr::GetSkillSlot(id);
    if (slot < 0) return;
    GW::GameThread::Enqueue([slot]() -> void {
        GW::SkillbarMgr::UseSkill(slot, 0);
    });
}
bool CastOnSelfAction::isComplete() const
{
    const auto player = GW::Agents::GetPlayerAsAgentLiving();
    if (!player) return true;

    // Maybe need to wait a few frames before checking? 
    printf("iscomplete skill id %d", player->skill);
    return static_cast<GW::Constants::SkillID>(player->skill) != id;
}

/// ------------- CastOnTargetAction -------------
void CastOnTargetAction::initialAction()
{
    Action::initialAction();

    const auto target = GW::Agents::GetTargetAsAgentLiving();
    if (!target) return;
    const auto slot = GW::SkillbarMgr::GetSkillSlot(id);
    if (slot < 0) return;

    GW::GameThread::Enqueue([slot, targetId = target->agent_id]() -> void {
        GW::SkillbarMgr::UseSkill(slot, targetId);
    });
}
bool CastOnTargetAction::isComplete() const
{
    const auto player = GW::Agents::GetPlayerAsAgentLiving();
    if (!player) return true;

    printf("iscomplete skill id %d", player->skill);
    // Maybe need to wait a few frames before checking?
    return static_cast<GW::Constants::SkillID>(player->skill) != id;
}

/// ------------- UseItemAction -------------
void UseItemAction::initialAction()
{
    Action::initialAction();

    const auto item = FindMatchingItem(id);
    if (!item) return;
    GW::Items::UseItem(item);
}

/// ------------- SendDialogAction -------------
void SendDialogAction::initialAction()
{
    Action::initialAction();

    GW::Agents::SendDialog(id);
}

/// ------------- GoToNpcAction -------------
void GoToNpcAction::initialAction()
{
    Action::initialAction();
    
    npc = findAgentWithId(id);
    if (npc) {
        GW::Agents::GoNPC(npc);
    }
}
bool GoToNpcAction::isComplete() const
{
    if (!npc) return true;
    const auto player = GW::Agents::GetPlayerAsAgentLiving();
    if (!player) return true;

    return GW::GetSquareDistance(player->pos, npc->pos) < squareAccuracy;
}

/// ------------- WaitAction -------------
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

/// ------------- SendChatAction -------------
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
    
       GW::Chat::SendChat(channelId, message.data());
}
