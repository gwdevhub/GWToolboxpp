#include <InstanceInfo.h>

#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Player.h>
#include <GWCA/GameEntities/NPC.h>

#include <GWCA/Context/WorldContext.h>

#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <GWCA/Packets/StoC.h>
#include <GWCA/Constants/ItemIDs.h>

#include <enumUtils.h>

namespace {
    GW::HookEntry ObjectiveUpdateName_Entry;
    GW::HookEntry ObjectiveDone_Entry;
    GW::HookEntry InstanceLoadFile_Entry;
    GW::HookEntry UseItem_Entry;
    GW::HookEntry DisplayDialogue_Entry;

    bool isTargetableMiniPet(uint32_t itemId) 
    {
        switch (itemId) 
        {
            case GW::Constants::ItemID::GuildLord:
            case GW::Constants::ItemID::HighPriestZhang:
            case GW::Constants::ItemID::GhostlyPriest:
            case GW::Constants::ItemID::RiftWarden:
                return true;
            default:
                return false;
        }
    }

    void removeTextInAngledBrackets(std::string& str)
    {
        while (true) 
        {
            const auto start = str.find_first_of('<');
            const auto end = str.find_first_of('>',start);

            if (start == std::string::npos || end == std::string::npos) break;
            str.erase(start, end+1);
        }
    }
} // namespace

void InstanceInfo::initialize()
{
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::ObjectiveUpdateName>(&ObjectiveUpdateName_Entry, [this](GW::HookStatus*, const GW::Packet::StoC::ObjectiveUpdateName* packet) {
        this->objectiveStatus[packet->objective_id] = QuestStatus::Started;
    });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::ObjectiveDone>(&ObjectiveDone_Entry, [this](GW::HookStatus*, const GW::Packet::StoC::ObjectiveDone* packet) {
        this->objectiveStatus[packet->objective_id] = QuestStatus::Completed;
    });
    GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::InstanceLoadFile>(&InstanceLoadFile_Entry, [this](GW::HookStatus*, const GW::Packet::StoC::InstanceLoadFile*) {
        using namespace std::chrono_literals;

        this->objectiveStatus.clear();
        this->decodedAgentNames.clear();
        this->decodedItemNames.clear();
        this->storedTargets.clear();

        mpStatus.poppedMinipetId = std::nullopt;
        mpStatus.lastPop = std::chrono::steady_clock::now() - 1h;

        ++instanceId;
    });
    GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::DisplayDialogue>(&DisplayDialogue_Entry, [this](GW::HookStatus*, const GW::Packet::StoC::DisplayDialogue* packet) {
        if (wmemcmp(packet->message, L"\x8102\x5d41\xa992\xf927\x29f7", 5) == 0) 
        {
            // Dhuum quest does not send a `ObjectiveUpdateName` StoC
            this->objectiveStatus[157] = QuestStatus::Started;
        }
    });

    RegisterUIMessageCallback(&UseItem_Entry, GW::UI::UIMessage::kSendUseItem, 
        [&](GW::HookStatus*, GW::UI::UIMessage, void* wparam, void*){
            if (!wparam)return;

            const auto item = GW::Items::GetItemById((uint32_t)wparam);
            if (!item) return;

            const auto isGhostInTheBox = item->model_id == GW::Constants::ItemID::GhostInTheBox;
            if (!isGhostInTheBox && !isTargetableMiniPet(item->model_id)) return;

            if (mpStatus.poppedMinipetId && mpStatus.poppedMinipetId.value() == item->model_id) {
                mpStatus.poppedMinipetId = std::nullopt;
                return;
            }

            const auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - mpStatus.lastPop).count() < 10'010) 
            {
                return;
            }
            mpStatus.lastPop = now;

            if (!isGhostInTheBox) {
                mpStatus.poppedMinipetId = item->model_id;
            }
        }
    );
}

void InstanceInfo::terminate() 
{
    GW::StoC::RemovePostCallback<GW::Packet::StoC::ObjectiveUpdateName>(&ObjectiveUpdateName_Entry);
    GW::StoC::RemovePostCallback<GW::Packet::StoC::ObjectiveDone>(&ObjectiveDone_Entry);
    GW::StoC::RemovePostCallback<GW::Packet::StoC::InstanceLoadFile>(&InstanceLoadFile_Entry);
    RemoveUIMessageCallback(&UseItem_Entry, GW::UI::UIMessage::kSendUseItem);
}

QuestStatus InstanceInfo::getObjectiveStatus(uint32_t id)
{
    return objectiveStatus[id];
}
std::string InstanceInfo::getDecodedAgentName(GW::AgentID id)
{
    auto& wName = decodedAgentNames[id];

    if (wName.empty()) {
        const wchar_t* encodedName = nullptr;

        // Check in agent infos
        const auto& agentInfos = GW::GetWorldContext()->agent_infos;
        if (id >= agentInfos.size()) return "";
        encodedName = agentInfos[id].name_enc;

        // Check players
        if (!encodedName)
        {
            for (const auto& player : GW::GetWorldContext()->players)
            {
                if (player.agent_id == id) {
                    encodedName = player.name_enc;
                    break;
                }
            }
        }

        // Fall back to NPC name
        if (!encodedName) 
        {
            const auto agent = GW::Agents::GetAgentByID(id);
            if (!agent || !agent->GetIsLivingType()) return "";
            const auto npc = GW::Agents::GetNPCByID(agent->GetAsAgentLiving()->player_number);
            encodedName = npc->name_enc;
        }

        if (!encodedName) return "";
        GW::UI::AsyncDecodeStr(encodedName, &wName);
    }
    return WStringToString(wName);
}
std::string InstanceInfo::getDecodedItemName(uint32_t item_id)
{
    auto& wName = decodedItemNames[item_id];

    if (wName.empty()) {
        const wchar_t* encodedName = nullptr;

        // Check in agent infos
        const auto item = GW::Items::GetItemById(item_id);
        if (!item) return "";
        encodedName = item->single_item_name;

        if (!encodedName) return "";
        GW::UI::AsyncDecodeStr(encodedName, &wName);
    }

    auto decoded = WStringToString(wName);
    removeTextInAngledBrackets(decoded);
    return decoded;
}

bool InstanceInfo::canPopAgent() const 
{
    const auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - mpStatus.lastPop).count() >= 10'010;
}
bool InstanceInfo::hasMinipetPopped() const 
{
    return mpStatus.poppedMinipetId != std::nullopt;
}

void InstanceInfo::storeTarget(const GW::AgentLiving* agent, int storageId) 
{
    if (!agent)
    {
        auto it = storedTargets.find(storageId);
        if (it != storedTargets.end())
        {
            storedTargets.erase(it);
        }
        return;
    } 
    storedTargets[storageId] = agent->agent_id;
}
const GW::AgentLiving* InstanceInfo::retrieveTarget(int storageId) const
{
    const auto it = storedTargets.find(storageId);
    if (it == storedTargets.end()) return nullptr;
    const auto agent = GW::Agents::GetAgentByID(it->second);
    if (agent && agent->GetIsLivingType()) 
    {
        return agent->GetAsAgentLiving();
    }
    return nullptr;
}
