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


// Get the agent we're currently tracking
uint32_t ObserverPlayerWindow::GetTracking() {
    if (!ObserverModule::Instance().IsActive()) return previously_tracked_agent_id;

	// keep tracking up-to-date with the current desired target
	const GW::Agent* agent = GW::Agents::GetPlayer();
    if (!agent) return previously_tracked_agent_id;

	const GW::AgentLiving* living = agent->GetAsAgentLiving();
    if (!living) return previously_tracked_agent_id;

	previously_tracked_agent_id = living->agent_id;

	return living->agent_id;
}

// Get the agent we're comparing to
uint32_t ObserverPlayerWindow::GetComparison() {
    if (!ObserverModule::Instance().IsActive()) return previously_compared_agent_id;

	// keep tracking up-to-date with the current desired target
	const GW::Agent* agent = GW::Agents::GetTarget();
    if (!agent) return previously_compared_agent_id;

	const GW::AgentLiving* living = agent->GetAsAgentLiving();
    if (!living) return previously_compared_agent_id;

	previously_compared_agent_id = living->agent_id;

	return living->agent_id;
}

// Draw the headers for player skills
void ObserverPlayerWindow::DrawSkillHeaders() {
	float offset = 0;
	ImGui::Text("#");
	ImGui::SameLine(offset += text_tiny);
	ImGui::Text("Name");
	// cancelled
	ImGui::SameLine(offset += text_long);
	ImGui::Text(ObserverLabel::Cancels);
	// interrupted
	ImGui::SameLine(offset += text_tiny);
	ImGui::Text(ObserverLabel::Interrupts);
	// finished
	ImGui::SameLine(offset += text_tiny);
	ImGui::Text(ObserverLabel::Finishes);
}

// Draw the skills of a player
void ObserverPlayerWindow::DrawSkills(const std::unordered_map<uint32_t, ObserverModule::ObservedSkill*>& skills,
    const std::vector<uint32_t>& skill_ids) {

    float offset = 0;
	size_t i = 0;
	for (uint32_t skill_id : skill_ids) {
		i += 1;
		offset = 0;
		ObserverModule::ObservableSkill* skill = ObserverModule::Instance().GetObservableSkillById(skill_id);

		ImGui::Text((std::to_string(i) + ".").c_str());
		ImGui::SameLine(offset += text_tiny);

		if (skill == nullptr) {
			ImGui::Text((std::string("? (") + std::to_string(skill_id) + ")").c_str());
			ImGui::SameLine(offset += text_long);
		} else {
			ImGui::Text(skill->Name().c_str());
			ImGui::SameLine(offset += text_long);
		}

		auto it_usages = skills.find(skill_id);
		if (it_usages == skills.end()) {
			ImGui::Text("?");
		} else {
			ImGui::Text(std::to_string(it_usages->second->stopped).c_str());
			ImGui::SameLine(offset += text_tiny);
			ImGui::Text(std::to_string(it_usages->second->interrupted).c_str());
			ImGui::SameLine(offset += text_tiny);
			ImGui::Text(std::to_string(it_usages->second->finished).c_str());
		}
	}
}


// Draw the window
void ObserverPlayerWindow::Draw(IDirect3DDevice9* pDevice) {
    UNREFERENCED_PARAMETER(pDevice);
    if (!visible) return;
    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 250), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags()))
        return ImGui::End();

    ObserverModule& observer_module = ObserverModule::Instance();
    const bool is_active = observer_module.IsActive();

	Prepare();
    uint32_t tracking_agent_id = GetTracking();
    uint32_t comparison_agent_id = GetComparison();

	ObserverModule::ObservableAgent* tracking = observer_module.GetObservableAgentById(tracking_agent_id);
	ObserverModule::ObservableAgent* compared = observer_module.GetObservableAgentById(comparison_agent_id);

    if (tracking_agent_id != NO_AGENT && !tracking) {
        ImGui::Text("???");
    } else if (!tracking) {
        ImGui::Text("No selection");
    } else {
        ImGui::Text(tracking->Name().c_str());

        float global = ImGui::GetIO().FontGlobalScale;
        text_long   = 220.0f * global;
        text_medium = 150.0f * global;
        text_short  = 80.0f  * global;
        text_tiny	= 40.0f  * global;

		if (show_tracking) {
			// skills
			ImGui::Separator();
			ImGui::Text("Skills:");
			DrawSkillHeaders();
			ImGui::Separator();
			DrawSkills(tracking->stats.skills_used, tracking->stats.skill_ids_used);
			ImGui::Text("");
        }


        if (show_comparison && compared) {
			// skills
			ImGui::Text((std::string("Skills used on: ") + compared->Name()).c_str());
			DrawSkillHeaders();
			ImGui::Separator();
            auto it_used_on_agent_skills = tracking->stats.skills_used_on_agent.find(compared->agent_id);
            auto it_used_on_agent_skill_ids = tracking->stats.skills_ids_used_on_agent.find(compared->agent_id);
            if ((it_used_on_agent_skills != tracking->stats.skills_used_on_agent.end()) &&
                (it_used_on_agent_skill_ids != tracking->stats.skills_ids_used_on_agent.end())) {
                DrawSkills(it_used_on_agent_skills->second, it_used_on_agent_skill_ids->second);
            }
        }
    }
    ImGui::End();
}

// Load settings
void ObserverPlayerWindow::LoadSettings(CSimpleIni* ini) {
	ToolboxWindow::LoadSettings(ini);

	show_tracking = ini->GetBoolValue(Name(), VAR_NAME(show_tracking), true);
    show_comparison = ini->GetBoolValue(Name(), VAR_NAME(show_comparison), true);
}


// Save settings
void ObserverPlayerWindow::SaveSettings(CSimpleIni* ini) {
	ToolboxWindow::SaveSettings(ini);

	ini->SetBoolValue(Name(), VAR_NAME(show_tracking), show_tracking);
    ini->SetBoolValue(Name(), VAR_NAME(show_comparison), show_comparison);
}

// Draw settings
void ObserverPlayerWindow::DrawSettingInternal() {
    ImGui::Text("Make sure the Observer Module is enabled.");
    ImGui::Checkbox("Show tracking player", &show_tracking);
    ImGui::Checkbox("Show player comparison", &show_comparison);
}
