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
#include <ToolboxWindow.h>

#include <Utils/TextUtils.h>
#include <unordered_map>
#include <imgui_test_engine_hooks/imgui_test_engine_hooks.h>

namespace {
    char search_buf[128] = "";

    struct SearchResult {
        std::string nav_section; // SettingsName() used for NavigateToSection
        std::string label;       // empty for category results
        std::string row_label;
        int score = 0;
    };

    // Settings sub-headers (ImGui::TreeNodeEx within a section's draw) declared via RegisterSubSection,
    // so search can surface them even while the parent section is collapsed.
    std::vector<std::pair<std::string, std::string>> sub_sections; // {section, label}

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

    std::string NavSectionForModule(ToolboxModule* module)
    {
        if (!module) {
            return {};
        }
        const auto& callbacks = ToolboxModule::GetSettingsCallbacks();
        const auto* settings_name = module->SettingsName();
        if (callbacks.contains(settings_name)) {
            return settings_name;
        }
        std::string found;
        for (const auto& [section, list] : callbacks) {
            const auto drawn_here = std::ranges::any_of(list, [module](const SectionDrawCallbackInfo& info) {
                return info.module == module;
            });
            if (drawn_here && (found.empty() || section < found)) {
                found = section;
            }
        }
        return found;
    }

    const std::string& CachedToLower(const std::string& s)
    {
        static std::unordered_map<std::string, std::string> lowered;
        const auto found = lowered.find(s);
        if (found != lowered.end()) {
            return found->second;
        }
        return lowered.emplace(s, TextUtils::ToLower(s)).first->second;
    }

    std::vector<SearchResult> BuildSearchResults(const std::string& query_lower)
    {
        std::vector<SearchResult> results;
        for (const auto& section : ToolboxModule::GetSettingsCallbacks() | std::views::keys) {
            const auto score = MatchScore(CachedToLower(section), query_lower);
            if (score >= 0) {
                results.push_back({.nav_section = section, .score = score});
            }
        }
        for (const auto& e : SettingsRegistry::GetEntries()) {
            if (!e.in_settings_window) {
                continue; // Drawn by the module's own UI; navigating here would land on nothing
            }
            auto best = MatchScore(CachedToLower(e.label), query_lower);
            for (const auto& text : {e.section, e.description}) {
                if (text.empty()) {
                    continue;
                }
                const auto score = MatchScore(CachedToLower(text), query_lower);
                if (score >= 0 && (best < 0 || score < best)) {
                    best = score;
                }
            }
            if (best < 0) {
                continue;
            }
            if (const auto nav_section = NavSectionForModule(e.module); !nav_section.empty()) {
                results.push_back({.nav_section = nav_section, .label = e.label, .score = best});
            }
        }
        for (const auto& [section, label] : sub_sections) {
            const auto score = MatchScore(CachedToLower(label), query_lower);
            if (score >= 0) {
                results.push_back({.nav_section = section, .label = label, .score = score});
            }
        }
        // The "Enable the following features" checkboxes; labels match the checkbox text so locate works
        const auto* toggles_section = ToolboxSettings::Instance().SettingsName();
        for (const auto& [name, description] : ToolboxSettings::GetOptionalModuleToggles()) {
            auto best = MatchScore(CachedToLower(name), query_lower);
            if (*description) {
                const auto score = MatchScore(CachedToLower(description), query_lower);
                if (score >= 0 && (best < 0 || score < best)) {
                    best = score;
                }
            }
            if (best >= 0) {
                results.push_back({.nav_section = toggles_section, .label = name, .score = best});
            }
        }
        std::ranges::sort(results, [](const SearchResult& a, const SearchResult& b) {
            return std::tie(a.score, a.nav_section, a.label) < std::tie(b.score, b.nav_section, b.label);
        });
        constexpr size_t max_results = 50;
        if (results.size() > max_results) {
            results.resize(max_results);
        }
        const auto& icons = ToolboxModule::GetSettingsIcons();
        for (size_t i = 0; i < results.size(); i++) {
            auto& result = results[i];
            const char* icon = nullptr;
            if (const auto it = icons.find(result.nav_section); it != icons.end()) {
                icon = it->second;
            }
            result.row_label = result.label.empty()
                                   ? std::format("{}  {}##result_{}", icon ? icon : " ", result.nav_section, i)
                                   : std::format("{}  {} > {}##result_{}", icon ? icon : " ", result.nav_section, result.label, i);
        }
        return results;
    }

    void DrawSearchResults(const bool activate_selected)
    {
        static int selected_index = 0;
        static std::string last_query;
        static std::vector<SearchResult> results;
        if (last_query != search_buf) {
            last_query = search_buf;
            selected_index = 0;
            results = BuildSearchResults(TextUtils::ToLower(search_buf));
        }
        if (results.empty()) {
            ImGui::TextDisabled("没有与 '%s' 匹配的设置", search_buf);
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
            for (size_t i = 0; i < results.size(); i++) {
                const auto& result = results[i];
                const bool is_selected = static_cast<int>(i) == selected_index;
                if (ImGui::Selectable(result.row_label.c_str(), is_selected) || (is_selected && activate_selected)) {
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
            SettingsWindow::Instance().NavigateToSection(section.c_str(), !label.empty());
            if (!label.empty()) {
                locate.Arm(label);
            }
        }
    }
} // namespace

void SettingsWindow::NavigateToSection(const char* section, const bool expand_subsections)
{
    visible = true;
    pending_uncollapse = true;
    pending_navigate_to = section;
    pending_expand_subsections = expand_subsections;
}

void SettingsWindow::RegisterSubSection(const char* section, const char* label)
{
    const auto exists = std::ranges::any_of(sub_sections, [&](const auto& e) {
        return e.first == section && e.second == label;
    });
    if (!exists) {
        sub_sections.emplace_back(section, label);
    }
}

bool SettingsWindow::SubSectionHeader(const char* section, const char* label)
{
    auto& self = Instance();
    if (self.pending_expand_subsections && self.pending_navigate_to == section) {
        ImGui::SetNextItemOpen(true, ImGuiCond_Always);
    }
    return ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth);
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
        const bool enter_pressed = ImGui::InputTextWithHint("##settings_search", ICON_FA_SEARCH "  搜索设置...", search_buf, sizeof(search_buf), ImGuiInputTextFlags_EnterReturnsTrue);
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
            ImGui::SetTooltip("转到 %s", GWTOOLBOX_WEBSITE);
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
                    ImGui::Text("（已是最新）");
                }
                else {
                    ImGui::Text("版本 %s 已可用！", server_version.c_str());
                }
            }
        }
#ifdef _DEBUG
        ImGui::SameLine();
        ImGui::Text("（调试）");
#endif
#ifdef __clang__
        ImGui::SameLine();
        ImGui::Text("（Clang）");
#endif
        const float w = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2;
        if (ImGui::Button("打开当前设置文件夹", ImVec2(w, 0))) {
            ShellExecuteW(nullptr, L"open", Resources::GetSettingsFolderPath().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
        ImGui::SameLine();
        if (ImGui::Button("打开 GWToolbox++ 网站", ImVec2(w, 0))) {
            ShellExecuteA(nullptr, "open", GWTOOLBOX_WEBSITE, nullptr, nullptr, SW_SHOWNORMAL);
        }
        ImGui::Checkbox("进入可探索区域时隐藏设置", &hide_when_entering_explorable);
        ImGui::CheckboxWithHelp("发送匿名游戏统计数据", &ToolboxSettings::send_anonymous_gameplay_info, "工具箱的某些功能允许您通过向远程网站发送游戏内数据来为社区做出贡献。\n\n使用此信息的功能：\n\t- 将前哨战队伍信息发送至 https://party.gwtoolbox.com\n\t- 当通过市场浏览器向卖家/买家发送密语时，将购买分析发送至 https://gwmarket.net");
        ImGui::Text("常规：");

        if (ImGui::CollapsingHeader("帮助")) {
            if (ImGui::TreeNodeEx("通用界面", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
                ImGui::Bullet();
                ImGui::Text("双击标题栏可折叠窗口。");
                ImGui::Bullet();
                ImGui::Text("点击并拖动右下角可调整窗口大小。");
                ImGui::Bullet();
                ImGui::Text("点击并拖动任何空白区域可移动窗口。");
                ImGui::Bullet();
                ImGui::Text("鼠标滚轮可滚动。");
                if (ImGui::GetIO().FontAllowUserScaling) {
                    ImGui::Bullet();
                    ImGui::Text("Ctrl+鼠标滚轮可缩放窗口内容。");
                }
                ImGui::Bullet();
                ImGui::Text("Tab 或 Shift+Tab 可在可编辑字段间循环切换。");
                ImGui::Bullet();
                ImGui::Text("Ctrl+单击或双击滑块或拖拽框可输入文本。");
                ImGui::Bullet();
                ImGui::Text(
                    "编辑文本时：\n"
                    "- 按住 Shift 或使用鼠标选择文本\n"
                    "- Ctrl+左/右 逐词跳转\n"
                    "- Ctrl+A 或双击全选\n"
                    "- Ctrl+X、Ctrl+C、Ctrl+V 剪贴板操作\n"
                    "- Ctrl+Z、Ctrl+Y 撤销/重做\n"
                    "- Esc 取消更改\n"
                    "- 可在数值上使用算术运算符 +、*、/，使用 +- 进行减法。\n"
                );
                ImGui::TreePop();
            }
            if (ImGui::TreeNodeEx("打开和关闭窗口", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
                ImGui::Text("有几种方式可以打开和关闭工具箱窗口和小部件：");
                ImGui::Bullet();
                ImGui::Text("主窗口中的按钮。");
                ImGui::Bullet();
                ImGui::Text("信息窗口中的复选框。");
                ImGui::Bullet();
                ImGui::Text("下方每个设置标题右侧的复选框。");
                ImGui::Bullet();
                ImGui::Text("聊天命令 '/hide <名称>' 可隐藏窗口或小部件。");
                ImGui::Bullet();
                ImGui::Text("聊天命令 '/show <名称>' 可显示窗口或小部件。");
                ImGui::Bullet();
                ImGui::Text("聊天命令 '/tb <名称>' 可切换窗口或小部件。");
                ImGui::Indent();
                ImGui::Text("在上述命令中，<名称> 是标题栏中显示的窗口标题。例如，尝试 '/hide settings' 和 '/show settings'。");
                ImGui::Text("注意：没有可见标题栏的小部件名称与下方设置标题中的名称相同。");
                ImGui::Unindent();
                ImGui::Bullet();
                ImGui::Text("发送聊天热键可输入上述命令。");
                ImGui::TreePop();
            }
            for (const auto module : GWToolbox::GetAllModules()) {
                module->DrawHelp();
            }
        }

        const auto& settings_sections = GetSettingsCallbacks();

        DrawSettingsSection(ToolboxTheme::Instance().SettingsName());
        DrawSettingsSection(ToolboxSettings::Instance().SettingsName());

        // Section names only change when a module is toggled or (un)registers settings content, not per frame.
        struct CachedSections {
            std::vector<ToolboxModule*> source;
            std::vector<std::string> sections;
        };
        static CachedSections modules, windows, widgets;
        static uint32_t cached_callbacks_revision = static_cast<uint32_t>(-1);

        const auto callbacks_revision = ToolboxModule::GetSettingsCallbacksRevision();
        const bool callbacks_changed = cached_callbacks_revision != callbacks_revision;
        cached_callbacks_revision = callbacks_revision;

        const auto sync_sections = [&](CachedSections& cache, const auto& src) {
            if (!callbacks_changed && std::ranges::equal(cache.source, src)) {
                return;
            }
            cache.source.assign(src.begin(), src.end());
            std::vector<ToolboxModule*> sorted(cache.source);
            std::ranges::sort(sorted, [](const ToolboxModule* a, const ToolboxModule* b) {
                return strcmp(a->Name(), b->Name()) < 0;
            });

            cache.sections.clear();
            for (const auto m : sorted) {
                if (!m->HasSettings()) {
                    continue;
                }
                for (const auto& [section, cb] : settings_sections) {
                    for (const auto& cbs : cb) {
                        if (cbs.module == m) {
                            cache.sections.push_back(section);
                            break;
                        }
                    }
                }
                cache.sections.emplace_back(m->SettingsName());
            }
            std::ranges::sort(cache.sections);
        };
        const auto draw_sections = [&](const CachedSections& cache) {
            for (const auto& s : cache.sections) {
                DrawSettingsSection(s.c_str());
            }
        };

        sync_sections(modules, GWToolbox::GetModules());
        draw_sections(modules);

        sync_sections(windows, GWToolbox::GetWindows());
        if (!windows.source.empty()) {
            ImGui::Text("Windows:");
        }
        draw_sections(windows);

        sync_sections(widgets, GWToolbox::GetWidgets());
        if (!widgets.source.empty()) {
            ImGui::Text("Widgets:");
        }
        draw_sections(widgets);

        if (ImGui::Button("立即保存", ImVec2(w, 0))) {
            GWToolbox::SaveSettings();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("工具箱通常在退出时保存设置。\n点击立即保存到磁盘。");
        }
        ImGui::SameLine();
        if (ImGui::Button("立即加载", ImVec2(w, 0))) {
            GWToolbox::LoadSettings();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("工具箱通常在启动时加载设置。\n点击立即从磁盘重新加载。");
        }
        ImGui::PopTextWrapPos();
    }
    ImGui::End();
    UpdateLocate();
    // Clear only targets present at frame start (consumed or stale); keep a mid-draw set for next frame.
    if (had_pending_navigate) {
        pending_navigate_to.clear();
        pending_expand_subsections = false;
    }
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
    ImGui::PushID(section);
    const bool is_showing = ImGui::CollapsingHeader("", ImGuiTreeNodeFlags_AllowOverlap);
    ImGui::SameLine(header_text_offset_x);
    if (icon) {
        ImGui::TextUnformatted(icon);
        ImGui::SameLine(header_text_offset_x += text_height + padding.x);
    }
    ImGui::TextUnformatted(section);
    if (should_navigate) {
        ImGui::SetScrollHereY(0.0f);
    }

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