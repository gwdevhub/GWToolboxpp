#include "stdafx.h"
#include <cstdlib>

#include <GWCA/Utilities/Macros.h>
#include <GWCA/Utilities/Scanner.h>

#include <GWCA/GameEntities/Agent.h>

#include <GWCA/Managers/AgentMgr.h>

#include <Logger.h>
#include <GuiUtils.h>

#include <Modules/ObserverModule.h>
#include <Windows/ObserverTargetWindow.h>

#define NO_AGENT 0

void ObserverTargetWindow::Initialize() {
    ToolboxWindow::Initialize();
}

// Draw a row of skill headers
void ObserverTargetWindow::DrawSkillHeaders(float _long, float _mid, float _small, float _tiny) {
    UNREFERENCED_PARAMETER(_long);
    UNREFERENCED_PARAMETER(_mid);
    UNREFERENCED_PARAMETER(_small);
    UNREFERENCED_PARAMETER(_tiny);
	float offset = 0;
	ImGui::Text("#");
	ImGui::SameLine(offset += _tiny);
	ImGui::Text("Name");
	ImGui::SameLine(offset += _long);
	ImGui::Text("Cnl");
	ImGui::SameLine(offset += _tiny);
	ImGui::Text("Int");
	ImGui::SameLine(offset += _tiny);
	ImGui::Text("Fin");

}

// Draw a row of skills
void ObserverTargetWindow::DrawSkills(float _long, float _mid, float _small, float _tiny, const std::vector<uint32_t>& skill_ids,
    const std::unordered_map<uint32_t, ObserverModule::ObservedSkill*>& skills) {
    UNREFERENCED_PARAMETER(_long);
    UNREFERENCED_PARAMETER(_mid);
    UNREFERENCED_PARAMETER(_small);
    UNREFERENCED_PARAMETER(_tiny);
	size_t i = 0;
	for (uint32_t skill_id : skill_ids) {
		float offset = 0;
		i += 1;
		// auto usage_it = skills_used_on_agent;
		ObserverModule::ObservableSkill* skill = ObserverModule::Instance().GetObservableSkillBySkillId(skill_id);

		ImGui::Text((std::to_string(i) + ".").c_str());
		ImGui::SameLine(offset += _tiny);

		if (skill == nullptr) {
			ImGui::Text((std::string("? (") + std::to_string(skill_id) + ")").c_str());
			ImGui::SameLine(offset += _long);
		} else {
			ImGui::Text(skill->Name().c_str());
			ImGui::SameLine(offset += _long);
		}

		auto it_usages = skills.find(skill_id);
		if (it_usages == skills.end()) {
			ImGui::Text("?");
		} else {
			ImGui::Text(std::to_string(it_usages->second->stopped).c_str());
			ImGui::SameLine(offset += _tiny);
			ImGui::Text(std::to_string(it_usages->second->interrupted).c_str());
			ImGui::SameLine(offset += _tiny);
			ImGui::Text(std::to_string(it_usages->second->finished).c_str());
		}
	}
}

// Draw the window
void ObserverTargetWindow::Draw(IDirect3DDevice9* pDevice) {
    UNREFERENCED_PARAMETER(pDevice);
    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 250), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags()))
        return ImGui::End();

    ObserverModule& observer_module = ObserverModule::Instance();

    const bool is_active = observer_module.IsActive();
    if (is_active) {
        const GW::Agent* player_agent = GW::Agents::GetPlayer();
        const GW::Agent* player_living = player_agent == nullptr ? nullptr : player_agent->GetAsAgentLiving();
        if (player_living != nullptr) last_player_id = player_living->agent_id;

        const GW::Agent* target_agent = GW::Agents::GetTarget();
        const GW::Agent* target_living = target_agent == nullptr ? nullptr : target_agent->GetAsAgentLiving();
        if (target_living != nullptr) last_target_id = target_living->agent_id;
    }
    const uint32_t use_target_id = last_target_id;
    const uint32_t use_player_id = last_player_id;

    ObserverModule::ObservableAgent* player_observable = observer_module.GetObservableAgentByAgentId(use_player_id);
    ObserverModule::ObservableAgent* target_observable = observer_module.GetObservableAgentByAgentId(use_target_id);

    if ((use_player_id != NO_AGENT && player_observable == nullptr) || (use_target_id != NO_AGENT && target_observable == nullptr)) {
        ImGui::Text("???");
    } else if (player_observable == nullptr || target_observable == nullptr) {
        ImGui::Text("No selection");
    } else if (target_observable != nullptr) {
        // ImGui::Text(target_observable->Name().c_str());
        const float _long	= 200.0f * ImGui::GetIO().FontGlobalScale;
        const float _mid	= 150.0f * ImGui::GetIO().FontGlobalScale;
        const float _small	= 80.0f * ImGui::GetIO().FontGlobalScale;
        const float _tiny	= 40.0f * ImGui::GetIO().FontGlobalScale;

        ImGui::Separator();
        ImGui::Text((std::string("\"") +target_observable->Name() + "\" -> \""  + player_observable->Name() + "\"").c_str());
        DrawSkillHeaders(_long, _mid, _small, _tiny);

        auto it_on_player = target_observable->skills_used_on_agent.find(player_observable->agent_id);
        auto it_on_player_ids = target_observable->skills_ids_used_on_agent.find(player_observable->agent_id);

        if (it_on_player == target_observable->skills_used_on_agent.end()
            || it_on_player_ids == target_observable->skills_ids_used_on_agent.end()) {
            // target hasn't used any skills on player yet
			ImGui::Text("None");
        } else {
            // target has used skills on the player
            std::unordered_map<uint32_t, ObserverModule::ObservedSkill*>& skills_used = it_on_player->second;
            std::vector<uint32_t>& skill_ids_used = it_on_player_ids->second;
            DrawSkills(_long, _mid, _small, _tiny, skill_ids_used, skills_used);
        }

        if (target_observable->agent_id != player_observable->agent_id) {
            // target != player
			ImGui::Separator();
			ImGui::Text((std::string("\"") + player_observable->Name() + "\" -> \""  + target_observable->Name() + "\"").c_str());
			DrawSkillHeaders(_long, _mid, _small, _tiny);

			auto it_on_target = target_observable->skills_received_by_agent.find(player_observable->agent_id);
			auto it_on_target_ids = target_observable->skills_ids_received_by_agent.find(player_observable->agent_id);

			if (it_on_target == target_observable->skills_received_by_agent.end()
				|| it_on_target_ids == target_observable->skills_ids_received_by_agent.end()) {
				// player hasn't used any skills on target yet
				ImGui::Text("None");
			} else {
				// target has used skills on the player
				std::unordered_map<uint32_t, ObserverModule::ObservedSkill*>& skills_used = it_on_target->second;
				std::vector<uint32_t>& skill_ids_used = it_on_target_ids->second;
				DrawSkills(_long, _mid, _small, _tiny, skill_ids_used, skills_used);
			}
        }
    }
    ImGui::End();
}

