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
    SettingsRegistry::Register(this, settings);
}

void ObserverPlayerWindow::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxWindow::LoadSettings(doc, legacy);
    doc.GetStruct(Name(), settings);
}

void ObserverPlayerWindow::SaveSettings(SettingsDoc& doc)
{
    ToolboxWindow::SaveSettings(doc);
    doc.SetStruct(Name(), settings);
}


// 获取当前正在追踪的成员
uint32_t ObserverPlayerWindow::GetTracking()
{
    if (!ObserverModule::Instance().IsActive()) {
        return previously_tracked_agent_id;
    }

    // 保持追踪与当前期望目标同步
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

// 获取用于比较的成员
uint32_t ObserverPlayerWindow::GetComparison()
{
    if (!ObserverModule::Instance().IsActive()) {
        return previously_compared_agent_id;
    }

    // 保持比较与当前期望目标同步
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

// 绘制玩家技能的列头
void ObserverPlayerWindow::DrawHeaders() const
{
    float offset = 0;
    ImGui::Text("技能名称");
    float offset_d = text_long;
    // 尝试
    if (settings.show_attempts) {
        ImGui::SameLine(offset += offset_d);
        ImGui::Text(ObserverLabel::Attempts);
        offset_d = text_tiny;
    }
    // 取消
    if (settings.show_cancels) {
        ImGui::SameLine(offset += offset_d);
        ImGui::Text(ObserverLabel::Cancels);
        offset_d = text_tiny;
    }
    // 打断
    if (settings.show_interrupts) {
        ImGui::SameLine(offset += offset_d);
        ImGui::Text(ObserverLabel::Interrupts);
        offset_d = text_tiny;
    }
    // 完成
    if (settings.show_finishes) {
        ImGui::SameLine(offset += offset_d);
        ImGui::Text(ObserverLabel::Finishes);
        offset_d = text_tiny;
    }
    // 完整度
    if (settings.show_integrity) {
        ImGui::SameLine(offset += offset_d);
        ImGui::Text(ObserverLabel::Integrity);
        offset_d = text_tiny;
    }
    // 伤害
    if (settings.show_damage) {
        ImGui::SameLine(offset += offset_d);
        ImGui::Text("伤害");
    }
}

void ObserverPlayerWindow::DrawAction(const std::string& name, const ObserverModule::ObservedAction* action) const
{
    float offset = 0;
    ImGui::Text(name.c_str());
    float offset_d = text_long;
    // 尝试
    if (settings.show_attempts) {
        ImGui::SameLine(offset += offset_d);
        ImGui::Text(std::to_string(action->started).c_str());
        offset_d = text_tiny;
    }
    // 取消
    if (settings.show_cancels) {
        ImGui::SameLine(offset += offset_d);
        ImGui::Text(std::to_string(action->stopped).c_str());
        offset_d = text_tiny;
    }
    // 打断
    if (settings.show_interrupts) {
        ImGui::SameLine(offset += offset_d);
        ImGui::Text(std::to_string(action->interrupted).c_str());
        offset_d = text_tiny;
    }
    // 完成
    if (settings.show_finishes) {
        ImGui::SameLine(offset += offset_d);
        ImGui::Text(std::to_string(action->finished).c_str());
        offset_d = text_tiny;
    }
    // 完整度
    if (settings.show_integrity) {
        ImGui::SameLine(offset += offset_d);
        ImGui::Text(std::to_string(action->integrity).c_str());
        offset_d = text_tiny;
    }
    // 伤害
    if (settings.show_damage) {
        ImGui::SameLine(offset += offset_d);
        ImGui::Text(std::to_string(action->total_damage).c_str());
    }
}

// 绘制玩家的技能
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


// 绘制窗口
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

        // 如果可用，显示生命值和能量信息
        const GW::Agent* agent = GW::Agents::GetAgentByID(tracking_agent_id);
        if (agent) {
            const GW::AgentLiving* living = agent->GetAsAgentLiving();
            if (living) {
                // 获取最大生命值（从缓存或直接观测）
                uint32_t max_hp = om.GetCachedMaxHP(tracking_agent_id);
                
                // 根据百分比计算当前生命值
                uint32_t cur_hp = static_cast<uint32_t>(living->hp * max_hp);
                
                ImGui::Text(("生命值: "s + std::to_string(cur_hp) + " / " + std::to_string(max_hp)).c_str());
                
                // 从缓存获取能量（如果从未观测到则为 0）
                uint32_t cur_energy = om.GetCachedEnergy(tracking_agent_id);
                uint32_t max_energy = om.GetCachedMaxEnergy(tracking_agent_id);
                
                if (max_energy > 0) {
                    ImGui::Text(("能量: "s + std::to_string(cur_energy) + " / " + std::to_string(max_energy)).c_str());
                }
            }
        }

        const float global = ImGui::FontScale();
        text_long = 220.0f * global;
        text_medium = 150.0f * global;
        text_short = 80.0f * global;
        text_tiny = 40.0f * global;

        // 显示总造成伤害和受到伤害
        if (settings.show_damage_details) {
            ImGui::Separator();
            ImGui::Text("伤害与治疗摘要：");
            ImGui::Text(("总造成伤害: "s + std::to_string(tracking->stats.total_damage_dealt)).c_str());
            ImGui::Text(("总受到伤害: "s + std::to_string(tracking->stats.total_damage_received)).c_str());
            ImGui::Text(("总造成治疗: "s + std::to_string(tracking->stats.total_healing_dealt)).c_str());
            ImGui::Text(("总受到治疗: "s + std::to_string(tracking->stats.total_healing_received)).c_str());
            
            // 为盟友和对手创建独立的表格
            if (!tracking->stats.damage_dealt_to_agents.empty() || 
                !tracking->stats.damage_received_from_agents.empty() ||
                !tracking->stats.healing_dealt_to_agents.empty() ||
                !tracking->stats.healing_received_from_agents.empty()) {
                
                // 收集并将成员区分为盟友和对手
                std::set<uint32_t> ally_agent_ids;
                std::set<uint32_t> opponent_agent_ids;
                
                // 获取追踪成员的队伍
                uint32_t tracking_party_id = tracking->party_id;
                
                // 收集所有唯一的成员 ID 并进行分类
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
                
                // 对成员进行分类
                for (const auto& agent_id : all_agent_ids) {
                    ObserverModule::ObservableAgent* categorized_agent = om.GetObservableAgentById(agent_id);
                    if (!categorized_agent) continue;
                    
                    if (categorized_agent->party_id == tracking_party_id) {
                        ally_agent_ids.insert(agent_id);
                    } else {
                        opponent_agent_ids.insert(agent_id);
                    }
                }
                
                // 绘制盟友表格（仅治疗）
                if (!ally_agent_ids.empty()) {
                    ImGui::Text("");
                    ImGui::Text("盟友：");
                    
                    float offset = 0;
                    ImGui::Text("玩家");
                    ImGui::SameLine(offset += text_long);
                    ImGui::Text("治疗+");
                    ImGui::SameLine(offset += text_short);
                    ImGui::Text("治疗-");
                    ImGui::Separator();
                    
                    for (const auto& agent_id : ally_agent_ids) {
                        ObserverModule::ObservableAgent* ally_agent = om.GetObservableAgentById(agent_id);
                        if (!ally_agent) continue;
                        
                        offset = 0;
                        ImGui::Text(ally_agent->DisplayName().c_str());
                        ImGui::SameLine(offset += text_long);
                        
                        // 造成治疗
                        const auto it_heal_dealt = tracking->stats.healing_dealt_to_agents.find(agent_id);
                        if (it_heal_dealt != tracking->stats.healing_dealt_to_agents.end()) {
                            ImGui::Text(std::to_string(it_heal_dealt->second).c_str());
                        } else {
                            ImGui::Text("0");
                        }
                        ImGui::SameLine(offset += text_short);
                        
                        // 受到治疗
                        const auto it_heal_recv = tracking->stats.healing_received_from_agents.find(agent_id);
                        if (it_heal_recv != tracking->stats.healing_received_from_agents.end()) {
                            ImGui::Text(std::to_string(it_heal_recv->second).c_str());
                        } else {
                            ImGui::Text("0");
                        }
                    }
                }
                
                // 绘制对手表格（仅伤害）
                if (!opponent_agent_ids.empty()) {
                    ImGui::Text("");
                    ImGui::Text("对手：");
                    
                    float offset = 0;
                    ImGui::Text("玩家");
                    ImGui::SameLine(offset += text_long);
                    ImGui::Text("伤害+");
                    ImGui::SameLine(offset += text_short);
                    ImGui::Text("伤害-");
                    ImGui::Separator();
                    
                    for (const auto& agent_id : opponent_agent_ids) {
                        ObserverModule::ObservableAgent* opponent_agent = om.GetObservableAgentById(agent_id);
                        if (!opponent_agent) continue;
                        
                        offset = 0;
                        ImGui::Text(opponent_agent->DisplayName().c_str());
                        ImGui::SameLine(offset += text_long);
                        
                        // 造成伤害
                        const auto it_dmg_dealt = tracking->stats.damage_dealt_to_agents.find(agent_id);
                        if (it_dmg_dealt != tracking->stats.damage_dealt_to_agents.end()) {
                            ImGui::Text(std::to_string(it_dmg_dealt->second).c_str());
                        } else {
                            ImGui::Text("0");
                        }
                        ImGui::SameLine(offset += text_short);
                        
                        // 受到伤害
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

        if (settings.show_tracking) {
            // 技能
            ImGui::Separator();
            ImGui::Text("技能：");
            DrawHeaders();
            ImGui::Separator();
            DrawSkills(tracking->stats.skills_used, tracking->stats.skill_ids_used);
        }

        if (settings.show_comparison && compared && !(!settings.show_skills_used_on_self && tracking && compared->agent_id == tracking->agent_id)) {
            // 技能
            ImGui::Text(""); // 新行
            ImGui::Text(("对 "s + compared->DisplayName() + " 使用的技能").c_str());
            DrawHeaders();
            ImGui::Separator();
            const auto it_used_on_agent_skills = tracking->stats.skills_used_on_agents.find(compared->agent_id);
            const auto it_used_on_agent_skill_ids = tracking->stats.skill_ids_used_on_agents.find(compared->agent_id);
            if (it_used_on_agent_skills != tracking->stats.skills_used_on_agents.end() &&
                it_used_on_agent_skill_ids != tracking->stats.skill_ids_used_on_agents.end()) {
                DrawSkills(it_used_on_agent_skills->second, it_used_on_agent_skill_ids->second);
            }

            // 显示与特定玩家之间的伤害和治疗
            if (settings.show_damage_details) {
                ImGui::Text("");
                ImGui::Text(("与 "s + compared->DisplayName() + " 的统计数据").c_str());
                ImGui::Text(("  造成伤害: " + std::to_string(tracking->stats.LazyGetDamageDealedAgainst(compared->agent_id))).c_str());
                ImGui::Text(("  受到伤害: " + std::to_string(tracking->stats.LazyGetDamageReceivedFrom(compared->agent_id))).c_str());
                ImGui::Text(("  造成治疗: " + std::to_string(tracking->stats.LazyGetHealingDealedTo(compared->agent_id))).c_str());
                ImGui::Text(("  受到治疗: " + std::to_string(tracking->stats.LazyGetHealingReceivedFrom(compared->agent_id))).c_str());
            }
        }
    }

    ImGui::End();
}

// 绘制设置
void ObserverPlayerWindow::DrawSettingsInternal()
{
    ImGui::Text("请确保观战模块已启用。");
    ImGui::Checkbox("显示追踪玩家", &settings.show_tracking);
    ImGui::Checkbox("显示玩家比较", &settings.show_comparison);
    ImGui::Checkbox("显示对自身使用的技能", &settings.show_skills_used_on_self);
    ImGui::Checkbox(("显示尝试次数 ("s + ObserverLabel::Attempts + ")").c_str(), &settings.show_attempts);
    ImGui::Checkbox(("显示取消 ("s + ObserverLabel::Cancels + ")").c_str(), &settings.show_cancels);
    ImGui::Checkbox(("显示打断 ("s + ObserverLabel::Interrupts + ")").c_str(), &settings.show_interrupts);
    ImGui::Checkbox(("显示完成 ("s + ObserverLabel::Finishes + ")").c_str(), &settings.show_finishes);
    ImGui::Checkbox(("显示完整度 ("s + ObserverLabel::Integrity + ")").c_str(), &settings.show_integrity);
    ImGui::Checkbox("显示伤害", &settings.show_damage);
    ImGui::Checkbox("显示伤害详情", &settings.show_damage_details);
}
