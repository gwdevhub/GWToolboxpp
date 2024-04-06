#include <InstanceInfo.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Packets/StoC.h>

#include "windows.h"
#include <stringapiset.h>

namespace {
    GW::HookEntry ObjectiveUpdateName_Entry;
    GW::HookEntry ObjectiveDone_Entry;
    GW::HookEntry InstanceLoadFile_Entry;

   std::string WStringToString(const std::wstring_view str)
    {
        if (str.empty()) 
        {
            return "";
        }
        const auto size_needed = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, str.data(), static_cast<int>(str.size()), nullptr, 0, nullptr, nullptr);
        std::string str_to(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), str_to.data(), size_needed, NULL, NULL);
        return str_to;
    }
}
void InstanceInfo::initialize()
{
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::ObjectiveUpdateName>(&ObjectiveUpdateName_Entry, [this](GW::HookStatus*, const GW::Packet::StoC::ObjectiveUpdateName* packet) {
        this->questStatus[(GW::Constants::QuestID)packet->objective_id] = QuestStatus::Started;
    });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::ObjectiveDone>(&ObjectiveDone_Entry, [this](GW::HookStatus*, const GW::Packet::StoC::ObjectiveDone* packet) {
        this->questStatus[(GW::Constants::QuestID)packet->objective_id] = QuestStatus::Completed;
    });
    GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::InstanceLoadFile>(&InstanceLoadFile_Entry, [this](GW::HookStatus*, const GW::Packet::StoC::InstanceLoadFile*) {
        this->questStatus.clear();
        this->decodedNames.clear();
    });
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
