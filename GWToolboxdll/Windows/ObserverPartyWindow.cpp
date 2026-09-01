#include "stdafx.h"

#include <Utils/GuiUtils.h>

#include <Modules/ObserverModule.h>

#include <Windows/ObserverPartyWindow.h>
using namespace std::string_literals;

namespace {
    ObserverPartyWindow::Settings settings;
}

void ObserverPartyWindow::Initialize()
{
    ToolboxWindow::Initialize();
    SettingsRegistry::Register(this, settings);
}

void ObserverPartyWindow::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxWindow::LoadSettings(doc, legacy);
    doc.GetStruct(Name(), settings);
}

void ObserverPartyWindow::SaveSettings(SettingsDoc& doc)
{
    ToolboxWindow::SaveSettings(doc);
    doc.SetStruct(Name(), settings);
}


void ObserverPartyWindow::DrawHeaders(const size_t party_count) const
{
    float offset = 0;

    if (settings.show_player_number) {
        ImGui::Text("");
        ImGui::SameLine(offset += text_tiny);
    }

    for (size_t i = 0; i < party_count; i += 1) {
        if (settings.show_profession) {
            ImGui::Text(ObserverLabel::Profession);
            ImGui::SameLine(offset += text_short);
        }

        ImGui::Text(ObserverLabel::Name);
        ImGui::SameLine(offset += text_long);

        if (settings.show_player_guild_tag) {
            ImGui::Text(ObserverLabel::PlayerGuildTag);
            ImGui::SameLine(offset += text_short);
        }

        if (settings.show_player_guild_rating) {
            ImGui::Text(ObserverLabel::PlayerGuildRating);
            ImGui::SameLine(offset += text_tiny);
        }

        if (settings.show_player_guild_rank) {
            ImGui::Text(ObserverLabel::PlayerGuildRank);
            ImGui::SameLine(offset += text_tiny);
        }

        if (settings.show_kills) {
            ImGui::Text(ObserverLabel::Kills);
            ImGui::SameLine(offset += text_tiny);
        }

        if (settings.show_deaths) {
            ImGui::Text(ObserverLabel::Deaths);
            ImGui::SameLine(offset += text_tiny);
        }

        if (settings.show_kdr) {
            ImGui::Text(ObserverLabel::KDR);
            ImGui::SameLine(offset += text_tiny);
        }

        if (settings.show_cancels) {
            ImGui::Text(ObserverLabel::Cancels);
            ImGui::SameLine(offset += text_tiny);
        }

        if (settings.show_interrupts) {
            ImGui::Text(ObserverLabel::Interrupts);
            ImGui::SameLine(offset += text_tiny);
        }

        if (settings.show_knockdowns) {
            ImGui::Text(ObserverLabel::Knockdowns);
            ImGui::SameLine(offset += text_tiny);
        }

        if (settings.show_dealt_party_attacks) {
            ImGui::Text(ObserverLabel::AttacksReceivedFromOtherParties);
            ImGui::SameLine(offset += text_tiny);
        }

        if (settings.show_received_party_attacks) {
            ImGui::Text(ObserverLabel::AttacksDealtToOtherParties);
            ImGui::SameLine(offset += text_tiny);
        }

        if (settings.show_received_party_crits) {
            ImGui::Text(ObserverLabel::CritsReceivedFromOtherParties);
            ImGui::SameLine(offset += text_tiny);
        }

        if (settings.show_dealt_party_crits) {
            ImGui::Text(ObserverLabel::CritsDealToOtherParties);
            ImGui::SameLine(offset += text_tiny);
        }

        if (settings.show_received_party_skills) {
            ImGui::Text(ObserverLabel::SkillsReceivedFromOtherParties);
            ImGui::SameLine(offset += text_tiny);
        }

        if (settings.show_dealt_party_skills) {
            ImGui::Text(ObserverLabel::SkillsUsedOnOtherParties);
            ImGui::SameLine(offset += text_tiny);
        }

        if (settings.show_skills_used) {
            ImGui::Text(ObserverLabel::SkillsUsed);
            ImGui::SameLine(offset += text_tiny);
        }

        if (settings.show_damage_dealt) {
            ImGui::Text("伤害+");
            ImGui::SameLine(offset += text_short);
        }

        if (settings.show_damage_received) {
            ImGui::Text("伤害-");
            ImGui::SameLine(offset += text_short);
        }

        if (settings.show_healing_dealt) {
            ImGui::Text("治疗+");
            ImGui::SameLine(offset += text_short);
        }

        if (settings.show_healing_received) {
            ImGui::Text("治疗-");
            ImGui::SameLine(offset += text_short);
        }

        if (settings.show_max_hp) {
            ImGui::Text("最大生命");
            ImGui::SameLine(offset += text_tiny);
        }
    }
}


void ObserverPartyWindow::DrawBlankPartyMember(float& offset) const
{
    uint16_t tinys = 0;
    uint16_t shorts = 0;
    if (settings.show_profession) {
        shorts += 1;
    }
    if (settings.show_player_guild_tag) {
        shorts += 1;
    }
    if (settings.show_player_guild_rating) {
        tinys += 1;
    }
    if (settings.show_player_guild_rank) {
        tinys += 1;
    }
    if (settings.show_kills) {
        tinys += 1;
    }
    if (settings.show_deaths) {
        tinys += 1;
    }
    if (settings.show_kdr) {
        tinys += 1;
    }
    if (settings.show_cancels) {
        tinys += 1;
    }
    if (settings.show_interrupts) {
        tinys += 1;
    }
    if (settings.show_knockdowns) {
        tinys += 1;
    }
    if (settings.show_received_party_attacks) {
        tinys += 1;
    }
    if (settings.show_dealt_party_attacks) {
        tinys += 1;
    }
    if (settings.show_received_party_crits) {
        tinys += 1;
    }
    if (settings.show_dealt_party_crits) {
        tinys += 1;
    }
    if (settings.show_received_party_skills) {
        tinys += 1;
    }
    if (settings.show_dealt_party_skills) {
        tinys += 1;
    }
    if (settings.show_skills_used) {
        tinys += 1;
    }
    if (settings.show_damage_dealt) {
        shorts += 1;
    }
    if (settings.show_damage_received) {
        shorts += 1;
    }
    if (settings.show_healing_dealt) {
        shorts += 1;
    }
    if (settings.show_healing_received) {
        shorts += 1;
    }
    if (settings.show_max_hp) {
        tinys += 1;
    }

    ImGui::Text("");
    ImGui::SameLine(offset += text_long + shorts * text_short + tinys * text_tiny);
}


void ObserverPartyWindow::DrawPartyMember(float& offset, ObserverModule::ObservableAgent& agent, const ObserverModule::ObservableGuild* guild,
                                          const bool odd, const bool, const bool) const
{
    auto& Text = odd ? ImGui::TextDisabled : ImGui::Text;

    if (settings.show_profession) {
        Text(agent.profession.c_str());
        ImGui::SameLine(offset += text_short);
    }

    Text(agent.DisplayName().c_str());
    ImGui::SameLine(offset += text_long);

    if (settings.show_player_guild_tag) {
        if (guild) {
            ImGui::Text(guild->wrapped_tag.c_str());
        }
        else {
            ImGui::Text("");
        }
        ImGui::SameLine(offset += text_short);
    }

    if (settings.show_player_guild_rating) {
        if (guild) {
            ImGui::Text(std::to_string(guild->rating).c_str());
        }
        else {
            ImGui::Text("");
        }
        ImGui::SameLine(offset += text_tiny);
    }

    if (settings.show_player_guild_rank) {
        if (guild) {
            ImGui::Text(std::to_string(guild->rank).c_str());
        }
        else {
            ImGui::Text("");
        }
        ImGui::SameLine(offset += text_tiny);
    }

    if (settings.show_kills) {
        Text(std::to_string(agent.stats.kills).c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    if (settings.show_deaths) {
        Text(std::to_string(agent.stats.deaths).c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    if (settings.show_kdr) {
        Text(agent.stats.kdr_str.c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    if (settings.show_cancels) {
        Text(std::to_string(agent.stats.cancelled_skills_count).c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    if (settings.show_interrupts) {
        Text(std::to_string(agent.stats.interrupted_skills_count).c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    if (settings.show_knockdowns) {
        Text(std::to_string(agent.stats.knocked_down_count).c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    if (settings.show_received_party_attacks) {
        Text(std::to_string(agent.stats.total_attacks_received_from_other_parties.finished).c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    if (settings.show_dealt_party_attacks) {
        Text(std::to_string(agent.stats.total_attacks_dealt_to_other_parties.finished).c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    if (settings.show_received_party_crits) {
        Text(std::to_string(agent.stats.total_party_crits_received).c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    if (settings.show_dealt_party_crits) {
        Text(std::to_string(agent.stats.total_party_crits_dealt).c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    if (settings.show_received_party_skills) {
        Text(std::to_string(agent.stats.total_skills_received_from_other_parties.finished).c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    if (settings.show_dealt_party_skills) {
        Text(std::to_string(agent.stats.total_skills_used_on_other_parties.finished).c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    if (settings.show_skills_used) {
        Text(std::to_string(agent.stats.total_skills_used.finished).c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    if (settings.show_damage_dealt) {
        Text(std::to_string(agent.stats.total_damage_dealt).c_str());
        ImGui::SameLine(offset += text_short);
    }

    if (settings.show_damage_received) {
        Text(std::to_string(agent.stats.total_damage_received).c_str());
        ImGui::SameLine(offset += text_short);
    }

    if (settings.show_healing_dealt) {
        Text(std::to_string(agent.stats.total_healing_dealt).c_str());
        ImGui::SameLine(offset += text_short);
    }

    if (settings.show_healing_received) {
        Text(std::to_string(agent.stats.total_healing_received).c_str());
        ImGui::SameLine(offset += text_short);
    }

    if (settings.show_max_hp) {
        uint32_t max_hp = ObserverModule::Instance().GetCachedMaxHP(agent.agent_id);
        if (max_hp > 0) {
            Text(std::to_string(max_hp).c_str());
        } else {
            Text("-");
        }
        ImGui::SameLine(offset += text_tiny);
    }
}


void ObserverPartyWindow::DrawParty(float& offset, const ObserverModule::ObservableParty& party) const
{
    ImGui::Text(party.display_name.c_str());
    ImGui::SameLine(offset += text_long);

    if (settings.show_profession) {
        ImGui::Text("");
        ImGui::SameLine(offset += text_short);
    }

    if (settings.show_player_guild_tag) {
        // 标签在 display_name 中
        // 这使得它不可隐藏
        ImGui::SameLine(offset += text_short);
    }

    if (settings.show_player_guild_rating) {
        ImGui::Text(std::to_string(party.rating).c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    if (settings.show_player_guild_rank) {
        ImGui::Text(std::to_string(party.rank).c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    if (settings.show_kills) {
        ImGui::Text(std::to_string(party.stats.kills).c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    if (settings.show_deaths) {
        ImGui::Text(std::to_string(party.stats.deaths).c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    if (settings.show_kdr) {
        ImGui::Text(party.stats.kdr_str.c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    if (settings.show_cancels) {
        ImGui::Text(std::to_string(party.stats.cancelled_skills_count).c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    if (settings.show_interrupts) {
        ImGui::Text(std::to_string(party.stats.interrupted_skills_count).c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    if (settings.show_knockdowns) {
        ImGui::Text(std::to_string(party.stats.knocked_down_count).c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    if (settings.show_received_party_attacks) {
        ImGui::Text(std::to_string(party.stats.total_attacks_received_from_other_parties.finished).c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    if (settings.show_dealt_party_attacks) {
        ImGui::Text(std::to_string(party.stats.total_attacks_dealt_to_other_parties.finished).c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    if (settings.show_received_party_crits) {
        ImGui::Text(std::to_string(party.stats.total_party_crits_received).c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    if (settings.show_received_party_crits) {
        ImGui::Text(std::to_string(party.stats.total_party_crits_dealt).c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    if (settings.show_received_party_skills) {
        ImGui::Text(std::to_string(party.stats.total_skills_received_from_other_parties.finished).c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    if (settings.show_dealt_party_skills) {
        ImGui::Text(std::to_string(party.stats.total_skills_used_on_other_parties.finished).c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    if (settings.show_skills_used) {
        ImGui::Text(std::to_string(party.stats.total_skills_used.finished).c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    if (settings.show_damage_dealt) {
        ImGui::Text(std::to_string(party.stats.total_damage_dealt).c_str());
        ImGui::SameLine(offset += text_short);
    }

    if (settings.show_damage_received) {
        ImGui::Text(std::to_string(party.stats.total_damage_received).c_str());
        ImGui::SameLine(offset += text_short);
    }

    if (settings.show_healing_dealt) {
        ImGui::Text(std::to_string(party.stats.total_healing_dealt).c_str());
        ImGui::SameLine(offset += text_short);
    }

    if (settings.show_healing_received) {
        ImGui::Text(std::to_string(party.stats.total_healing_received).c_str());
        ImGui::SameLine(offset += text_short);
    }

    if (settings.show_max_hp) {
        ImGui::Text("");
        ImGui::SameLine(offset += text_tiny);
    }
}


void ObserverPartyWindow::Draw(IDirect3DDevice9*)
{
    if (!visible) {
        return;
    }
    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 250), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        return ImGui::End();
    }

    ObserverModule& observer_module = ObserverModule::Instance();

    // 这应该适用于 2/3（+？）支队伍，优先支持 2 支

    auto max_party_size = 0u;
    const std::vector<uint32_t>& party_ids = observer_module.GetObservablePartyIds();
    const size_t party_count = party_ids.size();
    std::vector<const ObserverModule::ObservableParty*> parties;
    size_t actual_party_count = 0;
    for (const uint32_t party_id : party_ids) {
        const ObserverModule::ObservableParty* party = observer_module.GetObservablePartyById(party_id);
        if (!party) {
            continue;
        }
        parties.push_back(party);
        actual_party_count += 1;
        const auto size = party->agent_ids.size();
        if (size > max_party_size) {
            max_party_size = size;
        }
    }

    const float global = ImGui::FontScale();
    text_long = 200.0f * global;
    text_medium = 150.0f * global;
    text_short = 55.0f * global;
    text_tiny = 40.0f * global;

    for (auto party_member_index = -1; party_member_index < static_cast<int>(max_party_size); party_member_index += 1) {
        if (party_member_index == 0) {
            ImGui::Text("");
            ImGui::Separator();
            DrawHeaders(actual_party_count);
            ImGui::Text("");
            ImGui::Separator();
        }
        // 每个玩家强制换行
        else if (party_member_index > 0) {
            ImGui::Text("");
        }
        // else if (party_member_index > 0) ImGui::Separator();

        float offset = 0;

        for (auto party_index = 0u; party_index < party_count; party_index += 1) {
            if (settings.show_player_number && party_index == 0) {
                // 显示 #1. <玩家> 给玩家，而不是队伍
                if (party_member_index != -1) {
                    ImGui::Text(("# "s + std::to_string(party_member_index + 1) + ".").c_str());
                }
                else {
                    ImGui::Text("");
                }
                ImGui::SameLine(offset += text_tiny);
            }

            const ObserverModule::ObservableParty* party = parties[party_index];
            if (!party) {
                return;
            }

            if (party_member_index == -1) {
                DrawParty(offset, *party);
                continue;
            }

            if (party_member_index >= static_cast<int>(party->agent_ids.size())) {
                DrawBlankPartyMember(offset);
                continue;
            }

            const uint32_t party_member_id = party->agent_ids[party_member_index];

            if (party_member_id == NO_AGENT) {
                DrawBlankPartyMember(offset);
                continue;
            }

            ObserverModule::ObservableAgent* party_member =
                observer_module.GetObservableAgentById(party_member_id);
            if (!party_member) {
                DrawBlankPartyMember(offset);
                continue;
            }

            const ObserverModule::ObservableGuild* guild = observer_module.GetObservableGuildById(party_member->guild_id);
            DrawPartyMember(offset, *party_member, guild, party_member_index % 2, false, false);
        }
    }

    ImGui::End();
}


void ObserverPartyWindow::DrawSettingsInternal()
{
    ImGui::Text("请确保观战模块已启用。");
    ImGui::Checkbox("显示玩家编号 (#)", &settings.show_player_number);
    ImGui::Checkbox(("显示职业 ("s
                     + ObserverLabel::Profession
                     + ")").c_str(), &settings.show_profession);

    ImGui::Checkbox(("显示玩家公会标签 ("s
                     + ObserverLabel::PlayerGuildTag
                     + ")").c_str(), &settings.show_player_guild_tag);

    ImGui::Checkbox(("显示玩家公会等级分 ("s
                     + ObserverLabel::PlayerGuildRating
                     + ")").c_str(), &settings.show_player_guild_rating);

    ImGui::Checkbox(("显示玩家公会排名 ("s
                     + ObserverLabel::PlayerGuildRank
                     + ")").c_str(), &settings.show_player_guild_rank);

    ImGui::Checkbox(("显示击杀 ("s
                     + ObserverLabel::Kills
                     + ")").c_str(), &settings.show_kills);

    ImGui::Checkbox(("显示死亡 ("s
                     + ObserverLabel::Deaths
                     + ")").c_str(), &settings.show_deaths);

    ImGui::Checkbox(("显示击杀/死亡比 ("s
                     + ObserverLabel::KDR
                     + ")").c_str(), &settings.show_kdr);

    ImGui::Checkbox(("显示取消 ("s
                     + ObserverLabel::Cancels
                     + ")").c_str(), &settings.show_cancels);

    ImGui::Checkbox(("显示打断 ("s
                     + ObserverLabel::Interrupts
                     + ")").c_str(), &settings.show_interrupts);

    ImGui::Checkbox(("显示击倒 ("s
                     + ObserverLabel::Knockdowns + ")").c_str(), &settings.show_knockdowns);

    ImGui::Checkbox(("显示来自其他队伍的攻击 ("s
                     + ObserverLabel::AttacksReceivedFromOtherParties
                     + ")").c_str(), &settings.show_received_party_attacks);

    ImGui::Checkbox(("显示对其他队伍的攻击 ("s
                     + ObserverLabel::AttacksDealtToOtherParties
                     + ")").c_str(), &settings.show_dealt_party_attacks);

    ImGui::Checkbox(("显示来自其他队伍的暴击 ("s
                     + ObserverLabel::CritsReceivedFromOtherParties
                     + ")").c_str(), &settings.show_received_party_crits);

    ImGui::Checkbox(("显示对其他队伍的暴击 ("s
                     + ObserverLabel::CritsDealToOtherParties
                     + ")").c_str(), &settings.show_dealt_party_crits);

    ImGui::Checkbox(("显示来自其他队伍的技能 ("s
                     + ObserverLabel::SkillsReceivedFromOtherParties
                     + ")").c_str(),
                    &settings.show_received_party_skills);

    ImGui::Checkbox(("显示对其他队伍使用的技能 ("s
                     + ObserverLabel::SkillsUsedOnOtherParties
                     + ")").c_str(), &settings.show_dealt_party_skills);

    ImGui::Checkbox(("显示使用的技能 ("s
                     + ObserverLabel::SkillsUsed
                     + ")").c_str(), &settings.show_skills_used);

    ImGui::Checkbox("显示造成伤害 (伤害+)", &settings.show_damage_dealt);
    ImGui::Checkbox("显示受到伤害 (伤害-)", &settings.show_damage_received);
    ImGui::Checkbox("显示造成治疗 (治疗+)", &settings.show_healing_dealt);
    ImGui::Checkbox("显示受到治疗 (治疗-)", &settings.show_healing_received);
    ImGui::Checkbox("显示最大生命 (最大生命)", &settings.show_max_hp);
}
