#include "stdafx.h"

#include <GWCA/GameContainers/Array.h>

#include <GWCA/GameEntities/Hero.h>

#include <GWCA/GameEntities/Agent.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <Logger.h>
#include <Utils/GuiUtils.h>

#include <Modules/Resources.h>
#include <Windows/HeroBuildsWindow.h>

constexpr const wchar_t* INI_FILENAME = L"herobuilds.ini";

namespace {

    using GW::Constants::HeroID;

    // hero index is an arbitrary index.
    // We aim to have the same order as in the gw client.
    // Razah is after the mesmers because all players that don't have mercs have it set as mesmer.
    const HeroID HeroIndexToID[] = {
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

    const size_t GetPlayerHeroCount() {
        size_t ret = 0;
        GW::PartyInfo* party_info = GW::PartyMgr::GetPartyInfo();
        if (!party_info) return ret;
        const GW::HeroPartyMemberArray& party_heros = party_info->heroes;
        if (!party_heros.valid()) return ret;
        GW::AgentLiving* me = GW::Agents::GetPlayerAsAgentLiving();
        if (!me) return ret;
        uint32_t my_player_id = me->login_number;
        for (size_t i = 0; i < party_heros.size(); i++) {
            if (party_heros[i].owner_player_id == my_player_id)
                ret++;
        }
        return ret;
    }
    const GW::HeroFlag* GetHeroFlagInfo(uint32_t hero_id) {
        const GW::GameContext* g = GW::GetGameContext();
        if (!g || !g->world) return nullptr;
        const GW::HeroFlagArray& flags = g->world->hero_flags;
        for (const GW::HeroFlag& flag : flags) {
            if (flag.hero_id == hero_id)
                return &flag;
        }
        return nullptr;
    }

}

unsigned int HeroBuildsWindow::TeamHeroBuild::cur_ui_id = 0;

GW::HeroPartyMember* HeroBuildsWindow::GetPartyHeroByID(HeroID hero_id, size_t* out_hero_index) {
    GW::PartyInfo* party_info = GW::PartyMgr::GetPartyInfo();
    if (!party_info) return nullptr;
    GW::HeroPartyMemberArray &party_heros = party_info->heroes;
    if (!party_heros.valid()) return nullptr;
    GW::AgentLiving* me = GW::Agents::GetPlayerAsAgentLiving();
    if (!me) return nullptr;
    uint32_t my_player_id = me->login_number;
    for (size_t i = 0; i < party_heros.size(); i++) {
        if (party_heros[i].owner_player_id == my_player_id &&
            party_heros[i].hero_id == hero_id) {
            if (out_hero_index)
                *out_hero_index = i + 1;
            return &party_heros[i];
        }
    }
    return nullptr;
}

void HeroBuildsWindow::Initialize() {
    ToolboxWindow::Initialize();
    send_timer = TIMER_INIT();
    GW::Chat::CreateCommand(L"heroteam", &CmdHeroTeamBuild);
    GW::Chat::CreateCommand(L"herobuild", &CmdHeroTeamBuild);
}

void HeroBuildsWindow::Terminate() {
    ToolboxWindow::Terminate();
    teambuilds.clear();
}

void HeroBuildsWindow::Draw(IDirect3DDevice9*) {
    if (visible) {
        ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(300, 250), ImGuiCond_FirstUseEver);
        if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
            const float btn_width = 60.0f * ImGui::GetIO().FontGlobalScale;
            const float &item_spacing = ImGui::GetStyle().ItemInnerSpacing.x;
            for (TeamHeroBuild& tbuild : teambuilds) {
                ImGui::PushID(static_cast<int>(tbuild.ui_id));
                ImGui::GetStyle().ButtonTextAlign = ImVec2(0.0f, 0.5f);
                if (ImGui::Button(tbuild.name, ImVec2(ImGui::GetContentRegionAvail().x - item_spacing - btn_width, 0))) {
                    if (one_teambuild_at_a_time && !tbuild.edit_open) {
                        for (auto& tb : teambuilds) {
                            tb.edit_open = false;
                        }
                    }
                    tbuild.edit_open = !tbuild.edit_open;
                }
                ImGui::GetStyle().ButtonTextAlign = ImVec2(0.5f, 0.5f);
                ImGui::SameLine(0, item_spacing);
                if (ImGui::Button(ImGui::GetIO().KeyCtrl ? "Send" : "Load", ImVec2(btn_width, 0))) {
                    if (ImGui::GetIO().KeyCtrl)
                        Send(tbuild);
                    else
                        Load(tbuild);
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip(ImGui::GetIO().KeyCtrl ? "Click to send to team chat" : "Click to load builds to heroes and player. Ctrl + Click to send to chat.");
                ImGui::PopID();
            }
            if (ImGui::Button("Add Teambuild", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                TeamHeroBuild tb = TeamHeroBuild("");
                tb.builds.reserve(8); // at this point why don't we use a static array ??
                tb.builds.push_back(HeroBuild("", "", -2));
                for (int i = 0; i < 7; ++i) {
                    tb.builds.push_back(HeroBuild("", ""));
                }

                builds_changed = true;
                teambuilds.push_back(tb);
            }
            /* Code for copying a teambuild */
            std::vector<const char*> names(teambuilds.size(), "\0");
            std::ranges::transform(teambuilds
                                   ,
                                   names.begin(),
                                   [](const TeamHeroBuild& tb) { return tb.name; }
            );
            const int num_elements = static_cast<int>(names.size());
            static int selectedTeambuild = 0;
            ImGui::PushItemWidth(-60.0f - ImGui::GetStyle().ItemInnerSpacing.x);
            ImGui::Combo("###teamBuildCombo", &selectedTeambuild, names.data(), num_elements);
            ImGui::PopItemWidth();
            ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
            if (ImGui::Button("Copy##1", ImVec2(60.0f, 0))) {
                TeamHeroBuild new_tb = teambuilds[selectedTeambuild];
                std::string copy_name = std::string(new_tb.name) + " (Copy)";
                GuiUtils::StrCopy(new_tb.name, copy_name.c_str(), sizeof(new_tb.name));
                builds_changed = true;
                teambuilds.push_back(new_tb);
            }
        }
        ImGui::End();
    }

    for (size_t i = 0; i < teambuilds.size(); ++i) {
        if (!teambuilds[i].edit_open) continue;
        TeamHeroBuild& tbuild = teambuilds[i];
        constexpr size_t winname_buffer_size = 256;
        char winname[winname_buffer_size];
        snprintf(winname, winname_buffer_size, "%s###herobuild%d", tbuild.name, tbuild.ui_id);
        ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(500, 0), ImGuiCond_FirstUseEver);
        if (ImGui::Begin(winname, &tbuild.edit_open)) {
            constexpr size_t name_buffer_size = 128;
            builds_changed |= ImGui::InputText("Hero Build Name", tbuild.name, name_buffer_size);
            const float btn_width = 50.0f * ImGui::GetIO().FontGlobalScale;
            const float icon_btn_width = btn_width / 1.75f;
            const float panel_width = btn_width + 12.0f;
            const float item_spacing = ImGui::GetStyle().ItemInnerSpacing.x;
            const float text_item_width = (ImGui::GetContentRegionAvail().x - btn_width - btn_width - btn_width - panel_width - (item_spacing * 3)) / 3;
            float offset = btn_width;
            ImGui::SetCursorPosX(offset);
            ImGui::Text("Name");
            ImGui::SameLine(offset += text_item_width + item_spacing);
            ImGui::Text("Template");
            for (size_t j = 0; j < tbuild.builds.size(); ++j) {
                offset = btn_width;
                HeroBuild& build = tbuild.builds[j];
                ImGui::PushID(static_cast<int>(j));
                if (j == 0) {
                    ImGui::Text("P");
                } else {
                    ImGui::Text("H#%d", j);
                }
                ImGui::SameLine(offset);
                ImGui::PushItemWidth(text_item_width);
                builds_changed |= ImGui::InputText("###name", build.name, name_buffer_size);
                ImGui::SameLine(offset += text_item_width + item_spacing);
                builds_changed |= ImGui::InputText("###code", build.code, name_buffer_size);
                ImGui::SameLine(offset += text_item_width + item_spacing);
                if (j == 0) {
                    ImGui::TextDisabled("Player");
                    ImGui::SameLine(offset += text_item_width + item_spacing + btn_width + 10.0f + item_spacing);
                    ImGui::PopItemWidth();
                } else {
                    if (ImGui::MyCombo("###heroid", "Choose Hero", &build.hero_index,
                        [](void*, int idx, const char** out_text) -> bool {
                            if (idx < 0) return false;
                            if (idx >= hero_count) return false;
                            auto id = HeroIndexToID[idx];
                            if (id < HeroID::Merc1 || id > HeroID::Merc8) {
                                *out_text = HeroName[HeroIndexToID[idx]];
                                return true;
                            }
                            bool match = false;
                            auto ctx = GW::GetGameContext();
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
                        },
                        nullptr, hero_count))
                    {
                        builds_changed = true;
                    }
                    ImGui::PopItemWidth();
                    ImGui::SameLine(offset += text_item_width + item_spacing);
                    const auto* icon = reinterpret_cast<const char*>(build.show_panel ? ICON_FA_EYE : ICON_FA_EYE_SLASH);
                    if (ImGui::Button(icon, ImVec2(icon_btn_width, 0))) {
                        build.show_panel = !build.show_panel;
                        builds_changed = true;
                    }
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip(build.show_panel ? "Hero panel: Show" : "Hero panel: Hide");
                    ImGui::SameLine(offset += icon_btn_width + item_spacing);
                    const char* hero_stance_icon = reinterpret_cast<const char*>(ICON_FA_SHIELD_ALT);
                    const char* hero_stance_tooltip = "Hero behaviour: Guard";
                    switch (build.behavior) {
                    case 2:
                            hero_stance_icon = reinterpret_cast<const char*>(ICON_FA_DOVE);
                        hero_stance_tooltip = "Hero behaviour: Avoid Combat";
                        break;
                    case 0:
                        hero_stance_icon = reinterpret_cast<const char*>(ICON_FA_FIST_RAISED);
                        hero_stance_tooltip = "Hero behaviour: Fight";
                        break;
                    }
                    if (ImGui::Button(hero_stance_icon, ImVec2(icon_btn_width, 0))) {
                        build.behavior++;
                        if (build.behavior > 2)
                            build.behavior = 0;
                        builds_changed = true;
                    }
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip(hero_stance_tooltip);
                    ImGui::SameLine(offset += icon_btn_width + item_spacing);
                }

                if (ImGui::Button(ImGui::GetIO().KeyCtrl ? "Send" : "View", ImVec2(btn_width, 0))) {
                    if (ImGui::GetIO().KeyCtrl)
                        Send(tbuild, j);
                    else
                        View(tbuild, j);
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip(ImGui::GetIO().KeyCtrl ? "Click to send to team chat" : "Click to view build. Ctrl + Click to send to chat.");
                ImGui::SameLine(offset += btn_width + item_spacing);
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
            ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - ImGui::GetStyle().WindowPadding.x - 40);
            if (ImGui::Button("Close", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                tbuild.edit_open = false;
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Close this window");

            if (ImGui::BeginPopupModal("Delete Teambuild?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("Are you sure?\nThis operation cannot be undone.\n\n");
                if (ImGui::Button("OK", ImVec2(120, 0))) {
                    teambuilds.erase(teambuilds.begin() + static_cast<int>(i));
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
void HeroBuildsWindow::View(const TeamHeroBuild& tbuild, size_t idx) {
    if (idx >= tbuild.builds.size()) return;
    const HeroBuild& build = tbuild.builds[idx];

    std::string build_name;
    HeroBuildName(tbuild, idx, &build_name);
    if (build_name.empty()) {
        return; // No name = no build.
    }

    GW::UI::ChatTemplate* t = new GW::UI::ChatTemplate();
    constexpr size_t buffer_size = 128;
    t->code.m_buffer = new wchar_t[buffer_size];
    MultiByteToWideChar(CP_UTF8, 0, build.code, -1, t->code.m_buffer, buffer_size);
    t->code.m_size = t->code.m_capacity = wcslen(t->code.m_buffer);
    t->name = new wchar_t[buffer_size];
    MultiByteToWideChar(CP_UTF8, 0, build_name.c_str(), -1, t->name, buffer_size);
    GW::GameThread::Enqueue([t] {
        GW::UI::SendUIMessage(GW::UI::UIMessage::kOpenTemplate, t);
        delete[] t->code.m_buffer;
        delete[] t->name;
        delete t;
        });

}
void HeroBuildsWindow::Send(const TeamHeroBuild& tbuild) {
    if (!std::string(tbuild.name).empty()) {
        send_queue.push(tbuild.name);
    }
    for (size_t i = 0; i < tbuild.builds.size(); ++i) {
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

    constexpr int buffer_size = 139;
    char buffer[buffer_size];
    std::string build_name;
    HeroBuildName(tbuild, idx, &build_name);
    if (build_name.empty()) {
        return; // No name = no build.
    }

    if (code.empty()) {
        snprintf(buffer, buffer_size, "%s", build_name.c_str());
    }
    else {
        snprintf(buffer, buffer_size, "[%s;%s]", build_name.c_str(), build.code);
    }
    if(buffer[0])
        send_queue.push(buffer);
}
void HeroBuildsWindow::HeroBuildName(const TeamHeroBuild& tbuild, size_t idx, std::string* out) {
    if (idx >= tbuild.builds.size()) return;
    const HeroBuild& build = tbuild.builds[idx];
    const std::string name(build.name);
    const std::string code(build.code);
    constexpr int buffer_size = 128;
    char buffer[buffer_size];
    auto id = idx > 0 && build.hero_index > 0 ? HeroIndexToID[build.hero_index] : 0;
    if (name.empty() && code.empty() && id == HeroID::NoHero) {
        return; // nothing to do here
    }
    const char* c = "";
    if (id < HeroID::Merc1 || id > HeroID::Merc8) {
        c = HeroName[id];
    }
    else if(idx > 0) {
        bool match = false;
        auto ctx = GW::GetGameContext();
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
        if (idx > 0) snprintf(buffer, buffer_size, "%s", c);
    }
    else {
        snprintf(buffer, buffer_size, "%s (%s)", name.c_str(), idx == 0 ? "Player" : c);
    }
    if (buffer[0])
        out->assign(buffer);
}
const char* HeroBuildsWindow::BuildName(size_t idx) const {
    if (idx < teambuilds.size()) {
        return teambuilds[idx].name;
    } else {
        return nullptr;
    }
}
void HeroBuildsWindow::Load(size_t idx) {
    if (idx < teambuilds.size()) {
        Load(teambuilds[idx]);
    }
}
void HeroBuildsWindow::Load(const HeroBuildsWindow::TeamHeroBuild& tbuild) {
    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost) return;

    GW::PartyMgr::KickAllHeroes();
    kickall_timer = TIMER_INIT();
    pending_hero_loads.clear();
    if (tbuild.mode > 0) {
        GW::PartyMgr::SetHardMode(tbuild.mode == 2);
    }
    for (size_t i = 0; i < tbuild.builds.size(); ++i) {
        Load(tbuild, i);
    }
    send_timer = TIMER_INIT(); // give GW time to update the hero structs after adding them.
}

void HeroBuildsWindow::Load(const TeamHeroBuild& tbuild, size_t idx) {
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

        pending_hero_loads.push_back({ code.c_str(), heroid,build.show_panel, build.behavior });
    }
}

void HeroBuildsWindow::Update(float) {
    const GW::Constants::InstanceType& instance_type = GW::Map::GetInstanceType();
    if (instance_type != last_instance_type) {
        // Run tasks on map change without an StoC hook
        if (hide_when_entering_explorable && instance_type == GW::Constants::InstanceType::Explorable) {
            for (auto& hb : teambuilds) {
                hb.edit_open = false;
            }
            visible = false;
        }
        if (instance_type == GW::Constants::InstanceType::Loading) {
            while (send_queue.size())
                send_queue.pop();
            kickall_timer = 0;
            pending_hero_loads.clear();
        }
        last_instance_type = instance_type;
    }

    if (!send_queue.empty() && TIMER_DIFF(send_timer) > 600) {
        GW::Chat::SendChat('#', send_queue.front().c_str());
        send_queue.pop();
        send_timer = TIMER_INIT();
    }
    if (kickall_timer) {
        if (TIMER_DIFF(kickall_timer) > 500 || instance_type != GW::Constants::InstanceType::Outpost || !GetPlayerHeroCount())
            kickall_timer = 0;
    }
    for (size_t i = 0; !kickall_timer && i < pending_hero_loads.size();i++) {
        if (instance_type != GW::Constants::InstanceType::Outpost) {
            pending_hero_loads.clear();
            break;
        }
        if (pending_hero_loads[i].Process()) {
            pending_hero_loads.erase(pending_hero_loads.begin() + static_cast<int>(i));
            break; // Continue loop on next frame
        }
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
    last_instance_type = instance_type;
}

void HeroBuildsWindow::CmdHeroTeamBuild(const wchar_t*, int argc, LPWSTR* argv) {
    if (argc < 2) {
        Log::ErrorW(L"Syntax: /%s [hero_build_name]",argv[0]);
        return;
    }
    std::wstring argBuildname = argv[1];
    for (int i = 2; i < argc; i++) {
        argBuildname.append(L" ");
        argBuildname.append(argv[i]);
    }
    std::string argBuildName_s = GuiUtils::WStringToString(argBuildname);
    HeroBuildsWindow::TeamHeroBuild* found = Instance().GetTeambuildByName(argBuildName_s);
    if (!found) {
        Log::ErrorW(L"No hero build found for %s",argBuildname.c_str());
        return;
    }
    const TeamHeroBuild& tbuild = *(const TeamHeroBuild*)found;
    Instance().Load(tbuild);
}

void HeroBuildsWindow::LoadSettings(ToolboxIni* ini) {
    ToolboxWindow::LoadSettings(ini);
    hide_when_entering_explorable = ini->GetBoolValue(Name(), VAR_NAME(hide_when_entering_explorable), hide_when_entering_explorable);
    one_teambuild_at_a_time = ini->GetBoolValue(Name(), VAR_NAME(one_teambuild_at_a_time), one_teambuild_at_a_time);
    LoadFromFile();
}

void HeroBuildsWindow::DrawSettingInternal() {
    ImGui::Checkbox("Hide Hero Build windows when entering explorable area", &hide_when_entering_explorable);
    ImGui::Checkbox("Only show one teambuild window at a time", &one_teambuild_at_a_time);
    ImGui::ShowHelp("Close other teambuild windows when you open a new one");
}

void HeroBuildsWindow::SaveSettings(ToolboxIni* ini) {
    ToolboxWindow::SaveSettings(ini);
    ini->SetBoolValue(Name(), VAR_NAME(hide_when_entering_explorable), hide_when_entering_explorable);
    ini->SetBoolValue(Name(), VAR_NAME(one_teambuild_at_a_time), one_teambuild_at_a_time);
    SaveToFile();
}

void HeroBuildsWindow::LoadFromFile() {
    // clear builds from toolbox
    teambuilds.clear();

    inifile->LoadFile(Resources::GetPath(INI_FILENAME).c_str());

    // then load
    ToolboxIni::TNamesDepend entries;
    inifile->GetAllSections(entries);
    for (ToolboxIni::Entry& entry : entries) {
        const char* section = entry.pItem;

        TeamHeroBuild tb(inifile->GetValue(section, "buildname", ""));
        tb.mode = inifile->GetLongValue(section, "mode", false);
        tb.builds.reserve(8);

        constexpr size_t buffer_size = 16;
        for (int i = 0; i < 8; ++i) {
            char namekey[buffer_size];
            char templatekey[buffer_size];
            char heroindexkey[buffer_size];
            char showpanelkey[buffer_size];
            char behaviorkey[buffer_size];
            snprintf(namekey, buffer_size, "name%d", i);
            snprintf(templatekey, buffer_size, "template%d", i);
            snprintf(heroindexkey, buffer_size, "heroindex%d", i);
            snprintf(showpanelkey, buffer_size, "panel%d", i);
            snprintf(behaviorkey, buffer_size, "behavior%d", i);
            const char* nameval = inifile->GetValue(section, namekey, "");
            const char* templateval = inifile->GetValue(section, templatekey, "");
            int hero_index = inifile->GetLongValue(section, heroindexkey, -1);
            if (hero_index < -2) {
                hero_index = -1; // can happen due to an old bug
            }
            int show_panel = inifile->GetLongValue(section, showpanelkey, 0);
            uint32_t behavior = static_cast<uint32_t>(inifile->GetLongValue(section, behaviorkey, 1));
            HeroBuild build(nameval, templateval, hero_index, show_panel == 1 ? 1 : 0, behavior);
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
    constexpr size_t buffer_size = 16;
    if (builds_changed) {
        // clear builds from ini
        inifile->Reset();

        // then save
        for (size_t i = 0; i < teambuilds.size(); ++i) {
            const TeamHeroBuild& tbuild = teambuilds[i];
            char section[buffer_size];
            snprintf(section, buffer_size, "builds%03d", i);
            inifile->SetValue(section, "buildname", tbuild.name);
            inifile->SetLongValue(section, "mode", tbuild.mode);
            for (size_t j = 0; j < tbuild.builds.size(); ++j) {
                const HeroBuild& build = tbuild.builds[j];

                char namekey[buffer_size];
                char templatekey[buffer_size];
                char heroindexkey[buffer_size];
                char showpanelkey[buffer_size];
                char behaviorkey[buffer_size];
                snprintf(namekey, buffer_size, "name%d", j);
                snprintf(templatekey, buffer_size, "template%d", j);
                snprintf(heroindexkey, buffer_size, "heroindex%d", j);
                snprintf(showpanelkey, buffer_size, "panel%d", j);
                snprintf(behaviorkey, buffer_size, "behavior%d", j);
                inifile->SetValue(section, namekey, build.name);
                inifile->SetValue(section, templatekey, build.code);
                inifile->SetLongValue(section, heroindexkey, build.hero_index);
                inifile->SetLongValue(section, showpanelkey, build.show_panel);
                inifile->SetLongValue(section, behaviorkey, build.behavior);
            }
        }

        inifile->SaveFile(Resources::GetPath(INI_FILENAME).c_str());
    }
}

bool HeroBuildsWindow::CodeOnHero::Process() {
    if (!started)
        started = TIMER_INIT();
    if (TIMER_DIFF(started) > 1000)
        return true; // Consume, timeout.
    switch (stage) {
    case Add: // Need to add hero to party
        GW::PartyMgr::AddHero(heroid);
        stage = Load;
    case Load: // Waiting for hero to be added to party
        {
            const GW::HeroPartyMember* hero = GetPartyHeroByID(heroid, &party_hero_index);
            if (!hero)
                break;
            const GW::HeroFlag* flag = GetHeroFlagInfo(heroid);
            if (!flag)
                break;
            if (code[0]) // Build optional
                GW::SkillbarMgr::LoadSkillTemplate(code, party_hero_index);
            if (show_panel)
                GW::UI::SendUIMessage(GW::UI::UIMessage::kShowHeroPanel, (void*)heroid);
            else
                GW::UI::SendUIMessage(GW::UI::UIMessage::kHideHeroPanel, (void*)heroid);
            if (flag->hero_behavior != behavior)
                GW::PartyMgr::SetHeroBehavior(hero->agent_id, behavior);
        }
        stage = Finished;
    case Finished: // Success, hero added and build loaded.
        return true;
    }
    return false;
}

HeroBuildsWindow::TeamHeroBuild* HeroBuildsWindow::GetTeambuildByName(std::string& build_name_search) {
    std::string compare = GuiUtils::ToLower(GuiUtils::RemovePunctuation(build_name_search));
    for (auto& tb : teambuilds) {
        std::string name = GuiUtils::ToLower(GuiUtils::RemovePunctuation(tb.name));
        if (name.length() < compare.length())
            continue; // String entered by user is longer
        if (name.rfind(compare) == 0)
            return &tb;
    }
    return nullptr;
}