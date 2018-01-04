#include "HeroBuildsWindow.h"

#include <GWCA\Managers\AgentMgr.h>
#include <GWCA\Managers\ChatMgr.h>
#include <GWCA\Managers\MapMgr.h>
#include <GWCA\Managers\PartyMgr.h>
#include <GWCA\Managers\SkillbarMgr.h>

#include "GuiUtils.h"
#include <Modules\Resources.h>

unsigned int HeroBuildsWindow::TeamHeroBuild::cur_ui_id = 0;

namespace {
	const int hero_count = 37;
	const char* hero_names[hero_count] = { "Norgu", "Goren", "Tahlkora", 
		"Master Of Whispers", "Acolyte Jin", "Koss", "Dunkoro", 
		"Acolyte Sousuke", "Melonni", "Zhed Shadowhoof", 
		"General Morgahn", "Magrid The Sly", "Zenmai", 
		"Olias", "Razah", "MOX", "Jora", "Keiran Thackeray", 
		"Pyre Fierceshot", "Anton", "Livia", "Hayda", 
		"Kahmu", "Gwen", "Xandra", "Vekk", "Ogden", 
		"Mercenary Hero 1", "Mercenary Hero 2", "Mercenary Hero 3", 
		"Mercenary Hero 4", "Mercenary Hero 5", "Mercenary Hero 6", 
		"Mercenary Hero 7", "Mercenary Hero 8", "Miku", "Zei Ri" };
}

void HeroBuildsWindow::Initialize() {
	ToolboxWindow::Initialize();
	Resources::Instance().LoadTextureAsync(&button_texture, Resources::GetPath("img/icons", "list.png"), IDB_Icon_list);
	send_timer = TIMER_INIT();
}

void HeroBuildsWindow::Terminate() {
	ToolboxWindow::Terminate();
	teambuilds.clear();
}

void HeroBuildsWindow::Draw(IDirect3DDevice9* pDevice) {
	if (visible) {
		ImGui::SetNextWindowPosCenter(ImGuiSetCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(300, 250), ImGuiSetCond_FirstUseEver);
		if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
			for (TeamHeroBuild& tbuild : teambuilds) {
				ImGui::PushID(tbuild.ui_id);
				if (ImGui::Button(tbuild.name, ImVec2(ImGui::GetWindowContentRegionWidth()
					- ImGui::GetStyle().ItemInnerSpacing.x - 60.0f, 0))) {
					Load(tbuild);
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Click to load builds to heroes and player");
				}
				ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
				if (ImGui::Button("Edit", ImVec2(60.0f, 0))) {
					tbuild.edit_open = true;
				}
				ImGui::PopID();
			}
			if (ImGui::Button("Add Teambuild", ImVec2(ImGui::GetWindowContentRegionWidth(), 0))) {
				teambuilds.push_back(TeamHeroBuild());
				teambuilds.back().edit_open = true; // open by default
				teambuilds.back().builds.resize(8, HeroBuild());
				builds_changed = true;
			}
		}
		ImGui::End();
	}

	for (unsigned int i = 0; i < teambuilds.size(); ++i) {
		if (!teambuilds[i].edit_open) continue;
		TeamHeroBuild& tbuild = teambuilds[i];
		char winname[128];
		_snprintf_s(winname, 128, "%s###herobuild%d", tbuild.name, tbuild.ui_id);
		ImGui::SetNextWindowPosCenter(ImGuiSetCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(500, 0), ImGuiSetCond_FirstUseEver);
		if (ImGui::Begin(winname, &tbuild.edit_open)) {
			ImGui::PushItemWidth(-120.0f);
			if (ImGui::InputText("Hero Build Name", tbuild.name, 128)) builds_changed = true;
			ImGui::PopItemWidth();
			for (unsigned int j = 0; j < tbuild.builds.size(); ++j) {
				HeroBuild& build = tbuild.builds[j];
				ImGui::PushID(j);
				if(j==0)ImGui::Text("P");
				else ImGui::Text("H#%d", j);
				ImGui::SameLine(37.0f);
				ImGui::PushItemWidth((ImGui::GetWindowContentRegionWidth() - 24.0f - 50.0f - 50.0f - 30.0f
					- ImGui::GetStyle().ItemInnerSpacing.x * 4) / 3);
				if (ImGui::InputText("###name", build.name, 128)) builds_changed = true;
				ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
				if (ImGui::InputText("###code", build.code, 128)) builds_changed = true;
				if (j != 0) {
					ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
					if (ImGui::MyCombo("###heroid", "Choose Hero", &build.heroid, 
						[](void* data, int idx, const char** out_text) -> bool {
						if (idx < 0) return false;
						if (idx >= hero_count) return false;
						*out_text = hero_names[idx];
						return true;
					}, nullptr, hero_count)) {
						builds_changed = true;
					}
				}
				ImGui::PopItemWidth();
				ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
				if (ImGui::Button("Load", ImVec2(50.0f, 0))) {
					Load(tbuild, j);
				}
				if (j == 0) { 
					if (ImGui::IsItemHovered()) ImGui::SetTooltip("Load Build on Player"); 
				} else { 
					if (ImGui::IsItemHovered()) ImGui::SetTooltip("Load Build on Hero"); 
				}
				if (j != 0) {
					ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
					if (ImGui::Button("x", ImVec2(24.0f, 0))) {
						tbuild.builds.erase(tbuild.builds.begin() + j);
						builds_changed = true;
					}
					if (ImGui::IsItemHovered()) ImGui::SetTooltip("Delete build");
				}
				ImGui::PopID();
			}
			ImGui::Spacing();
			// issue: moving a teambuild up or down will change the teambuild window id
			// which will make the window change size, which is pretty annoying
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
			ImGui::SameLine();
			ImGui::Checkbox("Hard Mode?", &tbuild.hardmode);
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

const char* HeroBuildsWindow::BuildName(unsigned int idx) const {
	if (idx < teambuilds.size()) {
		return teambuilds[idx].name;
	} else {
		return nullptr;
	}
}
void HeroBuildsWindow::Load(unsigned int idx) {
	if (idx < teambuilds.size()) {
		Load(teambuilds[idx]);
	}
}
void HeroBuildsWindow::Load(const HeroBuildsWindow::TeamHeroBuild& tbuild) {
	GW::PartyMgr::KickAllHeroes();
	GW::PartyMgr::SetHardMode(tbuild.hardmode);
	for (unsigned int i = 0; i < tbuild.builds.size(); ++i) {
		Load(tbuild, i);
	}
	send_timer = TIMER_INIT(); // give GW time to update the hero structs after adding them. 
}

void HeroBuildsWindow::Load(const TeamHeroBuild& tbuild, unsigned int idx) {
	if (idx >= tbuild.builds.size()) return;
	if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost) return;
	const HeroBuild& build = tbuild.builds[idx];
	const std::string code(build.code);
	const int heroid = build.heroid + 1;

	if (idx == 0) { // Player 
		if (!code.empty()) {
			GW::SkillbarMgr::LoadSkillTemplate(build.code);
		}
	} else if (heroid > 0 && heroid <= hero_count) {
		GW::PartyMgr::AddHero(heroid);
		if (!code.empty()) {
			queue.push(CodeOnHero(code.c_str(), idx));
		}
	}
}

void HeroBuildsWindow::Update() {
	if (!queue.empty() && TIMER_DIFF(send_timer) > 600) {
		if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost
			&& GW::Agents::GetPlayer()) {
			GW::SkillbarMgr::LoadSkillTemplate(queue.front().code, queue.front().heroind);
			queue.pop();
			send_timer = TIMER_INIT();
		}
	}
}

void HeroBuildsWindow::LoadSettings(CSimpleIni* ini) {
	ToolboxWindow::LoadSettings(ini);

	// clear builds from toolbox
	teambuilds.clear();

	// then load
	CSimpleIni::TNamesDepend entries;
	ini->GetAllSections(entries);
	for (CSimpleIni::Entry& entry : entries) {
		const char* section = entry.pItem;
		if (strncmp(section, "herobuilds", 10) == 0) {
			// default to -1 because we didn't have the count field before
			int count = ini->GetLongValue(section, "count", -1);
			int count2 = (count >= 0 ? count : 8);
			teambuilds.push_back(TeamHeroBuild(ini->GetValue(section, "herobuildname", "")));
			TeamHeroBuild& tbuild = teambuilds.back();
			tbuild.hardmode = ini->GetBoolValue(section, "hardmode", "");
			for (int i = 0; i < count2; ++i) {
				char namekey[16];
				char templatekey[16];
				char heroidkey[16];
				sprintf_s(namekey, "name%d", i);
				sprintf_s(templatekey, "template%d", i);
				sprintf_s(heroidkey, "heroid%d", i);
				const char* nameval = ini->GetValue(section, namekey, "");
				const char* templateval = ini->GetValue(section, templatekey, "");
				const int heroidval = atoi(ini->GetValue(section, heroidkey, ""));
				if (count == -1) {
					// only add if nonempty
					if (strcmp(nameval, "") || strcmp(templateval, "") || heroidval == 0L) {
						tbuild.builds.push_back(HeroBuild(nameval, templateval, heroidval));
					}
				}
				else {
					tbuild.builds.push_back(HeroBuild(nameval, templateval, heroidval));
				}
			}
		}
	}

	builds_changed = false;
}

void HeroBuildsWindow::SaveSettings(CSimpleIni* ini) {
	ToolboxWindow::SaveSettings(ini);

	if (builds_changed) {
		// clear builds from ini
		CSimpleIni::TNamesDepend entries;
		ini->GetAllSections(entries);
		for (CSimpleIni::Entry& entry : entries) {
			if (strncmp(entry.pItem, "herobuilds", 6) == 0) {
				ini->Delete(entry.pItem, nullptr);
			}
		}

		// then save
		for (unsigned int i = 0; i < teambuilds.size(); ++i) {
			const TeamHeroBuild& tbuild = teambuilds[i];
			char section[16];
			sprintf_s(section, "herobuilds%03d", i);
			ini->SetValue(section, "herobuildname", tbuild.name);
			ini->SetLongValue(section, "count", tbuild.builds.size());
			ini->SetBoolValue(section, "hardmode", tbuild.hardmode);
			for (unsigned int j = 0; j < tbuild.builds.size(); ++j) {
				const HeroBuild& build = tbuild.builds[j];
				char namekey[16];
				char templatekey[16];
				char heroidkey[16];
				sprintf_s(namekey, "name%d", j);
				sprintf_s(templatekey, "template%d", j);
				sprintf_s(heroidkey, "heroid%d", j);
				ini->SetValue(section, namekey, build.name);
				ini->SetValue(section, templatekey, build.code);
				ini->SetValue(section, heroidkey, std::to_string(build.heroid).c_str());
			}
		}
	}
}
