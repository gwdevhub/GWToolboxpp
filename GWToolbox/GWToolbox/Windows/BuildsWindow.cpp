#include "BuildsWindow.h"

#include <GWCA\Managers\AgentMgr.h>
#include <GWCA\Managers\ChatMgr.h>
#include <GWCA\Managers\MapMgr.h>
#include <GWCA\Managers\SkillbarMgr.h>

#include "GuiUtils.h"
#include <Modules\Resources.h>

unsigned int BuildsWindow::TeamBuild::cur_ui_id = 0;

#define INI_FILENAME "builds.ini"

void BuildsWindow::Initialize() {
	ToolboxWindow::Initialize();
	Resources::Instance().LoadTextureAsync(&button_texture, Resources::GetPath("img/icons", "list.png"), IDB_Icon_list);
	send_timer = TIMER_INIT();
}

void BuildsWindow::Terminate() {
	ToolboxWindow::Terminate();
	teambuilds.clear();
	inifile->Reset();
	delete inifile;
}

void BuildsWindow::Draw(IDirect3DDevice9* pDevice) {
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
				if (ImGui::Button("Send", ImVec2(50.0f, 0))) {
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
				tbuild.builds.push_back(Build());
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
	}
}

const char* BuildsWindow::BuildName(unsigned int idx) const {
	if (idx < teambuilds.size()) {
		return teambuilds[idx].name;
	} else {
		return nullptr;
	}
}
void BuildsWindow::Send(unsigned int idx) {
	if (idx < teambuilds.size()) {
		Send(teambuilds[idx]);
	}
}
void BuildsWindow::Send(const BuildsWindow::TeamBuild& tbuild) {
	if (!std::string(tbuild.name).empty()) {
		queue.push(tbuild.name);
	}
	for (unsigned int i = 0; i < tbuild.builds.size(); ++i) {
		const Build& build = tbuild.builds[i];
		Send(tbuild, i);
	}
}

void BuildsWindow::Send(const TeamBuild& tbuild, unsigned int idx) {
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
		
		snprintf(buf, buf_size, "[%s %d;%s]", tbuild.name, idx + 1, build.code);
	} else if (code.empty()) {
		// code is empty, just print the name without template format
		snprintf(buf, buf_size, "%s", build.name);
	} else if (tbuild.show_numbers) {
		// add numbers in front of name
		snprintf(buf, buf_size, "[%d - %s;%s]", idx + 1, build.name, build.code);
	} else {
		// simple template
		snprintf(buf, buf_size, "[%s;%s]", build.name, build.code);
	}
	queue.push(buf);
}

void BuildsWindow::Update(float delta) {
	if (!queue.empty() && TIMER_DIFF(send_timer) > 600) {
		if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading
			&& GW::Agents::GetPlayer()) {

			send_timer = TIMER_INIT();
			GW::Chat::SendChat('#', queue.front().c_str());
			queue.pop();
		}
	}

	// if we open the window, load from file. If we close the window, save to file. 
	static bool _visible = false;
	if (visible != _visible) {
		_visible = visible;
		if (visible) {
			LoadFromFile();
		} else {
			SaveToFile();
		}
	}
}

void BuildsWindow::LoadSettings(CSimpleIni* ini) {
	ToolboxWindow::LoadSettings(ini);
	show_menubutton = ini->GetBoolValue(Name(), VAR_NAME(show_menubutton), true);
	if (MoveOldBuilds(ini)) {
		// loaded
	} else {
		LoadFromFile();
	}
}

void BuildsWindow::SaveSettings(CSimpleIni* ini) {
	ToolboxWindow::SaveSettings(ini);
	SaveToFile();
}

bool BuildsWindow::MoveOldBuilds(CSimpleIni* ini) {
	if (!teambuilds.empty()) return false; // builds are already loaded, skip

	bool found_old_build = false;
	CSimpleIni::TNamesDepend oldentries;
	ini->GetAllSections(oldentries);
	for (CSimpleIni::Entry& oldentry : oldentries) {
		const char* section = oldentry.pItem;
		if (strncmp(section, "builds", 6) == 0) {
			int count = ini->GetLongValue(section, "count", 12);
			teambuilds.push_back(TeamBuild(ini->GetValue(section, "buildname", "")));
			TeamBuild& tbuild = teambuilds.back();
			tbuild.show_numbers = ini->GetBoolValue(section, "showNumbers", true);
			for (int i = 0; i < count; ++i) {
				char namekey[16];
				char templatekey[16];
				snprintf(namekey, 16, "name%d", i);
				snprintf(templatekey, 16, "template%d", i);
				const char* nameval = ini->GetValue(section, namekey, "");
				const char* templateval = ini->GetValue(section, templatekey, "");
				tbuild.builds.push_back(Build(nameval, templateval));
			}
			found_old_build = true;
			ini->Delete(section, nullptr);
		}
	}

	if (found_old_build) {
		builds_changed = true;
		SaveToFile();
		return true;
	} else {
		return false;
	}
}

void BuildsWindow::LoadFromFile() {
	// clear builds from toolbox
	teambuilds.clear();

	if (inifile == nullptr) inifile = new CSimpleIni(false, false, false);
	inifile->LoadFile(Resources::GetPath(INI_FILENAME).c_str());

	// then load
	CSimpleIni::TNamesDepend entries;
	inifile->GetAllSections(entries);
	for (CSimpleIni::Entry& entry : entries) {
		const char* section = entry.pItem;
		int count = inifile->GetLongValue(section, "count", 12);
		teambuilds.push_back(TeamBuild(inifile->GetValue(section, "buildname", "")));
		TeamBuild& tbuild = teambuilds.back();
		tbuild.show_numbers = inifile->GetBoolValue(section, "showNumbers", true);
		for (int i = 0; i < count; ++i) {
			char namekey[16];
			char templatekey[16];
			snprintf(namekey, 16, "name%d", i);
			snprintf(templatekey, 16, "template%d", i);
			const char* nameval = inifile->GetValue(section, namekey, "");
			const char* templateval = inifile->GetValue(section, templatekey, "");
			tbuild.builds.push_back(Build(nameval, templateval));
		}
	}

	builds_changed = false;
}

void BuildsWindow::SaveToFile() {
	if (inifile == nullptr) inifile = new CSimpleIni(false, false, false);

	if (builds_changed) {
		// clear builds from ini
		inifile->Reset();

		// then save
		for (unsigned int i = 0; i < teambuilds.size(); ++i) {
			const TeamBuild& tbuild = teambuilds[i];
			char section[16];
			snprintf(section, 16, "builds%03d", i);
			inifile->SetValue(section, "buildname", tbuild.name);
			inifile->SetBoolValue(section, "showNumbers", tbuild.show_numbers);
			inifile->SetLongValue(section, "count", tbuild.builds.size());
			for (unsigned int j = 0; j < tbuild.builds.size(); ++j) {
				const Build& build = tbuild.builds[j];
				char namekey[16];
				char templatekey[16];
				snprintf(namekey, 16, "name%d", j);
				snprintf(templatekey, 16, "template%d", j);
				inifile->SetValue(section, namekey, build.name);
				inifile->SetValue(section, templatekey, build.code);
			}
		}

		inifile->SaveFile(Resources::GetPath(INI_FILENAME).c_str());
	}
}
