#include "stdafx.h"
#include <algorithm>
#include <set>

#include <GWCA/Constants/Constants.h>
#include <GWCA/Context/WorldContext.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Packets/StoC.h>

#include <Color.h>
#include <Timer.h>
#include <Utils/GuiUtils.h>
#include <Windows/DupingWindow.h>


namespace {
    struct TrackedEnemy {
        clock_t last_duped = 0;
        size_t dupe_count = 0;
        uint32_t aggro_group = 0;
        bool will_follow = false;
    };
    std::unordered_map<GW::AgentID, TrackedEnemy> tracked_enemies{};

    float range = 1600.0f;
    const float sqr_aggro_range = GW::Constants::Range::Earshot * GW::Constants::Range::Earshot;
    const float sqr_lost_aggro_range = GW::Constants::Range::Spellcast * GW::Constants::Range::Spellcast;

    bool hide_when_nothing = true;
    bool show_souls_counter = true;
    bool show_waters_counter = true;
    bool show_minds_counter = true;
    float souls_threshhold = 0.6f;
    float waters_threshhold = 0.5f;
    float minds_threshhold = 0.0f;
    GW::AgentID last_attacked_agent_id;
    GW::HookEntry AgentAttack_Entry;

    uint32_t GetAgentMaxHP(const GW::AgentLiving* agent)
    {
        if (!agent) {
            return 0; // Invalid agent
        }
        if (agent->max_hp) {
            return agent->max_hp;
        }
        switch (agent->player_number) {
            case GW::Constants::ModelID::DoA::SoulTormentor:
            case GW::Constants::ModelID::DoA::VeilSoulTormentor:
            case GW::Constants::ModelID::DoA::WaterTormentor:
            case GW::Constants::ModelID::DoA::VeilWaterTormentor:
            case GW::Constants::ModelID::DoA::MindTormentor:
            case GW::Constants::ModelID::DoA::VeilMindTormentor:
                return GW::PartyMgr::GetIsPartyInHardMode() ? 1080 : 840;
            default: ;
        }
        return 0;
    }

    int GetHealthRegenPips(const GW::AgentLiving* agent)
    {
        const auto max_hp = GetAgentMaxHP(agent);
        if (!(max_hp && agent->hp_pips != .0f)) {
            return 0; // Invalid agent, unknown max HP, or no regen or degen
        }
        const float health_regen_per_second = max_hp * agent->hp_pips;
        const float pips = std::ceil(health_regen_per_second / 2.f); // 1 pip = 2 health per second
        return static_cast<int>(pips);
    }

    void DrawTrackedEnemy(int agent_id, const TrackedEnemy* info)
    {
        const auto agent = GW::Agents::GetAgentByID(agent_id);
        if (!agent) return;

        const auto living = agent->GetAsAgentLiving();
        if (!living) return;

        const GW::Agent* target = GW::Agents::GetTarget();
        const auto selected = target && target->agent_id == living->agent_id;

        ImGui::PushID(agent_id);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);

        if (ImGui::Selectable("", selected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap, ImVec2(0, 23))) {
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
            ImGui::Text("%.*s", pips > 0 && pips < 11 ? pips : 0, ">>>>>>>>>>");
        }

        ImGui::TableSetColumnIndex(3);
        ImGui::Text("%d", info->dupe_count);

        ImGui::TableSetColumnIndex(4);
        if (info->last_duped > 0) {
            const auto seconds_ago = static_cast<int>((TIMER_DIFF(info->last_duped) / CLOCKS_PER_SEC));
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

    void DrawEnemies(const char* label, const std::vector<std::pair<int, const TrackedEnemy*>>& enemies)
    {
        if (enemies.empty()) {
            return;
        }

        ImGui::Spacing();
        ImGui::Text(label);
        if (ImGui::BeginTable(label, 5)) {
            ImGui::TableSetupColumn("Selection", ImGuiTableColumnFlags_WidthFixed, 0, 0);
            ImGui::TableSetupColumn("HP", ImGuiTableColumnFlags_WidthStretch, -1, 0);
            ImGui::TableSetupColumn("Regen", ImGuiTableColumnFlags_WidthFixed, 70, 1);
            ImGui::TableSetupColumn("Dupe Count", ImGuiTableColumnFlags_WidthFixed, 20, 2);
            ImGui::TableSetupColumn("Last Duped", ImGuiTableColumnFlags_WidthFixed, 40, 3);

            for (const auto& [id, info] : enemies) {
                DrawTrackedEnemy(id, info);
            }

            ImGui::EndTable();
        }
    }

    void DrawEnemies(const char* label, const std::vector<std::vector<std::pair<int, const TrackedEnemy*>>>& grouped_enemies)
    {
        if (grouped_enemies.empty()) {
            return;
        }

        ImGui::Spacing();
        ImGui::Text(label);

        bool is_first = true;
        for (const auto& enemies : grouped_enemies) {
            if (!is_first) {
                ImGui::Spacing();
                ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal);
                is_first = false;
            }

            if (ImGui::BeginTable(label, 5)) {
                ImGui::TableSetupColumn("Selection", ImGuiTableColumnFlags_WidthFixed, 0, 0);
                ImGui::TableSetupColumn("HP", ImGuiTableColumnFlags_WidthStretch, -1, 0);
                ImGui::TableSetupColumn("Regen", ImGuiTableColumnFlags_WidthFixed, 70, 1);
                ImGui::TableSetupColumn("Dupe Count", ImGuiTableColumnFlags_WidthFixed, 20, 2);
                ImGui::TableSetupColumn("Last Duped", ImGuiTableColumnFlags_WidthFixed, 40, 3);


                for (const auto& [id, info] : enemies) {
                    DrawTrackedEnemy(id, info);
                }

                ImGui::EndTable();
            }
        }
    }

    void GainedAggro(const GW::AgentLiving* aggro_agent)
    {
        std::vector<GW::Agent*> chained_agents{};
        uint32_t aggro_group = 0;

        const GW::AgentArray* agents = GW::Agents::GetAgentArray();
        for (auto* agent : *agents) {
            const GW::AgentLiving* living = agent ? agent->GetAsAgentLiving() : nullptr;
            if (!living ||
                !living->GetIsAlive() ||
                living->allegiance != GW::Constants::Allegiance::Enemy ||
                GetSquareDistance(aggro_agent->pos, living->pos) > sqr_aggro_range
            ) {
                continue;
            }

            auto& info = tracked_enemies[agent->agent_id];
            if (info.aggro_group == 0) {
                // this agent got chain aggro
                chained_agents.push_back(agent);
            } else {
                // this agent was already in aggro, so the newly aggroed enemy and all enemies
                // that got chain aggro should join the same aggro group.
                // TODO: figure out which group it should join if there are multiple different
                // groups in earshot - for now defaulting to the max value.
                aggro_group = std::max(aggro_group, info.aggro_group);
            }
        }

        if (aggro_group == 0) {
            // we need a new aggro group because there is no existing one to join
            auto it = std::max_element(
                tracked_enemies.begin(),
                tracked_enemies.end(),
                [](const auto& a, const auto& b) { return a.second.aggro_group < b.second.aggro_group; }
            );
            if (it == tracked_enemies.end()) {
                aggro_group = 1;
            } else {
                aggro_group = it->second.aggro_group + 1;
            }
        }

        // update the aggro group for the enemy and all chained enemies
        tracked_enemies[aggro_agent->agent_id].aggro_group = aggro_group;
        for (auto* agent : chained_agents) {
            auto& dupe_info = tracked_enemies[agent->agent_id];
            dupe_info.aggro_group = aggro_group;
        }
    }

    void LostAggro(const GW::AgentLiving* agent)
    {
        tracked_enemies[agent->agent_id].aggro_group = 0;
        tracked_enemies[agent->agent_id].will_follow = false;
    }
} // namespace

void DupingWindow::Initialize()
{
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericValue>(&AgentAttack_Entry, [](GW::HookStatus*, const GW::Packet::StoC::GenericValue* packet) {
        if (packet->agent_id != GW::Agents::GetObservingId() || packet->value_id != 8 || packet->value != 1) {
            return;
        }

        const auto agent = GW::Agents::GetTarget();
        if (agent) {
            const auto living = agent->GetAsAgentLiving();
            if (living && living->GetIsAlive() && living->allegiance == GW::Constants::Allegiance::Enemy) {
                last_attacked_agent_id = living->agent_id;
            }
        }
    });

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentProjectileLaunched>(&AgentAttack_Entry, [](GW::HookStatus*, const GW::Packet::StoC::AgentProjectileLaunched* packet) {
        if (packet->agent_id != GW::Agents::GetObservingId() || !packet->is_attack || last_attacked_agent_id == 0) {
            return;
        }

        const auto agent = GW::Agents::GetAgentByID(last_attacked_agent_id);
        if (!agent) return;

        const auto living = agent->GetAsAgentLiving();
        if (living && living->GetIsAlive() && living->allegiance == GW::Constants::Allegiance::Enemy) {
            bool has_aggro = tracked_enemies.contains(living->agent_id) && tracked_enemies[living->agent_id].aggro_group > 0;
            if (!has_aggro) {
                GainedAggro(living);
            }
        }
    });
}

void DupingWindow::Terminate()
{
    ToolboxWindow::Terminate();
    tracked_enemies.clear();
}

void DupingWindow::Update(float)
{
    const bool is_in_doa = GW::Map::GetMapID() == GW::Constants::MapID::Domain_of_Anguish &&
        GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable;
    if (!is_in_doa) {
        tracked_enemies.clear();
        return;
    }

    const GW::AgentArray* agents = GW::Agents::GetAgentArray();
    const GW::Agent* me = agents ? GW::Agents::GetObservingAgent() : nullptr;

    if (!me)
        return;

    const float sqr_range = range * range;
    std::set<GW::AgentID> tracked_enemy_ids{};
    for (auto* agent : *agents) {
        const auto living = agent ? agent->GetAsAgentLiving() : nullptr;
        if (!living ||
            !living->GetIsAlive() ||
            living->allegiance != GW::Constants::Allegiance::Enemy ||
            GetSquareDistance(me->pos, living->pos) > sqr_range
        ) {
            continue;
        }

        tracked_enemy_ids.insert(living->agent_id);

        auto& dupe_info = tracked_enemies[living->agent_id];

        if (dupe_info.aggro_group == 0) {
            if (GetSquareDistance(me->pos, living->pos) <= sqr_aggro_range) {
                GainedAggro(living);
            }
        } else {
            if (GetSquareDistance(me->pos, living->pos) > sqr_lost_aggro_range) {
                LostAggro(living);
            }
        }

        // update duping data
        const bool is_duping = living->skill == static_cast<uint16_t>(GW::Constants::SkillID::Call_to_the_Torment);
        if (is_duping && TIMER_DIFF(dupe_info.last_duped) > 5000) {
            dupe_info.last_duped = TIMER_INIT();
            dupe_info.dupe_count += 1;
        }

        // update other data
        if (dupe_info.aggro_group > 0 && !dupe_info.will_follow) {
            if (living->GetInCombatStance() && (living->weapon_type == 0 || living->weapon_type == 512)) {
                dupe_info.will_follow = true;
            }
        }
    }

    // cleanup dupe info for enemies that are no longer in range / dead etc.
    std::erase_if(tracked_enemies, [tracked_enemy_ids](auto& pair) {
        return !tracked_enemy_ids.contains(pair.first);
    });
}

void DupingWindow::Draw(IDirect3DDevice9*)
{
    if (!visible) {
        return;
    }
    const bool is_in_doa = GW::Map::GetMapID() == GW::Constants::MapID::Domain_of_Anguish && GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable;
    if (hide_when_nothing && !is_in_doa) {
        return;
    }

    std::unordered_map<int, std::vector<std::pair<int, const TrackedEnemy*>>> souls_by_aggro_group;
    std::vector<std::pair<int, const TrackedEnemy*>> waters;
    std::vector<std::pair<int, const TrackedEnemy*>> minds;
    int following_soul_count = 0;
    int total_soul_count = 0;
    int total_water_count = 0;
    int total_mind_count = 0;
    for (const auto& [id, info] : tracked_enemies) {
        auto agent = GW::Agents::GetAgentByID(id);
        if (!agent) continue;

        auto living = agent->GetAsAgentLiving();
        if (!living || living->GetIsDead()) continue;

        switch (living->player_number) {
            case GW::Constants::ModelID::DoA::SoulTormentor:
            case GW::Constants::ModelID::DoA::VeilSoulTormentor:
                total_soul_count++;

                if (info.aggro_group > 0 && info.will_follow)
                    following_soul_count++;

                if (living->hp <= souls_threshhold)
                    souls_by_aggro_group[info.aggro_group].push_back(std::make_pair(id, &info));
                break;
            case GW::Constants::ModelID::DoA::WaterTormentor:
            case GW::Constants::ModelID::DoA::VeilWaterTormentor:
                total_water_count++;

                if (living->hp <= waters_threshhold)
                    waters.push_back(std::make_pair(id, &info));
                break;
            case GW::Constants::ModelID::DoA::MindTormentor:
            case GW::Constants::ModelID::DoA::VeilMindTormentor:
                total_mind_count++;

                if (living->hp <= minds_threshhold)
                    minds.push_back(std::make_pair(id, &info));
                break;
        }
    }

    if (hide_when_nothing &&
        (!show_souls_counter || total_soul_count == 0) &&
        (!show_waters_counter || total_water_count == 0) &&
        (!show_minds_counter || total_mind_count == 0)
    ) {
        return;
    }

    std::vector<std::vector<std::pair<int, const TrackedEnemy*>>> soul_groups;
    for (auto& [aggro_group, souls] : souls_by_aggro_group) {
        std::ranges::sort(souls, {}, &std::pair<int, const TrackedEnemy*>::first);
        soul_groups.push_back(souls);
    }

    std::ranges::sort(waters, {}, &std::pair<int, const TrackedEnemy*>::first);
    std::ranges::sort(minds, {}, &std::pair<int, const TrackedEnemy*>::first);

    // ==== Draw the window ====
    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        const auto total_counters = (show_souls_counter ? 1 : 0)
                                    + (show_waters_counter ? 1 : 0)
                                    + (show_minds_counter ? 1 : 0);

        if (total_counters > 0) {
            const auto total_width = ImGui::GetContentRegionAvail().x;
            const auto souls_text = total_soul_count > following_soul_count
                ? std::format("{} / {} Souls", following_soul_count, total_soul_count)
                : std::format("{} Souls", total_soul_count);
            const auto waters_text = std::format("{} Waters", total_water_count);
            const auto minds_text = std::format("{} Minds", total_mind_count);
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

        DrawEnemies("Souls", soul_groups);
        DrawEnemies("Waters", waters);
        DrawEnemies("Minds", minds);
    }
    ImGui::End();
}

void DupingWindow::DrawSettingsInternal()
{
    ImGui::Checkbox("Hide when there is nothing to show", &hide_when_nothing);
    ImGui::DragFloat("Range", &range, 50, 0, 5000);

    ImGui::Separator();
    ImGui::Text("Enemy Counters:");
    ImGui::StartSpacedElements(275.f);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Show souls", &show_souls_counter);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Show waters", &show_waters_counter);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Show minds", &show_minds_counter);

    ImGui::Separator();
    ImGui::Text("Duping thresholds:");
    ImGui::ShowHelp("Threshold HP below which enemy duping info is displayed");

    ImGui::DragFloat("Souls", &souls_threshhold, 0.01f, 0, 1);
    ImGui::DragFloat("Waters", &waters_threshhold, 0.01f, 0, 1);
    ImGui::DragFloat("Minds", &minds_threshhold, 0.01f, 0, 1);
}

void DupingWindow::LoadSettings(ToolboxIni* ini)
{
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

void DupingWindow::SaveSettings(ToolboxIni* ini)
{
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
