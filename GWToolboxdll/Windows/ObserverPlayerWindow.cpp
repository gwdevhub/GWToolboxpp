#include "stdafx.h"

#include <set>

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
    const GW::Agent* agent = GW::Agents::GetObservingAgent();
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
        offset_d = text_tiny;
    }
    // damage
    if (show_damage) {
        ImGui::SameLine(offset += offset_d);
        ImGui::Text("Damage");
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
    // damage
    if (show_damage) {
        ImGui::SameLine(offset += offset_d);
        ImGui::Text(std::to_string(action->total_damage).c_str());
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

        // Display health and energy information if available
        const GW::Agent* agent = GW::Agents::GetAgentByID(tracking_agent_id);
        if (agent) {
            const GW::AgentLiving* living = agent->GetAsAgentLiving();
            if (living) {
                // Get max HP (from cache or direct if observed)
                uint32_t max_hp = om.GetCachedMaxHP(tracking_agent_id);
                
                // Calculate current HP from percentage
                uint32_t cur_hp = static_cast<uint32_t>(living->hp * max_hp);
                
                ImGui::Text(("HP: "s + std::to_string(cur_hp) + " / " + std::to_string(max_hp)).c_str());
                
                // Get energy from cache (will be 0 if never observed)
                uint32_t cur_energy = om.GetCachedEnergy(tracking_agent_id);
                uint32_t max_energy = om.GetCachedMaxEnergy(tracking_agent_id);
                
                if (max_energy > 0) {
                    ImGui::Text(("Energy: "s + std::to_string(cur_energy) + " / " + std::to_string(max_energy)).c_str());
                }
            }
        }

        const float global = ImGui::FontScale();
        text_long = 220.0f * global;
        text_medium = 150.0f * global;
        text_short = 80.0f * global;
        text_tiny = 40.0f * global;

        // Display total damage dealt and received
        if (show_damage_details) {
            ImGui::Separator();
            ImGui::Text("Damage & Healing Summary:");
            ImGui::Text(("Total Damage Dealt: "s + std::to_string(tracking->stats.total_damage_dealt)).c_str());
            ImGui::Text(("Total Damage Received: "s + std::to_string(tracking->stats.total_damage_received)).c_str());
            ImGui::Text(("Total Healing Dealt: "s + std::to_string(tracking->stats.total_healing_dealt)).c_str());
            ImGui::Text(("Total Healing Received: "s + std::to_string(tracking->stats.total_healing_received)).c_str());
            
            // Create separate tables for allies and opponents
            if (!tracking->stats.damage_dealt_to_agents.empty() || 
                !tracking->stats.damage_received_from_agents.empty() ||
                !tracking->stats.healing_dealt_to_agents.empty() ||
                !tracking->stats.healing_received_from_agents.empty()) {
                
                // Collect and separate agents into allies and opponents
                std::set<uint32_t> ally_agent_ids;
                std::set<uint32_t> opponent_agent_ids;
                
                // Get tracking agent's party
                uint32_t tracking_party_id = tracking->party_id;
                
                // Collect all unique agent IDs and categorize them
                std::set<uint32_t> all_agent_ids;
                for (const auto& [agent_id, _] : tracking->stats.damage_dealt_to_agents) {
                    all_agent_ids.insert(agent_id);
                }
                for (const auto& [agent_id, _] : tracking->stats.damage_received_from_agents) {
                    all_agent_ids.insert(agent_id);
                }
                for (const auto& [agent_id, _] : tracking->stats.healing_dealt_to_agents) {
                    all_agent_ids.insert(agent_id);
                }
                for (const auto& [agent_id, _] : tracking->stats.healing_received_from_agents) {
                    all_agent_ids.insert(agent_id);
                }
                
                // Categorize agents
                for (const auto& agent_id : all_agent_ids) {
                    ObserverModule::ObservableAgent* categorized_agent = om.GetObservableAgentById(agent_id);
                    if (!categorized_agent) continue;
                    
                    if (categorized_agent->party_id == tracking_party_id) {
                        ally_agent_ids.insert(agent_id);
                    } else {
                        opponent_agent_ids.insert(agent_id);
                    }
                }
                
                // Draw Allies table (healing only)
                if (!ally_agent_ids.empty()) {
                    ImGui::Text("");
                    ImGui::Text("Allies:");
                    
                    float offset = 0;
                    ImGui::Text("Player");
                    ImGui::SameLine(offset += text_long);
                    ImGui::Text("Heal+");
                    ImGui::SameLine(offset += text_short);
                    ImGui::Text("Heal-");
                    ImGui::Separator();
                    
                    for (const auto& agent_id : ally_agent_ids) {
                        ObserverModule::ObservableAgent* ally_agent = om.GetObservableAgentById(agent_id);
                        if (!ally_agent) continue;
                        
                        offset = 0;
                        ImGui::Text(ally_agent->DisplayName().c_str());
                        ImGui::SameLine(offset += text_long);
                        
                        // Healing dealt
                        const auto it_heal_dealt = tracking->stats.healing_dealt_to_agents.find(agent_id);
                        if (it_heal_dealt != tracking->stats.healing_dealt_to_agents.end()) {
                            ImGui::Text(std::to_string(it_heal_dealt->second).c_str());
                        } else {
                            ImGui::Text("0");
                        }
                        ImGui::SameLine(offset += text_short);
                        
                        // Healing received
                        const auto it_heal_recv = tracking->stats.healing_received_from_agents.find(agent_id);
                        if (it_heal_recv != tracking->stats.healing_received_from_agents.end()) {
                            ImGui::Text(std::to_string(it_heal_recv->second).c_str());
                        } else {
                            ImGui::Text("0");
                        }
                    }
                }
                
                // Draw Opponents table (damage only)
                if (!opponent_agent_ids.empty()) {
                    ImGui::Text("");
                    ImGui::Text("Opponents:");
                    
                    float offset = 0;
                    ImGui::Text("Player");
                    ImGui::SameLine(offset += text_long);
                    ImGui::Text("Dmg+");
                    ImGui::SameLine(offset += text_short);
                    ImGui::Text("Dmg-");
                    ImGui::Separator();
                    
                    for (const auto& agent_id : opponent_agent_ids) {
                        ObserverModule::ObservableAgent* opponent_agent = om.GetObservableAgentById(agent_id);
                        if (!opponent_agent) continue;
                        
                        offset = 0;
                        ImGui::Text(opponent_agent->DisplayName().c_str());
                        ImGui::SameLine(offset += text_long);
                        
                        // Damage dealt
                        const auto it_dmg_dealt = tracking->stats.damage_dealt_to_agents.find(agent_id);
                        if (it_dmg_dealt != tracking->stats.damage_dealt_to_agents.end()) {
                            ImGui::Text(std::to_string(it_dmg_dealt->second).c_str());
                        } else {
                            ImGui::Text("0");
                        }
                        ImGui::SameLine(offset += text_short);
                        
                        // Damage received
                        const auto it_dmg_recv = tracking->stats.damage_received_from_agents.find(agent_id);
                        if (it_dmg_recv != tracking->stats.damage_received_from_agents.end()) {
                            ImGui::Text(std::to_string(it_dmg_recv->second).c_str());
                        } else {
                            ImGui::Text("0");
                        }
                    }
                }
            }
        }

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

            // Display damage and healing for this specific player
            if (show_damage_details) {
                ImGui::Text("");
                ImGui::Text(("Stats with "s + compared->DisplayName()).c_str());
                ImGui::Text(("  Damage dealt: " + std::to_string(tracking->stats.LazyGetDamageDealedAgainst(compared->agent_id))).c_str());
                ImGui::Text(("  Damage received: " + std::to_string(tracking->stats.LazyGetDamageReceivedFrom(compared->agent_id))).c_str());
                ImGui::Text(("  Healing dealt: " + std::to_string(tracking->stats.LazyGetHealingDealedTo(compared->agent_id))).c_str());
                ImGui::Text(("  Healing received: " + std::to_string(tracking->stats.LazyGetHealingReceivedFrom(compared->agent_id))).c_str());
            }
        }
    }

    ImGui::End();
}

// Load settings
void ObserverPlayerWindow::LoadSettings(ToolboxIni* ini)
{
    ToolboxWindow::LoadSettings(ini);

    show_tracking = ini->GetBoolValue(Name(), VAR_NAME(show_tracking), true);
    show_comparison = ini->GetBoolValue(Name(), VAR_NAME(show_comparison), true);
    show_skills_used_on_self = ini->GetBoolValue(Name(), VAR_NAME(show_skills_used_on_self), true);
    show_attempts = ini->GetBoolValue(Name(), VAR_NAME(show_attempts), false);
    show_cancels = ini->GetBoolValue(Name(), VAR_NAME(show_cancels), true);
    show_interrupts = ini->GetBoolValue(Name(), VAR_NAME(show_interrupts), true);
    show_finishes = ini->GetBoolValue(Name(), VAR_NAME(show_finishes), true);
    show_integrity = ini->GetBoolValue(Name(), VAR_NAME(show_integrity), false);
    show_damage = ini->GetBoolValue(Name(), VAR_NAME(show_damage), true);
    show_damage_details = ini->GetBoolValue(Name(), VAR_NAME(show_damage_details), true);
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
    SAVE_BOOL(show_damage);
    SAVE_BOOL(show_damage_details);
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
    ImGui::Checkbox("Show damage", &show_damage);
    ImGui::Checkbox("Show damage details", &show_damage_details);
}
