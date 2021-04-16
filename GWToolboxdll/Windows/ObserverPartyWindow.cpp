#include "stdafx.h"
#include <cstdlib>


#include <Logger.h>
#include <GuiUtils.h>

#include <GWCA/Utilities/Macros.h>
#include <GWCA/Utilities/Scanner.h>

#include <GWCA/GameEntities/Agent.h>

#include <GWCA/Managers/AgentMgr.h>

#include <Modules/ObserverModule.h>

#include <Windows/ObserverPartyWindow.h>

#include <Modules/Resources.h>

#define NO_AGENT 0

// row: [#] [name        ] [kills] [deaths] [kdr] [skill_rupts] [skill_cancels] [skills_finished]

// row:

// [#:tiny]
// [name:long]
// [kills:tiny]
// [deaths:tiny]
// [kdr:tiny]
// [cancels:tiny]
// [rupts:tiny]
// [kds:tiny]
// [-atk:tiny]
// [+atk:tiny]
// [-crt:tiny]
// [+crt:tiny]
// [-skl:tiny]
// [+skl:tiny]



void ObserverPartyWindow::Initialize() {
    ToolboxWindow::Initialize();
}


// Draw stats headers for the parties
void ObserverPartyWindow::DrawHeaders(const size_t party_count) {
    float offset = 0;

	if (show_player_number) {
		ImGui::Text("");
		ImGui::SameLine(offset += text_tiny);
    }

    for (size_t i = 0; i < party_count; i += 1) {
        // don't flip it
        // [name:long]
        ImGui::Text(ObserverLabel::Name);
        ImGui::SameLine(offset += text_long);

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

    }
}


// Draw a blank
void ObserverPartyWindow::DrawBlankPartyMember(float& offset) {
	uint16_t tinys = 0;
    if (!show_kills) tinys += 1;
    if (!show_deaths) tinys += 1;
    if (!show_kdr) tinys += 1;
    if (!show_cancels) tinys += 1;
	if (!show_interrupts) tinys += 1;
	if (!show_knockdowns) tinys += 1;
	if (!show_received_party_attacks) tinys += 1;
	if (!show_dealt_party_attacks) tinys += 1;
	if (!show_received_party_crits) tinys += 1;
	if (!show_dealt_party_crits) tinys += 1;
	if (!show_received_party_skills) tinys += 1;
	if (!show_dealt_party_skills) tinys += 1;

    ImGui::Text("");
    ImGui::SameLine(offset += (text_long + tinys * text_tiny));
}


// Draw a Party Member
void ObserverPartyWindow::DrawPartyMember(float& offset, ObserverModule::ObservableAgent& agent, const bool odd,
    const bool is_player, const bool is_target) {
    UNREFERENCED_PARAMETER(is_player);
    UNREFERENCED_PARAMETER(is_target);

    auto& Text = odd ? ImGui::TextDisabled : ImGui::Text;

	// [name:long]
	Text(agent.Name().c_str());
	ImGui::SameLine(offset += text_long);

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
		Text(std::to_string(agent.stats.total_attacks_received_by_other_party.finished).c_str());
		ImGui::SameLine(offset += text_tiny);
    }

	// [+atk:tiny]
    if (show_dealt_party_attacks) {
		Text(std::to_string(agent.stats.total_attacks_done_on_other_party.finished).c_str());
		ImGui::SameLine(offset += text_tiny);
    }

	// [-Crt:tiny]
    if(show_received_party_crits) {
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
		Text(std::to_string(agent.stats.total_skills_received_by_other_party.finished).c_str());
		ImGui::SameLine(offset += text_tiny);
    }

	// [+skl:tiny]
    if (show_dealt_party_skills) {
		Text(std::to_string(agent.stats.total_skills_used_on_other_party.finished).c_str());
		ImGui::SameLine(offset += text_tiny);
    }
}


// Draw a Party row
void ObserverPartyWindow::DrawParty(float& offset, const ObserverModule::ObservableParty& party) {
    // [name:long]
    // TODO: get guild name & put it here
    ImGui::Text("");
    ImGui::SameLine(offset += text_long);

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
		ImGui::Text(std::to_string(party.stats.total_attacks_received_by_other_party.finished).c_str());
		ImGui::SameLine(offset += text_tiny);
    }

    // [+atk:tiny]
    if (show_dealt_party_attacks) {
		ImGui::Text(std::to_string(party.stats.total_attacks_done_on_other_party.finished).c_str());
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
		ImGui::Text(std::to_string(party.stats.total_skills_received_by_other_party.finished).c_str());
		ImGui::SameLine(offset += text_tiny);
    }

    // [+skl:tiny]
    if (show_dealt_party_skills) {
		ImGui::Text(std::to_string(party.stats.total_skills_used_on_other_party.finished).c_str());
		ImGui::SameLine(offset += text_tiny);
    }

}


// Draw everything
void ObserverPartyWindow::Draw(IDirect3DDevice9* pDevice) {
    UNREFERENCED_PARAMETER(pDevice);
    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 250), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) return ImGui::End();

    // TODO: background colour in the currently following player
    // TODO: background colour in the currently targetted player
    // use ObserverPartyWindow::Draw for inspiration for colouring in backgrounds

    ObserverModule& observer_module = ObserverModule::Instance();
    const bool is_active = observer_module.IsActive();

    // this should work with both 2/3(+?) parties, with preference on 2

    int max_party_size = 0;
    const std::vector<ObserverModule::ObservableParty*> parties = observer_module.GetObservablePartyArray();
    size_t party_count = parties.size();
    size_t actual_party_count = 0;
    for (const ObserverModule::ObservableParty* party : parties) {
        if (party == nullptr) continue;
        actual_party_count += 1;
        int size = static_cast<int>(party->agent_ids.size());
        if (size > max_party_size) max_party_size = size;
    }


	float global = ImGui::GetIO().FontGlobalScale;
	text_long   = 200.0f * global;
	text_medium = 150.0f * global;
	text_short  = 80.0f  * global;
	text_tiny	= 40.0f  * global;

    DrawHeaders(actual_party_count);
    ImGui::Text(""); // new line

    ImGui::Separator();

    // iterate through each party member
    for (int party_member_index = -1; party_member_index < max_party_size; party_member_index += 1) {
        // `party_member_offset == -1` is the party info
        // `party_member_offset == 0` is player 1
        // put a separator before player 1
        if (party_member_index == 0) {
            ImGui::Text("");
            ImGui::Separator();
        }
        // force new line
        else if (party_member_index > 0) ImGui::Text("");
        // else if (party_member_index > 0) ImGui::Separator();

        // line offset
		float offset = 0;

		// iterate through each party
		for (size_t party_index = 0; party_index < party_count; party_index += 1) {
			// party member #
			if (show_player_number && party_index == 0) {
				// print #1. <player> for players, not the party
				if (party_member_index != -1) {
					ImGui::Text((std::string("# ") + std::to_string(party_member_index + 1) + ".").c_str());
				} else {
					ImGui::Text("");
				}
				ImGui::SameLine(offset += text_tiny);
			}

		    ObserverModule::ObservableParty* party = parties[party_index];
			if (party == nullptr) {
				// nothing to print...
			    continue;
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

		    uint32_t party_member_id = party->agent_ids[party_member_index];

			// no party_member
	        if (party_member_id == NO_AGENT) {
		        DrawBlankPartyMember(offset);
		        continue;
		    }

			// party_member not found
	        ObserverModule::ObservableAgent* party_member =
				observer_module.GetObservableAgentById(party_member_id);
	        if (party_member == nullptr) {
			    DrawBlankPartyMember(offset);
			    continue;
	        }

	        // party member found!
	        DrawPartyMember(offset, *party_member, party_member_index % 2, false, false);
		}
    }

    ImGui::End();
}


// Load settings
void ObserverPartyWindow::LoadSettings(CSimpleIni* ini) {
	ToolboxWindow::LoadSettings(ini);

	show_player_number = ini->GetBoolValue(Name(), VAR_NAME(show_player_number), true);
    show_kills = ini->GetBoolValue(Name(), VAR_NAME(show_kills), true);
    show_deaths = ini->GetBoolValue(Name(), VAR_NAME(show_deaths), true);
    show_kdr = ini->GetBoolValue(Name(), VAR_NAME(show_kdr), true);
    show_cancels = ini->GetBoolValue(Name(), VAR_NAME(show_cancels), true);
	show_interrupts = ini->GetBoolValue(Name(), VAR_NAME(show_interrupts), true);
	show_knockdowns = ini->GetBoolValue(Name(), VAR_NAME(show_knockdowns), true);
	show_received_party_attacks = ini->GetBoolValue(Name(), VAR_NAME(show_received_party_attacks), true);
	show_dealt_party_attacks = ini->GetBoolValue(Name(), VAR_NAME(show_dealt_party_attacks), true);
	show_received_party_crits = ini->GetBoolValue(Name(), VAR_NAME(show_received_party_crits), true);
	show_dealt_party_crits = ini->GetBoolValue(Name(), VAR_NAME(show_dealt_party_crits), true);
	show_received_party_skills = ini->GetBoolValue(Name(), VAR_NAME(show_received_party_skills), true);
	show_dealt_party_skills = ini->GetBoolValue(Name(), VAR_NAME(show_dealt_party_skills), true);
}


// Save settings
void ObserverPartyWindow::SaveSettings(CSimpleIni* ini) {
	ToolboxWindow::SaveSettings(ini);

	ini->SetBoolValue(Name(), VAR_NAME(show_player_number), show_player_number);
    ini->SetBoolValue(Name(), VAR_NAME(show_kills), show_kills);
    ini->SetBoolValue(Name(), VAR_NAME(show_deaths), show_deaths);
    ini->SetBoolValue(Name(), VAR_NAME(show_kdr), show_kdr);
    ini->SetBoolValue(Name(), VAR_NAME(show_cancels), show_cancels);
	ini->SetBoolValue(Name(), VAR_NAME(show_interrupts), show_interrupts);
	ini->SetBoolValue(Name(), VAR_NAME(show_knockdowns), show_knockdowns);
	ini->SetBoolValue(Name(), VAR_NAME(show_received_party_attacks), show_received_party_attacks);
	ini->SetBoolValue(Name(), VAR_NAME(show_dealt_party_attacks), show_dealt_party_attacks);
	ini->SetBoolValue(Name(), VAR_NAME(show_received_party_crits), show_received_party_crits);
	ini->SetBoolValue(Name(), VAR_NAME(show_dealt_party_crits), show_dealt_party_crits);
	ini->SetBoolValue(Name(), VAR_NAME(show_received_party_skills), show_received_party_skills);
	ini->SetBoolValue(Name(), VAR_NAME(show_dealt_party_skills), show_dealt_party_skills);
}

// Draw settings
void ObserverPartyWindow::DrawSettingInternal() {
    ImGui::Checkbox("Show player number (#)", &show_player_number);
    ImGui::Checkbox((std::string("Show kills (")
		+ ObserverLabel::Kills
		+ ")").c_str(), &show_kills);

    ImGui::Checkbox((std::string("Show deaths (")
		+ ObserverLabel::Deaths
		+ ")").c_str(), &show_deaths);

    ImGui::Checkbox((std::string("Show KDR (")
		+ ObserverLabel::KDR
		+ ")").c_str(), &show_kdr);

    ImGui::Checkbox((std::string("Show cancels (")
		+ ObserverLabel::Cancels
		+ ")").c_str(), &show_cancels);

	ImGui::Checkbox((std::string("Show interrupts (")
		+ ObserverLabel::Interrupts
		+ ")").c_str(), &show_interrupts);

	ImGui::Checkbox((std::string("Show knockdowns (")
		+ ObserverLabel::Knockdowns + ")").c_str(), &show_knockdowns);

	ImGui::Checkbox((std::string("Show attacks from other parties (")
		+ ObserverLabel::AttacksReceivedFromOtherParties
		+ ")").c_str(), &show_received_party_attacks);

	ImGui::Checkbox((std::string("Show attacks to other parties (")
		+ ObserverLabel::AttacksDealtToOtherParties
		+ ")").c_str(), &show_dealt_party_attacks);

	ImGui::Checkbox((std::string("Show crits from other parties (")
		+ ObserverLabel::CritsReceivedFromOtherParties
		+ ")").c_str(), &show_received_party_crits);

	ImGui::Checkbox((std::string("Show crits to other parties (")
		+ ObserverLabel::CritsDealToOtherParties
		+ ")").c_str(), &show_dealt_party_crits);

    ImGui::Checkbox((std::string("Show skills from other parties (")
		+ ObserverLabel ::SkillsReceivedFromOtherParties
		+ ")") .c_str(),
        &show_received_party_skills);

	ImGui::Checkbox((std::string("Show skills used on other parties (")
		+ ObserverLabel::SkillsUsedOnOtherParties
		+ ")").c_str(), &show_dealt_party_skills);
}
