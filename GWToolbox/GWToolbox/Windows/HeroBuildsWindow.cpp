#include "HeroBuildsWindow.h"

#include <GWCA\Managers\AgentMgr.h>
#include <GWCA\Managers\ChatMgr.h>
#include <GWCA\Managers\MapMgr.h>
#include <GWCA\Managers\PartyMgr.h>
#include <GWCA\Managers\SkillbarMgr.h>

#include <logger.h>
#include "GuiUtils.h"
#include <Modules\Resources.h>

#define INI_FILENAME "herobuilds.ini"

namespace {
	using GW::Constants::HeroID;

	// hero index is an arbitrary index. 
	// We aim to have the same order as in the gw client. 
	// Razah is after the mesmers because all players that don't have mercs have it set as mesmer. 
	HeroID HeroIndexToID[] = {
		HeroID::NoHero,
		HeroID::Goren,
		HeroID::Koss,
		HeroID::Jora,
		HeroID::AcolyteJin,
		HeroID::MargridTheSly,
		HeroID::PyreFierceshot,
		HeroID::Tahlkora,
		HeroID::Dunkoro,
		HeroID::Ogden,
		HeroID::MasterOfWhispers,
		HeroID::Olias,
		HeroID::Livia,
		HeroID::Norgu,
		HeroID::Razah,
		HeroID::Gwen,
		HeroID::AcolyteSousuke,
		HeroID::ZhedShadowhoof,
		HeroID::Vekk,
		HeroID::Zenmai,
		HeroID::Anton,
		HeroID::Miku,
		HeroID::Xandra,
		HeroID::ZeiRi,
		HeroID::GeneralMorgahn,
		HeroID::KeiranThackeray,
		HeroID::Hayda,
		HeroID::Melonni,
		HeroID::MOX,
		HeroID::Kahmu,
		HeroID::Merc1,
		HeroID::Merc2,
		HeroID::Merc3,
		HeroID::Merc4,
		HeroID::Merc5,
		HeroID::Merc6,
		HeroID::Merc7,
		HeroID::Merc8
	};

	const int hero_count = _countof(HeroIndexToID);

	const char *HeroName[] = {
		"No Hero", "Norgu", "Goren", "Tahlkora",
		"Master Of Whispers", "Acolyte Jin", "Koss", "Dunkoro",
		"Acolyte Sousuke", "Melonni", "Zhed Shadowhoof",
		"General Morgahn", "Magrid The Sly", "Zenmai",
		"Olias", "Razah", "MOX", "Jora", "Keiran Thackeray",
		"Pyre Fierceshot", "Anton", "Livia", "Hayda",
		"Kahmu", "Gwen", "Xandra", "Vekk", "Ogden",
		"Mercenary Hero 1", "Mercenary Hero 2", "Mercenary Hero 3",
		"Mercenary Hero 4", "Mercenary Hero 5", "Mercenary Hero 6",
		"Mercenary Hero 7", "Mercenary Hero 8", "Miku", "Zei Ri"
	};
}

unsigned int HeroBuildsWindow::TeamHeroBuild::cur_ui_id = 0;

void HeroBuildsWindow::Initialize() {
	ToolboxWindow::Initialize();
	Resources::Instance().LoadTextureAsync(&button_texture, Resources::GetPath("img/icons", "list.png"), IDB_Icon_list);
	send_timer = TIMER_INIT();
}

void HeroBuildsWindow::Terminate() {
	ToolboxWindow::Terminate();
	teambuilds.clear();
	inifile->Reset();
	delete inifile;
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
				TeamHeroBuild tb = TeamHeroBuild();
				tb.builds.reserve(8); // at this point why don't we use a static array ??
				tb.builds.push_back(HeroBuild("", "", -2));
				for (int i = 0; i < 7; ++i) {
					tb.builds.push_back(HeroBuild());
				}

				builds_changed = true;

				teambuilds.push_back(tb);
			}
		}
		ImGui::End();
	}

	for (size_t i = 0; i < teambuilds.size(); ++i) {
		if (!teambuilds[i].edit_open) continue;
		TeamHeroBuild& tbuild = teambuilds[i];
		char winname[256];
		snprintf(winname, 256, "%s###herobuild%d", tbuild.name, tbuild.ui_id);
		ImGui::SetNextWindowPosCenter(ImGuiSetCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(500, 0), ImGuiSetCond_FirstUseEver);
		if (ImGui::Begin(winname, &tbuild.edit_open)) {
			ImGui::PushItemWidth(-120.0f);
			if (ImGui::InputText("Hero Build Name", tbuild.name, 128)) builds_changed = true;
			ImGui::PopItemWidth();
			ImGui::SetCursorPosX(37.0f);
			ImGui::Text("Name");
			ImGui::SameLine(10.0f + ImGui::GetWindowContentRegionWidth() / 3);
			ImGui::Text("Template");
			for (size_t j = 0; j < tbuild.builds.size(); ++j) {
				HeroBuild& build = tbuild.builds[j];
				ImGui::PushID(j);
				if (j == 0) {
					ImGui::Text("P");
				} else {
					ImGui::Text("H#%d", j);
				}
				ImGui::SameLine(37.0f);
				ImGui::PushItemWidth((ImGui::GetWindowContentRegionWidth() - 50.0f - 24.0f
					- ImGui::GetStyle().ItemInnerSpacing.x * 4) / 3);
				if (ImGui::InputText("###name", build.name, 128)) builds_changed = true;
				ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
				if (ImGui::InputText("###code", build.code, 128)) builds_changed = true;
				ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
				if (j == 0) {
					ImGui::Text("Player");
				} else {
					if (ImGui::MyCombo("###heroid", "Choose Hero", &build.hero_index,
						[](void* data, int idx, const char** out_text) -> bool {
						if (idx < 0) return false;
						if (idx >= hero_count) return false;
						*out_text = HeroName[HeroIndexToID[idx]];
						return true;
					}, nullptr, hero_count)) {
						builds_changed = true;
					}
				}
				ImGui::PopItemWidth();
				ImGui::SameLine(ImGui::GetWindowWidth() - 50.0f - ImGui::GetStyle().WindowPadding.x);
				if (ImGui::Button("Load", ImVec2(50.0f, 0))) {
					Load(tbuild, j);
				}
				if (j == 0) {
					if (ImGui::IsItemHovered()) ImGui::SetTooltip("Load Build on Player");
				} else {
					if (ImGui::IsItemHovered()) ImGui::SetTooltip("Load Build on Hero");
				}
				ImGui::PopID();
			}
			ImGui::Spacing();

			if (ImGui::SmallButton("Up") && i > 0) {
				std::swap(teambuilds[i - 1], teambuilds[i]);
				builds_changed = true;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Move the teambuild up in the list");
			ImGui::SameLine();
			if (ImGui::SmallButton("Down") && i + 1 < (int)teambuilds.size()) {
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
			ImGui::SameLine(ImGui::GetWindowWidth() -
				ImGui::GetStyle().WindowPadding.x - ImGui::GetWindowContentRegionWidth() * 0.4f);
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
	if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost) return;
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

	if (idx == 0) { // Player 
		// note: build.hero_index should be -1
		if (!code.empty()) GW::SkillbarMgr::LoadSkillTemplate(build.code);
	} else {
		if (build.hero_index < 0 || build.hero_index >= hero_count) {
			Log::Error("Bad hero index '%d' for build '%s'", build.hero_index, build.name);
		}
		GW::Constants::HeroID heroid = HeroIndexToID[build.hero_index];

		if (heroid == HeroID::NoHero) return;

		GW::PartyMgr::AddHero(heroid);
		if (!code.empty()) {
			queue.push(CodeOnHero(code.c_str(), idx));
		}
	}
}

void HeroBuildsWindow::Update(float delta) {
	if (!queue.empty() && TIMER_DIFF(send_timer) > 600) {
		if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost
			&& GW::Agents::GetPlayer()) {
			GW::SkillbarMgr::LoadSkillTemplate(queue.front().code, queue.front().heroind);
			queue.pop();
			send_timer = TIMER_INIT();
		} else {
			queue.empty(); 
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

void HeroBuildsWindow::LoadSettings(CSimpleIni* ini) {
	ToolboxWindow::LoadSettings(ini);
	LoadFromFile();
}

void HeroBuildsWindow::SaveSettings(CSimpleIni* ini) {
	ToolboxWindow::SaveSettings(ini);
	SaveToFile();
}

void HeroBuildsWindow::LoadFromFile() {
	if (inifile == nullptr) inifile = new CSimpleIni(false, false, false);
	inifile->LoadFile(Resources::GetPath(INI_FILENAME).c_str());

	// clear builds from toolbox
	teambuilds.clear();

	// then load
	CSimpleIni::TNamesDepend entries;
	inifile->GetAllSections(entries);
	for (CSimpleIni::Entry& entry : entries) {
		const char* section = entry.pItem;

		TeamHeroBuild tb;
		strncpy(tb.name, inifile->GetValue(section, "buildname", ""), 128);
		tb.hardmode = inifile->GetBoolValue(section, "hardmode", false);
		tb.builds.reserve(8);

		for (int i = 0; i < 8; ++i) {
			char namekey[16];
			char templatekey[16];
			char heroindexkey[16];
			snprintf(namekey, 16, "name%d", i);
			snprintf(templatekey, 16, "template%d", i);
			snprintf(heroindexkey, 16, "heroindex%d", i);
			const char* nameval = inifile->GetValue(section, namekey, "");
			const char* templateval = inifile->GetValue(section, templatekey, "");
			const int hero_index = inifile->GetLongValue(section, heroindexkey, -1);

			HeroBuild build;
			strncpy(build.name, nameval, 128);
			strncpy(build.code, templateval, 128);
			build.hero_index = hero_index;

			tb.builds.push_back(build);
		}

		// Check the binary to see if we should instead take the ptr to read everything to avoid the copy
		// But this might not matter at all, because we would do copy anyway for each element and hence
		// as much copy.
		teambuilds.push_back(tb);
	}

	builds_changed = false;
}

void HeroBuildsWindow::SaveToFile() {
	if (builds_changed) {
		// clear builds from ini
		CSimpleIni::TNamesDepend entries;
		inifile->GetAllSections(entries);
		for (CSimpleIni::Entry& entry : entries) {
			inifile->Delete(entry.pItem, nullptr);
		}

		// then save
		for (unsigned int i = 0; i < teambuilds.size(); ++i) {
			const TeamHeroBuild& tbuild = teambuilds[i];
			char section[16];
			snprintf(section, 16, "builds%03d", i);
			inifile->SetValue(section, "buildname", tbuild.name);
			inifile->SetBoolValue(section, "hardmode", tbuild.hardmode);
			for (unsigned int j = 0; j < tbuild.builds.size(); ++j) {
				const HeroBuild& build = tbuild.builds[j];
				char namekey[16];
				char templatekey[16];
				char heroindexkey[16];
				snprintf(namekey, 16, "name%d", j);
				snprintf(templatekey, 16, "template%d", j);
				snprintf(heroindexkey, 16, "heroindex%d", j);
				inifile->SetValue(section, namekey, build.name);
				inifile->SetValue(section, templatekey, build.code);
				inifile->SetLongValue(section, heroindexkey, build.hero_index);
			}
		}
		inifile->SaveFile(Resources::GetPath(INI_FILENAME).c_str());
	}
}
