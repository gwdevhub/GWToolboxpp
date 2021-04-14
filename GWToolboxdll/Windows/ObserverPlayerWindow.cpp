#include "stdafx.h"
#include <cstdlib>

#include <GWCA/Utilities/Macros.h>
#include <GWCA/Utilities/Scanner.h>

#include <GWCA/GameEntities/Agent.h>

#include <GWCA/Managers/AgentMgr.h>

#include <Logger.h>
#include <GuiUtils.h>

#include <Modules/ObserverModule.h>
#include <Windows/ObserverPlayerWindow.h>

#define NO_AGENT 0

void ObserverPlayerWindow::Initialize() {
    ToolboxWindow::Initialize();
}

void ObserverPlayerWindow::Draw(IDirect3DDevice9* pDevice) {
    UNREFERENCED_PARAMETER(pDevice);
    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 250), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags()))
        return ImGui::End();

    float offset = 0.0f;

    ObserverModule& observer_module = ObserverModule::Instance();

    const bool is_active = observer_module.IsActive();
    if (is_active) {
        const GW::Agent* player_agent = GW::Agents::GetPlayer();
        const GW::Agent* player_living = player_agent == nullptr ? nullptr : player_agent->GetAsAgentLiving();
        if (player_living != nullptr) last_player_id = player_living->agent_id;
    }
    const uint32_t use_agent_id = last_player_id;

    ObserverModule::ObservableAgent* player_observable = observer_module.GetObservableAgentByAgentId(use_agent_id);

    if (use_agent_id != NO_AGENT && player_observable == nullptr) {
        ImGui::Text("???");
    } else if (player_observable == nullptr) {
        ImGui::Text("No selection");
    } else if (player_observable != nullptr) {
        ImGui::Text(player_observable->Name().c_str());

        const float long_text	= 200.0f * ImGui::GetIO().FontGlobalScale;
        const float medium_text	= 150.0f * ImGui::GetIO().FontGlobalScale;
        const float short_text	= 80.0f * ImGui::GetIO().FontGlobalScale;
        const float mini_text	= 40.0f * ImGui::GetIO().FontGlobalScale;

        // TODO: show guild

        // offset = 0;
        // ImGui::Text("Me:");
        // ImGui::SameLine(offset += long_text);
        // ImGui::Text(std::to_string(GW::Agents::GetPlayerId()).c_str());

        // offset = 0;
        // ImGui::Text("Target:");
        // ImGui::SameLine(offset += long_text);
        // ImGui::Text(std::to_string(GW::Agents::GetTargetId()).c_str());

        // offset = 0;
        // ImGui::Text("Team:");
        // ImGui::SameLine(offset += long_text);
        // ImGui::Text(std::to_string(player_observable->team_id).c_str());

        // offset = 0;
        // ImGui::Text("KDR:");
        // ImGui::SameLine(offset += long_text);
        // ImGui::Text(player_observable->kdr_str.c_str());

        // offset = 0;
        // ImGui::Text("Critical hits:");
        // ImGui::SameLine(offset += long_text);
        // ImGui::Text(std::to_string(player_observable->total_crits_dealt).c_str());

        // offset = 0;
        // ImGui::Text("Critical hits received:");
        // ImGui::SameLine(offset += long_text);
        // ImGui::Text(std::to_string(player_observable->total_crits_received).c_str());

        // offset = 0;
        // ImGui::Text("knocked down:");
        // ImGui::SameLine(offset += long_text);
        // ImGui::Text(std::to_string(player_observable->knocked_down_count).c_str());
        // ImGui::SameLine(offset += short_text);
        // ImGui::Text("times");

        // offset = 0;
        // ImGui::Text("cancelled:");
        // ImGui::SameLine(offset += long_text);
        // ImGui::Text(std::to_string(player_observable->cancelled_count).c_str());
        // ImGui::SameLine(offset += short_text);
        // ImGui::Text("times");

        // offset = 0;
        // ImGui::Text("interrupted:");
        // ImGui::SameLine(offset += long_text);
        // ImGui::Text(std::to_string(player_observable->interrupted_count).c_str());
        // ImGui::SameLine(offset += short_text);
        // ImGui::Text("times");

        // attack stats

        // ImGui::Separator();

        // offset = 0;
        // ImGui::Text("");
        // ImGui::SameLine(offset += mini_text);
        // ImGui::Text("Name");
        // ImGui::SameLine(offset += long_text);
        // ImGui::Text("Atm");
        // ImGui::SameLine(offset += mini_text);
        // ImGui::Text("Cnl");
        // ImGui::SameLine(offset += mini_text);
        // ImGui::Text("Int");
        // ImGui::SameLine(offset += mini_text);
        // ImGui::Text("Fin");

        // ImGui::Separator();

        // offset = 0;
        // ImGui::Text("");
        // ImGui::SameLine(offset += mini_text);
        // ImGui::Text("+Attacks:");
        // ImGui::SameLine(offset += long_text);
        // ImGui::Text(std::to_string(player_observable->total_attacks_done.started).c_str());
        // ImGui::SameLine(offset += mini_text);
        // ImGui::Text(std::to_string(player_observable->total_attacks_done.stopped).c_str());
        // ImGui::SameLine(offset += mini_text);
        // ImGui::Text(std::to_string(player_observable->total_attacks_done.interrupted).c_str());
        // ImGui::SameLine(offset += mini_text);
        // ImGui::Text(std::to_string(player_observable->total_attacks_done.finished).c_str());

        // offset = 0;
        // ImGui::Text("");
        // ImGui::SameLine(offset += mini_text);
        // ImGui::Text("+Skills:");
        // ImGui::SameLine(offset += long_text);
        // ImGui::Text(std::to_string(player_observable->total_skills_used.started).c_str());
        // ImGui::SameLine(offset += mini_text);
        // ImGui::Text(std::to_string(player_observable->total_skills_used.stopped).c_str());
        // ImGui::SameLine(offset += mini_text);
        // ImGui::Text(std::to_string(player_observable->total_skills_used.interrupted).c_str());
        // ImGui::SameLine(offset += mini_text);
        // ImGui::Text(std::to_string(player_observable->total_skills_used.finished).c_str());

        // offset = 0;
        // ImGui::Text("");
        // ImGui::SameLine(offset += mini_text);
        // ImGui::Text("-Attacks");
        // ImGui::SameLine(offset += long_text);
        // ImGui::Text(std::to_string(player_observable->total_attacks_received.started).c_str());
        // ImGui::SameLine(offset += mini_text);
        // ImGui::Text(std::to_string(player_observable->total_attacks_received.stopped).c_str());
        // ImGui::SameLine(offset += mini_text);
        // ImGui::Text(std::to_string(player_observable->total_attacks_received.interrupted).c_str());
        // ImGui::SameLine(offset += mini_text);
        // ImGui::Text(std::to_string(player_observable->total_attacks_received.finished).c_str());

        // offset = 0;
        // ImGui::Text("");
        // ImGui::SameLine(offset += mini_text);
        // ImGui::Text("-Skills:");
        // ImGui::SameLine(offset += long_text);
        // ImGui::Text(std::to_string(player_observable->total_skills_received.started).c_str());
        // ImGui::SameLine(offset += mini_text);
        // ImGui::Text(std::to_string(player_observable->total_skills_received.stopped).c_str());
        // ImGui::SameLine(offset += mini_text);
        // ImGui::Text(std::to_string(player_observable->total_skills_received.interrupted).c_str());
        // ImGui::SameLine(offset += mini_text);
        // ImGui::Text(std::to_string(player_observable->total_skills_received.finished).c_str());

        // skills

        ImGui::Separator();

        offset = 0;
        ImGui::Text("#");
        ImGui::SameLine(offset += mini_text);
        ImGui::Text("Name");
        ImGui::SameLine(offset += long_text);
        ImGui::Text("Cnl");
        ImGui::SameLine(offset += mini_text);
        ImGui::Text("Int");
        ImGui::SameLine(offset += mini_text);
        ImGui::Text("Fin");

        ImGui::Separator();

        size_t i = 0;
        for (uint32_t skill_id : player_observable->skill_ids_used) {
            i += 1;
            offset = 0;
            // auto usage_it = skills_used_on_agent;
            ObserverModule::ObservableSkill* skill = observer_module.GetObservableSkillBySkillId(skill_id);

            ImGui::Text((std::to_string(i) + ".").c_str());
            ImGui::SameLine(offset += mini_text);

            if (skill == nullptr) {
                ImGui::Text((std::string("? (") + std::to_string(skill_id) + ")").c_str());
                ImGui::SameLine(offset += long_text);
            } else {
                ImGui::Text(skill->Name().c_str());
                ImGui::SameLine(offset += long_text);
            }

            auto it_usages = player_observable->skills_used.find(skill_id);
            if (it_usages == player_observable->skills_used.end()) {
                ImGui::Text("?");
            } else {
                // ImGui::Text(std::to_string(it_usages->second->started).c_str());
                // ImGui::SameLine(offset += mini_text);
                ImGui::Text(std::to_string(it_usages->second->stopped).c_str());
                ImGui::SameLine(offset += mini_text);
                ImGui::Text(std::to_string(it_usages->second->interrupted).c_str());
                ImGui::SameLine(offset += mini_text);
                ImGui::Text(std::to_string(it_usages->second->finished).c_str());
            }
        }
    }
    ImGui::End();
}

