#include "stdafx.h"

#include <Utils/GuiUtils.h>

#include <Modules/ObserverModule.h>

#include <Windows/ObserverPartyWindow.h>
using namespace std::string_literals;

void ObserverPartyWindow::Initialize()
{
    ToolboxWindow::Initialize();
}


// Draw stats headers for the parties
void ObserverPartyWindow::DrawHeaders(const size_t party_count) const
{
    float offset = 0;

    if (show_player_number) {
        ImGui::Text("");
        ImGui::SameLine(offset += text_tiny);
    }

    for (size_t i = 0; i < party_count; i += 1) {
        // [profession:short]
        if (show_profession) {
            ImGui::Text(ObserverLabel::Profession);
            ImGui::SameLine(offset += text_short);
        }

        // [name:long]
        ImGui::Text(ObserverLabel::Name);
        ImGui::SameLine(offset += text_long);

        // [guild-tag:short]
        if (show_player_guild_tag) {
            ImGui::Text(ObserverLabel::PlayerGuildTag);
            ImGui::SameLine(offset += text_short);
        }

        // [guild-rating:tiny]
        if (show_player_guild_rating) {
            ImGui::Text(ObserverLabel::PlayerGuildRating);
            ImGui::SameLine(offset += text_tiny);
        }

        // [guild-rank]
        if (show_player_guild_rank) {
            ImGui::Text(ObserverLabel::PlayerGuildRank);
            ImGui::SameLine(offset += text_tiny);
        }

        // [kills:tiny]
        if (show_kills) {
            ImGui::Text(ObserverLabel::Kills);
            ImGui::SameLine(offset += text_tiny);
        }

        // [deaths:tiny]
        if (show_deaths) {
            ImGui::Text(ObserverLabel::Deaths);
            ImGui::SameLine(offset += text_tiny);
        }

        // [kdr:tiny]
        if (show_kdr) {
            ImGui::Text(ObserverLabel::KDR);
            ImGui::SameLine(offset += text_tiny);
        }

        // [cancels:tiny]
        if (show_cancels) {
            ImGui::Text(ObserverLabel::Cancels);
            ImGui::SameLine(offset += text_tiny);
        }

        // [rupts:tiny]
        if (show_interrupts) {
            ImGui::Text(ObserverLabel::Interrupts);
            ImGui::SameLine(offset += text_tiny);
        }

        // [kds:tiny]
        if (show_knockdowns) {
            ImGui::Text(ObserverLabel::Knockdowns);
            ImGui::SameLine(offset += text_tiny);
        }

        // [-atk:tiny]
        if (show_dealt_party_attacks) {
            ImGui::Text(ObserverLabel::AttacksReceivedFromOtherParties);
            ImGui::SameLine(offset += text_tiny);
        }

        // [+atk:tiny]
        if (show_received_party_attacks) {
            ImGui::Text(ObserverLabel::AttacksDealtToOtherParties);
            ImGui::SameLine(offset += text_tiny);
        }

        // [-Crt:tiny]
        if (show_received_party_crits) {
            ImGui::Text(ObserverLabel::CritsReceivedFromOtherParties);
            ImGui::SameLine(offset += text_tiny);
        }

        // [+crit:tiny]
        if (show_dealt_party_crits) {
            ImGui::Text(ObserverLabel::CritsDealToOtherParties);
            ImGui::SameLine(offset += text_tiny);
        }

        // [-skl:tiny]
        if (show_received_party_skills) {
            ImGui::Text(ObserverLabel::SkillsReceivedFromOtherParties);
            ImGui::SameLine(offset += text_tiny);
        }

        // [+skl:tiny]
        if (show_dealt_party_skills) {
            ImGui::Text(ObserverLabel::SkillsUsedOnOtherParties);
            ImGui::SameLine(offset += text_tiny);
        }

        // [+skl:tiny]
        if (show_skills_used) {
            ImGui::Text(ObserverLabel::SkillsUsed);
            ImGui::SameLine(offset += text_tiny);
        }
    }
}


// Draw a blank
void ObserverPartyWindow::DrawBlankPartyMember(float& offset) const
{
    uint16_t tinys = 0;
    uint16_t shorts = 0;
    if (show_profession) {
        shorts += 1;
    }
    if (show_player_guild_tag) {
        shorts += 1;
    }
    if (show_player_guild_rating) {
        tinys += 1;
    }
    if (show_player_guild_rank) {
        tinys += 1;
    }
    if (show_kills) {
        tinys += 1;
    }
    if (show_deaths) {
        tinys += 1;
    }
    if (show_kdr) {
        tinys += 1;
    }
    if (show_cancels) {
        tinys += 1;
    }
    if (show_interrupts) {
        tinys += 1;
    }
    if (show_knockdowns) {
        tinys += 1;
    }
    if (show_received_party_attacks) {
        tinys += 1;
    }
    if (show_dealt_party_attacks) {
        tinys += 1;
    }
    if (show_received_party_crits) {
        tinys += 1;
    }
    if (show_dealt_party_crits) {
        tinys += 1;
    }
    if (show_received_party_skills) {
        tinys += 1;
    }
    if (show_dealt_party_skills) {
        tinys += 1;
    }

    ImGui::Text("");
    ImGui::SameLine(offset += text_long + shorts * text_short + tinys * text_tiny);
}


// Draw a Party Member
void ObserverPartyWindow::DrawPartyMember(float& offset, ObserverModule::ObservableAgent& agent, const ObserverModule::ObservableGuild* guild,
                                          const bool odd, const bool, const bool) const
{
    auto& Text = odd ? ImGui::TextDisabled : ImGui::Text;

    // [profession:short]
    if (show_profession) {
        Text(agent.profession.c_str());
        ImGui::SameLine(offset += text_short);
    }

    // [name:long]
    Text(agent.DisplayName().c_str());
    ImGui::SameLine(offset += text_long);

    // [guild-tag:short]
    if (show_player_guild_tag) {
        if (guild) {
            ImGui::Text(guild->wrapped_tag.c_str());
        }
        else {
            ImGui::Text("");
        }
        ImGui::SameLine(offset += text_short);
    }

    // [guild-rating:tiny]
    if (show_player_guild_rating) {
        if (guild) {
            ImGui::Text(std::to_string(guild->rating).c_str());
        }
        else {
            ImGui::Text("");
        }
        ImGui::SameLine(offset += text_tiny);
    }

    // [guild-rank]
    if (show_player_guild_rank) {
        if (guild) {
            ImGui::Text(std::to_string(guild->rank).c_str());
        }
        else {
            ImGui::Text("");
        }
        ImGui::SameLine(offset += text_tiny);
    }

    // [kills:tiny]
    if (show_kills) {
        Text(std::to_string(agent.stats.kills).c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    // [deaths:tiny]
    if (show_deaths) {
        Text(std::to_string(agent.stats.deaths).c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    // [kdr:tiny]
    if (show_kdr) {
        Text(agent.stats.kdr_str.c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    // [cancels:tiny]
    if (show_cancels) {
        Text(std::to_string(agent.stats.cancelled_skills_count).c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    // [rupts:tiny]
    if (show_interrupts) {
        Text(std::to_string(agent.stats.interrupted_skills_count).c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    // [kds:tiny]
    if (show_knockdowns) {
        Text(std::to_string(agent.stats.knocked_down_count).c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    // [-atk:tiny]
    if (show_received_party_attacks) {
        Text(std::to_string(agent.stats.total_attacks_received_from_other_parties.finished).c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    // [+atk:tiny]
    if (show_dealt_party_attacks) {
        Text(std::to_string(agent.stats.total_attacks_dealt_to_other_parties.finished).c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    // [-Crt:tiny]
    if (show_received_party_crits) {
        Text(std::to_string(agent.stats.total_party_crits_received).c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    // [+crit:tiny]
    if (show_dealt_party_crits) {
        Text(std::to_string(agent.stats.total_party_crits_dealt).c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    // [-skl:tiny]
    if (show_received_party_skills) {
        Text(std::to_string(agent.stats.total_skills_received_from_other_parties.finished).c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    // [+skl:tiny]
    if (show_dealt_party_skills) {
        Text(std::to_string(agent.stats.total_skills_used_on_other_parties.finished).c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    // [+skl:tiny]
    if (show_skills_used) {
        Text(std::to_string(agent.stats.total_skills_used.finished).c_str());
        ImGui::SameLine(offset += text_tiny);
    }
}


// Draw a Party row
void ObserverPartyWindow::DrawParty(float& offset, const ObserverModule::ObservableParty& party) const
{
    // [name:long]
    ImGui::Text(party.display_name.c_str());
    ImGui::SameLine(offset += text_long);

    // [profession:tiny]
    if (show_profession) {
        ImGui::Text("");
        ImGui::SameLine(offset += text_short);
    }

    // [guild-tag:short]
    if (show_player_guild_tag) {
        // tag is in display_name
        // this makes it not hideable
        ImGui::SameLine(offset += text_short);
    }

    // [guild-rating:tiny]
    if (show_player_guild_rating) {
        ImGui::Text(std::to_string(party.rating).c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    // [guild-rank]
    if (show_player_guild_rank) {
        ImGui::Text(std::to_string(party.rank).c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    // [kills:tiny]
    if (show_kills) {
        ImGui::Text(std::to_string(party.stats.kills).c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    // [deaths:tiny]
    if (show_deaths) {
        ImGui::Text(std::to_string(party.stats.deaths).c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    // [kdr:tiny]
    if (show_kdr) {
        ImGui::Text(party.stats.kdr_str.c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    // [cancels:tiny]
    if (show_cancels) {
        ImGui::Text(std::to_string(party.stats.cancelled_skills_count).c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    // [rupts:tiny]
    if (show_interrupts) {
        ImGui::Text(std::to_string(party.stats.interrupted_skills_count).c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    // [kds:tiny]
    if (show_knockdowns) {
        ImGui::Text(std::to_string(party.stats.knocked_down_count).c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    // [-atk:tiny]
    if (show_received_party_attacks) {
        ImGui::Text(std::to_string(party.stats.total_attacks_received_from_other_parties.finished).c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    // [+atk:tiny]
    if (show_dealt_party_attacks) {
        ImGui::Text(std::to_string(party.stats.total_attacks_dealt_to_other_parties.finished).c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    // [-Crt:tiny]
    if (show_received_party_crits) {
        ImGui::Text(std::to_string(party.stats.total_party_crits_received).c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    // [+crit:tiny]
    if (show_received_party_crits) {
        ImGui::Text(std::to_string(party.stats.total_party_crits_dealt).c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    // [-skl:tiny]
    if (show_received_party_skills) {
        ImGui::Text(std::to_string(party.stats.total_skills_received_from_other_parties.finished).c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    // [+skl:tiny]
    if (show_dealt_party_skills) {
        ImGui::Text(std::to_string(party.stats.total_skills_used_on_other_parties.finished).c_str());
        ImGui::SameLine(offset += text_tiny);
    }

    // [+skl:tiny]
    if (show_skills_used) {
        ImGui::Text(std::to_string(party.stats.total_skills_used.finished).c_str());
        ImGui::SameLine(offset += text_tiny);
    }
}


// Draw everything
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

    // TODO: background colour in the currently following player
    // TODO: background colour in the currently targetted player
    // use ObserverPartyWindow::Draw for inspiration for colouring in backgrounds

    ObserverModule& observer_module = ObserverModule::Instance();

    // this should work with both 2/3(+?) parties, with preference on 2

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

    const float global = ImGui::GetIO().FontGlobalScale;
    text_long = 200.0f * global;
    text_medium = 150.0f * global;
    text_short = 55.0f * global;
    text_tiny = 40.0f * global;

    // DrawHeaders(actual_party_count);
    // ImGui::Text(""); // new line
    // ImGui::Separator();

    // iterate through each party member
    for (auto party_member_index = -1; party_member_index < static_cast<int>(max_party_size); party_member_index += 1) {
        // `party_member_offset == -1` is the party info
        // `party_member_offset == 0` is player 1
        // put a separator before player 1
        if (party_member_index == 0) {
            ImGui::Text("");
            ImGui::Separator();
            DrawHeaders(actual_party_count);
            ImGui::Text("");
            ImGui::Separator();
        }
        // force new line for each player
        else if (party_member_index > 0) {
            ImGui::Text("");
        }
        // else if (party_member_index > 0) ImGui::Separator();

        // line offset
        float offset = 0;

        // iterate through each party
        for (auto party_index = 0u; party_index < party_count; party_index += 1) {
            // party member #
            if (show_player_number && party_index == 0) {
                // print #1. <player> for players, not the party
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

            // draw party total
            if (party_member_index == -1) {
                DrawParty(offset, *party);
                continue;
            }

            if (party_member_index >= static_cast<int>(party->agent_ids.size())) {
                // overflowed party size
                // fill blank...
                DrawBlankPartyMember(offset);
                continue;
            }

            const uint32_t party_member_id = party->agent_ids[party_member_index];

            // no party_member
            if (party_member_id == NO_AGENT) {
                DrawBlankPartyMember(offset);
                continue;
            }

            // party_member not found
            ObserverModule::ObservableAgent* party_member =
                observer_module.GetObservableAgentById(party_member_id);
            if (!party_member) {
                DrawBlankPartyMember(offset);
                continue;
            }

            // party member found!
            const ObserverModule::ObservableGuild* guild = observer_module.GetObservableGuildById(party_member->guild_id);
            DrawPartyMember(offset, *party_member, guild, party_member_index % 2, false, false);
        }
    }

    ImGui::End();
}


// Load settings
void ObserverPartyWindow::LoadSettings(ToolboxIni* ini)
{
    ToolboxWindow::LoadSettings(ini);



















}


// Save settings
void ObserverPartyWindow::SaveSettings(ToolboxIni* ini)
{
    ToolboxWindow::SaveSettings(ini);

    SAVE_BOOL(show_player_number);
    SAVE_BOOL(show_profession);
    SAVE_BOOL(show_player_guild_tag);
    SAVE_BOOL(show_player_guild_rank);
    SAVE_BOOL(show_player_guild_rating);
    SAVE_BOOL(show_kills);
    SAVE_BOOL(show_deaths);
    SAVE_BOOL(show_kdr);
    SAVE_BOOL(show_cancels);
    SAVE_BOOL(show_interrupts);
    SAVE_BOOL(show_knockdowns);
    SAVE_BOOL(show_received_party_attacks);
    SAVE_BOOL(show_dealt_party_attacks);
    SAVE_BOOL(show_received_party_crits);
    SAVE_BOOL(show_dealt_party_crits);
    SAVE_BOOL(show_received_party_skills);
    SAVE_BOOL(show_dealt_party_skills);
    SAVE_BOOL(show_skills_used);
}

// Draw settings
void ObserverPartyWindow::DrawSettingsInternal()
{
    ImGui::Text("Make sure the Observer Module is enabled.");
    ImGui::Checkbox("Show player number (#)", &show_player_number);
    ImGui::Checkbox(("Show professions ("s
                     + ObserverLabel::Profession
                     + ")").c_str(), &show_profession);

    ImGui::Checkbox(("Show Player Guild Tags ("s
                     + ObserverLabel::PlayerGuildTag
                     + ")").c_str(), &show_player_guild_tag);

    ImGui::Checkbox(("Show Player Guild Rating ("s
                     + ObserverLabel::PlayerGuildRating
                     + ")").c_str(), &show_player_guild_rating);

    ImGui::Checkbox(("Show Player Guild Rank ("s
                     + ObserverLabel::PlayerGuildRank
                     + ")").c_str(), &show_player_guild_rank);

    ImGui::Checkbox(("Show kills ("s
                     + ObserverLabel::Kills
                     + ")").c_str(), &show_kills);

    ImGui::Checkbox(("Show deaths ("s
                     + ObserverLabel::Deaths
                     + ")").c_str(), &show_deaths);

    ImGui::Checkbox(("Show KDR ("s
                     + ObserverLabel::KDR
                     + ")").c_str(), &show_kdr);

    ImGui::Checkbox(("Show cancels ("s
                     + ObserverLabel::Cancels
                     + ")").c_str(), &show_cancels);

    ImGui::Checkbox(("Show interrupts ("s
                     + ObserverLabel::Interrupts
                     + ")").c_str(), &show_interrupts);

    ImGui::Checkbox(("Show knockdowns ("s
                     + ObserverLabel::Knockdowns + ")").c_str(), &show_knockdowns);

    ImGui::Checkbox(("Show attacks from other parties ("s
                     + ObserverLabel::AttacksReceivedFromOtherParties
                     + ")").c_str(), &show_received_party_attacks);

    ImGui::Checkbox(("Show attacks to other parties ("s
                     + ObserverLabel::AttacksDealtToOtherParties
                     + ")").c_str(), &show_dealt_party_attacks);

    ImGui::Checkbox(("Show crits from other parties ("s
                     + ObserverLabel::CritsReceivedFromOtherParties
                     + ")").c_str(), &show_received_party_crits);

    ImGui::Checkbox(("Show crits to other parties ("s
                     + ObserverLabel::CritsDealToOtherParties
                     + ")").c_str(), &show_dealt_party_crits);

    ImGui::Checkbox(("Show skills from other parties ("s
                     + ObserverLabel::SkillsReceivedFromOtherParties
                     + ")").c_str(),
                    &show_received_party_skills);

    ImGui::Checkbox(("Show skills used on other parties ("s
                     + ObserverLabel::SkillsUsedOnOtherParties
                     + ")").c_str(), &show_dealt_party_skills);

    ImGui::Checkbox(("Show skills used ("s
                     + ObserverLabel::SkillsUsed
                     + ")").c_str(), &show_skills_used);
}
