#include "stdafx.h"

#include <algorithm>

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Player.h>
#include <GWCA/GameEntities/Skill.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Packets/StoC.h>

#include <GuiUtils.h>
#include <Modules/Resources.h>
#include <Windows/PartyStatistics.h>

void PartyStatisticsWindow::GetSkillName(const uint32_t skill_id, char* const skill_name) {
    const auto& skill_data = GW::SkillbarMgr::GetSkillConstantData(skill_id);

    const auto buffer_length = size_t{32};
    wchar_t buffer[buffer_length] = {L'\0'};

    if (GW::UI::UInt32ToEncStr(skill_data.name, buffer, buffer_length)) {
        GW::UI::AsyncDecodeStr(buffer, skill_name, buffer_length);
    }
}

void PartyStatisticsWindow::GetPlayerName(const GW::Agent* const agent, char* const agent_name) {
    if (nullptr == agent_name || nullptr == agent) return;

    auto buffer = GW::Agents::GetAgentEncName(agent);
    if (nullptr == buffer) return;

    const auto buffer_length = size_t{32};
    GW::UI::AsyncDecodeStr(buffer, agent_name, buffer_length);
}

bool PartyStatisticsWindow::PartySizeChanged() {
    static auto last_party_size = static_cast<size_t>(-1);
    if (static_cast<size_t>(-1) == last_party_size) {
        last_party_size = GW::PartyMgr::GetPartySize();
        party_size = last_party_size;
    }

    const auto current_party_size = GW::PartyMgr::GetPartySize();
    const auto party_size_change = current_party_size != last_party_size;
    if (party_size_change) {
        last_party_size = current_party_size;
        party_size = last_party_size;
    }

    return party_size_change;
}

void PartyStatisticsWindow::UnsetPlayerStatistics() {
    party_ids.clear();
    party_indicies.clear();
    party_names.clear();
    party_skills.clear();
}

void PartyStatisticsWindow::MapLoadedCallback() {
    switch (GW::Map::GetInstanceType()) {
        case GW::Constants::InstanceType::Outpost:
        case GW::Constants::InstanceType::Explorable: {
            UnsetPlayerStatistics();
            break;
        }
        case GW::Constants::InstanceType::Loading:
        default: break;
    }
}

void PartyStatisticsWindow::SkillCallback(const uint32_t value_id, const uint32_t caster_id, const uint32_t target_id,
    const uint32_t value, const bool no_target) {
    auto agent_id = caster_id;
    const auto activated_skill_id = value;

    switch (value_id) {
        case GW::Packet::StoC::GenericValueID::instant_skill_activated:
        case GW::Packet::StoC::GenericValueID::skill_activated:
        case GW::Packet::StoC::GenericValueID::skill_finished:
        case GW::Packet::StoC::GenericValueID::attack_skill_activated:
        case GW::Packet::StoC::GenericValueID::attack_skill_finished: {
            if (!no_target) {
                agent_id = target_id;
            }
            break;
        }
        default: {
            return;
        }
    }

    if (party_indicies.empty() || party_skills.empty()) return;

    const auto buffer_length = size_t{32};

    const auto agent_idx = party_indicies[agent_id];
    auto& member_skills = party_skills[agent_idx].skills;

    const auto skill_it = std::find_if(member_skills.begin(), member_skills.end(),
        [&activated_skill_id](const auto skill) { return skill.id == activated_skill_id; });
    const auto skill_found = skill_it != member_skills.end();

    /* Other player skill casted for the first time */
    if (!skill_found) {
        for (auto& [skill_id, count, name] : member_skills) {
            if (NONE_SKILL != skill_id) continue;

            char skill_name[buffer_length] = {'\0'};
            GetSkillName(activated_skill_id, skill_name);

            skill_id = activated_skill_id;
            count = 1;
            name = std::string{skill_name};

            break;
        }
        return;
    }

    for (auto& [skill_id, count, _] : member_skills) {
        if (activated_skill_id == skill_id) {
            ++count;
            break;
        };
    }
}

void PartyStatisticsWindow::Update(float delta) {
    UNREFERENCED_PARAMETER(delta);

    if (!chat_queue.empty() && TIMER_DIFF(send_timer) > TIME_DIFF_THRESH) {
        send_timer = TIMER_INIT();
        if (GW::Constants::InstanceType::Loading == GW::Map::GetInstanceType()) return;
        if (!GW::Agents::GetPlayer()) return;

        GW::Chat::SendChat('#', chat_queue.front().c_str());
        chat_queue.pop();
    }
}

std::string PartyStatisticsWindow::GetSkillFullInfoString(
    const std::string agent_name, const std::string skill_name, const uint32_t skill_count) {
    return agent_name + std::string{" Used Skill "} + skill_name + std::string{" "} + std::to_string(skill_count) +
           std::string{" times."};
}

void PartyStatisticsWindow::WritePlayerStatisticsSingleSkill(const uint32_t player_idx, const uint32_t skill_idx) {
    if (skill_idx > (MAX_NUM_SKILLS - 1)) return;

    const auto& member_stats = party_skills[player_idx];
    const auto& member_skills = member_stats.skills;
    const auto& agent_name = party_names[member_stats.agent_id];

    const auto skill_count = member_skills[skill_idx].count;
    const auto skill_name = member_skills[skill_idx].name;

    const auto member_stats_str = GetSkillFullInfoString(agent_name, skill_name, skill_count);

    chat_queue.push(std::wstring(member_stats_str.begin(), member_stats_str.end()));
}

void PartyStatisticsWindow::WritePlayerStatisticsAllSkillsFullInfo(const uint32_t player_idx) {
    const auto& member_stats = party_skills[player_idx];
    const auto& member_skills = member_stats.skills;
    const auto& agent_name = party_names[member_stats.agent_id];

    for (const auto& [_, skill_count, skill_name] : member_skills) {
        const auto member_stats_str = GetSkillFullInfoString(agent_name, skill_name, skill_count);

        chat_queue.push(std::wstring(member_stats_str.begin(), member_stats_str.end()));
    }
}

void PartyStatisticsWindow::WritePlayerStatisticsAllSkills(const uint32_t player_idx) {
    const auto& member_stats = party_skills[player_idx];
    const auto& member_skills = member_stats.skills;
    const auto& agent_name = party_names[member_stats.agent_id];

    auto member_stats_str = std::string(agent_name) + std::string{" ["};
    const auto num_skills = member_skills.size();
    for (size_t i = 0; i < num_skills; ++i) {
        const auto skill_count = member_skills[i].count;

        member_stats_str += std::to_string(skill_count);

        if (i < (num_skills - 1)) {
            member_stats_str += ", ";
        }
    }

    member_stats_str += "]";

    chat_queue.push(std::wstring(member_stats_str.begin(), member_stats_str.end()));
}

void PartyStatisticsWindow::WritePlayerStatistics(
    const uint32_t player_idx, const uint32_t skill_idx, const bool full_info) {
    if (GW::Constants::InstanceType::Loading == GW::Map::GetInstanceType()) return;

    if (player_idx >= party_skills.size()) return;

    if (static_cast<uint32_t>(-1) != skill_idx) {
        WritePlayerStatisticsSingleSkill(player_idx, skill_idx);
    } else if (full_info) {
        WritePlayerStatisticsAllSkillsFullInfo(player_idx);
    } else {
        WritePlayerStatisticsAllSkills(player_idx);
    }
}

void PartyStatisticsWindow::WritePartyStatistics() {
    for (size_t i = 0; i < party_skills.size(); ++i) {
        WritePlayerStatistics(i);
    }
}

const GW::Skillbar* PartyStatisticsWindow::GetPlayerSkillbar(const uint32_t player_id) {
    const auto skillbar_array = GW::SkillbarMgr::GetSkillbarArray();
    if (!skillbar_array.valid()) return nullptr;

    for (const auto& skillbar : skillbar_array) {
        if (skillbar.agent_id == player_id) return &skillbar;
    }

    return nullptr;
}

void PartyStatisticsWindow::SetPartyIndicies() {
    if (!GW::PartyMgr::GetIsPartyLoaded()) return;

    GW::PartyInfo* info = GW::PartyMgr::GetPartyInfo();
    if (info == nullptr) return;

    GW::PlayerArray players = GW::Agents::GetPlayerArray();
    if (!players.valid()) return;

    party_ids.clear();
    party_indicies.clear();

    size_t index = 0;
    for (GW::PlayerPartyMember& player : info->players) {
        uint32_t id = players[player.login_number].agent_id;
        party_indicies[id] = index++;

        party_ids.insert(id);

        for (GW::HeroPartyMember& hero : info->heroes) {
            if (hero.owner_player_id == player.login_number) {
                party_indicies[hero.agent_id] = index++;

                party_ids.insert(hero.agent_id);
            }
        }
    }
    for (GW::HenchmanPartyMember& hench : info->henchmen) {
        party_indicies[hench.agent_id] = index++;

        party_ids.insert(hench.agent_id);
    }
}

void PartyStatisticsWindow::SetPartyNames() {
    if (!GW::PartyMgr::GetIsPartyLoaded()) return;

    const auto agents = GW::Agents::GetAgentArray();
    if (!agents.valid()) return;

    const auto buffer_length = size_t{32};

    party_names.clear();

    for (const auto id : party_ids) {
        party_names[id] = std::string{"Hero/Henchman Slot"};
    }

    for (const auto agent : agents) {
        if (nullptr == agent) continue;

        const auto id = agent->agent_id;

        const auto it = party_ids.find(id);
        if (it == party_ids.end()) continue;

        char agent_name[buffer_length] = {'\0'};
        GetPlayerName(agent, agent_name);

        if (0 == strlen(agent_name)) continue; /* Should not occur */

        party_names[id] = std::string{agent_name};
    }
}

void PartyStatisticsWindow::SetPartyStats() {
    if (!GW::PartyMgr::GetIsPartyLoaded()) return;

    party_skills.clear();
    party_skills = std::vector<PlayerSkills>(party_size, PlayerSkills{});

    for (const auto [agent_id, party_idx] : party_indicies) {
        party_skills[party_idx] = {agent_id, Skills{}};
    }

    const auto buffer_length = size_t{32};

    auto party_idx = size_t{0};
    for (auto& member_stats : party_skills) {
        auto& player_skills = party_skills[party_idx];
        ++party_idx;

        const auto id = member_stats.agent_id;

        const auto skillbar = GetPlayerSkillbar(id);
        auto skills = Skills{};

        /* Skillbar for other players and henchmen is unknown in outpost init with No_Skill */
        if (nullptr == skillbar) {
            for (size_t skill_idx = 0; skill_idx < MAX_NUM_SKILLS; ++skill_idx) {
                skills[skill_idx] = {NONE_SKILL, 0U, std::string{"Unknown Skill"}};
            }
            player_skills.skills = skills;
            continue;
        }

        auto skill_idx = size_t{0};
        for (const auto& skill : skillbar->skills) {
            char skill_name[buffer_length] = {'\0'};
            GetSkillName(skill.skill_id, skill_name);

            skills[skill_idx] = {skill.skill_id, 0U, std::string{skill_name}};
            ++skill_idx;
        }

        player_skills.skills = skills;
    }
}

void PartyStatisticsWindow::SetPlayerStatistics() {
    if (!GW::PartyMgr::GetIsPartyLoaded()) return;

    SetPartyIndicies();
    SetPartyNames();
    SetPartyStats();
}

void PartyStatisticsWindow::Initialize() {
    send_timer = TIMER_INIT();

    ToolboxWindow::Initialize();

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::MapLoaded>(
        &MapLoaded_Entry, [this](GW::HookStatus* status, GW::Packet::StoC::MapLoaded* packet) -> void {
            UNREFERENCED_PARAMETER(status);
            UNREFERENCED_PARAMETER(packet);

            return MapLoadedCallback();
        });

    /* Skill on self or party player */
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericValue>(
        &GenericValueSelf_Entry, [this](GW::HookStatus* status, GW::Packet::StoC::GenericValue* packet) -> void {
            UNREFERENCED_PARAMETER(status);

            const auto value_id = packet->Value_id;
            const auto caster_id = packet->agent_id;
            const auto target_id = 0U;
            const auto value = packet->value;
            const auto no_target = true;
            SkillCallback(value_id, caster_id, target_id, value, no_target);
        });

    /* Skill on enemy player */
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericValueTarget>(&GenericValueTarget_Entry,
        [this](GW::HookStatus* status, GW::Packet::StoC::GenericValueTarget* packet) -> void {
            UNREFERENCED_PARAMETER(status);

            const auto value_id = packet->Value_id;
            const auto caster_id = packet->caster;
            const auto target_id = packet->target;
            const auto value = packet->value;
            const auto no_target = false;
            SkillCallback(value_id, caster_id, target_id, value, no_target);
        });

    UnsetPlayerStatistics();
}

void PartyStatisticsWindow::Terminate() {
    ToolboxWindow::Terminate();
    UnsetPlayerStatistics();
}

void PartyStatisticsWindow::Draw(IDirect3DDevice9* pDevice) {
    UNREFERENCED_PARAMETER(pDevice);
    if (!visible) return;

    if (!GW::Map::GetIsMapLoaded()) return;

    const auto party_size_changed = PartySizeChanged();

    if (party_size_changed && GW::Constants::InstanceType::Outpost == GW::Map::GetInstanceType()) {
        SetPlayerStatistics();
    }

    if (party_indicies.empty()) {
        SetPartyIndicies();
    }
    if (party_names.empty()) {
        SetPartyNames();
    }
    if (party_skills.empty()) {
        SetPartyStats();
    }

    if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        for (const auto& member_stats : party_skills) {
            DrawPartyMember(member_stats);
        }
    }

    ImGui::End();
}

void PartyStatisticsWindow::DrawPartyMember(const PlayerSkills& member_stats) {
    const auto buffer_length = size_t{32};
    char header_label[buffer_length] = {'\0'};

    const auto agent_name = party_names[member_stats.agent_id];
    snprintf(header_label, buffer_length, "%s###%u", agent_name.c_str(), member_stats.agent_id);

    if (ImGui::CollapsingHeader(header_label)) {
        auto total_num_skills = uint32_t{0};
        std::for_each(member_stats.skills.begin(), member_stats.skills.end(),
            [&total_num_skills](const Skill& p) { total_num_skills += p.count; });
        if (static_cast<uint32_t>(0U) == total_num_skills) total_num_skills = 1U;

        for (const auto [skill_id, skill_count, skill_name] : member_stats.skills) {
            const auto percentage = (static_cast<float>(skill_count) / static_cast<float>(total_num_skills)) * 100.0F;

            if (show_perc_values && show_abs_values) {
                ImGui::Text("%s: %u\t-\t%-3.2f%%", skill_name.c_str(), skill_count, percentage);
            } else if (show_perc_values && !show_abs_values) {
                ImGui::Text("%s: %-3.2f%%", skill_name.c_str(), percentage);
            } else if (!show_perc_values && show_abs_values) {
                ImGui::Text("%s: %u", skill_name.c_str(), percentage);
            } else {
                ImGui::Text("%s", skill_name.c_str());
            }
        }
    }
}

void PartyStatisticsWindow::LoadSettings(CSimpleIni* ini) {
    ToolboxWindow::LoadSettings(ini);
    show_abs_values = ini->GetBoolValue(Name(), VAR_NAME(show_abs_values), show_abs_values);
    show_perc_values = ini->GetBoolValue(Name(), VAR_NAME(show_perc_values), show_perc_values);
}

void PartyStatisticsWindow::SaveSettings(CSimpleIni* ini) {
    ToolboxWindow::SaveSettings(ini);
    ini->SetBoolValue(Name(), VAR_NAME(show_abs_values), show_abs_values);
    ini->SetBoolValue(Name(), VAR_NAME(show_perc_values), show_perc_values);
}

void PartyStatisticsWindow::DrawSettingInternal() {
    ImGui::Checkbox("Show the absolute skill count", &show_abs_values);
    ImGui::SameLine();
    ImGui::Checkbox("Show the percentage skill count", &show_perc_values);
}
