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

#define NO_AGENT 0

// row: [#] [name        ] [kills] [deaths] [kdr] [skill_rupts] [skill_cancels] [skills_finished]

// row:

// [#:tiny]
// [name:long]
// [kills:tiny]
// [deaths:tiny]
// [kdr:tiny]
// [rupts:tiny]
// [cancels:tiny]
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
void ObserverPartyWindow::DrawHeaders(const float _long, const float _tiny, const size_t party_count) {
    float offset = 0;

    ImGui::Text("");
    ImGui::SameLine(offset += _tiny);

    for (size_t i = 0; i < party_count; i += 1) {
        // don't flip it
        // [name:long]
        ImGui::Text("Name");
        ImGui::SameLine(offset += _long);

        // [kills:tiny]
        ImGui::TextDisabled("K");
        ImGui::SameLine(offset += _tiny);

        // [deaths:tiny]
        ImGui::TextDisabled("D");
        ImGui::SameLine(offset += _tiny);

        // [kdr:tiny]
        ImGui::TextDisabled("KDR");
        ImGui::SameLine(offset += _tiny);

        // [rupts:tiny]
        ImGui::TextDisabled("Rpts");
        ImGui::SameLine(offset += _tiny);

        // [cancels:tiny]
        ImGui::TextDisabled("Cncl");
        ImGui::SameLine(offset += _tiny);

        // [kds:tiny]
        ImGui::TextDisabled("Kds");
        ImGui::SameLine(offset += _tiny);

        // [-atk:tiny]
        ImGui::TextDisabled("-Atk");
        ImGui::SameLine(offset += _tiny);

        // [+atk:tiny]
        ImGui::TextDisabled("+Atk");
        ImGui::SameLine(offset += _tiny);

        // [-Crt:tiny]
        ImGui::TextDisabled("-Crt");
        ImGui::SameLine(offset += _tiny);

        // [+crit:tiny]
        ImGui::TextDisabled("+Crt");
        ImGui::SameLine(offset += _tiny);

        // [-skl:tiny]
        ImGui::TextDisabled("-Skl");
        ImGui::SameLine(offset += _tiny);

        // [+skl:tiny]
        ImGui::TextDisabled("+Skl");
        ImGui::SameLine(offset += _tiny);

    }
}


// Draw a blank
void ObserverPartyWindow::DrawBlankPartyMember(float& offset, const float _long, const float _tiny) {
    ImGui::Text("");
    ImGui::SameLine(offset += (_long + 12 * _tiny));
}


// Draw a Party Member
void ObserverPartyWindow::DrawPartyMember(float& offset, const float _long, const float _tiny, ObserverModule::ObservableAgent& agent, const bool odd) {
    if (!odd) {
        // [name:long]
        ImGui::Text(agent.Name().c_str());
        ImGui::SameLine(offset += _long);

        // [kills:tiny]
        ImGui::Text(std::to_string(agent.kills).c_str());
        ImGui::SameLine(offset += _tiny);

        // [deaths:tiny]
        ImGui::Text(std::to_string(agent.deaths).c_str());
        ImGui::SameLine(offset += _tiny);

        // [kdr:tiny]
        ImGui::Text(agent.kdr_str.c_str());
        ImGui::SameLine(offset += _tiny);

        // [rupts:tiny]
        ImGui::Text(std::to_string(agent.interrupted_skills_count).c_str());
        ImGui::SameLine(offset += _tiny);

        // [cancels:tiny]
        ImGui::Text(std::to_string(agent.cancelled_skills_count).c_str());
        ImGui::SameLine(offset += _tiny);

        // [kds:tiny]
        ImGui::Text(std::to_string(agent.knocked_down_count).c_str());
        ImGui::SameLine(offset += _tiny);

        // [-atk:tiny]
        ImGui::Text(std::to_string(agent.total_attacks_received.finished).c_str());
        ImGui::SameLine(offset += _tiny);

        // [+atk:tiny]
        ImGui::Text(std::to_string(agent.total_attacks_done.finished).c_str());
        ImGui::SameLine(offset += _tiny);

        // [-Crt:tiny]
        ImGui::Text(std::to_string(agent.total_crits_received).c_str());
        ImGui::SameLine(offset += _tiny);

        // [+crit:tiny]
        ImGui::Text(std::to_string(agent.total_crits_dealt).c_str());
        ImGui::SameLine(offset += _tiny);

        // [-skl:tiny]
        ImGui::Text(std::to_string(agent.total_skills_received.finished).c_str());
        ImGui::SameLine(offset += _tiny);

        // [+skl:tiny]
        ImGui::Text(std::to_string(agent.total_skills_used.finished).c_str());
        ImGui::SameLine(offset += _tiny);
    } else {
        // [name:long]
        ImGui::TextDisabled(agent.Name().c_str());
        ImGui::SameLine(offset += _long);

        // [kills:tiny]
        ImGui::TextDisabled(std::to_string(agent.kills).c_str());
        ImGui::SameLine(offset += _tiny);

        // [deaths:tiny]
        ImGui::TextDisabled(std::to_string(agent.deaths).c_str());
        ImGui::SameLine(offset += _tiny);

        // [kdr:tiny]
        ImGui::TextDisabled(agent.kdr_str.c_str());
        ImGui::SameLine(offset += _tiny);

        // [rupts:tiny]
        ImGui::TextDisabled(std::to_string(agent.interrupted_skills_count).c_str());
        ImGui::SameLine(offset += _tiny);

        // [cancels:tiny]
        ImGui::TextDisabled(std::to_string(agent.cancelled_skills_count).c_str());
        ImGui::SameLine(offset += _tiny);

        // [kds:tiny]
        ImGui::TextDisabled(std::to_string(agent.knocked_down_count).c_str());
        ImGui::SameLine(offset += _tiny);

        // [-atk:tiny]
        ImGui::TextDisabled(std::to_string(agent.total_attacks_received.finished).c_str());
        ImGui::SameLine(offset += _tiny);

        // [+atk:tiny]
        ImGui::TextDisabled(std::to_string(agent.total_attacks_done.finished).c_str());
        ImGui::SameLine(offset += _tiny);

        // [-Crt:tiny]
        ImGui::TextDisabled(std::to_string(agent.total_crits_received).c_str());
        ImGui::SameLine(offset += _tiny);

        // [+crit:tiny]
        ImGui::TextDisabled(std::to_string(agent.total_crits_dealt).c_str());
        ImGui::SameLine(offset += _tiny);

        // [-skl:tiny]
        ImGui::TextDisabled(std::to_string(agent.total_skills_received.finished).c_str());
        ImGui::SameLine(offset += _tiny);

        // [+skl:tiny]
        ImGui::TextDisabled(std::to_string(agent.total_skills_used.finished).c_str());
        ImGui::SameLine(offset += _tiny);
    }
}


// Draw a Party row
void ObserverPartyWindow::DrawParty(float& offset, const float _long, const float _tiny, ObserverModule::ObservableParty& party) {
    // [name:long]
    // TODO: get guild name & put it here
    ImGui::Text("");
    ImGui::SameLine(offset += _long);

    // [kills:tiny]
    ImGui::Text(std::to_string(party.kills).c_str());
    ImGui::SameLine(offset += _tiny);

    // [deaths:tiny]
    ImGui::Text(std::to_string(party.deaths).c_str());
    ImGui::SameLine(offset += _tiny);

    // [kdr:tiny]
    ImGui::Text(party.kdr_str.c_str());
    ImGui::SameLine(offset += _tiny);

    // [rupts:tiny]
    ImGui::Text(std::to_string(party.interrupted_skills_count).c_str());
    ImGui::SameLine(offset += _tiny);

    // [cancels:tiny]
    ImGui::Text(std::to_string(party.cancelled_skills_count).c_str());
    ImGui::SameLine(offset += _tiny);

    // [kds:tiny]
    ImGui::Text(std::to_string(party.knocked_down_count).c_str());
    ImGui::SameLine(offset += _tiny);

    // [-atk:tiny]
    ImGui::Text(std::to_string(party.total_attacks_received.finished).c_str());
    ImGui::SameLine(offset += _tiny);

    // [+atk:tiny]
    ImGui::Text(std::to_string(party.total_attacks_done.finished).c_str());
    ImGui::SameLine(offset += _tiny);

    // [-Crt:tiny]
    ImGui::Text(std::to_string(party.total_crits_received).c_str());
    ImGui::SameLine(offset += _tiny);

    // [+crit:tiny]
    ImGui::Text(std::to_string(party.total_crits_dealt).c_str());
    ImGui::SameLine(offset += _tiny);

    // [-skl:tiny]
    ImGui::Text(std::to_string(party.total_skills_received.finished).c_str());
    ImGui::SameLine(offset += _tiny);

    // [+skl:tiny]
    ImGui::Text(std::to_string(party.total_skills_used.finished).c_str());
    ImGui::SameLine(offset += _tiny);

}


// Draw everything
void ObserverPartyWindow::Draw(IDirect3DDevice9* pDevice) {
    UNREFERENCED_PARAMETER(pDevice);
    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 250), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags()))
        return ImGui::End();

    ObserverModule& observer_module = ObserverModule::Instance();
    const bool is_active = observer_module.IsActive();

    // this should work with both 2 and 3(+?) parties, with preference on 2

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


  const float _long	= 200.0f * ImGui::GetIO().FontGlobalScale;
  const float _tiny	= 40.0f * ImGui::GetIO().FontGlobalScale;

    DrawHeaders(_long, _tiny, actual_party_count);
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
            if (party_index == 0) {
            // print #1. <player> for players, not the party
                if (party_member_index != -1) {
                    ImGui::Text((std::string("# ") + std::to_string(party_member_index + 1) + ".").c_str());
                } else {
                    ImGui::Text("");
                }
                ImGui::SameLine(offset += _tiny);
            }

      ObserverModule::ObservableParty* party = parties[party_index];
            if (party == nullptr) {
                // nothing to print...
                continue;
            }

            // draw party total
            if (party_member_index == -1) {
        DrawParty(offset, _long, _tiny, *party);
                continue;
            }


            if (party_member_index >= static_cast<int>(party->agent_ids.size())) {
                // overflowed party size
                // fill blank...
                DrawBlankPartyMember(offset, _long, _tiny);
                continue;
            }

      uint32_t party_member_id = party->agent_ids[party_member_index];

            // no party_member
      if (party_member_id == NO_AGENT) {
        DrawBlankPartyMember(offset, _long, _tiny);
        continue;
      }

            // party_member not found
      ObserverModule::ObservableAgent* party_member =
        observer_module.GetObservableAgentByAgentId(party_member_id);
      if (party_member == nullptr) {
        DrawBlankPartyMember(offset, _long, _tiny);
                continue;
      }

            // party member found!
            DrawPartyMember(offset, _long, _tiny, *party_member, party_member_index % 2);
    }
    }

    ImGui::End();
}
