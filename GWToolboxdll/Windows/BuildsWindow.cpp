#include "stdafx.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameEntities/Agent.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/PlayerMgr.h>

#include <GuiUtils.h>
#include <Logger.h>

#include <Modules/Resources.h>
#include <Windows/BuildsWindow.h>
#include <Windows/PconsWindow.h>

unsigned int BuildsWindow::TeamBuild::cur_ui_id = 0;

bool order_by_changed = false;

#define INI_FILENAME L"builds.ini"

BuildsWindow::~BuildsWindow() {

}

void BuildsWindow::Initialize() {
    ToolboxWindow::Initialize();
    Resources::Instance().LoadTextureAsync(&button_texture, Resources::GetPath(L"img/icons", L"list.png"), IDB_Icon_list);
    send_timer = TIMER_INIT();

    GW::Chat::CreateCommand(L"loadbuild", CmdLoad);
}
void BuildsWindow::DrawHelp() {
    if (!ImGui::TreeNode("Build Chat Commands"))
        return;
    ImGui::Bullet(); ImGui::Text("'/load [build template|build name] [Hero index]' loads a build via Guild Wars builds. The build name must be between quotes if it contains spaces. First Hero index is 1, last is 7. Leave out for player");
    ImGui::Bullet(); ImGui::Text("'/loadbuild [teambuild] <build name|build code>' loads a build via GWToolbox Builds window. Does a partial search on team build name/build name/build code. Matches current player's profession.");
    ImGui::TreePop();
}
void BuildsWindow::CmdLoad(const wchar_t* message, int argc, LPWSTR* argv) {
    UNREFERENCED_PARAMETER(message);
    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost)
        return;
    if (argc > 2) {
        std::string build_name = GuiUtils::WStringToString(argv[2]);
        std::string team_build_name = GuiUtils::WStringToString(argv[1]);
        Instance().Load(team_build_name.c_str(), build_name.c_str());
        return;
    }
    if (argc > 1) {
        std::string build_name = GuiUtils::WStringToString(argv[1]);
        Instance().Load(build_name.c_str());
        return;
    }
    Log::Error("Not enough arguments. See Help for chat commands");
    return;
}
bool BuildsWindow::BuildSkillTemplateString(const TeamBuild& tbuild, unsigned int idx, char* out, unsigned int out_len) {
    if (!out || idx >= tbuild.builds.size()) return false;
    const Build& build = tbuild.builds[idx];
    const std::string name(build.name);
    const std::string code(build.code);

    if (name.empty() && code.empty())
        return false; // nothing to do here
    if (name.empty()) {
        // name is empty, fill it with the teambuild name
        snprintf(out, out_len, "[%s %d;%s]", tbuild.name, idx + 1, build.code);
    }
    else if (code.empty()) {
        // code is empty, just print the name without template format
        snprintf(out, out_len, "%s", build.name);
    }
    else if (tbuild.show_numbers) {
        // add numbers in front of name
        snprintf(out, out_len, "[%d - %s;%s]", idx + 1, build.name, build.code);
    }
    else {
        // simple template
        snprintf(out, out_len, "[%s;%s]", build.name, build.code);
    }
    return true;
}

void BuildsWindow::Terminate() {
    ToolboxWindow::Terminate();
    teambuilds.clear();
    if (inifile)
        delete inifile;
}

void BuildsWindow::DrawSettingInternal() {
    ImGui::Checkbox("Hide Build windows when entering explorable area", &hide_when_entering_explorable);
    ImGui::Checkbox("Only show one teambuild window at a time", &one_teambuild_at_a_time);
    ImGui::ShowHelp("Close other teambuild windows when you open a new one");
    ImGui::Checkbox("Auto load pcons",&auto_load_pcons);
    ImGui::ShowHelp("Automatically load pcons for a build when loaded onto a character");
    ImGui::Checkbox("Send pcons when pinging a build", &auto_send_pcons);
    ImGui::ShowHelp("Automatically send a second message after the build template in team chat,\nshowing the pcons that the build uses.");
    ImGui::Text("Order team builds by: ");
    ImGui::SameLine(0, -1);
    if (ImGui::Checkbox("Index", &order_by_index)) {
        order_by_changed = true;
        order_by_name = !order_by_index;
    }
    ImGui::SameLine(0, -1);
    if (ImGui::Checkbox("Name", &order_by_name)) {
        order_by_changed = true;
        order_by_index = !order_by_name;
    }
}

void BuildsWindow::DrawBuildSection(TeamBuild& tbuild, unsigned int j) {
    Build& build = tbuild.builds[j];
    const float font_scale = ImGui::GetIO().FontGlobalScale;
    const float btn_width = 50.0f * font_scale;
    const float del_width = 24.0f * font_scale;
    const float spacing = ImGui::GetStyle().ItemSpacing.y;
    const float btn_offset = ImGui::GetContentRegionAvailWidth() - del_width - btn_width * 3 - spacing * 3;
    ImGui::Text("#%d", j + 1);
    ImGui::PushItemWidth((btn_offset - btn_width - spacing * 2) / 2);
    ImGui::SameLine(btn_width, 0);
    if (ImGui::InputText("###name", build.name, 128)) builds_changed = true;
    ImGui::SameLine(0, spacing);
    if (ImGui::InputText("###code", build.code, 128)) builds_changed = true;
    ImGui::PopItemWidth();
    ImGui::SameLine(btn_offset);
    if (ImGui::Button(ImGui::GetIO().KeyCtrl ? "Send" : "View", ImVec2(btn_width, 0))) {
        if (ImGui::GetIO().KeyCtrl)
            Send(tbuild, j);
        else
            View(tbuild, j);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(ImGui::GetIO().KeyCtrl ? "Click to send to team chat" : "Click to view build. Ctrl + Click to send to chat.");
    ImGui::SameLine(0, spacing);
    if (ImGui::Button("Load", ImVec2(btn_width, 0)))
        Load(tbuild, j);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(!build.pcons.empty() ? "Click to load build template and pcons" : "Click to load build template");
    ImGui::SameLine(0, spacing);
    bool pcons_editing = tbuild.edit_pcons == static_cast<int>(j);
    if(pcons_editing) 
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));
    if (build.pcons.empty())
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    if (ImGui::Button("Pcons", ImVec2(btn_width, 0)))
        tbuild.edit_pcons = pcons_editing ? -1 : static_cast<int>(j);
    if(pcons_editing) ImGui::PopStyleColor();
    if(build.pcons.empty()) ImGui::PopStyleColor();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Click to modify pcons for this build");
    ImGui::SameLine(0, spacing);
    if (ImGui::Button("x", ImVec2(del_width, 0))) {
        if (delete_builds_without_prompt) {
            tbuild.builds.erase(tbuild.builds.begin() + static_cast<int>(j));
            builds_changed = true;
        }
        else {
            ImGui::OpenPopup("Delete Build?");
        }
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Delete build");
    if (ImGui::BeginPopupModal("Delete Build?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Are you sure?\nThis operation cannot be undone.\n\n");
        ImGui::Checkbox("Don't remind me again", &delete_builds_without_prompt);
        ImGui::ShowHelp("This can be re-enabled in settings");
        if (ImGui::Button("OK", ImVec2(120, 0))) {
            tbuild.builds.erase(tbuild.builds.begin() + static_cast<int>(j));
            builds_changed = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (tbuild.edit_pcons != static_cast<int>(j))
        return; // Not editing this build.
    const float indent = btn_width - ImGui::GetStyle().ItemSpacing.x;
    ImGui::Indent(indent);
    if (ImGui::Button("Send Pcons", ImVec2(btn_width * 2, 0))) {
        SendPcons(tbuild, j);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Send this build's Pcons to team chat");
    auto pcons = PconsWindow::Instance().pcons;
        
    float pos_x = 0;
    float third_w = ImGui::GetContentRegionAvailWidth() / 3;
    unsigned int offset = 0;
    for (size_t i = 0; i < pcons.size(); i++) {
        auto pcon = pcons[i];
        bool active = build.pcons.find(pcon->ini) != build.pcons.end();
        ImGui::SameLine(indent, pos_x += third_w);
        offset++;
        if (i % 3 == 0) {
            ImGui::NewLine();
            offset = 1;
            pos_x = 0;
        }
                
        char pconlabel[128];
        snprintf(pconlabel, 128, "%s###pcon_%s", pcon->chat, pcon->ini);
        if (ImGui::Checkbox(pconlabel, &active)) {
            if (active)
                build.pcons.emplace(pcon->ini);
            else
                build.pcons.erase((char*)pcon->ini);
            builds_changed = true;
        }
    }
    ImGui::Unindent(indent);
}

void BuildsWindow::Draw(IDirect3DDevice9* pDevice) {
    UNREFERENCED_PARAMETER(pDevice);
    // @Cleanup: Use the BuildsWindow instance, not a static variable
    static GW::Constants::InstanceType last_instance_type = GW::Constants::InstanceType::Loading;
    GW::Constants::InstanceType instance_type = GW::Map::GetInstanceType();

    if (instance_type != last_instance_type) {
        // Run tasks on map change without an StoC hook
        if (hide_when_entering_explorable && instance_type == GW::Constants::InstanceType::Explorable) {
            for (auto& hb : teambuilds) {
                hb.edit_open = false;
            }
            visible = false;
        }
        last_instance_type = instance_type;
    }

    if (visible) {
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver, ImVec2(.5f, .5f));
        ImGui::SetNextWindowSize(ImVec2(300, 250), ImGuiCond_FirstUseEver);
        if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
            for (TeamBuild& tbuild : teambuilds) {
                ImGui::PushID(static_cast<int>(tbuild.ui_id));
                ImGui::GetStyle().ButtonTextAlign = ImVec2(0.0f, 0.5f);
                if (ImGui::Button(tbuild.name, ImVec2(ImGui::GetWindowContentRegionWidth()-ImGui::GetStyle().ItemInnerSpacing.x - 60.0f * ImGui::GetIO().FontGlobalScale, 0))) {
                    if (one_teambuild_at_a_time && !tbuild.edit_open) {
                        for (auto& tb : teambuilds) {
                            tb.edit_open = false;
                        }
                    }
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
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver, ImVec2(.5f, .5f));
        ImGui::SetNextWindowSize(ImVec2(500, 0), ImGuiCond_FirstUseEver);
        if (ImGui::Begin(winname, &tbuild.edit_open)) {
            ImGui::PushItemWidth(-120.0f);
            if (ImGui::InputText("Build Name", tbuild.name, 128)) builds_changed = true;
            ImGui::PopItemWidth();
            const float btn_width = 50.0f * ImGui::GetIO().FontGlobalScale;
            const float btn_offset = ImGui::GetContentRegionAvailWidth() - btn_width * 3 - ImGui::GetStyle().FramePadding.x * 3;
            for (unsigned int j = 0; j < tbuild.builds.size(); ++j) {
                ImGui::PushID(static_cast<int>(j));
                BuildsWindow::DrawBuildSection(tbuild, j);
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
        Send(tbuild, i);
    }
}
void BuildsWindow::View(const TeamBuild& tbuild, unsigned int idx) {
    if (idx >= tbuild.builds.size()) return;
    const Build& build = tbuild.builds[idx];

    GW::UI::ChatTemplate* t = new GW::UI::ChatTemplate();
    t->code.m_buffer = new wchar_t[128];
    MultiByteToWideChar(CP_UTF8, 0, build.code, -1, t->code.m_buffer, 128);
    t->code.m_size = t->code.m_capacity = wcslen(t->code.m_buffer);
    t->name = new wchar_t[128];
    MultiByteToWideChar(CP_UTF8, 0, build.name, -1, t->name, 128);
    GW::GameThread::Enqueue([t] {
        GW::UI::SendUIMessage(GW::UI::kOpenTemplate, t);
        delete[] t->code.m_buffer;
        delete[] t->name;
        });
    
}
void BuildsWindow::Load(const TeamBuild& tbuild, unsigned int idx) {
    if (idx >= tbuild.builds.size()) return;
    const Build& build = tbuild.builds[idx];
    if (!GW::SkillbarMgr::LoadSkillTemplate(build.code)) {
        char buf[192] = { 0 };
        Log::Error("Failed to load build template %s", BuildSkillTemplateString(tbuild, idx, buf, 192) ? buf : build.name);
        return;
    }
    if (!tbuild.edit_open) {
        char buf[192] = { 0 };
        Log::Info("Build loaded: %s", BuildSkillTemplateString(tbuild, idx, buf, 192) ? buf : build.code);
    }
    LoadPcons(tbuild, idx);
}
void BuildsWindow::Load(const char* build_name) {
    return BuildsWindow::Load(nullptr, build_name);
}
void BuildsWindow::Load(const char* tbuild_name, const char* build_name) {
    if (!build_name || GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost)
        return;
    GW::SkillbarMgr::SkillTemplate t;
    GW::Constants::Profession prof = (GW::Constants::Profession)GW::Agents::GetPlayerAsAgentLiving()->primary;
    bool is_skill_template = GW::SkillbarMgr::DecodeSkillTemplate(&t, build_name);
    if (is_skill_template && t.primary != prof) {
        Log::Error("Invalid profession for %s (%s)", build_name, GW::Constants::GetProfessionAcronym(t.primary).c_str());
        return;
    }
    std::string tbuild_ws = tbuild_name ? GuiUtils::ToLower(tbuild_name) : "";
    std::string build_ws = GuiUtils::ToLower(build_name);

    std::vector<std::pair<TeamBuild,size_t>> local_teambuilds;
    for (auto& tb : teambuilds) {
        size_t found = local_teambuilds.size();
        size_t tbuild_best_match = tb.builds.size();
        if (tbuild_name) {
            found = GuiUtils::ToLower(tb.name).find(tbuild_ws.c_str());
            if (found == std::string::npos)
                continue; // Teambuild name doesn't match
        }
        if (is_skill_template) {
            for (size_t i = 0; i < tb.builds.size();i++) {
                if (strcmp(tb.builds[i].code, build_name) != 0)
                    continue;
                tbuild_best_match = i;
                break;
            }
        }
        else {
            for (size_t i = 0; i < tb.builds.size(); i++) {
                if (GuiUtils::ToLower(tb.builds[i].name).find(build_ws.c_str()) == std::string::npos)
                    continue;
                GW::SkillbarMgr::SkillTemplate bt;
                if (!GW::SkillbarMgr::DecodeSkillTemplate(&bt, tb.builds[i].code))
                    continue; // Invalid build code
                if (bt.primary != prof)
                    continue; // Wrong profession.
                tbuild_best_match = i;
                break;
            }
        }
        if (tbuild_best_match >= tb.builds.size())
            continue; // This team build has no matching valid build codes
        local_teambuilds.push_back({ tb,tbuild_best_match });
    }
    // These are now in order of build match.
    if (local_teambuilds.empty()) {
        Log::Error("Failed to find build for %s", build_name);
        return;
    }
    const auto &teambuild_it = local_teambuilds.begin();
    Load(teambuild_it->first, teambuild_it->second);
}
void BuildsWindow::LoadPcons(const TeamBuild& tbuild, unsigned int idx) {
    if (idx >= tbuild.builds.size()) return;
    const Build& build = tbuild.builds[idx];
    if (!auto_load_pcons || build.pcons.empty())
        return;
    std::vector<Pcon*> pcons_loaded;
    std::vector<Pcon*> pcons_not_visible;
    PconsWindow* pcw = &PconsWindow::Instance();
    for (auto pcon : pcw->pcons) {
        bool enable = build.pcons.find(pcon->ini) != build.pcons.end();
        if (enable) {
            if (!pcon->IsVisible()) {
                // Don't enable pcons that the user cant see!
                pcons_not_visible.push_back(pcon);
                continue;
            } 
            pcon->SetEnabled(false);
            pcons_loaded.push_back(pcon);
        }
        pcon->SetEnabled(enable);
    }
    if (pcons_loaded.size()) {
        std::string pcons_str;
        size_t i = 0;
        for (auto pcon : pcons_loaded) {
            if (i) pcons_str += ", ";
            i = 1;
            pcons_str += pcon->abbrev;
        }
        const char* str = pcons_str.c_str();
        Log::Info("Pcons loaded: %s", str);
    }
    if (pcons_not_visible.size()) {
        std::string pcons_str;
        size_t i = 0;
        for (auto pcon : pcons_not_visible) {
            if (i) pcons_str += ", ";
            i = 1;
            pcons_str += pcon->abbrev;
        }
        const char* str = pcons_str.c_str();
        Log::Warning("Pcons not loaded: %s.\nOnly pcons visible in the Pcons window will be auto enabled.", str);
    }
}
void BuildsWindow::SendPcons(const TeamBuild& tbuild, unsigned int idx, bool include_build_name) {
    if (idx >= tbuild.builds.size()) return;
    const Build& build = tbuild.builds[idx];
    if (build.pcons.empty())
        return;
    std::string pconsStr("[Pcons] ");
    if (include_build_name) {
        char buf[255] = { 0 };
        const std::string name(build.name);
        if(name.empty())
            snprintf(buf, 255, "[Pcons][%s %d] ", tbuild.name, idx + 1);
        else
            snprintf(buf, 255, "[Pcons][%s] ", name.c_str());
        pconsStr = buf;
    }
    size_t cnt = 0;
    for (auto pcon : PconsWindow::Instance().pcons) {
        if (build.pcons.find(pcon->ini) == build.pcons.end())
            continue;
        if (cnt) pconsStr += ", ";
        cnt = 1;
        pconsStr += pcon->abbrev;
    }
    if (cnt)
        queue.push(pconsStr.c_str());
}
void BuildsWindow::Send(const TeamBuild& tbuild, unsigned int idx) {
    char buf[192] = { 0 };
    if (!BuildSkillTemplateString(tbuild, idx, buf, 192))
        return;
    queue.push(buf);
    if (auto_send_pcons)
        SendPcons(tbuild, idx, false);
}

void BuildsWindow::Update(float delta) {
    UNREFERENCED_PARAMETER(delta);
    if (!queue.empty() && TIMER_DIFF(send_timer) > 600) {
        if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading
            && GW::Agents::GetPlayer()) {

            send_timer = TIMER_INIT();
            GW::Chat::SendChat('#', queue.front().c_str());
            queue.pop();
        }
    }
    if (order_by_changed) {
        LoadFromFile();
        order_by_changed = false;
    }
    // if we open the window, load from file. If we close the window, save to file. 
    static bool old_visible = false;
    bool cur_visible = false;
    cur_visible |= visible;
    for (TeamBuild& tbuild : teambuilds) {
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

void BuildsWindow::LoadSettings(CSimpleIni* ini) {
    ToolboxWindow::LoadSettings(ini);
    show_menubutton = ini->GetBoolValue(Name(), VAR_NAME(show_menubutton), true);
    order_by_name = ini->GetBoolValue(Name(), VAR_NAME(order_by_name), order_by_name);
    auto_load_pcons = ini->GetBoolValue(Name(), VAR_NAME(auto_load_pcons), auto_load_pcons);
    auto_send_pcons = ini->GetBoolValue(Name(), VAR_NAME(auto_send_pcons), auto_send_pcons);
    hide_when_entering_explorable = ini->GetBoolValue(Name(), VAR_NAME(hide_when_entering_explorable), hide_when_entering_explorable);
    one_teambuild_at_a_time = ini->GetBoolValue(Name(), VAR_NAME(one_teambuild_at_a_time), one_teambuild_at_a_time);

    order_by_index = !order_by_name;

    if (MoveOldBuilds(ini)) {
        // loaded
    } else {
        LoadFromFile();
    }
}

void BuildsWindow::SaveSettings(CSimpleIni* ini) {
    ToolboxWindow::SaveSettings(ini);
    ini->SetBoolValue(Name(), VAR_NAME(order_by_name), order_by_name);
    ini->SetBoolValue(Name(), VAR_NAME(auto_load_pcons), auto_load_pcons);
    ini->SetBoolValue(Name(), VAR_NAME(auto_send_pcons), auto_send_pcons);
    ini->SetBoolValue(Name(), VAR_NAME(hide_when_entering_explorable), hide_when_entering_explorable);
    ini->SetBoolValue(Name(), VAR_NAME(one_teambuild_at_a_time), one_teambuild_at_a_time);
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
            char pconskey[16];
            snprintf(namekey, 16, "name%d", i);
            snprintf(templatekey, 16, "template%d", i);
            snprintf(pconskey, 16, "pcons%d", i);
            const char* nameval = inifile->GetValue(section, namekey, "");
            const char* templateval = inifile->GetValue(section, templatekey, "");

            Build b(nameval, templateval);
            // Parse pcons
            std::string pconsval(inifile->GetValue(section, pconskey, ""));
            size_t pos = 0;
            std::string token;
            while ((pos = pconsval.find(",")) != std::string::npos) {
                token = pconsval.substr(0, pos);
                b.pcons.emplace(token.c_str());
                pconsval.erase(0, pos + 1);
            }
            if (pconsval.length())
                b.pcons.emplace(pconsval.c_str());
            tbuild.builds.push_back(b);
        }
    }
    // Sort by name
    if (order_by_name) {
        sort(teambuilds.begin(), teambuilds.end(), [](TeamBuild a, TeamBuild b) {
            return _stricmp(a.name, b.name) < 0;
        });
    }
    builds_changed = false;
}

void BuildsWindow::SaveToFile() {
    if (builds_changed) {
        if (inifile == nullptr) inifile = new CSimpleIni();

        // clear builds from ini
        inifile->Reset();

        // then save
        for (unsigned int i = 0; i < teambuilds.size(); ++i) {
            const TeamBuild& tbuild = teambuilds[i];
            char section[16];
            snprintf(section, 16, "builds%03d", i);
            inifile->SetValue(section, "buildname", tbuild.name);
            inifile->SetBoolValue(section, "showNumbers", tbuild.show_numbers);
            inifile->SetLongValue(section, "count", static_cast<long>(tbuild.builds.size()));
            for (unsigned int j = 0; j < tbuild.builds.size(); ++j) {
                const Build& build = tbuild.builds[j];
                char namekey[16];
                char templatekey[16];
                snprintf(namekey, 16, "name%d", j);
                snprintf(templatekey, 16, "template%d", j);
                inifile->SetValue(section, namekey, build.name);
                inifile->SetValue(section, templatekey, build.code);
                
                if (!build.pcons.empty()) {
                    char pconskey[16];
                    std::string pconsval;
                    snprintf(pconskey, 16, "pcons%d", j);
                    size_t k = 0;
                    for (auto pconstr : build.pcons) {
                        if (k) pconsval += ",";
                        k = 1;
                        pconsval += pconstr;
                    }
                    inifile->SetValue(section, pconskey, pconsval.c_str());
                }
            }
        }

        inifile->SaveFile(Resources::GetPath(INI_FILENAME).c_str());
    }
}
