#include "stdafx.h"
#include <stdint.h>

#include <GWCA\stdafx.h>
#include <ShellApi.h>

#include <string>
#include <functional>


#include <GWCA\Constants\Constants.h>

#include <GWCA\GameContainers\Array.h>
#include <GWCA\GameContainers\GamePos.h>

#include <GWCA\Packets\StoC.h>

#include <GWCA\GameEntities\Map.h>

#include <GWCA\Managers\UIMgr.h>
#include <GWCA\Managers\MapMgr.h>
#include <GWCA\Managers\ChatMgr.h>
#include <GWCA\Managers\AgentMgr.h>
#include <GWCA\Managers\SkillbarMgr.h>
#include <GWCA\Managers\StoCMgr.h>

#include <logger.h>
#include "GuiUtils.h"
#include <Modules\Resources.h>
#include "FactionLeaderboardWindow.h"



void FactionLeaderboardWindow::Initialize() {
	ToolboxWindow::Initialize();
	leaderboard.resize(15);
	Resources::Instance().LoadTextureAsync(&button_texture, Resources::GetPath(L"img/icons", L"list.png"), IDB_Icon_list);
	GW::StoC::AddCallback<GW::Packet::StoC::TownAllianceObject>(
		[this](GW::Packet::StoC::TownAllianceObject *pak) -> bool {
		LeaderboardEntry leaderboardEntry = {
			pak->map_id,
			pak->rank,
			pak->allegiance,
			pak->faction,
			pak->name,
			pak->tag
		};
		if (leaderboard.size() <= leaderboardEntry.rank)
			leaderboard.resize(leaderboardEntry.rank + 1);
		leaderboard.emplace(leaderboard.begin() + leaderboardEntry.rank, leaderboardEntry);
		return false;
	});
}

void FactionLeaderboardWindow::Terminate() {
	ToolboxWindow::Terminate();
}
void FactionLeaderboardWindow::Draw(IDirect3DDevice9* pDevice) {
	if (!visible)
		return;
	ImGui::SetNextWindowPosCenter(ImGuiSetCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(300, 250), ImGuiSetCond_FirstUseEver);
	if (!ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags()))
		return ImGui::End();
	float offset = 0.0f;
	const float tiny_text_width = 50.0f * ImGui::GetIO().FontGlobalScale;
	const float short_text_width = 80.0f * ImGui::GetIO().FontGlobalScale;
	const float avail_width = ImGui::GetContentRegionAvailWidth();
	const float long_text_width = 200.0f * ImGui::GetIO().FontGlobalScale;
	ImGui::Text("Rank");
	ImGui::SameLine(offset += tiny_text_width);
	ImGui::Text("Allegiance");
	ImGui::SameLine(offset += short_text_width);
	ImGui::Text("Faction");
	ImGui::SameLine(offset += short_text_width);
	ImGui::Text("Outpost");
	ImGui::SameLine(offset += long_text_width);
	ImGui::Text("Guild");
	ImGui::Separator();
	for (size_t i = 0; i < leaderboard.size(); i++) {
		LeaderboardEntry e = leaderboard.at(i);
		if (!e.initialised)
			continue;
		offset = 0.0f;
		if (e.map_name[0] == 0) {
			// Try to load map name in.
			GW::AreaInfo* info = GW::Map::GetMapInfo((GW::Constants::MapID)e.map_id);
			if (info && GW::UI::UInt32ToEncStr(info->name_id, e.map_name_enc, 256))
				GW::UI::AsyncDecodeStr(e.map_name_enc, e.map_name, 256);
		}
		ImGui::Text("%d",e.rank);
		ImGui::SameLine(offset += tiny_text_width);
		ImGui::Text(e.allegiance == 1 ? "Luxon" : "Kurzick");
		ImGui::SameLine(offset += short_text_width);
		ImGui::Text("%d",e.faction);
		ImGui::SameLine(offset += short_text_width);
		ImGui::Text(e.map_name);
		ImGui::SameLine(offset += long_text_width);
		std::string s = e.guild_str;
		s += " [";
		s += e.tag_str;
		s += "]";
		const char* sc = s.c_str();
		ImGui::Text(sc);
		ImGui::PushID(e.map_id);
		ImGui::SameLine(offset = avail_width - tiny_text_width);
		if (ImGui::Button("Wiki",ImVec2(tiny_text_width,0))) {
			ShellExecuteW(NULL, L"open", GuiUtils::ToWstr(e.guild_wiki_url).c_str(), NULL, NULL, SW_SHOWNORMAL);
		}
		ImGui::PopID();
	}
	return ImGui::End();
	/*if (visible) {
		ImGui::SetNextWindowPosCenter(ImGuiSetCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(300, 250), ImGuiSetCond_FirstUseEver);
		if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
			for (TeamBuild& tbuild : teambuilds) {
				ImGui::PushID(tbuild.ui_id);
				ImGui::GetStyle().ButtonTextAlign = ImVec2(0.0f, 0.5f);
				if (ImGui::Button(tbuild.name, ImVec2(ImGui::GetWindowContentRegionWidth() - ImGui::GetStyle().ItemInnerSpacing.x - 60.0f * ImGui::GetIO().FontGlobalScale, 0))) {
					tbuild.edit_open = !tbuild.edit_open;
				}
				ImGui::GetStyle().ButtonTextAlign = ImVec2(0.5f, 0.5f);
				ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
				if (ImGui::Button("Send", ImVec2(60.0f * ImGui::GetIO().FontGlobalScale, 0))) {
					Send(tbuild);
				}
				ImGui::PopID();
			}
			if (ImGui::Button("Add Teambuild", ImVec2(ImGui::GetWindowContentRegionWidth(), 0))) {
				teambuilds.push_back(TeamBuild(""));
				teambuilds.back().edit_open = true; // open by default
				teambuilds.back().builds.resize(4, Build("", ""));
				builds_changed = true;
			}
		}
		ImGui::End();
	}

	for (unsigned int i = 0; i < teambuilds.size(); ++i) {
		if (!teambuilds[i].edit_open) continue;
		TeamBuild& tbuild = teambuilds[i];
		char winname[256];
		snprintf(winname, 256, "%s###build%d", tbuild.name, tbuild.ui_id);
		ImGui::SetNextWindowPosCenter(ImGuiSetCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(500, 0), ImGuiSetCond_FirstUseEver);
		if (ImGui::Begin(winname, &tbuild.edit_open)) {
			ImGui::PushItemWidth(-120.0f);
			if (ImGui::InputText("Build Name", tbuild.name, 128)) builds_changed = true;
			ImGui::PopItemWidth();
			ImGui::SetCursorPosX(30.0f);
			ImGui::Text("Name");
			ImGui::SameLine(-50.0f + ImGui::GetWindowContentRegionWidth() / 2);
			ImGui::Text("Template");
			for (unsigned int j = 0; j < tbuild.builds.size(); ++j) {
				Build& build = tbuild.builds[j];
				ImGui::PushID(j);
				ImGui::Text("#%d", j + 1);
				ImGui::SameLine(30.0f);
				ImGui::PushItemWidth((ImGui::GetWindowContentRegionWidth() - 24.0f - 50.0f - 50.0f - 30.0f
					- ImGui::GetStyle().ItemInnerSpacing.x * 4) / 2);
				if (ImGui::InputText("###name", build.name, 128)) builds_changed = true;
				ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
				if (ImGui::InputText("###code", build.code, 128)) builds_changed = true;
				ImGui::PopItemWidth();
				ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
				if (ImGui::Button("Send", ImVec2(50.0f * ImGui::GetIO().FontGlobalScale, 0))) {
					Send(tbuild, j);
				}
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("Send to team chat");
				ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
				if (ImGui::Button("Load", ImVec2(50.0f, 0))) {
					GW::SkillbarMgr::LoadSkillTemplate(build.code);
				}
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("Load build on your character");
				ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
				if (ImGui::Button("x", ImVec2(24.0f, 0))) {
					tbuild.builds.erase(tbuild.builds.begin() + j);
					builds_changed = true;
				}
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("Delete build");
				ImGui::PopID();
			}
			if (ImGui::Checkbox("Show numbers", &tbuild.show_numbers)) builds_changed = true;
			ImGui::SameLine(ImGui::GetWindowContentRegionWidth() * 0.6f);
			if (ImGui::Button("Add Build", ImVec2(ImGui::GetWindowContentRegionWidth() * 0.4f, 0))) {
				tbuild.builds.push_back(Build("", ""));
				builds_changed = true;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Add another player build row");
			ImGui::Spacing();

			if (ImGui::SmallButton("Up") && i > 0) {
				std::swap(teambuilds[i - 1], teambuilds[i]);
				builds_changed = true;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Move the teambuild up in the list");
			ImGui::SameLine();
			if (ImGui::SmallButton("Down") && i + 1 < teambuilds.size()) {
				std::swap(teambuilds[i], teambuilds[i + 1]);
				builds_changed = true;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Move the teambuild down in the list");
			ImGui::SameLine();
			if (ImGui::SmallButton("Delete")) {
				ImGui::OpenPopup("Delete Teambuild?");
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Delete the teambuild");
			ImGui::SameLine(ImGui::GetWindowContentRegionWidth() * 0.6f);
			if (ImGui::Button("Close", ImVec2(ImGui::GetWindowContentRegionWidth() * 0.4f, 0))) {
				tbuild.edit_open = false;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Close this window");

			if (ImGui::BeginPopupModal("Delete Teambuild?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
				ImGui::Text("Are you sure?\nThis operation cannot be undone.\n\n");
				if (ImGui::Button("OK", ImVec2(120, 0))) {
					teambuilds.erase(teambuilds.begin() + i);
					builds_changed = true;
					ImGui::CloseCurrentPopup();
				}
				ImGui::SameLine();
				if (ImGui::Button("Cancel", ImVec2(120, 0))) {
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
			}
		}
		ImGui::End();
	}*/
}
