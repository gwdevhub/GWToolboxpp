#include "BuildPanel.h"

#include <GWCA\Managers\AgentMgr.h>
#include <GWCA\Managers\ChatMgr.h>
#include <GWCA\Managers\MapMgr.h>

#include "GuiUtils.h"
#include <OtherModules\Resources.h>

unsigned int BuildPanel::TeamBuild::cur_ui_id = 0;

void BuildPanel::Initialize() {
	ToolboxPanel::Initialize();
	Resources::Instance().LoadTextureAsync(&texture, "list.png", "img/icons");
	send_timer = TIMER_INIT();
}

void BuildPanel::Terminate() {
	ToolboxPanel::Terminate();
	teambuilds.clear();
}

void BuildPanel::Draw(IDirect3DDevice9* pDevice) {
	if (visible) {
		ImGui::SetNextWindowPosCenter(ImGuiSetCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(300, 250), ImGuiSetCond_FirstUseEver);
		if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
			for (TeamBuild& tbuild : teambuilds) {
				ImGui::PushID(tbuild.ui_id);
				if (ImGui::Button(tbuild.name, ImVec2(ImGui::GetWindowContentRegionWidth()
					-ImGui::GetStyle().ItemInnerSpacing.x - 60.0f, 0))) {
					Send(tbuild);
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Click to send teambuild to chat");
				}
				ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
				if (ImGui::Button("Edit", ImVec2(60.0f, 0))) {
					tbuild.edit_open = true;
				}
				ImGui::PopID();
			}
			if (ImGui::Button("Add Teambuild", ImVec2(ImGui::GetWindowContentRegionWidth(), 0))) {
				teambuilds.push_back(TeamBuild());
				teambuilds.back().edit_open = true; // open by default
				teambuilds.back().builds.resize(4, Build());
			}
		}
		ImGui::End();
	}

	for (unsigned int i = 0; i < teambuilds.size(); ++i) {
		if (!teambuilds[i].edit_open) continue;
		TeamBuild& tbuild = teambuilds[i];
		char winname[128];
		_snprintf_s(winname, 128, "%s###build%d", tbuild.name, tbuild.ui_id);
		ImGui::SetNextWindowPosCenter(ImGuiSetCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(500, 0), ImGuiSetCond_FirstUseEver);
		if (ImGui::Begin(winname, &tbuild.edit_open)) {
			ImGui::PushItemWidth(-120.0f);
			ImGui::InputText("Build Name", tbuild.name, 128);
			ImGui::PopItemWidth();
			for (unsigned int j = 0; j < tbuild.builds.size(); ++j) {
				Build& build = tbuild.builds[j];
				ImGui::PushID(j);
				ImGui::Text("#%d", j + 1);
				ImGui::SameLine(30.0f);
				ImGui::PushItemWidth((ImGui::GetWindowContentRegionWidth() - 24.0f - 50.0f - 30.0f
					- ImGui::GetStyle().ItemInnerSpacing.x * 3) / 2);
				ImGui::InputText("###name", build.name, 128);
				ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
				ImGui::InputText("###code", build.code, 128);
				ImGui::PopItemWidth();
				ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
				if (ImGui::Button("Send", ImVec2(50.0f, 0))) {
					Send(tbuild, j);
				}
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("Send to team chat");
				ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
				if (ImGui::Button("x", ImVec2(24.0f, 0))) {
					tbuild.builds.erase(tbuild.builds.begin() + j);
				}
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("Delete build");
				ImGui::PopID();
			}
			ImGui::Checkbox("Show numbers", &tbuild.show_numbers);
			ImGui::SameLine(ImGui::GetWindowContentRegionWidth() * 0.6f);
			if (ImGui::Button("Add Build", ImVec2(ImGui::GetWindowContentRegionWidth() * 0.4f, 0))) {
				tbuild.builds.push_back(Build());
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Add another player build row");
			ImGui::Spacing();
			// issue: moving a teambuild up or down will change the teambuild window id
			// which will make the window change size, which is pretty annoying
			if (ImGui::SmallButton("Up") && i > 0) {
				std::swap(teambuilds[i - 1], teambuilds[i]);
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Move the teambuild up in the list");
			ImGui::SameLine();
			if (ImGui::SmallButton("Down") && i + 1 < teambuilds.size()) {
				std::swap(teambuilds[i], teambuilds[i + 1]);
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
	}
}

const char* BuildPanel::BuildName(unsigned int idx) const {
	if (idx < teambuilds.size()) {
		return teambuilds[idx].name;
	} else {
		return nullptr;
	}
}
void BuildPanel::Send(unsigned int idx) {
	if (idx < teambuilds.size()) {
		Send(teambuilds[idx]);
	}
}
void BuildPanel::Send(const BuildPanel::TeamBuild& tbuild) {
	if (!std::string(tbuild.name).empty()) {
		queue.push(tbuild.name);
	}
	for (unsigned int i = 0; i < tbuild.builds.size(); ++i) {
		const Build& build = tbuild.builds[i];
		Send(tbuild, i);
	}
}

void BuildPanel::Send(const TeamBuild& tbuild, unsigned int idx) {
	if (idx >= tbuild.builds.size()) return;
	const Build& build = tbuild.builds[idx];
	const std::string name(build.name);
	const std::string code(build.code);

	const int buf_size = 139;
	char buf[buf_size];
	if (name.empty() && code.empty()) {
		return; // nothing to do here
	} else if (name.empty()) {
		// name is empty, fill it with the teambuild name
		
		_snprintf_s(buf, buf_size, "[%s %d;%s]", tbuild.name, idx + 1, build.code);
	} else if (code.empty()) {
		// code is empty, just print the name without template format
		_snprintf_s(buf, buf_size, "%s", build.name);
	} else if (tbuild.show_numbers) {
		// add numbers in front of name
		_snprintf_s(buf, buf_size, "[%d - %s;%s]", idx + 1, build.name, build.code);
	} else {
		// simple template
		_snprintf_s(buf, buf_size, "[%s;%s]", build.name, build.code);
	}
	queue.push(buf);
}

void BuildPanel::Update() {
	if (!queue.empty() && TIMER_DIFF(send_timer) > 600) {
		if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading
			&& GW::Agents::GetPlayer()) {

			send_timer = TIMER_INIT();
			GW::Chat::SendChat(queue.front().c_str(), '#');
			queue.pop();
		}
	}
}

void BuildPanel::LoadSettings(CSimpleIni* ini) {
	ToolboxPanel::LoadSettings(ini);

	// clear builds from toolbox
	teambuilds.clear();

	// then load
	CSimpleIni::TNamesDepend entries;
	ini->GetAllSections(entries);
	for (CSimpleIni::Entry& entry : entries) {
		const char* section = entry.pItem;
		if (strncmp(section, "builds", 6) == 0) {
			// default to -1 because we didn't have the count field before
			int count = ini->GetLongValue(section, "count", -1); 
			int count2 = (count >= 0 ? count : 12);
			teambuilds.push_back(TeamBuild(ini->GetValue(section, "buildname", "")));
			TeamBuild& tbuild = teambuilds.back();
			tbuild.show_numbers = ini->GetBoolValue(section, "showNumbers", true);
			for (int i = 0; i < count2; ++i) {
				char namekey[16];
				char templatekey[16];
				sprintf_s(namekey, "name%d", i);
				sprintf_s(templatekey, "template%d", i);
				const char* nameval = ini->GetValue(section, namekey, "");
				const char* templateval = ini->GetValue(section, templatekey, "");
				if (count == -1) {
					// only add if nonempty
					if (strcmp(nameval, "") || strcmp(templateval, "")) {
						tbuild.builds.push_back(Build(nameval, templateval));
					}
				} else {
					tbuild.builds.push_back(Build(nameval, templateval));
				}
			}
		}
	}
}

void BuildPanel::SaveSettings(CSimpleIni* ini) {
	ToolboxPanel::SaveSettings(ini);

	// clear builds from ini
	CSimpleIni::TNamesDepend entries;
	ini->GetAllSections(entries);
	for (CSimpleIni::Entry& entry : entries) {
		if (strncmp(entry.pItem, "builds", 6) == 0) {
			ini->Delete(entry.pItem, nullptr);
		}
	}

	// then save
	for (unsigned int i = 0; i < teambuilds.size(); ++i) {
		const TeamBuild& tbuild = teambuilds[i];
		char section[16];
		sprintf_s(section, "builds%03d", i);
		ini->SetValue(section, "buildname", tbuild.name);
		ini->SetBoolValue(section, "showNumbers", tbuild.show_numbers);
		ini->SetLongValue(section, "count", tbuild.builds.size());
		for (unsigned int j = 0; j < tbuild.builds.size(); ++j) {
			const Build& build = tbuild.builds[j];
			char namekey[16];
			char templatekey[16];
			sprintf_s(namekey, "name%d", j);
			sprintf_s(templatekey, "template%d", j);
			ini->SetValue(section, namekey, build.name);
			ini->SetValue(section, templatekey, build.code);
		}
	}
}
