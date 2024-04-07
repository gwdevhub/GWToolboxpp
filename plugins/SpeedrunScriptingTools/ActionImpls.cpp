#include <ActionImpls.h>
#include <utils.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/CtoSMgr.h>
#include <GWCA/Managers/EffectMgr.h>

#include <GWCA/Packets/Opcodes.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Skill.h>

#include "imgui.h"
#include <ImGuiCppWrapper.h>

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
    std::ostringstream& operator<<(std::ostringstream& stream, GW::Constants::SkillID id)
    {
        stream << (int)id;
        return stream;
    }
    constexpr double eps = 1e-3;
} // namespace

/// ------------- MoveToAction -------------
MoveToAction::MoveToAction(std::istringstream& stream)
{
    stream >> pos.x >> pos.y >> accuracy;
}
void MoveToAction::serialize(std::ostringstream& stream) const
{
    Action::serialize(stream);

    stream << pos.x << " " << pos.y << " " << accuracy << " ";
}
void MoveToAction::initialAction()
{
    Action::initialAction();

    GW::Agents::Move(pos);
}
bool MoveToAction::isComplete() const 
{
    const auto player = GW::Agents::GetPlayerAsAgentLiving();
    if (!player) return true;

    if (!player->GetIsMoving()) GW::Agents::Move(pos);

    return GW::GetDistance(player->pos, pos) < accuracy + eps;
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
}

/// ------------- CastOnSelfAction -------------
CastOnSelfAction::CastOnSelfAction(std::istringstream& stream)
{
    int read;
    stream >> read;
    id = (GW::Constants::SkillID)read;
}
void CastOnSelfAction::serialize(std::ostringstream& stream) const
{
    Action::serialize(stream);

    stream << id << " ";
}
void CastOnSelfAction::initialAction()
{
    Action::initialAction();

    const auto slot = GW::SkillbarMgr::GetSkillSlot(id);
    if (slot < 0) return;

    hasBegunCasting = false;
    GW::GameThread::Enqueue([slot]() -> void {
        GW::SkillbarMgr::UseSkill(slot, 0);
    });
}
bool CastOnSelfAction::isComplete() const
{
    const auto player = GW::Agents::GetPlayerAsAgentLiving();
    if (!player) return true;

    hasBegunCasting |= (static_cast<GW::Constants::SkillID>(player->skill) == id);

    return hasBegunCasting && static_cast<GW::Constants::SkillID>(player->skill) != id;
}
void CastOnSelfAction::drawSettings()
{
    ImGui::Text("Cast on self:");
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    ImGui::InputInt("Skill ID", reinterpret_cast<int*>(&id), 0);
}

/// ------------- CastOnTargetAction -------------
CastOnTargetAction::CastOnTargetAction(std::istringstream& stream)
{
    int read;
    stream >> read;
    id = (GW::Constants::SkillID)read;
}
void CastOnTargetAction::serialize(std::ostringstream& stream) const
{
    Action::serialize(stream);

    stream << id << " ";
}
void CastOnTargetAction::initialAction()
{
    Action::initialAction();

    const auto target = GW::Agents::GetTargetAsAgentLiving();
    if (!target) return;
    const auto slot = GW::SkillbarMgr::GetSkillSlot(id);
    if (slot < 0) return;

    hasBegunCasting = false;
    GW::GameThread::Enqueue([slot, targetId = target->agent_id]() -> void {
        GW::SkillbarMgr::UseSkill(slot, targetId);
    });
}
bool CastOnTargetAction::isComplete() const
{
    const auto player = GW::Agents::GetPlayerAsAgentLiving();
    if (!player) return true;

    hasBegunCasting |= (static_cast<GW::Constants::SkillID>(player->skill) == id);
    return hasBegunCasting && static_cast<GW::Constants::SkillID>(player->skill) != id;
}
void CastOnTargetAction::drawSettings()
{
    ImGui::Text("Cast on target:");
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    ImGui::InputInt("Skill ID", reinterpret_cast<int*>(&id), 0);
}

/// ------------- ChangeTargetAction -------------
ChangeTargetAction::ChangeTargetAction(std::istringstream& stream)
{
    stream >> id;
}
void ChangeTargetAction::serialize(std::ostringstream& stream) const
{
    Action::serialize(stream);

    stream << id << " ";
}
void ChangeTargetAction::initialAction()
{
    Action::initialAction();

    const auto agents = GW::Agents::GetAgentArray();
    if (!agents) return;

    for (const auto* agent : *agents) {
        if (!agent) continue;
        const auto living = agent->GetAsAgentLiving();
        if (!living) continue;
        if (living->player_number == id) {
            GW::Agents::ChangeTarget(living);
            return;
        }
    }
}
void ChangeTargetAction::drawSettings()
{
    ImGui::Text("Change target to agent with model ID:");
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    ImGui::InputInt("ID", &id, 0);
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
    GW::Items::UseItem(item);
}
void UseItemAction::drawSettings()
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

    GW::Agents::SendDialog(id);
}
void SendDialogAction::drawSettings()
{
    ImGui::Text("Send Dialog:");
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    ImGui::InputInt("ID", &id, 0);
}

/// ------------- GoToNpcAction -------------
GoToNpcAction::GoToNpcAction(std::istringstream& stream)
{
    stream >> id >> accuracy;
}
void GoToNpcAction::serialize(std::ostringstream& stream) const
{
    Action::serialize(stream);

    stream << id << " " << accuracy << " ";
}
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

    return GW::GetDistance(player->pos, npc->pos) < accuracy + eps;
}
void GoToNpcAction::drawSettings()
{
    ImGui::Text("Go to NPC:");
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    ImGui::InputInt("Model ID", &id, 0);
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
    int read;
    stream >> read;
    channel = (Channel)read;

    message = readStringWithSpaces(stream);
}
void SendChatAction::serialize(std::ostringstream& stream) const
{
    Action::serialize(stream);

    stream << (int)channel << " ";
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
    
    GW::Chat::SendChat(channelId, message.c_str());
}

void SendChatAction::drawSettings()
{
    ImGui::Text("Send Chat Message:");
    ImGui::SameLine();
    if (ImGui::Button(toString(channel).data(), ImVec2(100, 0))) {
        ImGui::OpenPopup("Pick channel");
    }
    if (ImGui::BeginPopup("Pick channel")) {
        for (auto i = 0; i <= (int)Channel::Emote; ++i) {
            if (ImGui::Selectable(toString((Channel)i).data())) {
                channel = (Channel)i;
            }
        }
        ImGui::EndPopup();
    }
    ImGui::PushItemWidth(300);
    ImGui::SameLine();
    ImGui::InputText("", &message);
}

/// ------------- CancelAction -------------
void CancelAction::initialAction()
{
    Action::initialAction();
    GW::CtoS::SendPacket(0x4, GAME_CMSG_CANCEL_MOVEMENT);
}

void CancelAction::drawSettings()
{
    ImGui::Text("Cancel Action");
}

/// ------------- DropBuffAction -------------
DropBuffAction::DropBuffAction(std::istringstream& stream)
{
    int read;
    stream >> read;
    id = (GW::Constants::SkillID)read;
}
void DropBuffAction::serialize(std::ostringstream& stream) const
{
    Action::serialize(stream);

    stream << (int)id << " ";
}
void DropBuffAction::initialAction()
{
    Action::initialAction();

    if (const auto buff = GW::Effects::GetPlayerBuffBySkillId(id)) 
    {
        GW::Effects::DropBuff(buff->buff_id);
    }
}
void DropBuffAction::drawSettings()
{
    ImGui::Text("Drop buff");
    ImGui::PushItemWidth(90);
    ImGui::SameLine();
    ImGui::InputInt("Skill ID", reinterpret_cast<int*>(&id), 0);
}
