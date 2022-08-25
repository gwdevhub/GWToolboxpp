#include "stdafx.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/StoCMgr.h>

#include <GWCA/Packets/StoC.h>

#include <Modules/Resources.h>
#include <Utils/GuiUtils.h>
#include <Windows/PartyStatisticsWindow.h>

/*************************/
/* Static Helper Methods */
/*************************/

namespace {
    constexpr wchar_t NONE_PLAYER_NAME[] = L"Hero/Henchman Slot";
    constexpr uint32_t NONE_SKILL = static_cast<uint32_t>(GW::Constants::SkillID::No_Skill);
    constexpr wchar_t UNKNOWN_SKILL_NAME[] = L"Unknown Skill";
    constexpr wchar_t UNKNOWN_PLAYER_NAME[] = L"Unknown Player";
} // namespace

IDirect3DTexture9* PartyStatisticsWindow::GetSkillImage(const GW::Constants::SkillID skill_id) {
    return *Resources::GetSkillImage(skill_id);
}
GuiUtils::EncString* PartyStatisticsWindow::GetSkillName(const GW::Constants::SkillID skill_id) {
    const auto found_it = skill_names.find(skill_id);

    if (found_it == skill_names.end()) {
        const GW::Skill* skill_data = GW::SkillbarMgr::GetSkillConstantData(skill_id);
        ASSERT(skill_data);
        skill_names[skill_id] = new GuiUtils::EncString(skill_data->name);
    }
    return skill_names[skill_id];
}
GuiUtils::EncString* PartyStatisticsWindow::GetAgentName(uint32_t agent_id) {
    const auto found_it = agent_names.find(agent_id);

    if (found_it == agent_names.end()) {
        agent_names[agent_id] = new GuiUtils::EncString(GW::Agents::GetAgentEncName(agent_id));
    }

    return agent_names[agent_id];
}

const GW::Skillbar* PartyStatisticsWindow::GetAgentSkillbar(const uint32_t agent_id) {
    const GW::SkillbarArray* skillbar_array = GW::SkillbarMgr::GetSkillbarArray();

    if (!skillbar_array) return nullptr;

    for (const GW::Skillbar& skillbar : *skillbar_array) {
        if (skillbar.agent_id == agent_id) return &skillbar;
    }

    return nullptr;
}

int PartyStatisticsWindow::GetSkillString(const std::wstring& agent_name, const std::wstring& skill_name,
    const uint32_t skill_count, wchar_t* out, size_t len) {
    int written = swprintf(out, len, skill_count == 1 ? L"%s used %s %d time." : L"%s used %s %d times.",
        agent_name.c_str(), skill_name.c_str(), skill_count);
    ASSERT(written != -1);
    return written;
}

/**********************/
/* Overridden Methods */
/**********************/

void PartyStatisticsWindow::Initialize() {
    ToolboxWindow::Initialize();

    send_timer = TIMER_INIT();

    GW::Chat::CreateCommand(L"skillstats", CmdSkillStatistics);

    GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::MapLoaded>(&MapLoaded_Entry, &MapLoadedCallback);

    /* Skill on self or party player */
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericValue>(
        &GenericValueSelf_Entry, [this](GW::HookStatus* status, GW::Packet::StoC::GenericValue* packet) -> void {
            UNREFERENCED_PARAMETER(status);

            const uint32_t value_id = packet->value_id;
            const uint32_t caster_id = packet->agent_id;
            const uint32_t target_id = 0U;
            const uint32_t value = packet->value;
            const bool no_target = true;
            SkillCallback(value_id, caster_id, target_id, value, no_target);
        });

    /* Skill on enemy player */
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericValueTarget>(&GenericValueTarget_Entry,
        [this](GW::HookStatus* status, GW::Packet::StoC::GenericValueTarget* packet) -> void {
            UNREFERENCED_PARAMETER(status);

            const uint32_t value_id = packet->Value_id;
            const uint32_t caster_id = packet->caster;
            const uint32_t target_id = packet->target;
            const uint32_t value = packet->value;
            const bool no_target = false;
            SkillCallback(value_id, caster_id, target_id, value, no_target);
        });

    UnsetPartyStatistics();
    pending_party_members = true;
}

void PartyStatisticsWindow::Update(float delta) {
    UNREFERENCED_PARAMETER(delta);

    if (pending_party_members && SetPartyMembers()) {
        pending_party_members = false;
    }

    const float time_diff_threshold{600.0F};
    if (!chat_queue.empty() && (TIMER_DIFF(send_timer) > time_diff_threshold)) {
        send_timer = TIMER_INIT();
        if (GW::Constants::InstanceType::Loading == GW::Map::GetInstanceType()) return;
        if (!GW::Agents::GetPlayer()) return;

        GW::Chat::SendChat('#', chat_queue.front().c_str());
        chat_queue.pop();
    }
}

void PartyStatisticsWindow::Draw(IDirect3DDevice9* pDevice) {
    UNREFERENCED_PARAMETER(pDevice);
    if (!visible) return;

    if (!GW::Map::GetIsMapLoaded()) return;
    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 250), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        if (!in_explorable) {
            ImGui::TextDisabled("Statistics will update in explorable area");
        }
        for (size_t i = 0; i < party_members.size(); i++) {
            DrawPartyMember(i);
        }
    }
    ImGui::End();
}

void PartyStatisticsWindow::LoadSettings(CSimpleIni* ini) {
    ToolboxWindow::LoadSettings(ini);
    show_abs_values = ini->GetBoolValue(Name(), VAR_NAME(show_abs_values), show_abs_values);
    show_perc_values = ini->GetBoolValue(Name(), VAR_NAME(show_perc_values), show_perc_values);
    print_by_click = ini->GetBoolValue(Name(), VAR_NAME(print_by_click), print_by_click);
}

void PartyStatisticsWindow::SaveSettings(CSimpleIni* ini) {
    ToolboxWindow::SaveSettings(ini);
    ini->SetBoolValue(Name(), VAR_NAME(show_abs_values), show_abs_values);
    ini->SetBoolValue(Name(), VAR_NAME(show_perc_values), show_perc_values);
    ini->SetBoolValue(Name(), VAR_NAME(print_by_click), print_by_click);
}

void PartyStatisticsWindow::DrawSettingInternal() {
    ImGui::Checkbox("Show the absolute skill count", &show_abs_values);
    ImGui::SameLine();
    ImGui::Checkbox("Show the percentage skill count", &show_perc_values);
    ImGui::SameLine();
    ImGui::Checkbox("Print skill statistics by Ctrl+LeftClick", &print_by_click);
}

void PartyStatisticsWindow::Terminate() {
    ToolboxWindow::Terminate();
    UnsetPartyStatistics();
}

/***********************/
/* Draw Helper Methods */
/***********************/

void PartyStatisticsWindow::DrawPartyMember(const size_t party_idx) {
    const PartyMember& party_member = party_members[party_idx];

    char header_label[256];
    snprintf(header_label, _countof(header_label), "%s###%u", party_member.name->string().c_str(), party_idx);

    if (ImGui::CollapsingHeader(header_label)) {
        const float start_y = ImGui::GetCursorPosY();

        char table_name[16];
        snprintf(table_name, _countof(table_name), "###Table%d", party_idx);

        const float width = ImGui::GetContentRegionAvail().x;
        float percentage = 0.f;
        if (party_member.skills.size() == 0) return;
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {0, 0});
        if (ImGui::BeginTable(table_name, party_member.skills.size())) {
            const float column_width = width / party_member.skills.size();
            const float scale = ImGui::GetIO().FontGlobalScale;
            const ImVec2 icon_size = {32.f * scale, 32.f * scale};
            for (size_t i = 0; i < party_member.skills.size(); i++) {
                char column_name[32];
                snprintf(column_name, _countof(table_name), "###%sColumn%d", table_name, i);
                ImGui::TableSetupColumn(column_name, ImGuiTableColumnFlags_WidthStretch, column_width);
            }
            ImGui::TableNextRow();
            for (size_t i = 0; i < party_member.skills.size(); i++) {
                const Skill& skill = party_member.skills[i];
                if (skill.id == GW::Constants::SkillID::No_Skill)
                    continue; // Skip empty skill slots (for heroes and yourself)
                ImGui::TableNextColumn();
                percentage = skill.count ? static_cast<float>(skill.count) /
                                            static_cast<float>(party_member.total_skills_used) * 100.f
                                        : 0.f;
                auto* texture = GetSkillImage(skill.id);
                if (texture) {
                    ImVec2 s(column_width, column_width);
                    ImGui::ImageCropped((ImTextureID)texture, icon_size);
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip(skill.name->string().c_str());
                    }
                }
                if (show_abs_values) {
                    ImGui::Text("%u", skill.count);
                }
                if (show_perc_values) {
                    ImGui::Text("%.2f%%", percentage);
                }
            }
            ImGui::EndTable();
        }
        ImGui::PopStyleVar();
        if (print_by_click) {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.0F);
            char button_name[32];
            snprintf(button_name, _countof(button_name), "###WriteStatistics%d", party_idx);
            const float height = ImGui::GetCursorPosY() - start_y;
            ImGui::SetCursorPosY(start_y);
            if (ImGui::Button(button_name, ImVec2(width, height)) && ImGui::IsKeyDown(ImGuiKey_ModCtrl)) {
                WritePlayerStatisticsAllSkills(party_idx);
            }
            ImGui::PopStyleVar();
        }
    }
}

/********************/
/* Callback Methods */
/********************/

void PartyStatisticsWindow::MapLoadedCallback(GW::HookStatus*, GW::Packet::StoC::MapLoaded*) {
    const bool in_explorable = GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable;
    auto& instance = Instance();
    if (in_explorable && !instance.in_explorable) {
        // Reset party stats; just entered explorable area.
        instance.UnsetPartyStatistics();
        instance.pending_party_members = true;
    }
    instance.in_explorable = in_explorable;
}

void PartyStatisticsWindow::SkillCallback(const uint32_t value_id, const uint32_t caster_id, const uint32_t target_id,
    const uint32_t value, const bool no_target) {
    uint32_t agent_id = caster_id;
    const auto activated_skill_id = static_cast<GW::Constants::SkillID>(value);

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

    if (NONE_SKILL == value) return;

    Skill* found_skill = 0;
    for (PartyMember& party_member : party_members) {
        if (party_member.agent_id != agent_id) continue;
        for (Skill& skill : party_member.skills) {
            if (skill.id == activated_skill_id) {
                found_skill = &skill;
                break;
            }
        }
        if (!found_skill) {
            party_member.skills.push_back(activated_skill_id);
            found_skill = &party_member.skills.back();
        }
        party_member.total_skills_used++;
        found_skill->count++;

        break;
    }
}

/********************/
/* Set Data Methods */
/********************/

void PartyStatisticsWindow::UnsetPartyStatistics() {
    party_members.clear();

    for (auto& [_, encoder] : skill_names) {
        delete encoder;
    }
    skill_names.clear();

    for (auto& [_, encoder] : agent_names) {
        delete encoder;
    }
    agent_names.clear();
}
PartyStatisticsWindow::~PartyStatisticsWindow() {
    UnsetPartyStatistics();
    for (auto& [_, psi] : skill_images) {
        ASSERT(!psi->loading || psi->texture);
        delete psi;
    }
    skill_images.clear();
}

bool PartyStatisticsWindow::SetPartyMembers() {
    if (!GW::PartyMgr::GetIsPartyLoaded()) return false;
    if (!GW::Map::GetIsMapLoaded()) return false;

    GW::PartyInfo* info = GW::PartyMgr::GetPartyInfo();
    if (info == nullptr) return false;

    uint32_t my_player_id = GW::Agents::GetPlayerId();
    const GW::Skillbar* my_skillbar = GetAgentSkillbar(my_player_id);
    if (!my_skillbar) {
        return false;
    }
    party_members.clear();
    player_party_idx = 0;
    for (GW::PlayerPartyMember& player : info->players) {
        uint32_t id = GW::Agents::GetAgentIdByLoginNumber(player.login_number);
        party_members.push_back(id);
        if (id == my_player_id) {
            player_party_idx = party_members.size() - 1;
        }
        if (!info->heroes.valid()) continue;
        for (GW::HeroPartyMember& hero : info->heroes) {
            if (hero.owner_player_id != player.login_number) continue;
            party_members.push_back(hero.agent_id);
            const GW::Skillbar* skillbar = GetAgentSkillbar(hero.agent_id);
            if (!skillbar) continue;
            PartyMember& party_member = party_members.back();
            /* Skillbar for other players and henchmen is unknown in outpost init with No_Skill */
            for (const GW::SkillbarSkill& skill : skillbar->skills) {
                party_member.skills.push_back(skill.skill_id);
            }
        }
    }
    if (info->henchmen.valid()) {
        for (GW::HenchmanPartyMember& hench : info->henchmen) {
            party_members.push_back(hench.agent_id);
        }
    }
    for (const GW::SkillbarSkill& skill : my_skillbar->skills) {
        party_members[player_party_idx].skills.push_back(skill.skill_id);
    }
    return true;
}

/************************/
/* Chat Command Methods */
/************************/

void PartyStatisticsWindow::CmdSkillStatistics(const wchar_t* message, int argc, LPWSTR* argv) {
    UNREFERENCED_PARAMETER(message);

    auto& instance = Instance();

    /* command: /skillstats */
    /* will write the stats of the self player */
    if (argc < 2) {
        instance.WritePlayerStatistics();
        return;
    }

    const std::wstring arg1 = GuiUtils::ToLower(argv[1]);

    if (argc == 2) {
        /* command: /skillstats reset */
        if (arg1 == L"reset") {
            instance.UnsetPartyStatistics();
            instance.pending_party_members = true;
        }
        /* command: /skllstats playerNum */
        else {
            uint32_t player_number = 0;
            if (GuiUtils::ParseUInt(argv[1], &player_number) && player_number > 0 &&
                player_number <= instance.party_members.size()) {
                --player_number; // List will start at index zero
                instance.WritePlayerStatistics(player_number);
            } else {
            Log::Error("Invalid player number '%ls', please use an integer value of 1 to %u", argv[1],
                instance.party_members.size());
            }
        }

        return;
    }

    /* command: /skillstats playerNum skillNum */
    if (argc >= 3) {
        uint32_t player_number = 0;
        if (GuiUtils::ParseUInt(argv[1], &player_number) && player_number > 0 &&
            player_number <= instance.party_members.size()) {
            --player_number;
            uint32_t skill_number = 0;
            if (GuiUtils::ParseUInt(argv[2], &skill_number) && skill_number > 0 && skill_number < 9) {
                --skill_number;
                instance.WritePlayerStatistics(player_number, skill_number);
            } else {
                Log::Error("Invalid skill number '%ls', please use an integer value of 1 to 8", argv[2]);
            }
        } else {
            Log::Error("Invalid player number '%ls', please use an integer value of 1 to %u", argv[1],
                instance.party_members.size());
        }
    }
}

void PartyStatisticsWindow::WritePlayerStatistics(const uint32_t player_idx, const uint32_t skill_idx) {
    if (GW::Constants::InstanceType::Loading == GW::Map::GetInstanceType()) return;

    /* all skills for self player */
    if (static_cast<size_t>(-1) == player_idx) {
        WritePlayerStatisticsAllSkills(player_party_idx);
    /* single skill for some player */
    } else if (static_cast<uint32_t>(-1) != skill_idx) {
        WritePlayerStatisticsSingleSkill(player_idx, skill_idx);
    /* all skills for some player */
    } else {
        WritePlayerStatisticsAllSkills(player_idx);
    }
}

void PartyStatisticsWindow::WritePlayerStatisticsAllSkills(const uint32_t player_idx) {
    if (player_idx >= party_members.size()) return;

    const PartyMember& party_member = party_members[player_idx];
    wchar_t chat_str[256];
    for (const Skill& skill : party_member.skills) {
        if (0U == skill.count) continue;
        GetSkillString(party_member.name->wstring(), skill.name->wstring(), skill.count, chat_str, _countof(chat_str));
        chat_queue.push(chat_str);
    }
}

void PartyStatisticsWindow::WritePlayerStatisticsSingleSkill(const uint32_t player_idx, const uint32_t skill_idx) {
    if (player_idx >= party_members.size()) return;

    const PartyMember& party_member = party_members[player_idx];
    if (skill_idx >= party_member.skills.size()) return;

    const Skill& skill = party_member.skills[skill_idx];

    wchar_t chat_str[256];
    GetSkillString(party_member.name->wstring(), skill.name->wstring(), skill.count, chat_str, _countof(chat_str));
    chat_queue.push(chat_str);
}
