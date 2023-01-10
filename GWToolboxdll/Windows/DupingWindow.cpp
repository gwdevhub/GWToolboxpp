#include "stdafx.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/MapMgr.h>

#include <Utils/GuiUtils.h>
#include <Modules/Resources.h>
#include <Modules/ToolboxSettings.h>
#include <Windows/DupingWindow.h>

void DupingWindow::Initialize() {
    ToolboxWindow::Initialize();
}

void DupingWindow::Terminate() {
    souls.clear();
    ToolboxWindow::Terminate();
}

bool DupingWindow::OrderDupeInfo(DupeInfo& a, DupeInfo& b) {
    const GW::AgentLiving* agentA = GW::Agents::GetAgentByID(a.agent_id)->GetAsAgentLiving();
    const GW::AgentLiving* agentB = GW::Agents::GetAgentByID(b.agent_id)->GetAsAgentLiving();

    if (agentA->hp > 0.35f && agentB->hp > 0.35f) {
        return agentA->hp_pips > agentB->hp_pips;
    }

    return agentA->hp > agentB->hp;
}

void DupingWindow::Draw(IDirect3DDevice9* device) {
    UNREFERENCED_PARAMETER(device);
    if (!visible || GW::Map::GetMapID() != GW::Constants::MapID::Domain_of_Anguish) return;
    if (hide_when_nothing && GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable) return;

    // ==== Calculate the data ====
    const GW::AgentArray* agents = GW::Agents::GetAgentArray();
    const GW::Agent* player = GW::Agents::GetPlayer();
    if (!agents || !player) return;

    auto now = duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    std::unordered_map<GW::AgentID, const GW::AgentLiving*> all_souls;
    std::unordered_map<GW::AgentID, const GW::AgentLiving*> all_waters;
    std::unordered_map<GW::AgentID, const GW::AgentLiving*> all_minds;

    const float sqr_range = range * range;
    int soul_count = 0;
    int water_count = 0;
    int mind_count = 0;
    for (auto* agent : *agents) {
        const GW::AgentLiving* living = agent ? agent->GetAsAgentLiving() : nullptr;

        if (!living || living->allegiance != GW::Constants::Allegiance::Enemy || !living->GetIsAlive() || GW::GetSquareDistance(player->pos, living->pos) > sqr_range)
            continue;

        switch (living->player_number) {
            case GW::Constants::ModelID::DoA::SoulTormentor:
            case GW::Constants::ModelID::DoA::VeilSoulTormentor:
                soul_count++;
                break;
            case GW::Constants::ModelID::DoA::WaterTormentor:
            case GW::Constants::ModelID::DoA::VeilWaterTormentor:
                water_count++;
                break;
            case GW::Constants::ModelID::DoA::MindTormentor:
            case GW::Constants::ModelID::DoA::VeilMindTormentor:
                mind_count++;
                break;
            default:
                continue;
        }

        bool is_duping = living->skill == static_cast<uint16_t>(GW::Constants::SkillID::Call_to_the_Torment);

        if (living->hp <= souls_threshhold
            && (living->player_number == GW::Constants::ModelID::DoA::SoulTormentor
                || living->player_number == GW::Constants::ModelID::DoA::VeilSoulTormentor)
        ) {
            all_souls.insert_or_assign(living->agent_id, living);

            auto found_soul = std::find_if(souls.begin(), souls.end(), [living](DupeInfo& info) {
                return info.agent_id == living->agent_id;
            });

            if (found_soul != souls.end()) {
                if (is_duping && (*found_soul).last_duped < now - 5000) {
                    (*found_soul).last_duped = now;
                    (*found_soul).dupe_count += 1;
                }
            }
            else {
                souls.push_back(DupeInfo(living->agent_id, is_duping ? now : 0, is_duping ? 1 : 0));
            }
        } else if (living->hp <= waters_threshhold
            && (living->player_number == GW::Constants::ModelID::DoA::WaterTormentor
                || living->player_number == GW::Constants::ModelID::DoA::VeilWaterTormentor)
        ) {
            all_waters.insert_or_assign(living->agent_id, living);

            auto found_water = std::find_if(waters.begin(), waters.end(), [living](DupeInfo& info) {
                return info.agent_id == living->agent_id;
            });

            if (found_water != waters.end()) {
                if (is_duping && (*found_water).last_duped < now - 5000) {
                    (*found_water).last_duped = now;
                    (*found_water).dupe_count += 1;
                }
            }
            else {
                waters.push_back(DupeInfo(living->agent_id, is_duping ? now : 0, is_duping ? 1 : 0));
            }
        }  else if (living->hp <= minds_threshhold
            && (living->player_number == GW::Constants::ModelID::DoA::MindTormentor
                || living->player_number == GW::Constants::ModelID::DoA::VeilMindTormentor)
        ) {
            all_minds.insert_or_assign(living->agent_id, living);

            auto found_mind = std::find_if(minds.begin(), minds.end(), [living](DupeInfo& info) {
                return info.agent_id == living->agent_id;
            });

            if (found_mind != minds.end()) {
                if (is_duping && (*found_mind).last_duped < now - 5000) {
                    (*found_mind).last_duped = now;
                    (*found_mind).dupe_count += 1;
                }
            }
            else {
                minds.push_back(DupeInfo(living->agent_id, is_duping ? now : 0, is_duping ? 1 : 0));
            }
        }
    }

    std::erase_if(souls, [all_souls](auto& info) { return !all_souls.contains(info.agent_id); });
    std::erase_if(waters, [all_waters](auto& info) { return !all_waters.contains(info.agent_id); });
    std::erase_if(minds, [all_minds](auto& info) { return !all_minds.contains(info.agent_id); });

    if (hide_when_nothing
        && !(show_souls_counter && soul_count > 0)
        && souls.size() == 0
        && !(show_waters_counter && water_count > 0)
        && waters.size() == 0
        && !(show_minds_counter && mind_count > 0)
        && minds.size() == 0
    ) return;

    std::sort(souls.begin(), souls.end(), &OrderDupeInfo);
    std::sort(waters.begin(), waters.end(), &OrderDupeInfo);
    std::sort(minds.begin(), minds.end(), &OrderDupeInfo);

    // ==== Draw the window ====
    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        auto total_counters = (show_souls_counter ? 1 : 0)
            + (show_waters_counter ? 1 : 0)
            + (show_minds_counter ? 1 : 0);

        if (total_counters > 0) {
            auto total_width = ImGui::GetContentRegionAvail().x;
            auto souls_text = std::format("{} Souls", soul_count);
            auto waters_text = std::format("{} Waters", water_count);
            auto minds_text = std::format("{} Minds", mind_count);
            auto souls_width = ImGui::CalcTextSize(souls_text.c_str()).x;
            auto waters_width = ImGui::CalcTextSize(waters_text.c_str()).x;
            auto minds_width = ImGui::CalcTextSize(minds_text.c_str()).x;

            auto padding = (
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

        DrawDuping("Souls", now, souls);
        DrawDuping("Waters", now, waters);
        DrawDuping("Minds", now, minds);
    }
    ImGui::End();
}

void DupingWindow::DrawDuping(const char* label, long long now, std::vector<DupeInfo> vec) {
    if (vec.size() == 0) return;

    ImGui::Spacing();
    ImGui::Text(label);
    if (ImGui::BeginTable(label, 5)) {

        ImGui::TableSetupColumn("Selection", ImGuiTableColumnFlags_WidthFixed, 0, 0);
        ImGui::TableSetupColumn("HP", ImGuiTableColumnFlags_WidthStretch, -1, 0);
        ImGui::TableSetupColumn("Regen", ImGuiTableColumnFlags_WidthFixed, 70, 1);
        ImGui::TableSetupColumn("Dupe Count", ImGuiTableColumnFlags_WidthFixed, 20, 2);
        ImGui::TableSetupColumn("Last Duped", ImGuiTableColumnFlags_WidthFixed, 40, 3);

        const GW::Agent* target = GW::Agents::GetTarget();

        for (auto& dupe_info : vec) {
            auto agent = GW::Agents::GetAgentByID(dupe_info.agent_id)->GetAsAgentLiving();
            auto selected = target && target->agent_id == agent->agent_id;

            ImGui::PushID(dupe_info.agent_id);
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);

            if (ImGui::Selectable("", selected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap, ImVec2(0, 23))) {
                if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                    GW::GameThread::Enqueue([agent]() {
                        GW::Agents::ChangeTarget(agent);
                    });
                }
            }

            ImGui::TableSetColumnIndex(1);
            ImGui::ProgressBar(agent->hp);

            ImGui::TableSetColumnIndex(2);
            ImGui::Text(std::string((int)std::ceil(agent->hp_pips / 0.001852), '>').c_str());

            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%d", dupe_info.dupe_count);

            ImGui::TableSetColumnIndex(4);
            if (dupe_info.last_duped > 0) {
                int seconds_ago = (int)(now - dupe_info.last_duped) / 1000;
                if (seconds_ago < 5) {
                    ImGui::PushStyleColor(ImGuiCol_Text, Colors::ARGB(205, 102, 153, 230));
                }

                auto div = std::div(seconds_ago, 60);
                ImGui::Text("%d:%02d", div.quot, div.rem);

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
    range = static_cast<float>(ini->GetDoubleValue(Name(), VAR_NAME(range), 1600.0f));

    LOAD_BOOL(show_souls_counter);
    LOAD_BOOL(show_waters_counter);
    LOAD_BOOL(show_minds_counter);

    souls_threshhold = static_cast<float>(ini->GetDoubleValue(Name(), VAR_NAME(souls_threshhold), 0.6f));
    waters_threshhold = static_cast<float>(ini->GetDoubleValue(Name(), VAR_NAME(waters_threshhold), 0.5f));
    minds_threshhold = static_cast<float>(ini->GetDoubleValue(Name(), VAR_NAME(minds_threshhold), 0.0f));
}

void DupingWindow::SaveSettings(ToolboxIni* ini) {
    ToolboxWindow::SaveSettings(ini);
    SAVE_BOOL(hide_when_nothing);
    ini->SetDoubleValue(Name(), VAR_NAME(range), range);

    SAVE_BOOL(show_souls_counter);
    SAVE_BOOL(show_waters_counter);
    SAVE_BOOL(show_minds_counter);

    ini->SetDoubleValue(Name(), VAR_NAME(souls_threshhold), souls_threshhold);
    ini->SetDoubleValue(Name(), VAR_NAME(waters_threshhold), waters_threshhold);
    ini->SetDoubleValue(Name(), VAR_NAME(minds_threshhold), minds_threshhold);
}
