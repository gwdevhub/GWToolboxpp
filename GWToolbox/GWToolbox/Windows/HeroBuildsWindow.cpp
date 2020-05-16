#include "stdafx.h"
#include "HeroBuildsWindow.h"

#include <GWCA\GameContainers\Array.h>
#include <GWCA\GameContainers\GamePos.h>

#include <GWCA\GameEntities\Hero.h>
#include <GWCA\GameEntities\Party.h>
#include <GWCA\GameEntities\Agent.h>

#include <GWCA\Context\GameContext.h>
#include <GWCA\Context\WorldContext.h>

#include <GWCA\Managers\MapMgr.h>
#include <GWCA\Managers\ChatMgr.h>
#include <GWCA\Managers\AgentMgr.h>
#include <GWCA\Managers\PartyMgr.h>
#include <GWCA\Managers\SkillbarMgr.h>
#include <GWCA\Managers\GameThreadMgr.h>
#include <GWCA\Managers\UIMgr.h>

#include <logger.h>
#include "GuiUtils.h"
#include <Modules\Resources.h>


#define INI_FILENAME L"herobuilds.ini"

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
		"General Morgahn", "Margrid The Sly", "Zenmai",
		"Olias", "Razah", "MOX", "Keiran Thackeray", "Jora",
		"Pyre Fierceshot", "Anton", "Livia", "Hayda",
		"Kahmu", "Gwen", "Xandra", "Vekk", "Ogden",
		"Mercenary Hero 1", "Mercenary Hero 2", "Mercenary Hero 3",
		"Mercenary Hero 4", "Mercenary Hero 5", "Mercenary Hero 6",
		"Mercenary Hero 7", "Mercenary Hero 8", "Miku", "Zei Ri"
	};

	char MercHeroNames[8][20] = { 0 };

	// Returns ptr to party member of this hero, optionally fills out out_hero_index to be the index of this hero for the player.
	GW::HeroPartyMember* GetPartyHeroByID(HeroID hero_id, uint32_t *out_hero_index) {
		auto party_info = GW::PartyMgr::GetPartyInfo();
		if (!party_info) return nullptr;
		auto party_heros = party_info->heroes;
		if (!party_heros.valid()) return nullptr;
		auto me = GW::Agents::GetPlayerAsAgentLiving();
		if (!me) return nullptr;
		uint32_t my_player_id = me->login_number;
		for (size_t i = 0; i < party_heros.size(); i++) {
			if (party_heros[i].owner_player_id == my_player_id &&
				party_heros[i].hero_id == hero_id) {
					if(out_hero_index) 
						*out_hero_index = i + 1;
					return &party_heros[i];
			}
		}
		return nullptr;
	}

}

unsigned int HeroBuildsWindow::TeamHeroBuild::cur_ui_id = 0;

HeroBuildsWindow::~HeroBuildsWindow() {
    if (inifile)
        delete inifile;
}

void HeroBuildsWindow::Initialize() {
	ToolboxWindow::Initialize();
	Resources::Instance().LoadTextureAsync(&button_texture, Resources::GetPath(L"img/icons", L"party.png"));
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
				ImGui::GetStyle().ButtonTextAlign = ImVec2(0.0f, 0.5f);
				if (ImGui::Button(tbuild.name, ImVec2(ImGui::GetWindowContentRegionWidth() - ImGui::GetStyle().ItemInnerSpacing.x - 60.0f * ImGui::GetIO().FontGlobalScale, 0))) {
					tbuild.edit_open = !tbuild.edit_open;
				}
				ImGui::GetStyle().ButtonTextAlign = ImVec2(0.5f, 0.5f);
				ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
				if (ImGui::Button(ImGui::GetIO().KeyCtrl ? "Send" : "Load", ImVec2(60.0f * ImGui::GetIO().FontGlobalScale, 0))) {
					if (ImGui::GetIO().KeyCtrl)
						Send(tbuild);
					else
						Load(tbuild);
				}
				if (ImGui::IsItemHovered()) ImGui::SetTooltip(ImGui::GetIO().KeyCtrl ? "Click to send to team chat" : "Click to load builds to heroes and player. Ctrl + Click to send to chat.");
				ImGui::PopID();
			}
			if (ImGui::Button("Add Teambuild", ImVec2(ImGui::GetWindowContentRegionWidth(), 0))) {
				TeamHeroBuild tb = TeamHeroBuild("");
				tb.builds.reserve(8); // at this point why don't we use a static array ??
				tb.builds.push_back(HeroBuild("", "", -2));
				for (int i = 0; i < 7; ++i) {
					tb.builds.push_back(HeroBuild("", ""));
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
			const float btn_width = 50.0f * ImGui::GetIO().FontGlobalScale;
            for (size_t j = 0; j < tbuild.builds.size(); ++j) {
                HeroBuild& build = tbuild.builds[j];
                ImGui::PushID(j);
                if (j == 0) {
                    ImGui::Text("P");
                } else {
                    ImGui::Text("H#%d", j);
                }
                ImGui::SameLine(btn_width);
                ImGui::PushItemWidth((ImGui::GetWindowContentRegionWidth() - btn_width * 3 - (ImGui::GetStyle().ItemInnerSpacing.x * 6)) / 3);
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
                        auto id = HeroIndexToID[idx];
                        if (id < HeroID::Merc1 || id > HeroID::Merc8) {
                            *out_text = HeroName[HeroIndexToID[idx]];
                            return true;
                        }
                        bool match = false;
                        auto ctx = GW::GameContext::instance();
                        auto& hero_array = ctx->world->hero_info;
                        for (auto& hero : hero_array) {
                            if (hero.hero_id == id) {
                                match = true;
                                wcstombs(MercHeroNames[id - HeroID::Merc1], hero.name, 20);
                                *out_text = MercHeroNames[id - HeroID::Merc1];
                            }
                        }
                        if (!match)
                            *out_text = HeroName[id];
                        return true;
                    }, nullptr, hero_count)) {
                        builds_changed = true;
                    }
                }
                ImGui::PopItemWidth();
				ImGui::SameLine(ImGui::GetWindowContentRegionWidth() - btn_width * 2 - ImGui::GetStyle().ItemInnerSpacing.x * 3,0);
				if (ImGui::Button(ImGui::GetIO().KeyCtrl ? "Send" : "View", ImVec2(btn_width, 0))) {
					if (ImGui::GetIO().KeyCtrl)
						Send(tbuild, j);
					else
						View(tbuild, j);
				}
				if (ImGui::IsItemHovered()) ImGui::SetTooltip(ImGui::GetIO().KeyCtrl ? "Click to send to team chat" : "Click to view build. Ctrl + Click to send to chat.");
				ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
                if (ImGui::Button("Load", ImVec2(btn_width, 0))) {
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
            ImGui::PushItemWidth(110.0f);
            const static char* modes[] = { "Don't change", "Normal Mode", "Hard Mode" };
            ImGui::Combo("Mode", &tbuild.mode, modes, 3);
            ImGui::PopItemWidth();
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
void HeroBuildsWindow::View(const TeamHeroBuild& tbuild, unsigned int idx) {
	if (idx >= tbuild.builds.size()) return;
	const HeroBuild& build = tbuild.builds[idx];

	std::string build_name;
	HeroBuildName(tbuild, idx, &build_name);
	if (build_name.empty()) {
		return; // No name = no build.
	}

	GW::UI::ChatTemplate* t = new GW::UI::ChatTemplate();
	t->code.m_buffer = new wchar_t[128];
	MultiByteToWideChar(CP_UTF8, 0, build.code, -1, t->code.m_buffer, 128);
	t->code.m_size = t->code.m_capacity = wcslen(t->code.m_buffer);
	t->name = new wchar_t[128];
	MultiByteToWideChar(CP_UTF8, 0, build_name.c_str(), -1, t->name, 128);
	GW::GameThread::Enqueue([t] {
		GW::UI::SendUIMessage(GW::UI::kOpenTemplate, t);
		delete[] t->code.m_buffer;
		delete[] t->name;
		delete t;
		});

}
void HeroBuildsWindow::Send(const TeamHeroBuild& tbuild) {
	if (!std::string(tbuild.name).empty()) {
		send_queue.push(tbuild.name);
	}
	for (unsigned int i = 0; i < tbuild.builds.size(); ++i) {
		if (i == 0) {
			const HeroBuild& build = tbuild.builds[i];
			if (build.code[0] == 0 && build.name[0] == 0)
				continue; // Player build is empty.
		}
		Send(tbuild, i);
	}
}
void HeroBuildsWindow::Send(const TeamHeroBuild& tbuild, size_t idx) {
	if (idx >= tbuild.builds.size()) return;
	const HeroBuild& build = tbuild.builds[idx];
	const std::string name(build.name);
	const std::string code(build.code);

	const int buf_size = 139;
	char buf[buf_size];
	std::string build_name;
	HeroBuildName(tbuild, idx, &build_name);
	if (build_name.empty()) {
		return; // No name = no build.
	}

	if (code.empty()) {
		snprintf(buf, buf_size, "%s", build_name.c_str());
	}
	else {
		snprintf(buf, buf_size, "[%s;%s]", build_name.c_str(), build.code);
	}
	if(buf[0])
		send_queue.push(buf);
}
void HeroBuildsWindow::HeroBuildName(const TeamHeroBuild& tbuild, unsigned int idx, std::string* out) {
	if (idx >= tbuild.builds.size()) return;
	const HeroBuild& build = tbuild.builds[idx];
	const std::string name(build.name);
	const std::string code(build.code);
	char buf[128];
	auto id = idx > 0 && build.hero_index > 0 ? HeroIndexToID[build.hero_index] : 0;
	if (name.empty() && code.empty() && id == HeroID::NoHero) {
		return; // nothing to do here
	}
	const char* c;
	if (id < HeroID::Merc1 || id > HeroID::Merc8) {
		c = HeroName[id];
	}
	else if(idx > 0) {
		bool match = false;
		auto ctx = GW::GameContext::instance();
		auto& hero_array = ctx->world->hero_info;
		for (auto& hero : hero_array) {
			if (hero.hero_id != id)
				continue;
			wcstombs(MercHeroNames[id - HeroID::Merc1], hero.name, 20);
			c = MercHeroNames[id - HeroID::Merc1];
			match = true;
		}
		if (!match)
			c = HeroName[id];
	}
	if (name.empty()) {
		if (idx > 0)
			snprintf(buf, 128, "%s", c);
	}
	else {
		snprintf(buf, 128, "%s (%s)", name.c_str(), idx == 0 ? "Player" : c);
	}
	if (buf[0])
		out->assign(buf);
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
	pending_hero_loads.clear();
    if (tbuild.mode > 0) {
        GW::PartyMgr::SetHardMode(tbuild.mode == 2);
    }
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
	} else if (build.hero_index > 0) {
		if (build.hero_index < 0 || build.hero_index >= hero_count) {
			Log::Error("Bad hero index '%d' for build '%s'", build.hero_index, build.name);
			return;
		}
		GW::Constants::HeroID heroid = HeroIndexToID[build.hero_index];

		if (heroid == HeroID::NoHero) return;

		GW::PartyMgr::AddHero(heroid);
		if (!code.empty()) {
			pending_hero_loads.push_back(CodeOnHero(code.c_str(), heroid));
		}
	}
}

void HeroBuildsWindow::Update(float delta) {
	if (!send_queue.empty() && TIMER_DIFF(send_timer) > 600) {
		if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading && GW::Agents::GetPlayer()) {
			GW::Chat::SendChat('#', send_queue.front().c_str());
			send_queue.pop();
			send_timer = TIMER_INIT();
		}
		else {
			while (!send_queue.empty())
				send_queue.pop();
		}
	}
	static uint32_t hero_index = 0;
	for (size_t i = 0; i < pending_hero_loads.size() && TIMER_DIFF(load_timer) > 100;i++) {
		if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost) {
			pending_hero_loads.clear();
			break;
		}
		if (!GetPartyHeroByID(pending_hero_loads[i].heroid, &hero_index)) {
			if (TIMER_DIFF(pending_hero_loads[i].started) > 1000) {
				// Waited for 1000ms and still no hero - presume user doesn't have space in party or hero isn't unlocked.
				pending_hero_loads.erase(pending_hero_loads.begin() + i);
				break; // Continue loop on next frame
			}
			continue; // Hero not found in party list... yet!
		}
		GW::SkillbarMgr::LoadSkillTemplate(pending_hero_loads[i].code, hero_index);
		load_timer = TIMER_INIT();
		pending_hero_loads.erase(pending_hero_loads.begin() + i);
		break; // Continue loop on next frame
	}

	// if we open the window, load from file. If we close the window, save to file. 
	static bool old_visible = false;
	bool cur_visible = false;
	cur_visible |= visible;
	for (TeamHeroBuild& tbuild : teambuilds) {
		cur_visible |= tbuild.edit_open;
	}

	if (cur_visible != old_visible) {
		old_visible = cur_visible;
		if (cur_visible) {
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
	// clear builds from toolbox
	teambuilds.clear();

	if (inifile == nullptr) inifile = new CSimpleIni(false, false, false);
	inifile->LoadFile(Resources::GetPath(INI_FILENAME).c_str());

	// then load
	CSimpleIni::TNamesDepend entries;
	inifile->GetAllSections(entries);
	for (CSimpleIni::Entry& entry : entries) {
		const char* section = entry.pItem;

		TeamHeroBuild tb(inifile->GetValue(section, "buildname", ""));
		tb.mode = inifile->GetLongValue(section, "mode", false);
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
			int hero_index = inifile->GetLongValue(section, heroindexkey, -1);
			if (hero_index < -2) {
				hero_index = -1; // can happen due to an old bug
			}
			HeroBuild build(nameval, templateval, hero_index);
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
		if (inifile == nullptr) inifile = new CSimpleIni();

		// clear builds from ini
		inifile->Reset();

		// then save
		for (unsigned int i = 0; i < teambuilds.size(); ++i) {
			const TeamHeroBuild& tbuild = teambuilds[i];
			char section[16];
			snprintf(section, 16, "builds%03d", i);
			inifile->SetValue(section, "buildname", tbuild.name);
			inifile->SetLongValue(section, "mode", tbuild.mode);
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
