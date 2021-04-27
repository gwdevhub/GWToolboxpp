#include "stdafx.h"

#include <cstdlib>

#include <GWCA/Utilities/Macros.h>
#include <GWCA/Utilities/Scanner.h>

#include <GWCA/GameEntities/Agent.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ChatMgr.h>

#include <Logger.h>
#include <GuiUtils.h>

#include <Modules/Resources.h>
#include <Modules/ObserverModule.h>

#include <Windows/ObserverExportWindow.h>

#define NO_AGENT 0

void ObserverExportWindow::Initialize() {
    ToolboxWindow::Initialize();
}

// ObservableAgent to JSON
nlohmann::json ObserverExportWindow::ObservableAgentToJSON(ObserverModule::ObservableAgent& agent) {
    nlohmann::json json;
    json["name"] = agent.Name();
    json["party_id"] = agent.party_id;
    json["party_index"] = agent.party_index;
    json["primary"] = agent.primary;
    json["secondary"] = agent.secondary;
    json["profession"] = agent.profession;
    json["stats"] = ObservableAgentStatsToJSON(agent.stats);
    for (uint32_t skill_id : agent.stats.skill_ids_used) {
        ObserverModule::ObservableSkill* skill = ObserverModule::Instance().GetObservableSkillById(skill_id);
        if (!skill) {
            json["skills"].push_back(nlohmann::json::value_t::null);
            continue;
        }
        json["skills"].push_back(ObservableSkillToJSON(*skill));
    }
    return json;
}

// SharedStats to JSON
nlohmann::json ObserverExportWindow::SharedStatsToJSON(ObserverModule::SharedStats& stats) {
    nlohmann::json json;
    json["total_crits_received"] = stats.total_crits_received;
    json["total_crits_dealt"] = stats.total_crits_dealt;
    json["total_party_crits_received"] = stats.total_party_crits_received;
    json["total_party_crits_dealt"] = stats.total_party_crits_dealt;
    json["knocked_down_count"] = stats.knocked_down_count;
    json["interrupted_count"] = stats.interrupted_count;
    json["interrupted_skills_count"] = stats.interrupted_skills_count;
    json["cancelled_count"] = stats.cancelled_count;
    json["cancelled_skills_count"] = stats.cancelled_skills_count;
    json["knocked_down_duration"] = stats.knocked_down_duration;
    json["deaths"] = stats.deaths;
    json["kills"] = stats.kills;
    json["kdr_str"] = stats.kdr_str;
    return json;
}

// ObservableAgentStats to JSON
nlohmann::json ObserverExportWindow::ObservableAgentStatsToJSON(ObserverModule::ObservableAgentStats& stats) {
    nlohmann::json json;
    json.merge_patch(SharedStatsToJSON(stats));
    return json;
}

// ObservablePartyStats to JSON
nlohmann::json ObserverExportWindow::ObservablePartyStatsToJSON(ObserverModule::ObservablePartyStats& stats) {
    nlohmann::json json;
    json.merge_patch(SharedStatsToJSON(stats));
    return json;
}

// ObservableParty to JSON
nlohmann::json ObserverExportWindow::ObservablePartyToJSON(ObserverModule::ObservableParty& party) {
    nlohmann::json json;
    json["party_id"] = party.party_id;
    json["stats"] = ObservablePartyStatsToJSON(party.stats);
    for (uint32_t agent_id : party.agent_ids) {
        ObserverModule::ObservableAgent* agent = ObserverModule::Instance().GetObservableAgentById(agent_id);
        if (!agent) {
            json["members"].push_back(nlohmann::json::value_t::null);
            continue;
        }
        json["members"].push_back(ObservableAgentToJSON(*agent));
    }
    return json;
}

nlohmann::json ObserverExportWindow::ObservableSkillToJSON(ObserverModule::ObservableSkill& skill) {
    nlohmann::json json;
    json["name"] = skill.Name();
    return json;
}


// Convert to JSON
nlohmann::json ObserverExportWindow::ToJSON() {
    nlohmann::json json;
    const std::vector<ObserverModule::ObservableParty*>& parties = ObserverModule::Instance().GetObservablePartyArray();
    json["version"] = "0.0";
    for (ObserverModule::ObservableParty* party : parties) {
        if (!party) {
            json["parties"].push_back(nlohmann::json::value_t::null);
            continue;
        }
        json["parties"].push_back(ObservablePartyToJSON(*party));
    }

    return json;
}

// Export as JSON
void ObserverExportWindow::ExportToJSON() {
    std::wstring file_location = Resources::GetPath(L"observer.json");
    if (PathFileExistsW(file_location.c_str())) {
        DeleteFileW(file_location.c_str());
    }

    nlohmann::json json = ToJSON();
    std::ofstream out(file_location);
    out << json.dump();
    out.close();
    wchar_t file_location_wc[512];
    size_t msg_len = 0;
    const wchar_t* message = file_location.c_str();

    size_t max_len = _countof(file_location_wc) - 1;
    
    for (size_t i = 0; i < file_location.length(); i++) {
        // Break on the end of the message
        if (!message[i])
            break;
        // Double escape backsashes
        if (message[i] == '\\')
            file_location_wc[msg_len++] = message[i];
        if (msg_len >= max_len)
            break;
        file_location_wc[msg_len++] = message[i];
    }
    file_location_wc[msg_len] = 0;
    wchar_t chat_message[1024];
    swprintf(chat_message, _countof(chat_message), L"Match exported to <a=1>\x200C%s</a>", file_location_wc);
    GW::Chat::WriteChat(GW::Chat::CHANNEL_GLOBAL, chat_message);
}


// Draw the window
void ObserverExportWindow::Draw(IDirect3DDevice9* pDevice) {
    UNREFERENCED_PARAMETER(pDevice);
    if (!visible) return;
    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 250), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags()))
        return ImGui::End();

    ImGui::Text("Export Observer matches to JSON");

    if (ImGui::Button("Export to JSON"))
        ExportToJSON();

    ImGui::End();
}

// Load settings
void ObserverExportWindow::LoadSettings(CSimpleIni* ini) {
    ToolboxWindow::LoadSettings(ini);
}


// Save settings
void ObserverExportWindow::SaveSettings(CSimpleIni* ini) {
    ToolboxWindow::SaveSettings(ini);
}

// Draw settings
void ObserverExportWindow::DrawSettingInternal() {
    // No internal settings yet
}
