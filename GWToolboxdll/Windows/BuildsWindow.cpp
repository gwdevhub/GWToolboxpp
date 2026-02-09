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
#include <Utils/TextUtils.h>
#include <Utils/ToolboxUtils.h>
#include <Color.h>

namespace {

    struct TeamBuild;
    struct Build;
    std::vector<TeamBuild*> teambuilds{};
    std::vector<Build*> preferred_skill_order_builds{};

    Color build_edit_pcon_enabled_color = Colors::ARGB(102, 0, 255, 0);

    struct Build {

        Build(const std::string_view _name, const std::string_view _code) :
            name(_name),
            code(_code) {
            name.reserve(128);
            code.reserve(128);
            memset(&skill_template, 0, sizeof(skill_template));
        };
        Build(TeamBuild& _tbuild, const std::string_view _name, const std::string_view _code) : Build(_name, _code) {
            tbuild = &_tbuild;
        };
        ~Build();
        TeamBuild* tbuild = nullptr;
        std::string name;
        std::string code;
        GW::SkillbarMgr::SkillTemplate skill_template;
        std::set<std::string> pcons{};

        const GW::Constants::SkillID* skills() {
            if (!decode()) {
                return nullptr;
            }
            return skill_template.skills;
        };
        const GW::SkillbarMgr::SkillTemplate* decode() {
            if (!decoded() && !DecodeSkillTemplate(skill_template, code.c_str())) {
                skill_template.primary = skill_template.secondary = GW::Constants::Profession::None;
            }
            return decoded() ? &skill_template : nullptr;
        };
        bool decoded() const { return !(skill_template.primary == GW::Constants::Profession::None && skill_template.secondary == GW::Constants::Profession::None); }
        // Vector of pcons to use for this build, listed by ini name e.g. "cupcake"
        const size_t index() const;


    };

    uint32_t cur_ui_id = 0;

    struct TeamBuild {
        TeamBuild(const std::string_view _name) : name(_name)
            , ui_id(++cur_ui_id) {
            name.reserve(128);
        };
        ~TeamBuild() {
            while (!builds.empty()) {
                delete builds[0];
            }
            std::erase(teambuilds, this);
        }

        bool edit_open = false;
        int edit_pcons = -1;
        bool show_numbers = false;
        std::string name;
        std::vector<Build*> builds{};
        unsigned int ui_id; // should be const but then assignment operator doesn't get created automatically, and I'm too lazy to redefine it, so just don't change this value, okay?
    };

    const size_t Build::index() const {
        ASSERT(tbuild);
        const auto& builds = tbuild->builds;
        const auto found = std::find(builds.begin(), builds.end(), this);
        ASSERT(found != builds.end());
        return found - builds.begin();
    }
    Build::~Build() {
        for (auto teambuild : teambuilds)
            std::erase(teambuild->builds, this);
        std::erase(preferred_skill_order_builds, this);
    }

    bool builds_changed = false;

    bool order_by_name = false;
    bool order_by_index = !order_by_name;
    bool auto_load_pcons = true;
    bool auto_send_pcons = true;
    bool delete_builds_without_prompt = false;
    bool hide_when_entering_explorable = false;
    bool one_teambuild_at_a_time = false;


    clock_t send_timer = 0;
    std::queue<std::string> queue{};

    ToolboxIni* inifile = nullptr;

    // Preferred skill orders
    bool preferred_skill_orders_visible = false;

    GuiUtils::EncString preferred_skill_order_tooltip;
    char preferred_skill_order_code[128] = { 0 };

    bool order_by_changed = false;

    constexpr auto INI_FILENAME = L"builds.ini";
    GW::HookEntry ChatCmd_HookEntry;
    GW::HookEntry OnUIMessage_HookEntry;

    bool PrefillBuildFromAgent(uint32_t agent_id, Build* build)
    {
        if (!build) return false;

        const auto current_skill_bar = GW::SkillbarMgr::GetSkillbar(agent_id);
        if (!current_skill_bar || !current_skill_bar->IsValid()) return false;

        // Get current skill template
        GW::SkillbarMgr::SkillTemplate skill_template;
        if (!GW::SkillbarMgr::GetSkillTemplate(agent_id, skill_template)) return false;

        // Encode the skill template to get the build code
        char build_code[128];
        if (GW::SkillbarMgr::EncodeSkillTemplate(skill_template, build_code, sizeof(build_code))) {
            build->code = build_code;
        }

        build->name = std::format("{}/{}", GetProfessionAcronym(skill_template.primary), GetProfessionAcronym(skill_template.secondary));


        // Try to generate a name based on the elite skill or primary profession
        std::string generated_name;

        wchar_t encoded_name[8] = {0};

        // Find the elite skill (position 0-7, elites are typically in slots 0-2 for player builds)
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
                                ((Build*)param)->name += std::format(" - {}", TextUtils::WStringToString(s));
                        },
                        build
                    );
                    break;
                }
            }
        }
        return true;
    }

    void ClearAllBuilds() {
        while (teambuilds.size())
            delete teambuilds[0];
        while (preferred_skill_order_builds.size())
            delete preferred_skill_order_builds[0];
    }

    // Pass array of skills for a bar; if a preferred order is found, returns a new array of skills in order, otherwise nullptr.
    const GW::Constants::SkillID* GetPreferredSkillOrder(const GW::Constants::SkillID* skill_ids, Build** build_out = nullptr)
    {
        for (auto build : preferred_skill_order_builds) {
            bool found = false;
            const auto skills = build->skills();
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
                    *build_out = build;
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
                // Copy across the new order before the send is done
                memcpy(skills, found, sizeof(*skills) * 8);
                Log::Flash("Preferred skill order loaded");
            }
        } break;
        }
    }

    bool BuildSkillTemplateString(const Build* build, std::string* out)
    {
        if (!(build && out))
            return false;
        if (build->name.empty() && build->code.empty())
            return false; // nothing to do here

        const auto idx = build->index();

        if (build->name.empty()) {
            // name is empty, fill it with the teambuild name
            *out = std::format("[{} {};{}]", build->tbuild->name, idx + 1, build->code);
            return true;
        }
        if (build->code.empty()) {
            // code is empty, just print the name without template format
            *out = build->name;
            return true;
        }
        if (build->tbuild->show_numbers) {
            // add numbers in front of name
            *out = std::format("[{} - {};{}]", idx + 1, build->name, build->code);
            return true;
        }
        *out = std::format("[{};{}]", build->name, build->code);
        return true;
    }

    void SendPcons(const Build* build, const bool include_build_name = false)
    {
        if (!(build && build->pcons.size()))
            return;
        const auto idx = build->index();
        std::string pconsStr("[Pcons] ");
        if (include_build_name) {
            if (build->name.empty())
                pconsStr = std::format("[Pcons][{} {}] ", build->tbuild->name, idx + 1);
            else
                pconsStr = std::format("[Pcons][{}] ", build->name);
        }
        size_t cnt = 0;
        for (const auto pcon : PconsWindow::Instance().pcons) {
            if (!build->pcons.contains(pcon->ini))
                continue;
            if (cnt)
                pconsStr += ", ";
            cnt = 1;
            pconsStr += pcon->abbrev;
        }
        if (cnt) {
            queue.push(pconsStr);
        }
    }

    const Build* Find(const char* tbuild_name = nullptr, const char* build_name = nullptr) {
        if (!build_name)
            return nullptr;
        GW::SkillbarMgr::SkillTemplate t;
        const auto prof = static_cast<GW::Constants::Profession>(GW::Agents::GetControlledCharacter()->primary);
        const bool is_skill_template = DecodeSkillTemplate(t, build_name);
        if (is_skill_template && t.primary != prof) {
            Log::Error("Invalid profession for %s (%s)", build_name, GetProfessionAcronym(t.primary));
            return nullptr;
        }
        const std::string tbuild_ws = tbuild_name ? TextUtils::ToLower(tbuild_name) : "";
        const std::string build_ws = TextUtils::ToLower(build_name);

        const Build* best_match = nullptr;
        size_t best_match_pos = 0;

        std::vector<std::pair<TeamBuild, size_t>> local_teambuilds;
        for (const auto& tb : teambuilds) {
            if (tbuild_name && TextUtils::ToLower(tb->name).find(tbuild_ws.c_str()) == std::string::npos)
                continue; // Teambuild name doesn't match
            if (is_skill_template) {
                for (const auto build : tb->builds) {
                    if (build->code == build_name) {
                        best_match = build;
                    }

                }
            }
            else {
                for (const auto build : tb->builds) {
                    const auto text_match_pos = TextUtils::ToLower(build->name).find(build_ws.c_str());
                    if (text_match_pos == std::string::npos)
                        continue;
                    if (!DecodeSkillTemplate(t, build->code.c_str()))
                        continue; // Invalid build code
                    if (t.primary != prof)
                        continue; // Wrong profession.
                    if (best_match && text_match_pos < best_match_pos)
                        continue; // Current match is more accurate name
                    best_match = build;
                    best_match_pos = text_match_pos;
                }
            }
        }
        return best_match;
    }

    bool Send(const Build* build)
    {
        std::string buf;
        if (!(build && BuildSkillTemplateString(build, &buf)))
            return false;
        queue.push(buf);
        if (auto_send_pcons)
            SendPcons(build);
        return true;
    }

    void Send(const TeamBuild* tbuild) {
        if (!tbuild->name.empty()) {
            queue.push(tbuild->name);
        }
        for (const auto build : tbuild->builds) {
            Send(build);
        }
    }

    bool View(const Build* build)
    {
        if (!build)
            return false;
        GW::GameThread::Enqueue([build] {
            GW::UI::ChatTemplate t = { 0 };
            t.code.m_buffer = new wchar_t[128];
            MultiByteToWideChar(CP_UTF8, 0, build->code.c_str(), -1, t.code.m_buffer, 128);
            t.code.m_size = t.code.m_capacity = wcslen(t.code.m_buffer);
            t.name = new wchar_t[128];
            MultiByteToWideChar(CP_UTF8, 0, build->name.c_str(), -1, t.name, 128);

            SendUIMessage(GW::UI::UIMessage::kOpenTemplate, &t);
            delete[] t.code.m_buffer;
            delete[] t.name;
            });
        return true;
    }

    void Send(const unsigned int idx)
    {
        if (idx < teambuilds.size()) {
            Send(teambuilds[idx]);
        }
    }

    bool LoadPcons(const Build* build) {
        if (!build)
            return false;
        std::vector<Pcon*> pcons_loaded;
        std::vector<Pcon*> pcons_not_visible;
        const PconsWindow* pcw = &PconsWindow::Instance();
        for (auto pcon : pcw->pcons) {
            const bool enable = build->pcons.contains(pcon->ini);
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
            for (const auto pcon : pcons_loaded) {
                if (i) {
                    pcons_str += ", ";
                }
                i = 1;
                pcons_str += pcon->abbrev;
            }
            const char* str = pcons_str.c_str();
            Log::Flash("Pcons loaded: %s", str);
        }
        if (pcons_not_visible.size()) {
            std::string pcons_str;
            size_t i = 0;
            for (const auto pcon : pcons_not_visible) {
                if (i) {
                    pcons_str += ", ";
                }
                i = 1;
                pcons_str += pcon->abbrev;
            }
            const char* str = pcons_str.c_str();
            Log::Warning("Pcons not loaded: %s.\nOnly pcons visible in the Pcons window will be auto enabled.", str);
        }
        return true;
    }

    bool Load(const Build* build) {
        if (!(build && GW::SkillbarMgr::LoadSkillTemplate(build->code.c_str())))
            return false;
        if (auto_load_pcons)
            LoadPcons(build);
        if (!build->tbuild->edit_open) {
            std::string build_string;
            if (BuildSkillTemplateString(build, &build_string))
                Log::Flash("Build loaded: %s", build_string.c_str());
        }
        return true;
    }

    bool Load(const char* tbuild_name, const char* build_name)
    {
        const auto found = Find(tbuild_name, build_name);
        // These are now in order of build match.
        if (!found) {
            Log::Error("Failed to find build for %s", build_name);
            return false;
        }
        return Load(found);
    }

    bool Load(const char* build_name)
    {
        return Load(nullptr, build_name);
    }

    void OnDeleteBuildConfirmed(bool result, void* wparam) {
        if (!result)
            return;
        auto build = (Build*)wparam;
        delete build;
        builds_changed = true;
    }
    Build* editing_build = 0;
    void DrawBuildSection(Build* build)
    {
        const auto idx = (int)build->index();
        const float font_scale = ImGui::GetIO().FontGlobalScale;
        const float btn_width = 50.0f * font_scale;
        const float del_width = 24.0f * font_scale;
        const float spacing = ImGui::GetStyle().ItemSpacing.y;
        const float btn_offset = ImGui::GetContentRegionAvail().x - del_width - btn_width * 3 - spacing * 3;
        const auto editing = editing_build == build;

        ImGui::Text("#%d", idx + 1);
        ImGui::PushItemWidth(btn_offset - btn_width - spacing * 2);
        ImGui::SameLine(btn_width, 0);
        if (ImGui::InputText("###name", build->name)) {
            builds_changed = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip([build]() {
                ImGui::TextUnformatted(build->name.c_str());
                GuiUtils::DrawSkillbar(build->code.c_str());
            });
        }
        ImGui::PopItemWidth();
 
        ImGui::SameLine(btn_offset);
        if (ImGui::Button(ImGui::GetIO().KeyCtrl ? "Send" : "View", ImVec2(btn_width, 0))) {
            if (ImGui::GetIO().KeyCtrl) {
                Send(build);
            }
            else {
                View(build);
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(ImGui::GetIO().KeyCtrl ? "Click to send to team chat" : "Click to view build. Ctrl + Click to send to chat.");
        }
        ImGui::SameLine(0, spacing);
        if (ImGui::Button("Load", ImVec2(btn_width, 0))) {
            Load(build);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(!build->pcons.empty() ? "Click to load build template and pcons" : "Click to load build template");
        }
        ImGui::SameLine(0, spacing);
        if (editing) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));
        }
        if (ImGui::Button("Edit", ImVec2(btn_width, 0))) {
            if (editing_build == build) {
                editing_build = 0;
            }
            else {
                editing_build = build;
            }
        }
        if (editing) {
            ImGui::PopStyleColor();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Click to edit this build");
        }
        ImGui::SameLine(0, spacing);
        bool delete_confirmed = false;
        if (ImGui::ConfirmButton(ICON_FA_TRASH, &delete_confirmed, "Delete Build\n\nAre you sure you want to delete this build?\nThis operation cannot be undone.")) {
            OnDeleteBuildConfirmed(true, build);
            return;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Delete build");
        }
        if (editing) {
            // Edit mode
            const float indent = btn_width;
            ImGui::Indent(indent);
            ImGui::TextUnformatted("Build Code:");
            ImGui::SameLine();
            if (ImGui::InputText("###code", build->code)) {
                builds_changed = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip([build]() {
                    GuiUtils::DrawSkillbar(build->code.c_str());
                });
            }
            // Pcons
            ImGui::TextUnformatted("Pcons:");
            ImGui::ShowHelp("Enable or disable pcons that will be activated in the pcons window when this build is loaded");
            const auto& pcons = PconsWindow::Instance().pcons;

            
            const float skill_height = ImGui::CalcTextSize(" ").y * 2.f;
            const auto skill_size = ImVec2(skill_height, skill_height);
            ImGui::StartSpacedElements(skill_height + ImGui::GetStyle().ItemSpacing.x);
            for (const auto pcon : pcons) {
                ImGui::PushID(pcon);
                const auto active = build->pcons.contains(pcon->ini);
                ImGui::NextSpacedElement();
                if (active) {
                    ImGui::PushStyleColor(ImGuiCol_Button, build_edit_pcon_enabled_color);
                }
                if (ImGui::IconButton("", *pcon->GetTexture(), {skill_height, skill_height})) {
                    if (!active) {
                        build->pcons.emplace(pcon->ini);
                    }
                    else {
                        build->pcons.erase(pcon->ini);
                    }
                    builds_changed = true;
                }
                if (active) {
                    ImGui::PopStyleColor();
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip(pcon->chat.c_str());
                }
                ImGui::PopID();
            }
            
            ImGui::Unindent(indent);

        }
       
    }

    bool GetCurrentSkillBar(char* out, const size_t out_len)
    {
        if (!(out && out_len)) {
            return false;
        }
        GW::SkillbarMgr::SkillTemplate skill_template;
        return GetSkillTemplate(GW::Agents::GetControlledCharacterId(), skill_template)
            && EncodeSkillTemplate(skill_template, out, out_len) && DecodeSkillTemplate(skill_template, out);
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
        preferred_skill_order_builds.push_back(new Build(code, code));
        return nullptr;
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
        const auto skill_size = ImVec2(skill_height, skill_height);
        ImGui::Indent();
        for (auto build : preferred_skill_order_builds) {
            const GW::Constants::SkillID* skills = build->skills();
            for (size_t i = 0; skills && i < 8; i++) {
                if (i) {
                    ImGui::SameLine(0, 0);
                }
                ImGui::ImageCropped(*Resources::GetSkillImage(skills[i]), skill_size);
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
            snprintf(btn_label, sizeof(btn_label), "%s Remove###remove_preferred_skill_order_%p", reinterpret_cast<const char*>(ICON_FA_TRASH), build);
            if (ImGui::Button(btn_label, ImVec2(0, skill_height))) {
                delete build;
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
            return teambuilds[idx]->name.c_str();
        }
        return nullptr;
    }

    void LoadFromFile()
    {
        // clear builds from toolbox
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
            auto tbuild = new TeamBuild(inifile->GetValue(section, "buildname", ""));
            tbuild->show_numbers = inifile->GetBoolValue(section, "showNumbers", true);
            char key[16];
            for (int i = 0; i < count; ++i) {

                snprintf(key, _countof(key), "name%d", i);
                const char* nameval = inifile->GetValue(section, key, "");
                snprintf(key, _countof(key), "template%d", i);
                const char* templateval = inifile->GetValue(section, key, "");
                snprintf(key, _countof(key), "pcons%d", i);
                std::string pconsval = inifile->GetValue(section, key, "");

                auto build = new Build(*tbuild, nameval, templateval);

                // Parse pcons
                size_t pos = 0;
                std::string token;
                while ((pos = pconsval.find(",")) != std::string::npos) {
                    token = pconsval.substr(0, pos);
                    build->pcons.emplace(token.c_str());
                    pconsval.erase(0, pos + 1);
                }
                if (pconsval.length()) {
                    build->pcons.emplace(pconsval.c_str());
                }
                tbuild->builds.push_back(build);
            }
            teambuilds.push_back(tbuild);
        }
        // Sort by name
        if (order_by_name) {
            sort(teambuilds.begin(), teambuilds.end(), [](const TeamBuild* a, const TeamBuild* b) {
                return _stricmp(a->name.c_str(), b->name.c_str()) < 0;
            });
        }
        builds_changed = false;
    }

    void SaveToFile()
    {
        if (!(builds_changed || GWToolbox::SettingsFolderChanged()))
            return; // No change
        if (inifile == nullptr) {
            inifile = new ToolboxIni();
        }

        // clear builds from ini
        inifile->Reset();

        for (const auto build : preferred_skill_order_builds) {
            inifile->SetValue("preferred_skill_orders", build->code.c_str(), build->code.c_str());
        }

        // then save
        for (unsigned int i = 0; i < teambuilds.size(); ++i) {
            const auto tbuild = teambuilds[i];
            char section[16];
            snprintf(section, 16, "builds%03d", i);
            inifile->SetValue(section, "buildname", tbuild->name.c_str());
            inifile->SetBoolValue(section, "showNumbers", tbuild->show_numbers);
            inifile->SetLongValue(section, "count", tbuild->builds.size());
            for (unsigned int j = 0; j < tbuild->builds.size(); ++j) {
                const auto build = tbuild->builds[j];
                char namekey[16];
                char templatekey[16];
                snprintf(namekey, 16, "name%d", j);
                snprintf(templatekey, 16, "template%d", j);
                inifile->SetValue(section, namekey, build->name.c_str());
                inifile->SetValue(section, templatekey, build->code.c_str());

                if (!build->pcons.empty()) {
                    char pconskey[16];
                    std::string pconsval;
                    snprintf(pconskey, 16, "pcons%d", j);
                    size_t k = 0;
                    for (auto& pconstr : build->pcons) {
                        if (k) {
                            pconsval += ",";
                        }
                        k = 1;
                        pconsval += pconstr;
                    }
                    inifile->SetValue(section, pconskey, pconsval.c_str());
                }
            }
            ASSERT(inifile->SaveFile(Resources::GetSettingFile(INI_FILENAME).c_str()) == SI_OK);
        }
    }

    bool MoveOldBuilds(ToolboxIni* ini)
    {
        if (!teambuilds.empty()) {
            return false; // builds are already loaded, skip
        }

        bool found_old_build = false;
        ToolboxIni::TNamesDepend oldentries;
        ini->GetAllSections(oldentries);
        for (const ToolboxIni::Entry& oldentry : oldentries) {
            const char* section = oldentry.pItem;
            if (strncmp(section, "builds", 6) != 0)
                continue;
            const int count = ini->GetLongValue(section, "count", 12);
            auto tbuild = new TeamBuild(ini->GetValue(section, "buildname", ""));
            tbuild->show_numbers = ini->GetBoolValue(section, "showNumbers", true);
            char namekey[16];
            char templatekey[16];
            for (int i = 0; i < count; ++i) {
                snprintf(namekey, _countof(namekey), "name%d", i);
                snprintf(templatekey, _countof(templatekey), "template%d", i);
                const char* nameval = ini->GetValue(section, namekey, "");
                const char* templateval = ini->GetValue(section, templatekey, "");
                tbuild->builds.push_back(new Build(*tbuild, nameval, templateval));
            }
            found_old_build = true;
            teambuilds.push_back(tbuild);
            ini->Delete(section, nullptr);
        }

        if (found_old_build) {
            builds_changed = true;
            SaveToFile();
            return true;
        }
        return false;
    }

    const Build* FindBuildFromChatCmd(int argc, const LPWSTR* argv) {
        if (argc > 2) {
            const std::string build_name = TextUtils::WStringToString(argv[2]);
            const std::string team_build_name = TextUtils::WStringToString(argv[1]);
            return Find(team_build_name.c_str(), build_name.c_str());
        }
        if (argc > 1) {
            const std::string build_name = TextUtils::WStringToString(argv[1]);
            return Find(nullptr, build_name.c_str());
        }
        return nullptr;
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
        Send(FindBuildFromChatCmd(argc, argv));
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
    ImGui::Checkbox("Only show one teambuild window at a time", &one_teambuild_at_a_time);
    ImGui::ShowHelp("Close other teambuild windows when you open a new one");
    ImGui::Checkbox("Auto load pcons", &auto_load_pcons);
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
    if (ImGui::Button(ICON_FA_USER_COG " View Preferred Skill Orders")) {
        preferred_skill_orders_visible = !preferred_skill_orders_visible;
    }
}

void SetTooltipFromTeambuild(const TeamBuild* tbuild) {
    ImGui::SetTooltip([tbuild]() {
        for (auto build : tbuild->builds) {
            if (build->name.empty() && build->code.empty()) continue;
            ImGui::Spacing();
            ImGui::TextUnformatted(build->name.c_str());
            GuiUtils::DrawSkillbar(build->code.c_str(),false);
            ImGui::Spacing();
        }
    });
}

void BuildsWindow::Draw(IDirect3DDevice9* pDevice)
{
    DrawPreferredSkillOrders(pDevice);
    // @Cleanup: Use the BuildsWindow instance, not a static variable
    static auto last_instance_type = GW::Constants::InstanceType::Loading;
    const GW::Constants::InstanceType instance_type = GW::Map::GetInstanceType();

    if (instance_type != last_instance_type) {
        // Run tasks on map change without an StoC hook
        if (hide_when_entering_explorable && instance_type == GW::Constants::InstanceType::Explorable) {
            for (auto hb : teambuilds) {
                hb->edit_open = false;
            }
            visible = false;
        }
        last_instance_type = instance_type;
    }

    if (visible) {
        ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(300, 250), ImGuiCond_FirstUseEver);
        if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
            for (const auto tbuild : teambuilds) {
                ImGui::PushID(static_cast<int>(tbuild->ui_id));
                ImGui::GetStyle().ButtonTextAlign = ImVec2(0.0f, 0.5f);
                if (ImGui::Button(tbuild->name.c_str(), ImVec2(ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemInnerSpacing.x - 60.0f * ImGui::GetIO().FontGlobalScale, 0))) {
                    if (one_teambuild_at_a_time && !tbuild->edit_open) {
                        for (auto& tb : teambuilds) {
                            tb->edit_open = false;
                        }
                    }
                    tbuild->edit_open = !tbuild->edit_open;
                }
                if (ImGui::IsItemHovered()) {
                    SetTooltipFromTeambuild(tbuild);
                }
                ImGui::GetStyle().ButtonTextAlign = ImVec2(0.5f, 0.5f);
                ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
                if (ImGui::Button("Send", ImVec2(60.0f * ImGui::GetIO().FontGlobalScale, 0))) {
                    Send(tbuild);
                }
                ImGui::PopID();
            }
            if (ImGui::Button("Add Teambuild", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                auto tbuild = new TeamBuild(std::format("My New Teambuild, {}",TextUtils::GetFormattedDateTime()));
                tbuild->edit_open = true;
                const auto party_agent_ids = GW::PartyMgr::GetPartyAgentIds();
                for (const auto agent_id : party_agent_ids) {
                    const auto b = new Build(*tbuild, "", "");
                    if (!PrefillBuildFromAgent(agent_id, b)) break;
                    tbuild->builds.push_back(b);
                }
                teambuilds.push_back(tbuild);
                builds_changed = true;
            }
        }
        ImGui::End();
    }

    for (size_t i = 0, size = teambuilds.size(); i < size;i++) {
        const auto tbuild = teambuilds[i];
        if (!tbuild->edit_open) {
            continue;
        }
        ImGui::PushID(tbuild->ui_id);
        char winname[192];
        ASSERT(snprintf(winname,_countof(winname), "%s###teambuild%d", tbuild->name.c_str(), tbuild->ui_id) > 0);
        ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(500, 0), ImGuiCond_FirstUseEver);
        if (ImGui::Begin(winname, &tbuild->edit_open)) {
            ImGui::PushItemWidth(-120.0f);
            if (ImGui::InputText("Build Name", tbuild->name)) {
                builds_changed = true;
            }
            ImGui::PopItemWidth();
            bool tmp_builds_changed = builds_changed;
            builds_changed = false;
            for (const auto build : tbuild->builds) {
                ImGui::PushID(build);
                DrawBuildSection(build);
                ImGui::PopID();
                if (builds_changed) break;
            }
            builds_changed |= tmp_builds_changed;
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            if (ImGui::Checkbox("Show numbers", &tbuild->show_numbers)) {
                builds_changed = true;
            }
            ImGui::SameLine();
            const auto btn_width = 140.f;
            ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - btn_width);
            if (ImGui::Button("Add Build", ImVec2(btn_width, 0))) {
                const auto b = new Build(*tbuild, "", "");
                PrefillBuildFromAgent(GW::Agents::GetControlledCharacterId(), b);
                tbuild->builds.push_back(b);
                editing_build = b;
                builds_changed = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Add another player build row");
            }
            ImGui::Spacing();

            if (ImGui::Button("Up") && i > 0) {
                std::swap(teambuilds[i - 1], teambuilds[i]);
                builds_changed = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Move the teambuild up in the list");
            }
            ImGui::SameLine();
            if (ImGui::Button("Down") && i + 1 < teambuilds.size()) {
                std::swap(teambuilds[i], teambuilds[i + 1]);
                builds_changed = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Move the teambuild down in the list");
            }
            ImGui::SameLine();
            bool delete_tbuild = false;
            if (ImGui::ConfirmButton("Delete", &delete_tbuild, "Delete Teambuild?\n\nAre you sure?\nThis operation cannot be undone.\n\n")) {
                delete tbuild;
                builds_changed = true;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::End();
        ImGui::PopID();
    }
}

void BuildsWindow::Update(const float)
{
    if (!queue.empty() && TIMER_DIFF(send_timer) > 600) {
        if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading
            && GW::Agents::GetControlledCharacter()) {
            send_timer = TIMER_INIT();
            if (!GW::Chat::SendChat('#', queue.front().c_str()))
                Log::Warning("Failed to send build message");
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
    for (const auto tbuild : teambuilds) {
        cur_visible |= tbuild->edit_open;
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
    send_timer = TIMER_INIT();

    GW::Chat::CreateCommand(&ChatCmd_HookEntry, L"loadbuild", CmdLoad);
    GW::Chat::CreateCommand(&ChatCmd_HookEntry, L"pingbuild", CmdPing);
    RegisterUIMessageCallback(&OnUIMessage_HookEntry, GW::UI::UIMessage::kSendLoadSkillTemplate, OnUIMessage);
    //GW::CtoS::RegisterPacketCallback(&on_load_skills_entry, GAME_CMSG_SKILLBAR_LOAD, OnSkillbarLoad);
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
