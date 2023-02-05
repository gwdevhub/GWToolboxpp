#include "stdafx.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/PartyMgr.h>

#include <Color.h>
#include <Utils/GuiUtils.h>
#include <Modules/Resources.h>
#include <Modules/ToolboxSettings.h>
#include <Windows/DupingWindow.h>

#include <Timer.h>
namespace {
    struct DupeInfo {
        DupeInfo(GW::AgentID agent_id) : agent_id(agent_id) {};


        GW::AgentID agent_id;
        clock_t last_duped = 0;
        size_t dupe_count = 0;
    };

    GW::AgentLiving* GetAgentLivingByID(uint32_t agent_id) {
        const auto a = GW::Agents::GetAgentByID(agent_id);
        return a ? a->GetAsAgentLiving() : nullptr;
    }

    uint32_t GetAgentMaxHP(const GW::AgentLiving* agent) {
        if (!agent)
            return 0; // Invalid agent
        if (agent->max_hp)
            return agent->max_hp;
        switch (agent->player_number) {
            case GW::Constants::ModelID::DoA::SoulTormentor:
            case GW::Constants::ModelID::DoA::VeilSoulTormentor:
            case GW::Constants::ModelID::DoA::WaterTormentor:
            case GW::Constants::ModelID::DoA::VeilWaterTormentor:
            case GW::Constants::ModelID::DoA::MindTormentor:
            case GW::Constants::ModelID::DoA::VeilMindTormentor:
                return GW::PartyMgr::GetIsPartyInHardMode() ? 1080 : 840;
        }
        return 0;
    }

    int GetHealthRegenPips(const GW::AgentLiving* agent) {
        const auto max_hp = GetAgentMaxHP(agent);
        if (!(max_hp && agent->hp_pips != .0f))
            return 0; // Invalid agent, unknown max HP, or no regen or degen
        const float health_regen_per_second = max_hp * agent->hp_pips;
        const float pips = std::ceil(health_regen_per_second / 2.f); // 1 pip = 2 health per second
        return static_cast<int>(pips);
    }

    bool OrderDupeInfo(DupeInfo& a, DupeInfo& b) {
        const auto agentA = GetAgentLivingByID(a.agent_id);
        const auto agentB = agentA ? GetAgentLivingByID(b.agent_id) : nullptr;

        if (!agentB) return false;

        if (agentA->hp > 0.35f && agentB->hp > 0.35f) {
            return agentA->hp_pips > agentB->hp_pips;
        }

        return agentA->hp > agentB->hp;
    }
    void DrawDuping(const char* label, const std::vector<DupeInfo>& vec) {
        if (vec.empty()) return;

        ImGui::Spacing();
        ImGui::Text(label);
        if (ImGui::BeginTable(label, 5)) {

            ImGui::TableSetupColumn("Selection", ImGuiTableColumnFlags_WidthFixed, 0, 0);
            ImGui::TableSetupColumn("HP", ImGuiTableColumnFlags_WidthStretch, -1, 0);
            ImGui::TableSetupColumn("Regen", ImGuiTableColumnFlags_WidthFixed, 70, 1);
            ImGui::TableSetupColumn("Dupe Count", ImGuiTableColumnFlags_WidthFixed, 20, 2);
            ImGui::TableSetupColumn("Last Duped", ImGuiTableColumnFlags_WidthFixed, 40, 3);

            const GW::Agent* target = GW::Agents::GetTarget();

            for (const auto& dupe_info : vec) {
                const auto living = GetAgentLivingByID(dupe_info.agent_id);
                if (!living)
                    continue;
                const auto selected = target && target->agent_id == living->agent_id;

                ImGui::PushID(dupe_info.agent_id);
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);

                if (ImGui::Selectable("", selected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap, ImVec2(0, 23))) {
                    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                        GW::GameThread::Enqueue([living] {
                            GW::Agents::ChangeTarget(living);
                            });
                    }
                }

                ImGui::TableSetColumnIndex(1);
                ImGui::ProgressBar(living->hp);

                ImGui::TableSetColumnIndex(2);
                const auto pips = GetHealthRegenPips(living);
                if (pips > 0 && pips < 11) {
                    ImGui::Text("%.*s",pips > 0 && pips < 11 ? pips : 0,">>>>>>>>>>");
                }

                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%d", dupe_info.dupe_count);

                ImGui::TableSetColumnIndex(4);
                if (dupe_info.last_duped > 0) {
                    const int seconds_ago = static_cast<int>((TIMER_DIFF(dupe_info.last_duped) / CLOCKS_PER_SEC));
                    if (seconds_ago < 5) {
                        ImGui::PushStyleColor(ImGuiCol_Text, Colors::ARGB(205, 102, 153, 230));
                    }

                    const auto [quot, rem] = std::div(seconds_ago, 60);
                    ImGui::Text("%d:%02d", quot, rem);

                    if (seconds_ago < 5) {
                        ImGui::PopStyleColor();
                    }
                }
                else {
                    ImGui::Text("-");
                }

                ImGui::PopID();
            }

            ImGui::EndTable();
        }
    }

    std::vector<DupeInfo> souls{};
    std::vector<DupeInfo> waters{};
    std::vector<DupeInfo> minds{};

    std::set<GW::AgentID> all_souls;
    std::set<GW::AgentID> all_waters;
    std::set<GW::AgentID> all_minds;

    void ClearDupes() {
        souls.clear();
        waters.clear();
        minds.clear();
        all_souls.clear();
        all_waters.clear();
        all_minds.clear();
    }

    float range = 1600.0f;
    bool hide_when_nothing = true;
    bool show_souls_counter = true;
    bool show_waters_counter = true;
    bool show_minds_counter = true;
    float souls_threshhold = 0.6f;
    float waters_threshhold = 0.5f;
    float minds_threshhold = 0.0f;
}

void DupingWindow::Terminate() {
    ToolboxWindow::Terminate();
    ClearDupes();
}

void DupingWindow::Draw(IDirect3DDevice9* device) {
    UNREFERENCED_PARAMETER(device);
    if (!visible) 
        return;
    const bool is_in_doa = (GW::Map::GetMapID() == GW::Constants::MapID::Domain_of_Anguish && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable);
    if (hide_when_nothing && !is_in_doa) 
        return;

    const float sqr_range = range * range;
    int soul_count = 0;
    int water_count = 0;
    int mind_count = 0;
    float threshold = .0f;
    std::set<GW::AgentID>* all_agents_of_type = nullptr;
    std::vector<DupeInfo>* duped_agents_of_type = nullptr;
    const GW::AgentArray* agents = nullptr; 
    const GW::Agent* player = nullptr;

    // ==== Calculate the data ====
    if (!is_in_doa) {
        ClearDupes();
        goto done_calculation;
    }

    all_souls.clear();
    all_waters.clear();
    all_minds.clear();

    agents = GW::Agents::GetAgentArray();
    player = agents ? GW::Agents::GetPlayer() : nullptr;
    
    if (!player)
        goto done_calculation;

    for (auto* agent : *agents) {
        const GW::AgentLiving* living = agent ? agent->GetAsAgentLiving() : nullptr;

        if (!living || living->allegiance != GW::Constants::Allegiance::Enemy || !living->GetIsAlive() || GW::GetSquareDistance(player->pos, living->pos) > sqr_range)
            continue;

        switch (living->player_number) {
            case GW::Constants::ModelID::DoA::SoulTormentor:
            case GW::Constants::ModelID::DoA::VeilSoulTormentor:
                all_agents_of_type = &all_souls;
                duped_agents_of_type = &souls;
                threshold = souls_threshhold;
                soul_count++;
                break;
            case GW::Constants::ModelID::DoA::WaterTormentor:
            case GW::Constants::ModelID::DoA::VeilWaterTormentor:
                all_agents_of_type = &all_waters;
                duped_agents_of_type = &waters;
                threshold = waters_threshhold;
                water_count++;
                break;
            case GW::Constants::ModelID::DoA::MindTormentor:
            case GW::Constants::ModelID::DoA::VeilMindTormentor:
                all_agents_of_type = &all_minds;
                duped_agents_of_type = &minds;
                threshold = minds_threshhold;
                mind_count++;
                break;
            default:
                continue;
        }

        

        if (living->hp <= threshold) {
            all_agents_of_type->insert(living->agent_id);

            const bool is_duping = living->skill == static_cast<uint16_t>(GW::Constants::SkillID::Call_to_the_Torment);

            auto found_dupe_info = std::ranges::find_if(*duped_agents_of_type, [living](const DupeInfo& info) {
                return info.agent_id == living->agent_id;
            });
            if (found_dupe_info == duped_agents_of_type->end()) {
                duped_agents_of_type->push_back(living->agent_id);
                found_dupe_info = duped_agents_of_type->end() - 1;
            }
            if (is_duping && TIMER_DIFF(found_dupe_info->last_duped) > 5000) {
                found_dupe_info->last_duped = TIMER_INIT();
                found_dupe_info->dupe_count += 1;
            }
        }
    }

    std::erase_if(souls, [](auto& info) { return !all_souls.contains(info.agent_id); });
    std::erase_if(waters, [](auto& info) { return !all_waters.contains(info.agent_id); });
    std::erase_if(minds, [](auto& info) { return !all_minds.contains(info.agent_id); });

    std::ranges::sort(souls, &OrderDupeInfo);
    std::ranges::sort(waters, &OrderDupeInfo);
    std::ranges::sort(minds, &OrderDupeInfo);

    done_calculation:

    if (hide_when_nothing
        && !(show_souls_counter && soul_count > 0)
        && souls.size() == 0
        && !(show_waters_counter && water_count > 0)
        && waters.size() == 0
        && !(show_minds_counter && mind_count > 0)
        && minds.size() == 0
    ) return;



    // ==== Draw the window ====
    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        const auto total_counters = (show_souls_counter ? 1 : 0)
                                    + (show_waters_counter ? 1 : 0)
                                    + (show_minds_counter ? 1 : 0);

        if (total_counters > 0) {
            const auto total_width = ImGui::GetContentRegionAvail().x;
            const auto souls_text = std::format("{} Souls", soul_count);
            const auto waters_text = std::format("{} Waters", water_count);
            const auto minds_text = std::format("{} Minds", mind_count);
            const auto souls_width = ImGui::CalcTextSize(souls_text.c_str()).x;
            const auto waters_width = ImGui::CalcTextSize(waters_text.c_str()).x;
            const auto minds_width = ImGui::CalcTextSize(minds_text.c_str()).x;

            const auto padding = (
                                     total_width
                                     - (show_souls_counter ? souls_width : 0)
                                     - (show_waters_counter ? waters_width : 0)
                                     - (show_minds_counter ? minds_width : 0)
                                 ) / (total_counters - 1);

            float offset = 0.0f;
            if (show_souls_counter) {
                ImGui::Text(souls_text.c_str());
                offset += souls_width + padding;
                ImGui::SameLine(padding < 0 ? 0 : offset);
            }
            if (show_waters_counter) {
                ImGui::Text(waters_text.c_str());
                offset += waters_width + padding;
                ImGui::SameLine(padding < 0 ? 0 : offset);
            }
            if (show_minds_counter) {
                ImGui::Text(minds_text.c_str());
            }
        }

        DrawDuping("Souls", souls);
        DrawDuping("Waters", waters);
        DrawDuping("Minds", minds);
    }
    ImGui::End();
}

void DupingWindow::DrawSettingInternal() {
    ImGui::Checkbox("Hide when there is nothing to show", &hide_when_nothing);
    ImGui::DragFloat("Range", &range, 50, 0, 5000);

    ImGui::Separator();
    ImGui::Text("Enemy Counters:");
    ImGui::StartSpacedElements(275.f);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Show souls", &show_souls_counter);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Show waters", &show_waters_counter);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Show minds", &show_minds_counter);

    ImGui::Separator();
    ImGui::Text("Duping thresholds:");
    ImGui::ShowHelp("Threshold HP below which enemy duping info is displayed");

    ImGui::DragFloat("Souls", &souls_threshhold, 0.01f, 0, 1);
    ImGui::DragFloat("Waters", &waters_threshhold, 0.01f, 0, 1);
    ImGui::DragFloat("Minds", &minds_threshhold, 0.01f, 0, 1);
}

void DupingWindow::LoadSettings(ToolboxIni* ini) {
    ToolboxWindow::LoadSettings(ini);
    LOAD_BOOL(hide_when_nothing);
    LOAD_BOOL(show_souls_counter);
    LOAD_BOOL(show_waters_counter);
    LOAD_BOOL(show_minds_counter);

    LOAD_FLOAT(souls_threshhold);
    LOAD_FLOAT(waters_threshhold);
    LOAD_FLOAT(minds_threshhold);
    LOAD_FLOAT(range);
}

void DupingWindow::SaveSettings(ToolboxIni* ini) {
    ToolboxWindow::SaveSettings(ini);
    SAVE_BOOL(hide_when_nothing);
    SAVE_BOOL(show_souls_counter);
    SAVE_BOOL(show_waters_counter);
    SAVE_BOOL(show_minds_counter);

    SAVE_FLOAT(souls_threshhold);
    SAVE_FLOAT(waters_threshhold);
    SAVE_FLOAT(minds_threshhold);
    SAVE_FLOAT(range);
}
