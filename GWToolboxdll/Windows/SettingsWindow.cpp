#include <GWCA/Constants/Constants.h>
#include <GWCA/Managers/MapMgr.h>

#include <Defines.h>
#include <GWToolbox.h>
#include <Utils/GuiUtils.h>

#include <Modules/ChatCommands.h>
#include <Modules/Resources.h>
#include <Modules/ToolboxSettings.h>
#include <Modules/ToolboxTheme.h>
#include <Modules/Updater.h>
#include <Windows/SettingsWindow.h>

#include <ToolboxWidget.h>

#include <Utils/TextUtils.h>
#include <imgui_test_engine_hooks/imgui_test_engine_hooks.h>

namespace {
    char search_buf[128] = "";

    struct SearchResult {
        std::string nav_section; // SettingsName() used for NavigateToSection
        std::string label;       // empty for category results
        int score = 0;
    };

    // -1 = no match; lower is better: exact prefix > word prefix > substring
    int MatchScore(const std::string& text_lower, const std::string& query_lower)
    {
        const auto pos = text_lower.find(query_lower);
        if (pos == std::string::npos) {
            return -1;
        }
        if (pos == 0) {
            return 0;
        }
        if (text_lower[pos - 1] == ' ') {
            return 1;
        }
        return 2;
    }

    // Pending jump-to-setting: matched via the imgui test-engine item hooks after NavigateToSection
    struct SettingsLocate {
        std::string target_label_lower;
        int frames_remaining = 0;
        ImGuiID matched_id = 0;
        ImRect item_rect;
        bool rect_valid = false;
        bool scrolled = false;
        double highlight_until = 0.0;

        [[nodiscard]] bool HooksNeeded() const { return frames_remaining > 0 || highlight_until > 0.0; }

        void Arm(const std::string& label)
        {
            *this = {};
            target_label_lower = TextUtils::ToLower(label);
            frames_remaining = 120;
        }
    } locate;

    // ItemAdd fires before ItemInfo within a widget; remember it so a label match can resolve its rect same-frame
    ImGuiID last_item_id = 0;
    ImRect last_item_rect;

    void OnLocateItemFound(ImGuiContext* ctx, ImGuiWindow* window, const ImRect& bb)
    {
        locate.item_rect = bb;
        locate.rect_valid = true;
        if (!locate.scrolled) {
            locate.scrolled = true;
            locate.frames_remaining = 0;
            locate.highlight_until = ctx->Time + 2.0;
            ImGui::ScrollToRect(window, bb, ImGuiScrollFlags_KeepVisibleCenterY);
        }
    }

    void OnImGuiItemAdd(ImGuiContext* ctx, const ImGuiID id, const ImRect& bb, const ImGuiLastItemData*)
    {
        if (!(ctx && ctx->CurrentWindow)) {
            return;
        }
        last_item_id = id;
        last_item_rect = bb;
        if (locate.matched_id && id == locate.matched_id) {
            OnLocateItemFound(ctx, ctx->CurrentWindow, bb);
        }
    }

    void OnImGuiItemInfo(ImGuiContext* ctx, const ImGuiID id, const char* label, ImGuiItemStatusFlags)
    {
        if (locate.frames_remaining <= 0 || locate.matched_id) {
            return;
        }
        if (!(ctx && ctx->CurrentWindow && ctx->CurrentWindow->RootWindow)) {
            return;
        }
        if (strcmp(ctx->CurrentWindow->RootWindow->Name, "Settings") != 0) {
            return;
        }
        if (!(label && *label)) {
            return;
        }
        const auto visible_end = ImGui::FindRenderedTextEnd(label);
        const auto visible_label = TextUtils::ToLower(std::string(label, visible_end));
        if (visible_label.empty()) {
            return;
        }
        const auto& target = locate.target_label_lower;
        const bool exact = visible_label == target;
        // Give exact matches a head start before accepting fuzzier substring hits
        const bool fuzzy_allowed = locate.frames_remaining < 90;
        const bool fuzzy = fuzzy_allowed && (visible_label.find(target) != std::string::npos || target.find(visible_label) != std::string::npos);
        if (!(exact || fuzzy)) {
            return;
        }
        locate.matched_id = id;
        if (id == last_item_id) {
            OnLocateItemFound(ctx, ctx->CurrentWindow, last_item_rect);
        }
    }

    void UpdateLocate()
    {
        const auto ctx = ImGui::GetCurrentContext();
        if (!ctx) {
            return;
        }
        if (locate.frames_remaining > 0) {
            locate.frames_remaining--;
        }
        if (locate.highlight_until > 0.0) {
            const auto remaining = locate.highlight_until - ctx->Time;
            if (remaining <= 0.0) {
                locate = {};
            }
            else if (locate.rect_valid) {
                const auto alpha = static_cast<float>(std::min(remaining, 1.0));
                const auto color = ImGui::ColorConvertFloat4ToU32({1.f, 0.85f, 0.f, alpha});
                const ImVec2 min = {locate.item_rect.Min.x - 4.f, locate.item_rect.Min.y - 4.f};
                const ImVec2 max = {locate.item_rect.Max.x + 4.f, locate.item_rect.Max.y + 4.f};
                ImGui::GetForegroundDrawList()->AddRect(min, max, color, 4.f, 0, 2.f);
            }
        }
        ctx->TestEngineHookItems = locate.HooksNeeded();
    }

    std::vector<SearchResult> BuildSearchResults(const std::string& query_lower)
    {
        std::vector<SearchResult> results;
        for (const auto& section : ToolboxModule::GetSettingsCallbacks() | std::views::keys) {
            const auto score = MatchScore(TextUtils::ToLower(section), query_lower);
            if (score >= 0) {
                results.push_back({section, "", score});
            }
        }
        for (const auto& e : SettingsRegistry::GetEntries()) {
            auto best = MatchScore(TextUtils::ToLower(e.label), query_lower);
            for (const auto& text : {e.key, e.section, e.description}) {
                if (text.empty()) {
                    continue;
                }
                const auto score = MatchScore(TextUtils::ToLower(text), query_lower);
                if (score >= 0 && (best < 0 || score < best)) {
                    best = score;
                }
            }
            if (best >= 0) {
                results.push_back({e.module->SettingsName(), e.label, best});
            }
        }
        // The "Enable the following features" checkboxes; labels match the checkbox text so locate works
        const auto* toggles_section = ToolboxSettings::Instance().SettingsName();
        for (const auto& [name, description] : ToolboxSettings::GetOptionalModuleToggles()) {
            auto best = MatchScore(TextUtils::ToLower(name), query_lower);
            if (*description) {
                const auto score = MatchScore(TextUtils::ToLower(description), query_lower);
                if (score >= 0 && (best < 0 || score < best)) {
                    best = score;
                }
            }
            if (best >= 0) {
                results.push_back({toggles_section, name, best});
            }
        }
        std::ranges::sort(results, [](const SearchResult& a, const SearchResult& b) {
            return std::tie(a.score, a.nav_section, a.label) < std::tie(b.score, b.nav_section, b.label);
        });
        constexpr size_t max_results = 50;
        if (results.size() > max_results) {
            results.resize(max_results);
        }
        return results;
    }

    void DrawSearchResults(const bool activate_selected)
    {
        static int selected_index = 0;
        static std::string last_query;
        if (last_query != search_buf) {
            last_query = search_buf;
            selected_index = 0;
        }
        const auto results = BuildSearchResults(TextUtils::ToLower(search_buf));
        if (results.empty()) {
            ImGui::TextDisabled("No settings match '%s'", search_buf);
            return;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
            selected_index++;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
            selected_index--;
        }
        selected_index = std::clamp(selected_index, 0, static_cast<int>(results.size()) - 1);

        const SearchResult* activated = nullptr;
        if (ImGui::BeginChild("##settings_search_results")) {
            const auto& icons = ToolboxModule::GetSettingsIcons();
            for (size_t i = 0; i < results.size(); i++) {
                const auto& result = results[i];
                const char* icon = nullptr;
                if (const auto it = icons.find(result.nav_section); it != icons.end()) {
                    icon = it->second;
                }
                const auto text = result.label.empty()
                                      ? std::format("{}  {}##result_{}", icon ? icon : " ", result.nav_section, i)
                                      : std::format("{}  {} > {}##result_{}", icon ? icon : " ", result.nav_section, result.label, i);
                const bool is_selected = static_cast<int>(i) == selected_index;
                if (ImGui::Selectable(text.c_str(), is_selected) || (is_selected && activate_selected)) {
                    activated = &result;
                }
            }
        }
        ImGui::EndChild();
        if (activated) {
            const auto section = activated->nav_section;
            const auto label = activated->label;
            search_buf[0] = 0;
            last_query.clear();
            SettingsWindow::Instance().NavigateToSection(section.c_str());
            if (!label.empty()) {
                locate.Arm(label);
            }
        }
    }
} // namespace

void SettingsWindow::NavigateToSection(const char* section)
{
    visible = true;
    pending_uncollapse = true;
    pending_navigate_to = section;
}

void SettingsWindow::Initialize()
{
    ToolboxWindow::Initialize();
    SettingsRegistry::RegisterField(this, "hide_when_entering_explorable", &hide_when_entering_explorable);
    imgui_test_engine_hook_callbacks.item_add = OnImGuiItemAdd;
    imgui_test_engine_hook_callbacks.item_info = OnImGuiItemInfo;
}

void SettingsWindow::Terminate()
{
    ToolboxWindow::Terminate();
    imgui_test_engine_hook_callbacks.item_add = nullptr;
    imgui_test_engine_hook_callbacks.item_info = nullptr;
    locate = {};
    if (const auto ctx = ImGui::GetCurrentContext()) {
        ctx->TestEngineHookItems = false;
    }
}

void SettingsWindow::Draw(IDirect3DDevice9*)
{
    static auto last_instance_type = GW::Constants::InstanceType::Loading;
    const GW::Constants::InstanceType instance_type = GW::Map::GetInstanceType();

    if (instance_type != last_instance_type) {
        if (hide_when_entering_explorable && instance_type == GW::Constants::InstanceType::Explorable) {
            visible = false;
        }
        last_instance_type = instance_type;
    }

    if (!visible) {
        if (locate.HooksNeeded()) {
            locate = {};
            if (const auto ctx = ImGui::GetCurrentContext()) {
                ctx->TestEngineHookItems = false;
            }
        }
        return;
    }
    if (pending_uncollapse) {
        ImGui::SetNextWindowCollapsed(false, ImGuiCond_Always);
        pending_uncollapse = false;
    }
    // Frame-start value: a navigate set mid-draw (by a button drawn after its target) must survive to next frame.
    const bool had_pending_navigate = !pending_navigate_to.empty();
    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(768, 768), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        drawn_settings.clear();
        ImGui::SetNextItemWidth(-1.f);
        const bool enter_pressed = ImGui::InputTextWithHint("##settings_search", ICON_FA_SEARCH "  Search settings...", search_buf, sizeof(search_buf), ImGuiInputTextFlags_EnterReturnsTrue);
        if (search_buf[0] && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            search_buf[0] = 0;
        }
        if (search_buf[0]) {
            DrawSearchResults(enter_pressed);
            ImGui::End();
            UpdateLocate();
            // pending_navigate_to deliberately kept: a result activated this frame is consumed next frame
            return;
        }
        const ImColor sCol(102, 187, 238, 255);
        ImGui::PushTextWrapPos();
        ImGui::Text("GWToolbox++");
        ImGui::SameLine(0, 0);
        ImGui::TextColored(sCol, " v%s ", GWTOOLBOXDLL_VERSION);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Go to %s", GWTOOLBOX_WEBSITE);
        }
        if (ImGui::IsItemClicked()) {
            ShellExecute(nullptr, "open", GWTOOLBOX_WEBSITE, nullptr, nullptr, SW_SHOWNORMAL);
        }
        if constexpr (!std::string_view(GWTOOLBOXDLL_VERSION_BETA).empty()) {
            ImGui::SameLine();
            ImGui::Text("- %s", GWTOOLBOXDLL_VERSION_BETA);
        }
        else {
            const std::string server_version = Updater::GetServerVersion();
            if (!server_version.empty()) {
                if (server_version == GWTOOLBOXDLL_VERSION) {
                    ImGui::SameLine();
                    ImGui::Text("(Up to date)");
                }
                else {
                    ImGui::Text("Version %s is available!", server_version.c_str());
                }
            }
        }
#ifdef _DEBUG
        ImGui::SameLine();
        ImGui::Text("(Debug)");
#endif
        const float w = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2;
        if (ImGui::Button("Open current settings folder", ImVec2(w, 0))) {
            ShellExecuteW(nullptr, L"open", Resources::GetSettingsFolderPath().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
        ImGui::SameLine();
        if (ImGui::Button("Open GWToolbox++ Website", ImVec2(w, 0))) {
            ShellExecuteA(nullptr, "open", GWTOOLBOX_WEBSITE, nullptr, nullptr, SW_SHOWNORMAL);
        }
        ImGui::Checkbox("Hide Settings when entering explorable area", &hide_when_entering_explorable);
        ImGui::CheckboxWithHelp("Send anonymous gameplay stats", &ToolboxSettings::send_anonymous_gameplay_info, "Some features of toolbox allow you to contribute to the community\nby sending in-game data to remote websites.\
        \n\nFeatures that use this info:\
        \n\t- Sending outpost party information to https://party.gwtoolbox.com\
        \n\t- Sending purchase analytics to https://gwmarket.net when whispering a seller/buyer from the Market Browser");
        ImGui::Text("General:");

        if (ImGui::CollapsingHeader("Help")) {
            if (ImGui::TreeNodeEx("General Interface", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
                ImGui::Bullet();
                ImGui::Text("Double-click on the title bar to collapse a window.");
                ImGui::Bullet();
                ImGui::Text("Click and drag on the lower right corner to resize a window.");
                ImGui::Bullet();
                ImGui::Text("Click and drag on any empty space to move a window.");
                ImGui::Bullet();
                ImGui::Text("Mouse Wheel to scroll.");
                if (ImGui::GetIO().FontAllowUserScaling) {
                    ImGui::Bullet();
                    ImGui::Text("CTRL+Mouse Wheel to zoom window contents.");
                }
                ImGui::Bullet();
                ImGui::Text("TAB or SHIFT+TAB to cycle through keyboard editable fields.");
                ImGui::Bullet();
                ImGui::Text("CTRL+Click or Double Click on a slider or drag box to input text.");
                ImGui::Bullet();
                ImGui::Text(
                    "While editing text:\n"
                    "- Hold SHIFT or use mouse to select text\n"
                    "- CTRL+Left/Right to word jump\n"
                    "- CTRL+A or double-click to select all\n"
                    "- CTRL+X,CTRL+C,CTRL+V clipboard\n"
                    "- CTRL+Z,CTRL+Y undo/redo\n"
                    "- ESCAPE to revert\n"
                    "- You can apply arithmetic operators +,*,/ on numerical values. Use +- to subtract.\n"
                );
                ImGui::TreePop();
            }
            if (ImGui::TreeNodeEx("Opening and closing windows", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
                ImGui::Text("There are several ways to open and close toolbox windows and widgets:");
                ImGui::Bullet();
                ImGui::Text("Buttons in the main window.");
                ImGui::Bullet();
                ImGui::Text("Checkboxes in the Info window.");
                ImGui::Bullet();
                ImGui::Text("Checkboxes on the right of each setting header below.");
                ImGui::Bullet();
                ImGui::Text("Chat command '/hide <name>' to hide a window or widget.");
                ImGui::Bullet();
                ImGui::Text("Chat command '/show <name>' to show a window or widget.");
                ImGui::Bullet();
                ImGui::Text("Chat command '/tb <name>' to toggle a window or widget.");
                ImGui::Indent();
                ImGui::Text("In the commands above, <name> is the title of the window as shown in the title bar. For example, try '/hide settings' and '/show settings'.");
                ImGui::Text("Note: the names of the widgets without a visible title bar are the same as in the setting headers below.");
                ImGui::Unindent();
                ImGui::Bullet();
                ImGui::Text("Send Chat hotkey to enter one of the commands above.");
                ImGui::TreePop();
            }
            for (const auto module : GWToolbox::GetAllModules()) {
                module->DrawHelp();
            }
        }

        const auto& settings_sections = GetSettingsCallbacks();

        std::vector<std::string> sections_to_draw;

        DrawSettingsSection(ToolboxTheme::Instance().SettingsName());
        DrawSettingsSection(ToolboxSettings::Instance().SettingsName());

        const auto sort = [](const ToolboxModule* a, const ToolboxModule* b) {
            return strcmp(a->Name(), b->Name()) < 0;
        };
        const auto queue_settings_for_module = [&](ToolboxModule* m) {
            for (const auto& [section, cb] : settings_sections) {
                for (const auto& cbs : cb) {
                    if (cbs.module == m) {
                        sections_to_draw.push_back(section);
                        break;
                    }
                }
            }
            sections_to_draw.push_back(m->SettingsName());
        };
        const auto sort_and_draw_settings = [&] {
            std::ranges::sort(sections_to_draw);
            for (auto& s : sections_to_draw) {
                DrawSettingsSection(s.c_str());
            }
            sections_to_draw.clear();
        };


        auto modules = GWToolbox::GetModules();
        std::ranges::sort(modules, sort);
        for (const auto m : modules) {
            if (m->HasSettings()) {
                queue_settings_for_module(m);
            }
        }
        sort_and_draw_settings();

        auto windows = GWToolbox::GetWindows();
        std::ranges::sort(windows, sort);
        if (!windows.empty()) {
            ImGui::Text("Windows:");
        }
        for (const auto m : windows) {
            if (m->HasSettings()) {
                queue_settings_for_module(m);
            }
        }
        sort_and_draw_settings();

        auto widgets = GWToolbox::GetWidgets();
        std::ranges::sort(widgets, sort);
        if (!widgets.empty()) {
            ImGui::Text("Widgets:");
        }
        for (const auto m : widgets) {
            if (m->HasSettings()) {
                queue_settings_for_module(m);
            }
        }
        sort_and_draw_settings();

        if (ImGui::Button("Save Now", ImVec2(w, 0))) {
            GWToolbox::SaveSettings();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Toolbox normally saves settings on exit.\nClick to save to disk now.");
        }
        ImGui::SameLine();
        if (ImGui::Button("Load Now", ImVec2(w, 0))) {
            GWToolbox::LoadSettings();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Toolbox normally loads settings on launch.\nClick to re-load from disk now.");
        }
        ImGui::PopTextWrapPos();
    }
    ImGui::End();
    UpdateLocate();
    // Clear only targets present at frame start (consumed or stale); keep a mid-draw set for next frame.
    if (had_pending_navigate) pending_navigate_to.clear();
}

bool SettingsWindow::DrawSettingsSection(const char* section)
{
    if (strcmp(section, "") == 0) {
        return true;
    }
    const auto& callbacks = GetSettingsCallbacks();
    const auto& icons = GetSettingsIcons();

    const auto& settings_section = callbacks.find(section);
    if (settings_section == callbacks.end()) {
        return false;
    }
    if (drawn_settings.contains(section)) {
        return true; // Already drawn
    }
    drawn_settings[section] = true;

    const char* icon = nullptr;
    if (const auto it = icons.find(section); it != icons.end()) {
        icon = it->second;
    }
    const auto text_height = ImGui::GetTextLineHeightWithSpacing();
    const auto pos = ImGui::GetCursorScreenPos();
    const auto padding = ImGui::GetStyle().FramePadding;
    float header_text_offset_x = text_height + padding.x * 3;
    const bool should_navigate = !pending_navigate_to.empty() && pending_navigate_to == section;
    if (should_navigate) {
        ImGui::SetNextItemOpen(true, ImGuiCond_Always);
    }
    else if (!pending_navigate_to.empty()) {
        ImGui::SetNextItemOpen(false, ImGuiCond_Always);
    }
    const bool is_showing = ImGui::CollapsingHeader(std::format("##{}", section).c_str(), ImGuiTreeNodeFlags_AllowOverlap);
    ImGui::SameLine(header_text_offset_x);
    if (icon) {
        ImGui::TextUnformatted(icon);
        ImGui::SameLine(header_text_offset_x += text_height + padding.x);
    }
    ImGui::TextUnformatted(section);
    if (should_navigate) {
        ImGui::SetScrollHereY(0.0f);
    }

    ImGui::PushID(section);
    size_t i = 0;
    if (is_showing) ImGui::Indent();
    for (const auto& setting_callback : settings_section->second) {
        // if (i && is_showing) ImGui::Separator();
        ImGui::PushID(i);
        setting_callback.callback(settings_section->first, is_showing);
        i++;
        ImGui::PopID();
    }
    if (is_showing) ImGui::Unindent();
    ImGui::PopID();
    return true;
}
