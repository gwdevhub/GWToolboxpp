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
#include <GWCA/Managers/MapMgr.h>

#include <GWCA/Packets/StoC.h>
#include <GWCA/Constants/ItemIDs.h>

#include <io.h>
#include <enumUtils.h>

namespace {
    GW::HookEntry InstanceLoadFile_Entry;
    GW::HookEntry UseItem_Entry;
    GW::HookEntry ManipulateMapObject_Entry;
    GW::HookEntry DungeonReward_Entry;
    GW::HookEntry CountdownStart_Entry;

    bool isTargetableMiniPet(uint32_t itemId) 
    {
        switch (itemId)
        {
            case GW::Constants::ModelID::Minipet::GuildLord:
            case GW::Constants::ModelID::Minipet::HighPriestZhang:
            case GW::Constants::ModelID::Minipet::GhostlyPriest:
            case GW::Constants::ModelID::Minipet::RiftWarden:
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
    GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::InstanceLoadFile>(&InstanceLoadFile_Entry, [this](GW::HookStatus*, const GW::Packet::StoC::InstanceLoadFile*) {
        using namespace std::chrono_literals;

        this->decodedAgentNames.clear();
        this->decodedItemNames.clear();
        this->storedTargets.clear();
        this->doorStatus.clear();
        instanceIsCompleted = false;

        mpStatus.poppedMinipetId = std::nullopt;
        mpStatus.lastPop = std::chrono::steady_clock::now() - 1h;

        ++instanceId;
    });
    
    GW::StoC::RegisterPacketCallback(&CountdownStart_Entry, GAME_SMSG_INSTANCE_COUNTDOWN, [this](GW::HookStatus*, GW::Packet::StoC::PacketBase*) {
        this->instanceIsCompleted = true;
    });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::DungeonReward>(&DungeonReward_Entry, [this](GW::HookStatus*, GW::Packet::StoC::DungeonReward*) {
        this->instanceIsCompleted = true;
    });

    RegisterUIMessageCallback(&UseItem_Entry, GW::UI::UIMessage::kSendUseItem, [&](GW::HookStatus*, GW::UI::UIMessage, void* wparam, void*) {
        if (!wparam) return;

        const auto item = GW::Items::GetItemById((uint32_t)wparam);
        if (!item) return;

        const auto isGhostInTheBox = item->model_id == GW::Constants::ItemID::GhostInTheBox;
        if (!isGhostInTheBox && !isTargetableMiniPet(item->model_id)) return;

        if (mpStatus.poppedMinipetId && mpStatus.poppedMinipetId.value() == item->model_id) {
            mpStatus.poppedMinipetId = std::nullopt;
            return;
        }

        const auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - mpStatus.lastPop).count() < 10'010) {
            return;
        }
        mpStatus.lastPop = now;

        if (!isGhostInTheBox) {
            mpStatus.poppedMinipetId = item->model_id;
        }
    });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::ManipulateMapObject>(&ManipulateMapObject_Entry, [this](GW::HookStatus*, const GW::Packet::StoC::ManipulateMapObject* packet) 
    {
        if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable || packet->animation_stage != 2) return;

        if (packet->animation_type == 16)
        {
            doorStatus[(DoorID)packet->object_id] = DoorStatus::Open;
        }
        else if (packet->animation_type == 9 || packet->animation_type == 3)
        {
            doorStatus[(DoorID)packet->object_id] = DoorStatus::Closed;
        }
    });
}

void InstanceInfo::terminate() 
{
    GW::StoC::RemovePostCallback<GW::Packet::StoC::InstanceLoadFile>(&InstanceLoadFile_Entry);
    // Fixed: was RemovePostCallback — wrong table for callbacks registered with RegisterPacketCallback; callbacks were never removed.
    GW::StoC::RemoveCallback<GW::Packet::StoC::DungeonReward>(&DungeonReward_Entry);
    GW::StoC::RemoveCallback<GW::Packet::StoC::ManipulateMapObject>(&ManipulateMapObject_Entry);
    GW::StoC::RemoveCallbacks(&CountdownStart_Entry); // Fixed: CountdownStart_Entry was registered but had no matching remove call; dangling callback after plugin unload.
    RemoveUIMessageCallback(&UseItem_Entry, GW::UI::UIMessage::kSendUseItem);
}

std::string InstanceInfo::getDecodedAgentName(GW::AgentID id)
{
    auto& wName = decodedAgentNames[id];

    if (wName.empty()) {
        const wchar_t* encodedName = nullptr;

        // Check in agent infos
        // Fixed: GetWorldContext() was called twice with no null check and its result used directly; crashes during map transitions.
        const auto* worldContext = GW::GetWorldContext();
        if (!worldContext) return "";
        const auto& agentInfos = worldContext->agent_infos;
        if (id >= agentInfos.size()) return "";
        encodedName = agentInfos[id].name_enc;

        // Check players
        if (!encodedName)
        {
            for (const auto& player : worldContext->players)
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
            if (!npc) return ""; // Fixed: GetNPCByID() can return null; was dereferenced unconditionally.
            encodedName = npc->name_enc;
        }

        if (!encodedName) return "";
        GW::UI::AsyncDecodeStr(
            encodedName,
            [](void* param, const wchar_t* s) {
                GWCA_ASSERT(param && s);
                std::wstring* str = (std::wstring*)param;
                *str = s;
            },
            &wName
        );
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
        GW::UI::AsyncDecodeStr(
            encodedName,
            [](void* param, const wchar_t* s) {
                GWCA_ASSERT(param && s);
                std::wstring* str = (std::wstring*)param;
                *str = s;
            },
            &wName
        );
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

bool InstanceInfo::isStoredTarget(const GW::AgentLiving& agent) const
{
    return std::ranges::any_of(storedTargets, [agent](const auto& element) {
        return element.second == agent.agent_id;
    });
}
