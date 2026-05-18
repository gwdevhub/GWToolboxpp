#include "stdafx.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/Context/GameContext.h>

#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>

#include <Utils/GuiUtils.h>
#include <Logger.h>

#include <Modules/Resources.h>
#include <Windows/BuildsWindow.h>
#include <Windows/PconsWindow.h>

#include <GWToolbox.h>
#include <Utils/TeamBuild.h>
#include <Utils/TextUtils.h>
#include <Utils/ToolboxUtils.h>
#include <Color.h>

namespace {

    std::vector<TeamBuild> teambuilds{};
    std::vector<Build> preferred_skill_order_builds{};

    bool builds_changed = false;

    bool order_by_name = false;
    bool order_by_index = !order_by_name;
    bool auto_load_pcons = true;
    bool auto_send_pcons = true;
    bool hide_when_entering_explorable = false;
    bool one_teambuild_at_a_time = false;


    ToolboxIni* inifile = nullptr;

    // Preferred skill orders
    bool preferred_skill_orders_visible = false;

    GuiUtils::EncString preferred_skill_order_tooltip;
    char preferred_skill_order_code[128] = { 0 };

    bool order_by_changed = false;

    constexpr auto INI_FILENAME = L"builds.ini";
    GW::HookEntry ChatCmd_HookEntry;
    GW::HookEntry OnUIMessage_HookEntry;

    // Locator for a build within a teambuild
    struct BuildRef {
        TeamBuild* tb = nullptr;
        size_t idx = 0;
        bool valid() const { return tb != nullptr && idx < tb->builds.size(); }
        Build* build() const { return valid() ? &tb->builds[idx] : nullptr; }
    };

    bool PrefillBuildFromAgent(uint32_t agent_id, Build& build)
    {
        const auto current_skill_bar = GW::SkillbarMgr::GetSkillbar(agent_id);
        if (!current_skill_bar || !current_skill_bar->IsValid()) return false;

        GW::SkillbarMgr::SkillTemplate skill_template;
        if (!GW::SkillbarMgr::GetSkillTemplate(agent_id, skill_template)) return false;

        char build_code[128];
        if (GW::SkillbarMgr::EncodeSkillTemplate(skill_template, build_code, sizeof(build_code))) {
            build.code = build_code;
        }

        build.name = std::format("{}/{}", ToolboxUtils::GetProfessionAcronym(skill_template.primary)->string(), ToolboxUtils::GetProfessionAcronym(skill_template.secondary)->string());

        wchar_t encoded_name[8] = {0};

        for (size_t i = 0; i < 8; i++) {
            const auto skill_id = skill_template.skills[i];
            if (skill_id != GW::Constants::SkillID(0)) {
                const auto* skill_data = GW::SkillbarMgr::GetSkillConstantData(skill_id);
                if (skill_data && skill_data->IsElite()) {
                    GW::UI::UInt32ToEncStr(skill_data->name, encoded_name, _countof(encoded_name));
                    GW::UI::AsyncDecodeStr(
                        encoded_name,
                        [](void* param, const wchar_t* s) {
                            if (s && *s)
                                static_cast<Build*>(param)->name += std::format(" - {}", TextUtils::WStringToString(s));
                        },
                        &build
                    );
                    break;
                }
            }
        }
        return true;
    }

    void ClearAllBuilds() {
        teambuilds.clear();
        preferred_skill_order_builds.clear();
    }

    // Pass array of skills for a bar; if a preferred order is found, returns a new array of skills in order, otherwise nullptr.
    const GW::Constants::SkillID* GetPreferredSkillOrder(const GW::Constants::SkillID* skill_ids, Build** build_out = nullptr)
    {
        for (auto& build : preferred_skill_order_builds) {
            bool found = false;
            const auto skills = build.Skills();
            for (size_t i = 0; skills && i < 8; i++) {
                found = false;
                for (size_t j = 0; !found && j < 8; j++) {
                    const auto* skill = GW::SkillbarMgr::GetSkillConstantData(skill_ids[j]);
                    found = skill && (skill->skill_id == skills[i] || skill->skill_id_pvp == skills[i]);
                }
                if (!found) {
                    break;
                }
            }
            if (found) {
                if (build_out)
                    *build_out = &build;
                return skills;
            }
        }
        return nullptr;
    }

    // Triggered when a set of skills is about to be loaded on player or hero
    void OnUIMessage(GW::HookStatus*, GW::UI::UIMessage message_id, void* wparam, void*) {
        switch (message_id) {
        case GW::UI::UIMessage::kSendLoadSkillTemplate: {
            const auto packet = (GW::UI::UIPacket::kSendLoadSkillTemplate*)wparam;
            const auto skills = packet->skill_template ? packet->skill_template->skills : nullptr;
            ASSERT(skills);
            const auto found = GetPreferredSkillOrder(skills);
            if (found && memcmp(skills, found, sizeof(*skills) * 8) != 0) {
                memcpy(skills, found, sizeof(*skills) * 8);
                Log::Flash("Preferred skill order loaded");
            }
        } break;
        }
    }

    bool BuildSkillTemplateString(const TeamBuild& tbuild, size_t idx, std::string* out)
    {
        if (!out || idx >= tbuild.builds.size())
            return false;
        const Build& build = tbuild.builds[idx];
        if (build.name.empty() && build.code.empty())
            return false;

        if (build.name.empty()) {
            *out = std::format("[{} {};{}]", tbuild.name, idx + 1, build.code);
            return true;
        }
        if (build.code.empty()) {
            *out = build.name;
            return true;
        }
        if (tbuild.show_numbers) {
            *out = std::format("[{} - {};{}]", idx + 1, build.name, build.code);
            return true;
        }
        *out = std::format("[{};{}]", build.name, build.code);
        return true;
    }

    void SendPcons(const TeamBuild& tbuild, size_t idx, const bool include_build_name = false)
    {
        if (idx >= tbuild.builds.size()) return;
        const Build& build = tbuild.builds[idx];
        if (build.pcons.empty()) return;

        std::string pconsStr("[Pcons] ");
        if (include_build_name) {
            if (build.name.empty())
                pconsStr = std::format("[Pcons][{} {}] ", tbuild.name, idx + 1);
            else
                pconsStr = std::format("[Pcons][{}] ", build.name);
        }
        size_t cnt = 0;
        for (const auto pcon : PconsWindow::Instance().pcons) {
            if (!build.pcons.contains(pcon->ini))
                continue;
            if (cnt)
                pconsStr += ", ";
            cnt = 1;
            pconsStr += pcon->abbrev;
        }
        if (cnt)
            Build::EnqueueSend(pconsStr);
    }

    BuildRef Find(const char* tbuild_name = nullptr, const char* build_name = nullptr) {
        if (!build_name)
            return {};
        GW::SkillbarMgr::SkillTemplate t;
        const auto prof = static_cast<GW::Constants::Profession>(GW::Agents::GetControlledCharacter()->primary);
        const bool is_skill_template = DecodeSkillTemplate(t, build_name);
        if (is_skill_template && t.primary != prof) {
            Log::Error("Invalid profession for %s (%s)", build_name, ToolboxUtils::GetProfessionAcronym(t.primary)->string().c_str());
            return {};
        }
        const std::string tbuild_ws = tbuild_name ? TextUtils::ToLower(tbuild_name) : "";
        const std::string build_ws = TextUtils::ToLower(build_name);

        BuildRef best_match{};
        size_t best_match_pos = 0;

        for (auto& tb : teambuilds) {
            if (tbuild_name && TextUtils::ToLower(tb.name).find(tbuild_ws.c_str()) == std::string::npos)
                continue;
            if (is_skill_template) {
                for (size_t j = 0; j < tb.builds.size(); j++) {
                    if (tb.builds[j].code == build_name) {
                        best_match = {&tb, j};
                    }
                }
            }
            else {
                for (size_t j = 0; j < tb.builds.size(); j++) {
                    const auto text_match_pos = TextUtils::ToLower(tb.builds[j].name).find(build_ws.c_str());
                    if (text_match_pos == std::string::npos)
                        continue;
                    if (!DecodeSkillTemplate(t, tb.builds[j].code.c_str()))
                        continue;
                    if (t.primary != prof)
                        continue;
                    if (best_match.valid() && text_match_pos < best_match_pos)
                        continue;
                    best_match = {&tb, j};
                    best_match_pos = text_match_pos;
                }
            }
        }
        return best_match;
    }

    bool Send(const TeamBuild& tbuild, size_t idx)
    {
        std::string buf;
        if (!BuildSkillTemplateString(tbuild, idx, &buf))
            return false;
        Build::EnqueueSend(buf);
        if (auto_send_pcons)
            SendPcons(tbuild, idx);
        return true;
    }

    void Send(const TeamBuild& tbuild) {
        if (!tbuild.name.empty())
            Build::EnqueueSend(tbuild.name);
        for (size_t j = 0; j < tbuild.builds.size(); j++)
            Send(tbuild, j);
    }

    bool View(const Build& build)
    {
        GW::GameThread::Enqueue([code = build.code, name = build.name] {
            GW::UI::ChatTemplate t = { 0 };

            auto code_ws = TextUtils::StringToWString(code);
            t.code.m_buffer = code_ws.data();
            t.code.m_size = t.code.m_capacity = code_ws.size() + 1;

            auto name_ws = TextUtils::StringToWString(name);
            t.name = name_ws.data();
            SendUIMessage(GW::UI::UIMessage::kOpenTemplate, &t);
        });
        return true;
    }

    bool Load(const TeamBuild& tbuild, size_t idx) {
        if (idx >= tbuild.builds.size()) return false;
        const Build& build = tbuild.builds[idx];
        if (build.code.empty() && build.hero_id == GW::Constants::HeroID::NoHero) return false;
        build.Load();
        if (!tbuild.edit_open) {
            std::string build_string;
            if (BuildSkillTemplateString(tbuild, idx, &build_string))
                Log::Flash("Build loaded: %s", build_string.c_str());
        }
        return true;
    }

    bool Load(BuildRef ref) {
        if (!ref.valid()) return false;
        return Load(*ref.tb, ref.idx);
    }

    bool Load(const char* tbuild_name, const char* build_name)
    {
        const auto found = Find(tbuild_name, build_name);
        if (!found.valid()) {
            Log::Error("Failed to find build for %s", build_name);
            return false;
        }
        return Load(found);
    }

    bool Load(const char* build_name)
    {
        return Load(nullptr, build_name);
    }

    const char* AddPreferredBuild(const char* code)
    {
        GW::SkillbarMgr::SkillTemplate templ;
        if (!DecodeSkillTemplate(templ, code)) {
            return "Failed to decode skill template from build code";
        }
        Build* found = nullptr;
        if (GetPreferredSkillOrder(templ.skills, &found)) {
            found->code = found->name = code;
            return nullptr;
        }
        preferred_skill_order_builds.push_back(Build(code, code));
        return nullptr;
    }

    bool GetCurrentSkillBar(char* out, const size_t out_len)
    {
        if (!(out && out_len)) return false;
        GW::SkillbarMgr::SkillTemplate skill_template;
        return GetSkillTemplate(GW::Agents::GetControlledCharacterId(), skill_template)
            && EncodeSkillTemplate(skill_template, out, out_len) && DecodeSkillTemplate(skill_template, out);
    }

    void DrawPreferredSkillOrders(IDirect3DDevice9*)
    {
        if (!preferred_skill_orders_visible) {
            return;
        }
        ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(300, 250), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Preferred Skill Orders", &preferred_skill_orders_visible, 0)) {
            ImGui::End();
            return;
        }
        ImGui::Text("When a set of skills is loaded that matches a set from this list, reorder it to match");
        const float skill_height = ImGui::CalcTextSize(" ").y * 2.f;
        ImGui::Indent();
        for (size_t bi = 0; bi < preferred_skill_order_builds.size(); bi++) {
            auto& build = preferred_skill_order_builds[bi];
            const GW::Constants::SkillID* skills = build.Skills();
            for (size_t i = 0; skills && i < 8; i++) {
                if (i) {
                    ImGui::SameLine(0, 0);
                }
                ImGui::ImageCropped(*Resources::GetSkillImage(skills[i]), ImVec2(skill_height, skill_height));
                if (ImGui::IsItemHovered()) {
                    const GW::Skill* s = GW::SkillbarMgr::GetSkillConstantData(skills[i]);
                    if (s) {
                        preferred_skill_order_tooltip.reset(s->name);
                        ImGui::SetTooltip("%s", preferred_skill_order_tooltip.string().c_str());
                    }
                }
            }
            ImGui::SameLine();
            char btn_label[48];
            snprintf(btn_label, sizeof(btn_label), "%s Remove###remove_preferred_skill_order_%zu", reinterpret_cast<const char*>(ICON_FA_TRASH), bi);
            if (ImGui::Button(btn_label, ImVec2(0, skill_height))) {
                preferred_skill_order_builds.erase(preferred_skill_order_builds.begin() + static_cast<ptrdiff_t>(bi));
                Log::Flash("Preferred skill order removed");
                break;
            }
        }
        ImGui::Unindent();
        ImGui::Separator();
        ImGui::InputText("Build code###preferred_skill_order_code", preferred_skill_order_code, sizeof(preferred_skill_order_code));
        ImGui::SameLine();
        const bool add_current = ImGui::Button(ICON_FA_COPY "###use_current_build_code");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Copy current build");
        }
        if (add_current) {
            GetCurrentSkillBar(preferred_skill_order_code, sizeof(preferred_skill_order_code));
        }

        if (ImGui::Button("Add###add_preferred_skill_order")) {
            const char* err = AddPreferredBuild(preferred_skill_order_code);
            if (err) {
                Log::Warning("%s", err);
            }
            else {
                builds_changed = true;
                Log::Flash("Preferred skill order updated");
            }
        }
        ImGui::End();
    }

    const char* BuildName(const unsigned int idx)
    {
        if (idx < teambuilds.size()) {
            return teambuilds[idx].name.c_str();
        }
        return nullptr;
    }

    // Load builds from the Guild Wars Templates directory
    bool LoadFromBuildsFolder();
    // Save toolbox builds to Guild Wars Templates directory
    bool SaveToBuildsFolder();

    void LoadFromFile()
    {
        ClearAllBuilds();

        if (inifile == nullptr) {
            inifile = new ToolboxIni(false, false, false);
        }
        inifile->LoadFile(Resources::GetSettingFile(INI_FILENAME).c_str());

        ToolboxIni::TNamesDepend entries;
        inifile->GetAllKeys("preferred_skill_orders", entries);
        for (const ToolboxIni::Entry& entry : entries) {
            AddPreferredBuild(entry.pItem);
        }

        entries.clear();
        inifile->GetAllSections(entries);
        for (const ToolboxIni::Entry& entry : entries) {
            if (memcmp(entry.pItem, "builds", 6) != 0) {
                continue;
            }
            const char* section = entry.pItem;
            const int count = inifile->GetLongValue(section, "count", 12);
            TeamBuild tbuild(inifile->GetValue(section, "buildname", ""));
            tbuild.has_hero_slots = false;
            tbuild.show_numbers = inifile->GetBoolValue(section, "showNumbers", true);
            char key[16];
            for (int i = 0; i < count; ++i) {
                snprintf(key, _countof(key), "name%d", i);
                const char* nameval = inifile->GetValue(section, key, "");
                snprintf(key, _countof(key), "template%d", i);
                const char* templateval = inifile->GetValue(section, key, "");
                snprintf(key, _countof(key), "pcons%d", i);
                std::string pconsval = inifile->GetValue(section, key, "");

                Build build(nameval, templateval);

                size_t pos = 0;
                std::string token;
                while ((pos = pconsval.find(",")) != std::string::npos) {
                    token = pconsval.substr(0, pos);
                    build.pcons.emplace(token.c_str());
                    pconsval.erase(0, pos + 1);
                }
                if (pconsval.length()) {
                    build.pcons.emplace(pconsval.c_str());
                }
                tbuild.builds.push_back(std::move(build));
            }
            teambuilds.push_back(std::move(tbuild));
        }
        if (order_by_name) {
            std::ranges::sort(teambuilds, [](const TeamBuild& a, const TeamBuild& b) {
                return _stricmp(a.name.c_str(), b.name.c_str()) < 0;
            });
        }
        builds_changed = false;
        LoadFromBuildsFolder();
    }

    std::filesystem::path GetBuildsFolder() {
        const auto gw_builds_folder = GW::MemoryMgr::GetBuildsDir();
        if (gw_builds_folder.empty()) return L"";
        return gw_builds_folder / L"GWToolbox" / L"Builds";
    }

    bool SaveToBuildsFolder()
    {
        const auto tb_builds_folder = GetBuildsFolder();
        if (tb_builds_folder.empty()) return false;

        std::set<std::filesystem::path> expected_paths;
        expected_paths.emplace(tb_builds_folder);

        for (const auto& tbuild : teambuilds) {
            if (tbuild.name.empty()) continue;

            const std::filesystem::path tbuild_folder = tb_builds_folder / TextUtils::SanitiseFilename(tbuild.name);
            expected_paths.emplace(tbuild_folder);

            const auto tbuild_name_path = tbuild_folder / "tbuild.name";
            if (!Resources::WriteFile(tbuild_name_path, tbuild.name)) return false;
            expected_paths.emplace(tbuild_name_path);

            for (size_t j = 0; j < tbuild.builds.size(); j++) {
                const auto& build = tbuild.builds[j];
                if (build.name.empty() && build.code.empty()) continue;

                const std::string file_stem = TextUtils::SanitiseFilename(build.name.empty() ? std::format("Build_{}", j + 1) : build.name);

                if (!build.name.empty()) {
                    const auto p = tbuild_folder / (file_stem + ".name");
                    if (!Resources::WriteFile(p, build.name)) return false;
                    expected_paths.emplace(p);
                }
                if (!build.code.empty()) {
                    const auto p = tbuild_folder / (file_stem + ".txt");
                    if (!Resources::WriteFile(p, build.code)) return false;
                    expected_paths.emplace(p);
                }
                if (!build.pcons.empty()) {
                    const auto p = tbuild_folder / (file_stem + ".pcons");
                    if (!Resources::WriteFile(p, TextUtils::Join({build.pcons.begin(), build.pcons.end()}, "\n"))) return false;
                    expected_paths.emplace(p);
                }
            }
        }

        if (std::filesystem::is_directory(tb_builds_folder)) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(tb_builds_folder)) {
                if (!expected_paths.contains(entry.path())) {
                    std::error_code ec;
                    std::filesystem::remove(entry.path(), ec);
                    if (ec) Log::Warning("Failed to remove stale build file: %s", ec.message().c_str());
                }
            }
        }

        return true;
    }

    bool LoadFromBuildsFolder()
    {
        const auto tb_builds_folder = GetBuildsFolder();
        if (tb_builds_folder.empty() || !std::filesystem::is_directory(tb_builds_folder)) return false;

        for (const auto& tbuild_entry : std::filesystem::directory_iterator(tb_builds_folder)) {
            if (!tbuild_entry.is_directory()) continue;

            std::string tbuild_name;
            const auto tbuild_name_path = tbuild_entry.path() / "tbuild.name";
            if (!Resources::ReadFile(tbuild_name_path, tbuild_name))
                tbuild_name = TextUtils::WStringToString(tbuild_entry.path().filename().wstring());
            tbuild_name = TextUtils::trim(tbuild_name);

            auto it = std::ranges::find_if(teambuilds, [&](const TeamBuild& tb) {
                return _stricmp(tb.name.c_str(), tbuild_name.c_str()) == 0;
            });
            TeamBuild* tbuild_ptr;
            if (it != teambuilds.end()) {
                tbuild_ptr = &(*it);
            }
            else {
                TeamBuild new_tb(tbuild_name);
                new_tb.has_hero_slots = false;
                teambuilds.push_back(std::move(new_tb));
                tbuild_ptr = &teambuilds.back();
            }

            for (const auto& file_entry : std::filesystem::directory_iterator(tbuild_entry.path())) {
                if (!file_entry.is_regular_file()) continue;
                const auto& path = file_entry.path();
                if (path.extension() != L".txt") continue;

                std::string code;
                if (!Resources::ReadFile(path, code)) continue;
                code = TextUtils::trim(code);
                if (code.empty()) continue;

                const auto stem = path.stem().wstring();

                std::string build_name;
                const auto name_path = path.parent_path() / (stem + L".name");
                if (!Resources::ReadFile(name_path, build_name)) build_name = TextUtils::WStringToString(stem);
                build_name = TextUtils::trim(build_name);

                auto bit = std::ranges::find_if(tbuild_ptr->builds, [&](const Build& b) {
                    return _stricmp(b.name.c_str(), build_name.c_str()) == 0;
                });
                if (bit != tbuild_ptr->builds.end()) {
                    bit->code = code;
                    bit->pcons.clear();
                }
                else {
                    tbuild_ptr->builds.push_back(Build(build_name, code));
                    bit = tbuild_ptr->builds.end() - 1;
                }

                const auto pcons_path = path.parent_path() / (stem + L".pcons");
                std::string pcons_content;
                if (Resources::ReadFile(pcons_path, pcons_content)) {
                    for (const auto& pcon : TextUtils::Split(TextUtils::trim(pcons_content), "\n"))
                        if (!pcon.empty()) bit->pcons.emplace(pcon);
                }
            }
        }

        builds_changed = false;
        return true;
    }

    void SaveToFile()
    {
        if (!(builds_changed || GWToolbox::SettingsFolderChanged()))
            return;
        if (inifile == nullptr) {
            inifile = new ToolboxIni();
        }

        inifile->Reset();

        for (const auto& build : preferred_skill_order_builds) {
            inifile->SetValue("preferred_skill_orders", build.code.c_str(), build.code.c_str());
        }

        for (unsigned int i = 0; i < teambuilds.size(); ++i) {
            const auto& tbuild = teambuilds[i];
            char section[16];
            snprintf(section, 16, "builds%03d", i);
            inifile->SetValue(section, "buildname", tbuild.name.c_str());
            inifile->SetBoolValue(section, "showNumbers", tbuild.show_numbers);
            inifile->SetLongValue(section, "count", tbuild.builds.size());
            for (unsigned int j = 0; j < tbuild.builds.size(); ++j) {
                const auto& build = tbuild.builds[j];
                char namekey[16];
                char templatekey[16];
                snprintf(namekey, 16, "name%d", j);
                snprintf(templatekey, 16, "template%d", j);
                inifile->SetValue(section, namekey, build.name.c_str());
                inifile->SetValue(section, templatekey, build.code.c_str());

                if (!build.pcons.empty()) {
                    char pconskey[16];
                    std::string pconsval;
                    snprintf(pconskey, 16, "pcons%d", j);
                    size_t k = 0;
                    for (auto& pconstr : build.pcons) {
                        if (k) pconsval += ",";
                        k = 1;
                        pconsval += pconstr;
                    }
                    inifile->SetValue(section, pconskey, pconsval.c_str());
                }
            }
            ASSERT(inifile->SaveFile(Resources::GetSettingFile(INI_FILENAME).c_str()) == SI_OK);
        }
        SaveToBuildsFolder();
    }

    bool MoveOldBuilds(ToolboxIni* ini)
    {
        if (!teambuilds.empty()) {
            return false;
        }

        bool found_old_build = false;
        ToolboxIni::TNamesDepend oldentries;
        ini->GetAllSections(oldentries);
        for (const ToolboxIni::Entry& oldentry : oldentries) {
            const char* section = oldentry.pItem;
            if (strncmp(section, "builds", 6) != 0)
                continue;
            const int count = ini->GetLongValue(section, "count", 12);
            TeamBuild tbuild(ini->GetValue(section, "buildname", ""));
            tbuild.has_hero_slots = false;
            tbuild.show_numbers = ini->GetBoolValue(section, "showNumbers", true);
            char namekey[16];
            char templatekey[16];
            for (int i = 0; i < count; ++i) {
                snprintf(namekey, _countof(namekey), "name%d", i);
                snprintf(templatekey, _countof(templatekey), "template%d", i);
                const char* nameval = ini->GetValue(section, namekey, "");
                const char* templateval = ini->GetValue(section, templatekey, "");
                tbuild.builds.push_back(Build(nameval, templateval));
            }
            found_old_build = true;
            teambuilds.push_back(std::move(tbuild));
            ini->Delete(section, nullptr);
        }

        if (found_old_build) {
            builds_changed = true;
            SaveToFile();
            return true;
        }
        return false;
    }

    BuildRef FindBuildFromChatCmd(int argc, const LPWSTR* argv) {
        if (argc > 2) {
            const std::string build_name = TextUtils::WStringToString(argv[2]);
            const std::string team_build_name = TextUtils::WStringToString(argv[1]);
            return Find(team_build_name.c_str(), build_name.c_str());
        }
        if (argc > 1) {
            const std::string build_name = TextUtils::WStringToString(argv[1]);
            return Find(nullptr, build_name.c_str());
        }
        return {};
    }

    void CHAT_CMD_FUNC(CmdLoad)
    {
        if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost) {
            return;
        }
        if (argc < 2) {
            Log::WarningW(L"Syntax: /%s [teambuild_name] [build_name|build_code]\n/%s [build_name|build_code]", *argv, *argv);
            return;
        }
        Load(FindBuildFromChatCmd(argc, argv));
    }
    void CHAT_CMD_FUNC(CmdPing)
    {
        if (argc < 2) {
            Log::WarningW(L"Syntax: /%s [teambuild_name] [build_name|build_code]\n/%s [build_name|build_code]", *argv, *argv);
            return;
        }
        const auto ref = FindBuildFromChatCmd(argc, argv);
        if (ref.valid()) Send(*ref.tb, ref.idx);
    }

}


void BuildsWindow::Terminate()
{
    ToolboxWindow::Terminate();
    ClearAllBuilds();
    if (inifile) {
        delete inifile;
        inifile = nullptr;
    }
    GW::Chat::DeleteCommand(&ChatCmd_HookEntry);
    GW::UI::RemoveUIMessageCallback(&OnUIMessage_HookEntry);
}

void BuildsWindow::DrawSettingsInternal()
{
    ImGui::Checkbox("Hide Build windows when entering explorable area", &hide_when_entering_explorable);
    ImGui::CheckboxWithHelp("Only show one teambuild window at a time", &one_teambuild_at_a_time, "Close other teambuild windows when you open a new one");
    ImGui::CheckboxWithHelp("Auto load pcons", &auto_load_pcons, "Automatically load pcons for a build when loaded onto a character");
    ImGui::CheckboxWithHelp("Send pcons when pinging a build", &auto_send_pcons, "Automatically send a second message after the build template in team chat,\nshowing the pcons that the build uses.");
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
    if (ImGui::Button(ICON_FA_USER_COG " View Preferred Skill Orders")) {
        preferred_skill_orders_visible = !preferred_skill_orders_visible;
    }
}

void SetTooltipFromTeambuild(const TeamBuild* tbuild) {
    ImGui::SetTooltip([tbuild]() {
        for (const auto& build : tbuild->builds) {
            if (build.name.empty() && build.code.empty()) continue;
            ImGui::Spacing();
            ImGui::TextUnformatted(build.name.c_str());
            GuiUtils::DrawSkillbar(build.code.c_str(), false);
            ImGui::Spacing();
        }
    });
}

void BuildsWindow::Draw(IDirect3DDevice9* pDevice)
{
    DrawPreferredSkillOrders(pDevice);
    static auto last_instance_type = GW::Constants::InstanceType::Loading;
    const GW::Constants::InstanceType instance_type = GW::Map::GetInstanceType();

    if (instance_type != last_instance_type) {
        if (hide_when_entering_explorable && instance_type == GW::Constants::InstanceType::Explorable) {
            for (auto& hb : teambuilds) {
                hb.edit_open = false;
            }
            visible = false;
        }
        last_instance_type = instance_type;
    }

    if (visible) {
        ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(300, 250), ImGuiCond_FirstUseEver);
        if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
            for (auto& tbuild : teambuilds) {
                ImGui::PushID(tbuild.ui_id.c_str());
                ImGui::GetStyle().ButtonTextAlign = ImVec2(0.0f, 0.5f);
                if (ImGui::Button(tbuild.name.c_str(), ImVec2(ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemInnerSpacing.x - 60.0f * ImGui::FontScale(), 0))) {
                    if (one_teambuild_at_a_time && !tbuild.edit_open) {
                        for (auto& tb : teambuilds) {
                            tb.edit_open = false;
                        }
                    }
                    tbuild.edit_open = !tbuild.edit_open;
                }
                if (ImGui::IsItemHovered()) {
                    SetTooltipFromTeambuild(&tbuild);
                }
                ImGui::GetStyle().ButtonTextAlign = ImVec2(0.5f, 0.5f);
                ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
                if (ImGui::Button("Send", ImVec2(60.0f * ImGui::FontScale(), 0))) {
                    Send(tbuild);
                }
                ImGui::PopID();
            }
            if (ImGui::Button("Add Teambuild", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                TeamBuild new_tbuild(std::format("My New Teambuild, {}", TextUtils::GetFormattedDateTime()));
                new_tbuild.has_hero_slots = false;
                new_tbuild.edit_open = true;
                const auto party_agent_ids = GW::PartyMgr::GetPartyAgentIds();
                new_tbuild.builds.reserve(party_agent_ids.size());
                for (const auto agent_id : party_agent_ids) {
                    new_tbuild.builds.emplace_back();
                    if (!PrefillBuildFromAgent(agent_id, new_tbuild.builds.back())) {
                        new_tbuild.builds.pop_back();
                        break;
                    }
                }
                teambuilds.push_back(std::move(new_tbuild));
                builds_changed = true;
            }
        }
        ImGui::End();
    }

    // Draw edit windows using the unified DrawEditWindow
    for (size_t i = 0; i < teambuilds.size(); i++) {
        if (!teambuilds[i].edit_open) continue;
        if (!teambuilds[i].DrawEditWindow(i, teambuilds, builds_changed)) {
            break; // teambuild deleted; vector modified
        }
    }
}

void BuildsWindow::Update(const float)
{
    if (order_by_changed) {
        LoadFromFile();
        order_by_changed = false;
    }
    static bool old_visible = false;
    bool cur_visible = false;
    cur_visible |= visible;
    for (const auto& tbuild : teambuilds) {
        cur_visible |= tbuild.edit_open;
    }

    if (cur_visible != old_visible) {
        old_visible = cur_visible;
        if (cur_visible) {
            LoadFromFile();
        }
        else {
            SaveToFile();
        }
    }
}

void BuildsWindow::LoadSettings(ToolboxIni* ini)
{
    ToolboxWindow::LoadSettings(ini);
    LOAD_BOOL(order_by_name);
    LOAD_BOOL(auto_load_pcons);
    LOAD_BOOL(auto_send_pcons);
    LOAD_BOOL(hide_when_entering_explorable);
    LOAD_BOOL(one_teambuild_at_a_time);

    order_by_index = !order_by_name;

    if (MoveOldBuilds(ini)) {
        // loaded
    }
    else {
        LoadFromFile();
    }
}

void BuildsWindow::SaveSettings(ToolboxIni* ini)
{
    ToolboxWindow::SaveSettings(ini);
    SAVE_BOOL(order_by_name);
    SAVE_BOOL(auto_load_pcons);
    SAVE_BOOL(auto_send_pcons);
    SAVE_BOOL(hide_when_entering_explorable);
    SAVE_BOOL(one_teambuild_at_a_time);
    SaveToFile();
}

void BuildsWindow::Initialize()
{
    ToolboxWindow::Initialize();

    GW::Chat::CreateCommand(&ChatCmd_HookEntry, L"loadbuild", CmdLoad);
    GW::Chat::CreateCommand(&ChatCmd_HookEntry, L"pingbuild", CmdPing);
    RegisterUIMessageCallback(&OnUIMessage_HookEntry, GW::UI::UIMessage::kSendLoadSkillTemplate, OnUIMessage);
}

void BuildsWindow::DrawHelp()
{
    if (!ImGui::TreeNodeEx("Build Chat Commands", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        return;
    }
    ImGui::Bullet();
    ImGui::Text("'/load [build template|build name] [Hero index]' loads a build via Guild Wars builds. The build name must be between quotes if it contains spaces. First Hero index is 1, last is 7. Leave out for player");
    ImGui::Bullet();
    ImGui::Text("'/loadbuild [teambuild] <build name|build code>' loads a build via GWToolbox Builds window. Does a partial search on team build name/build name/build code. Matches current player's profession.");
    ImGui::TreePop();
}
