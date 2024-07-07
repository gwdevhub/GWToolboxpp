#include "stdafx.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/Context/WorldContext.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/MapMgr.h>

#include <Utils/GuiUtils.h>
#include <Windows/EnemyWindow.h>
#include <Modules/Resources.h>

#include <GWCA/Managers/SkillbarMgr.h>
#include <Timer.h>

namespace {
    bool show_enemy_level = true;
    bool show_enemy_last_skill = true;
    float triangle_y_offset = 3.f;
    float enemies_threshhold = 1.f;
    float range = GW::Constants::Range::Spellcast;
    float triangle_spacing = 22.f;
    float last_skill_threshold = 3000.f;
    ImU32 HexedColor = IM_COL32(253, 113, 255, 255);
    ImU32 ConditionedColor = IM_COL32(160, 117, 85, 255);
    ImU32 EnchantedColor = IM_COL32(224, 253, 94, 255);

    std::unordered_map<uint32_t, GuiUtils::EncString*> agent_names_by_id;

    std::string& GetAgentName(uint32_t agent_id)
    {
        const auto enc_name = GW::Agents::GetAgentEncName(agent_id);
        if (!agent_names_by_id.contains(agent_id)) {
            agent_names_by_id[agent_id] = new GuiUtils::EncString();
        }
        auto enc_string = agent_names_by_id[agent_id];
        enc_string->reset(enc_name);
        return enc_string->string();
    }

    struct EnemyInfo {
        EnemyInfo(const GW::AgentID agent_id)
            : agent_id(agent_id) {}

        GW::AgentID agent_id;
        clock_t last_casted = 0;
        GW::Constants::SkillID last_skill = GW::Constants::SkillID::No_Skill;
        float distance = FLT_MAX;
    };

    GW::AgentLiving* GetAgentLivingByID(const uint32_t agent_id)
    {
        const auto a = GW::Agents::GetAgentByID(agent_id);
        return a ? a->GetAsAgentLiving() : nullptr;
    }

    void DrawStatusTriangle(const int triangleCount, const ImVec2 position, const ImU32 triangleColor, const bool upsidedown)
    {
        const auto point1 = ImVec2(position.x - (triangleCount * triangle_spacing), position.y + triangle_y_offset + (upsidedown ? 10.f : 0.f));
        const auto point2 = ImVec2(position.x + 20 - (triangleCount * triangle_spacing), position.y + triangle_y_offset + (upsidedown ? 10.f : 0.f));
        const auto point3 = ImVec2(position.x + 10 - (triangleCount * triangle_spacing), position.y + triangle_y_offset + (upsidedown ? 0.f : 10.f));

        ImGui::GetWindowDrawList()->AddTriangleFilled(point1, point2, point3, triangleColor);
    }

    void WriteEnemyName(const ImVec2 position, const std::string& agent_name, const std::string* skill_name, uint8_t level, const clock_t last_casted)
    {
        std::string text;

        if (show_enemy_level) {
            text += std::format("Lvl {} ", level);
        }
        text += agent_name;
        if (show_enemy_last_skill && TIMER_DIFF(last_casted) < last_skill_threshold && skill_name && !skill_name->empty()) {
            text += std::format(" - {}", *skill_name);
        }
        ImGui::GetWindowDrawList()->AddText(position, IM_COL32(253, 255, 255, 255), text.c_str());
    }

    uint32_t GetAgentMaxHP(const GW::AgentLiving* agent)
    {
        if (!agent) {
            return 0; // Invalid agent
        }
        if (agent->max_hp) {
            return agent->max_hp;
        }
        return 0;
    }

    int GetHealthRegenPips(const GW::AgentLiving* agent)
    {
        const auto max_hp = GetAgentMaxHP(agent);
        if (!(max_hp && agent->hp_pips != .0f)) {
            return 0;
        }
        const float health_regen_per_second = max_hp * agent->hp_pips;
        const float pips = std::ceil(health_regen_per_second / 2.f); // 1 pip = 2 health per second
        return static_cast<int>(pips);
    }

    bool OrderEnemyInfo(const EnemyInfo& a, const EnemyInfo& b)
    {
        return a.distance < b.distance;
    }

    void DrawEnemies(const char* label, const std::vector<EnemyInfo>& vec)
    {
        if (vec.empty()) {
            return;
        }

        ImGui::Spacing();
        if (ImGui::BeginTable(label, 4)) {
            ImGui::TableSetupColumn("Selection", ImGuiTableColumnFlags_WidthFixed, 0, 0);
            ImGui::TableSetupColumn("HP", ImGuiTableColumnFlags_WidthStretch, -1, 0);
            ImGui::TableSetupColumn("Regen", ImGuiTableColumnFlags_WidthFixed, 70, 1);
            ImGui::TableSetupColumn("Last Casted", ImGuiTableColumnFlags_WidthFixed, 40, 2);

            const GW::Agent* target = GW::Agents::GetTarget();

            for (const auto& enemy_info : vec) {
                const auto living = GetAgentLivingByID(enemy_info.agent_id);
                if (!living || enemy_info.distance > range * range) {
                    continue;
                }
                const auto selected = target && target->agent_id == living->agent_id;
                const auto agent_name_str = GetAgentName(living->agent_id);
                if (agent_name_str.empty())
                    continue;

                ImGui::PushID(enemy_info.agent_id);
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);

                if (ImGui::Selectable("", selected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap, ImVec2(0, 23))) {
                    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                        GW::GameThread::Enqueue([living] {
                            GW::Agents::ChangeTarget(living);
                        });
                    }
                }

                // Progress bar
                ImGui::TableSetColumnIndex(1);
                ImVec2 progressBarPos = ImGui::GetCursorScreenPos();
                const ImVec2 pos1 = ImVec2(progressBarPos.x + ImGui::GetContentRegionAvail().x * 0.025f, progressBarPos.y + 3);
                const ImVec2 pos2 = ImVec2(progressBarPos.x + ImGui::GetContentRegionAvail().x - 25, progressBarPos.y + 3);

                int triangles = 0;

                if (living->skill != 0) {
                    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.165f, 0.196f, 0.294f, 1.0f));
                }
                else {
                    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.6117647058823529f, 0.0f, 0.0f, 1.0f));
                }
                ImGui::ProgressBar(living->hp, ImVec2(-1, 0), "");
                ImGui::PopStyleColor();

                std::string* skill_name = nullptr;

                if (enemy_info.last_skill != GW::Constants::SkillID::No_Skill) {
                    const auto skill_data = GW::SkillbarMgr::GetSkillConstantData(enemy_info.last_skill);
                    auto enc_skillname = Resources::DecodeStringId(skill_data->name);
                    ASSERT(enc_skillname);
                    skill_name = &enc_skillname->string();
                }

                WriteEnemyName(pos1, agent_name_str, skill_name, living->level, enemy_info.last_casted);

                if (living->GetIsEnchanted()) {
                    DrawStatusTriangle(triangles, pos2, EnchantedColor, false);
                    triangles++;
                }

                if (living->GetIsHexed()) {
                    DrawStatusTriangle(triangles, pos2, HexedColor, true);
                    triangles++;
                }

                if (living->GetIsConditioned()) {
                    DrawStatusTriangle(triangles, pos2, ConditionedColor, true);
                }

                // Health pips
                ImGui::TableSetColumnIndex(2);
                const auto pips = GetHealthRegenPips(living);
                if (pips > 0 && pips < 11) {
                    ImGui::Text("%.*s", pips > 0 && pips < 11 ? pips : 0, ">>>>>>>>>>");
                }

                // Last Casted Skill
                ImGui::TableSetColumnIndex(3);
                if (enemy_info.last_casted != 0) {
                    const auto seconds_ago = static_cast<int>((TIMER_DIFF(enemy_info.last_casted) / CLOCKS_PER_SEC));
                    const auto [quot, rem] = std::div(seconds_ago, 60);
                    ImGui::Text("%d:%02d", quot, rem);
                }
                else {
                    ImGui::Text("-");
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }

    std::vector<EnemyInfo> enemies{};
    std::set<GW::AgentID> all_enemies;

    void ClearEnemies()
    {
        enemies.clear();
        all_enemies.clear();
        for (const auto enc_name : agent_names_by_id | std::views::values) {
            delete enc_name;
        }
        agent_names_by_id.clear();
    }
} // namespace

void EnemyWindow::Terminate()
{
    ToolboxWindow::Terminate();
    ClearEnemies();
}

void EnemyWindow::Draw(IDirect3DDevice9*)
{
    if (!visible) {
        return;
    }

    if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost) {
        ClearEnemies();
    }

    all_enemies.clear();

    const GW::AgentArray* agents = GW::Agents::GetAgentArray();
    const GW::Agent* player = agents ? GW::Agents::GetObservingAgent() : nullptr;

    if (player && agents) {
        for (auto* agent : *agents) {
            const GW::AgentLiving* living = agent ? agent->GetAsAgentLiving() : nullptr;

            if (!living || living->allegiance != GW::Constants::Allegiance::Enemy || !living->GetIsAlive()) {
                continue;
            }

            if (living->hp <= enemies_threshhold) {
                all_enemies.insert(living->agent_id);

                const bool is_casting = living->skill != static_cast<uint16_t>(GW::Constants::SkillID::No_Skill);

                auto found_enemy = std::ranges::find_if(enemies, [living](const EnemyInfo& info) {
                    return info.agent_id == living->agent_id;
                });
                if (found_enemy == enemies.end()) {
                    enemies.push_back(living->agent_id);
                    found_enemy = enemies.end() - 1;
                }
                if (is_casting) {
                    found_enemy->last_casted = TIMER_INIT();
                    found_enemy->last_skill = static_cast<GW::Constants::SkillID>(living->skill);
                }

                found_enemy->distance = GW::GetSquareDistance(player->pos, living->pos);
            }
        }

        std::erase_if(enemies, [](auto& info) {
            return !all_enemies.contains(info.agent_id);
        });

        std::ranges::sort(enemies, &OrderEnemyInfo);
    }

    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        DrawEnemies("enemies", enemies);
    }
    ImGui::End();
}

void EnemyWindow::DrawSettingsInternal()
{
    ImGui::DragFloat("Range", &range, 50.f, 0, 5000.f);
    ImGui::Separator();
    ImGui::StartSpacedElements(275.f);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Show level", &show_enemy_level);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Show enemy last skill", &show_enemy_last_skill);
    ImGui::Separator();
    ImGui::Text("HP thresholds:");
    ImGui::ShowHelp("Threshold HP below which enemy  info is displayed");
    ImGui::DragFloat("Percent", &enemies_threshhold, 0.01f, 0, 1.f);
    ImGui::Separator();
    ImGui::Text("Last skill casted threshold:");
    ImGui::DragFloat("Milliseconds", &last_skill_threshold, 1.f, 0, 60000.f);
    ImGui::Separator();
    ImGui::Text("Status triange spacing");
    ImGui::DragFloat("Spacing", &triangle_spacing, 0.01f, 0, 100.f);
}

void EnemyWindow::LoadSettings(ToolboxIni* ini)
{
    ToolboxWindow::LoadSettings(ini);
    LOAD_BOOL(show_enemy_level);
    LOAD_BOOL(show_enemy_last_skill);
    LOAD_FLOAT(enemies_threshhold);
    LOAD_FLOAT(range);
    LOAD_FLOAT(triangle_spacing);
    LOAD_FLOAT(last_skill_threshold);
}

void EnemyWindow::SaveSettings(ToolboxIni* ini)
{
    ToolboxWindow::SaveSettings(ini);
    SAVE_BOOL(show_enemy_level);
    SAVE_BOOL(show_enemy_last_skill);
    SAVE_FLOAT(enemies_threshhold);
    SAVE_FLOAT(range);
    SAVE_FLOAT(triangle_spacing);
    SAVE_FLOAT(last_skill_threshold);
}
