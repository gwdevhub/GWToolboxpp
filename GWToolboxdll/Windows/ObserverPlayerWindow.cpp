#include "stdafx.h"

#include <GWCA/GameEntities/Agent.h>

#include <GWCA/Managers/AgentMgr.h>

#include <Utils/GuiUtils.h>

#include <Modules/ObserverModule.h>
#include <Windows/ObserverPlayerWindow.h>

using namespace std::string_literals;

void ObserverPlayerWindow::Initialize()
{
    ToolboxWindow::Initialize();
}


// Get the agent we're currently tracking
uint32_t ObserverPlayerWindow::GetTracking()
{
    if (!ObserverModule::Instance().IsActive()) {
        return previously_tracked_agent_id;
    }

    // keep tracking up-to-date with the current desired target
    const GW::Agent* agent = GW::Agents::GetPlayer();
    if (!agent) {
        return previously_tracked_agent_id;
    }

    const GW::AgentLiving* living = agent->GetAsAgentLiving();
    if (!living) {
        return previously_tracked_agent_id;
    }

    previously_tracked_agent_id = living->agent_id;

    return living->agent_id;
}

// Get the agent we're comparing to
uint32_t ObserverPlayerWindow::GetComparison()
{
    if (!ObserverModule::Instance().IsActive()) {
        return previously_compared_agent_id;
    }

    // keep tracking up-to-date with the current desired target
    const GW::Agent* agent = GW::Agents::GetTarget();
    if (!agent) {
        return previously_compared_agent_id;
    }

    const GW::AgentLiving* living = agent->GetAsAgentLiving();
    if (!living) {
        return previously_compared_agent_id;
    }

    previously_compared_agent_id = living->agent_id;

    return living->agent_id;
}

// Draw the headers for player skills
void ObserverPlayerWindow::DrawHeaders() const
{
    float offset = 0;
    ImGui::Text("Name");
    float offset_d = text_long;
    // attempted
    if (show_attempts) {
        ImGui::SameLine(offset += offset_d);
        ImGui::Text(ObserverLabel::Attempts);
        offset_d = text_tiny;
    }
    // cancelled
    if (show_cancels) {
        ImGui::SameLine(offset += offset_d);
        ImGui::Text(ObserverLabel::Cancels);
        offset_d = text_tiny;
    }
    // interrupted
    if (show_interrupts) {
        ImGui::SameLine(offset += offset_d);
        ImGui::Text(ObserverLabel::Interrupts);
        offset_d = text_tiny;
    }
    // finished
    if (show_finishes) {
        ImGui::SameLine(offset += offset_d);
        ImGui::Text(ObserverLabel::Finishes);
        offset_d = text_tiny;
    }
    // integrity
    if (show_integrity) {
        ImGui::SameLine(offset += offset_d);
        ImGui::Text(ObserverLabel::Integrity);
    }
}

void ObserverPlayerWindow::DrawAction(const std::string& name, const ObserverModule::ObservedAction* action) const
{
    float offset = 0;
    ImGui::Text(name.c_str());
    float offset_d = text_long;
    // attempted
    if (show_attempts) {
        ImGui::SameLine(offset += offset_d);
        ImGui::Text(std::to_string(action->started).c_str());
        offset_d = text_tiny;
    }
    // cancelled
    if (show_cancels) {
        ImGui::SameLine(offset += offset_d);
        ImGui::Text(std::to_string(action->stopped).c_str());
        offset_d = text_tiny;
    }
    // interrupted
    if (show_interrupts) {
        ImGui::SameLine(offset += offset_d);
        ImGui::Text(std::to_string(action->interrupted).c_str());
        offset_d = text_tiny;
    }
    // finished
    if (show_finishes) {
        ImGui::SameLine(offset += offset_d);
        ImGui::Text(std::to_string(action->finished).c_str());
        offset_d = text_tiny;
    }
    // integrity
    if (show_integrity) {
        ImGui::SameLine(offset += offset_d);
        ImGui::Text(std::to_string(action->integrity).c_str());
        offset_d = text_tiny;
    }
}

// Draw the skills of a player
void ObserverPlayerWindow::DrawSkills(const std::unordered_map<GW::Constants::SkillID, ObserverModule::ObservedSkill*>& skills,
                                      const std::vector<GW::Constants::SkillID>& skill_ids) const
{
    auto i = 0u;
    for (auto skill_id : skill_ids) {
        i += 1;
        ObserverModule::ObservableSkill* skill = ObserverModule::Instance().GetObservableSkillById(skill_id);
        if (!skill) {
            continue;
        }
        auto it_usages = skills.find(skill_id);
        if (it_usages == skills.end()) {
            continue;
        }
        DrawAction(("# " + std::to_string(i) + ". " + skill->Name()).c_str(), it_usages->second);
    }
}


// Draw the window
void ObserverPlayerWindow::Draw(IDirect3DDevice9*)
{
    if (!visible) {
        return;
    }
    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 250), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        return ImGui::End();
    }

    ObserverModule& om = ObserverModule::Instance();

    Prepare();
    const uint32_t tracking_agent_id = GetTracking();
    const uint32_t comparison_agent_id = GetComparison();

    ObserverModule::ObservableAgent* tracking = om.GetObservableAgentById(tracking_agent_id);
    ObserverModule::ObservableAgent* compared = om.GetObservableAgentById(comparison_agent_id);

    if (tracking) {
        ImGui::Text(tracking->DisplayName().c_str());

        const float global = ImGui::GetIO().FontGlobalScale;
        text_long = 220.0f * global;
        text_medium = 150.0f * global;
        text_short = 80.0f * global;
        text_tiny = 40.0f * global;

        if (show_tracking) {
            // skills
            ImGui::Separator();
            ImGui::Text("Skills:");
            DrawHeaders();
            ImGui::Separator();
            DrawSkills(tracking->stats.skills_used, tracking->stats.skill_ids_used);
        }

        if (show_comparison && compared && !(!show_skills_used_on_self && tracking && compared->agent_id == tracking->agent_id)) {
            // skills
            ImGui::Text(""); // new line
            ImGui::Text(("Skills used on: "s + compared->DisplayName()).c_str());
            DrawHeaders();
            ImGui::Separator();
            const auto it_used_on_agent_skills = tracking->stats.skills_used_on_agents.find(compared->agent_id);
            const auto it_used_on_agent_skill_ids = tracking->stats.skill_ids_used_on_agents.find(compared->agent_id);
            if (it_used_on_agent_skills != tracking->stats.skills_used_on_agents.end() &&
                it_used_on_agent_skill_ids != tracking->stats.skill_ids_used_on_agents.end()) {
                DrawSkills(it_used_on_agent_skills->second, it_used_on_agent_skill_ids->second);
            }
        }
    }

    ImGui::End();
}

// Load settings
void ObserverPlayerWindow::LoadSettings(ToolboxIni* ini)
{
    ToolboxWindow::LoadSettings(ini);



    show_comparison = ini->GetBoolValue(Name(), VAR_NAME(show_skills_used_on_self), true);
    show_comparison = ini->GetBoolValue(Name(), VAR_NAME(show_attempts), false);
    show_comparison = ini->GetBoolValue(Name(), VAR_NAME(show_cancels), true);
    show_comparison = ini->GetBoolValue(Name(), VAR_NAME(show_interrupts), true);
    show_comparison = ini->GetBoolValue(Name(), VAR_NAME(show_finishes), true);
    show_comparison = ini->GetBoolValue(Name(), VAR_NAME(show_integrity), false);
}


// Save settings
void ObserverPlayerWindow::SaveSettings(ToolboxIni* ini)
{
    ToolboxWindow::SaveSettings(ini);

    SAVE_BOOL(show_tracking);
    SAVE_BOOL(show_comparison);
    SAVE_BOOL(show_skills_used_on_self);
    SAVE_BOOL(show_attempts);
    SAVE_BOOL(show_cancels);
    SAVE_BOOL(show_interrupts);
    SAVE_BOOL(show_finishes);
    SAVE_BOOL(show_integrity);
}

// Draw settings
void ObserverPlayerWindow::DrawSettingsInternal()
{
    ImGui::Text("Make sure the Observer Module is enabled.");
    ImGui::Checkbox("Show tracking player", &show_tracking);
    ImGui::Checkbox("Show player comparison", &show_comparison);
    ImGui::Checkbox("Show skills used on self", &show_skills_used_on_self);
    ImGui::Checkbox(("Show attempts ("s + ObserverLabel::Attempts + ")").c_str(), &show_attempts);
    ImGui::Checkbox(("Show cancels ("s + ObserverLabel::Cancels + ")").c_str(), &show_cancels);
    ImGui::Checkbox(("Show interrupts ("s + ObserverLabel::Interrupts + ")").c_str(), &show_interrupts);
    ImGui::Checkbox(("Show finishes ("s + ObserverLabel::Finishes + ")").c_str(), &show_finishes);
    ImGui::Checkbox(("Show integrity ("s + ObserverLabel::Integrity + ")").c_str(), &show_integrity);
}
