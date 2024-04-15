#include <InstanceInfo.h>

#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <GWCA/Packets/StoC.h>
#include <GWCA/Constants/ItemIDs.h>

#include "windows.h"
#include <stringapiset.h>

namespace {
    GW::HookEntry ObjectiveUpdateName_Entry;
    GW::HookEntry ObjectiveDone_Entry;
    GW::HookEntry InstanceLoadFile_Entry;
    GW::HookEntry UseItem_Entry;

    std::string WStringToString(const std::wstring_view str)
    {
        if (str.empty()) {
            return "";
        }
        const auto size_needed = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, str.data(), static_cast<int>(str.size()), nullptr, 0, nullptr, nullptr);
        std::string str_to(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), str_to.data(), size_needed, NULL, NULL);
        return str_to;
    }

    bool isTargetableMiniPet(uint32_t itemId) {
        switch (itemId) {
            case GW::Constants::ItemID::GuildLord:
            case GW::Constants::ItemID::HighPriestZhang:
            case GW::Constants::ItemID::GhostlyPriest:
            case GW::Constants::ItemID::RiftWarden:
                return true;
            default:
                return false;
        }
    }
} // namespace

void InstanceInfo::initialize()
{
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::ObjectiveUpdateName>(&ObjectiveUpdateName_Entry, [this](GW::HookStatus*, const GW::Packet::StoC::ObjectiveUpdateName* packet) {
        this->questStatus[(GW::Constants::QuestID)packet->objective_id] = QuestStatus::Started;
    });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::ObjectiveDone>(&ObjectiveDone_Entry, [this](GW::HookStatus*, const GW::Packet::StoC::ObjectiveDone* packet) {
        this->questStatus[(GW::Constants::QuestID)packet->objective_id] = QuestStatus::Completed;
    });
    GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::InstanceLoadFile>(&InstanceLoadFile_Entry, [this](GW::HookStatus*, const GW::Packet::StoC::InstanceLoadFile*) {
        using namespace std::chrono_literals;

        this->questStatus.clear();
        this->decodedNames.clear();

        mpStatus.poppedMinipetId = std::nullopt;
        mpStatus.lastPop = std::chrono::steady_clock::now() - 1h;

        ++instanceId;
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

QuestStatus InstanceInfo::getQuestStatus(GW::Constants::QuestID id)
{
    return questStatus[id];
}
std::string InstanceInfo::getDecodedName(GW::AgentID id)
{
    auto& wName = decodedNames[id];

    if (wName.empty()) {
        const auto agent = GW::Agents::GetAgentByID(id);
        GW::Agents::AsyncGetAgentName(agent, wName);
    }
    return WStringToString(wName);
}

bool InstanceInfo::canPopAgent() const {
    const auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - mpStatus.lastPop).count() >= 10'010;
}
bool InstanceInfo::hasMinipetPopped() const {
    return mpStatus.poppedMinipetId != std::nullopt;
}
