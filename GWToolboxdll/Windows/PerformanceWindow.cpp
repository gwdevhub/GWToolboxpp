#include "stdafx.h"

#include <charconv>

#include <GWCA/Utilities/Hooker.h>

#include <GWToolbox.h>
#include <Defines.h>
#include <ImGuiAddons.h>
#include <Modules/Resources.h>
#include <Windows/PerformanceWindow.h>

namespace {
    PerformanceWindow::Settings settings;

    uint64_t QpcToMicroseconds(LONGLONG ticks)
    {
        static LARGE_INTEGER freq = [] { LARGE_INTEGER f; QueryPerformanceFrequency(&f); return f; }();
        return static_cast<uint64_t>(ticks * 1000000 / freq.QuadPart);
    }

    // Present 钩子
    typedef HRESULT(__stdcall* Present_pt)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
    Present_pt Present_Func = nullptr, Present_Ret = nullptr;
    uint64_t present_time_us = 0;

    HRESULT __stdcall OnPresent(IDirect3DDevice9* device, const RECT* src, const RECT* dst, HWND hwnd, const RGNDATA* dirty)
    {
        GW::Hook::EnterHook();
        LARGE_INTEGER t0, t1;
        QueryPerformanceCounter(&t0);
        const HRESULT hr = Present_Ret(device, src, dst, hwnd, dirty);
        QueryPerformanceCounter(&t1);
        present_time_us = QpcToMicroseconds(t1.QuadPart - t0.QuadPart);
        GW::Hook::LeaveHook();
        return hr;
    }

    void HookPresent(IDirect3DDevice9* device)
    {
        if (Present_Func || !device) return;
        auto** vtable = *reinterpret_cast<void***>(device);
        Present_Func = reinterpret_cast<Present_pt>(vtable[17]);
        GW::Hook::CreateHook(reinterpret_cast<void**>(&Present_Func), OnPresent, reinterpret_cast<void**>(&Present_Ret));
        GW::Hook::EnableHooks(Present_Func);
    }

    void UnhookPresent()
    {
        if (!Present_Func) return;
        GW::Hook::RemoveHook(Present_Func);
        Present_Func = nullptr;
        Present_Ret = nullptr;
    }

    struct Stats {
        uint64_t min_us = UINT64_MAX;
        uint64_t max_us = 0;
        uint64_t sum_us = 0;
        uint32_t count = 0;

        void Record(uint64_t us)
        {
            if (us < min_us) min_us = us;
            if (us > max_us) max_us = us;
            sum_us += us;
            count++;
        }

        uint64_t Avg() const { return count > 0 ? sum_us / count : 0; }

        void Reset()
        {
            min_us = UINT64_MAX;
            max_us = 0;
            sum_us = 0;
            count = 0;
        }

        void Merge(const Stats& other)
        {
            if (other.count == 0) return;
            if (other.min_us < min_us) min_us = other.min_us;
            if (other.max_us > max_us) max_us = other.max_us;
            sum_us += other.sum_us;
            count += other.count;
        }
    };

    constexpr int WINDOW_SECONDS = 5;

    struct ModuleStats {
        Stats update, draw;
        std::map<uint32_t, Stats> ui_msgs;
    };

    // 1 秒快照的环形缓冲区
    Stats hist_frame[WINDOW_SECONDS], hist_tb_update[WINDOW_SECONDS], hist_tb_draw[WINDOW_SECONDS], hist_present[WINDOW_SECONDS];
    std::map<std::string, ModuleStats, std::less<>> hist_modules[WINDOW_SECONDS];
    int hist_index = 0;

    // 显示的统计（合并自环形缓冲区）
    Stats displayed_frame, displayed_tb_update, displayed_tb_draw, displayed_present;
    std::map<std::string, ModuleStats, std::less<>> displayed_modules;

    // 当前 1 秒窗口的累积统计
    Stats acc_frame, acc_tb_update, acc_tb_draw, acc_present;
    std::map<std::string, ModuleStats, std::less<>> acc_modules;
    DWORD window_start_tick = 0;

    // 原始 1 秒累加器值，便于离线重新计算真实平均值
    void AppendCsvRow(std::string& out, DWORD tick, const char* category, const char* name, const char* metric, const Stats& s)
    {
        if (s.count == 0) return;
        char line[192];
        snprintf(line, sizeof(line), "%lu,%s,%s,%s,%u,%llu,%llu,%llu\n",
                 tick, category, name, metric, s.count,
                 static_cast<unsigned long long>(s.sum_us),
                 static_cast<unsigned long long>(s.min_us),
                 static_cast<unsigned long long>(s.max_us));
        out += line;
    }

    // 识别生成 CSV 的编译器，使 MSVC 和 clang 的输出分到不同文件
    std::wstring CompilerTag()
    {
#ifdef __clang__
        return std::format(L"clang-{}.{}.{}", __clang_major__, __clang_minor__, __clang_patchlevel__);
#elifdef _MSC_VER
        return std::format(L"msvc-{}", _MSC_FULL_VER);
#else
        return L"unknown";
#endif
    }

    // 设置文件夹（非根目录），使文件位于“打开当前设置文件夹”指向的位置
    std::filesystem::path CsvPath() { return Resources::GetSettingFile(L"performance_log_" + CompilerTag() + L".csv"); }

    // 在工作线程中运行，避免绘制循环访问磁盘
    void WriteCsvRows(const std::string& rows)
    {
        static std::mutex csv_mutex;
        std::scoped_lock lock(csv_mutex);
        std::ofstream file(CsvPath(), std::ios::app);
        if (!file.is_open()) return;
        // 仅在全新文件时写入表头；追加模式保留之前的运行记录
        if (file.tellp() == 0)
            file << "tick_ms,category,name,metric,count,sum_us,min_us,max_us\n";
        file << rows;
    }

    void StreamSnapshotToCsv(DWORD tick)
    {
        std::string rows;
        AppendCsvRow(rows, tick, "frame", "frame", "period", acc_frame);
        AppendCsvRow(rows, tick, "toolbox", "tb_update", "update", acc_tb_update);
        AppendCsvRow(rows, tick, "toolbox", "tb_draw", "draw", acc_tb_draw);
        AppendCsvRow(rows, tick, "toolbox", "present", "present", acc_present);
        for (const auto& [name, ms] : acc_modules) {
            AppendCsvRow(rows, tick, "module", name.c_str(), "update", ms.update);
            AppendCsvRow(rows, tick, "module", name.c_str(), "draw", ms.draw);
        }
        if (!rows.empty())
            Resources::EnqueueWorkerTask([rows = std::move(rows)] { WriteCsvRows(rows); });
    }

    void FlushWindow(DWORD tick)
    {
        if (settings.stream_to_csv) StreamSnapshotToCsv(tick);

        hist_frame[hist_index] = acc_frame;
        hist_tb_update[hist_index] = acc_tb_update;
        hist_tb_draw[hist_index] = acc_tb_draw;
        hist_present[hist_index] = acc_present;
        hist_modules[hist_index] = acc_modules;
        hist_index = (hist_index + 1) % WINDOW_SECONDS;

        displayed_frame = {};
        displayed_tb_update = {};
        displayed_tb_draw = {};
        displayed_present = {};
        displayed_modules.clear();
        for (int i = 0; i < WINDOW_SECONDS; i++) {
            displayed_frame.Merge(hist_frame[i]);
            displayed_tb_update.Merge(hist_tb_update[i]);
            displayed_tb_draw.Merge(hist_tb_draw[i]);
            displayed_present.Merge(hist_present[i]);
            for (const auto& [name, ms] : hist_modules[i]) {
                auto& dm = displayed_modules[name];
                dm.update.Merge(ms.update);
                dm.draw.Merge(ms.draw);
                for (const auto& [msg_id, s] : ms.ui_msgs) {
                    dm.ui_msgs[msg_id].Merge(s);
                }
            }
        }

        acc_frame = {};
        acc_tb_update = {};
        acc_tb_draw = {};
        acc_present = {};
        acc_modules.clear();
    }

    ImU32 ColorForTime(uint64_t us)
    {
        if (us >= 1000) return IM_COL32(255, 80, 80, 255);
        if (us >= 100) return IM_COL32(255, 200, 50, 255);
        return IM_COL32(140, 255, 140, 255);
    }

    void DrawStatsColumns(const Stats& s)
    {
        if (s.count == 0) {
            ImGui::TableNextColumn(); ImGui::TextUnformatted("-");
            ImGui::TableNextColumn(); ImGui::TextUnformatted("-");
            ImGui::TableNextColumn(); ImGui::TextUnformatted("-");
            return;
        }
        const auto avg = s.Avg();
        ImGui::TableNextColumn(); ImGui::TextColored(ImColor(ColorForTime(s.min_us)).Value, "%llu", s.min_us);
        ImGui::TableNextColumn(); ImGui::TextColored(ImColor(ColorForTime(avg)).Value, "%llu", avg);
        ImGui::TableNextColumn(); ImGui::TextColored(ImColor(ColorForTime(s.max_us)).Value, "%llu", s.max_us);
    }

    // ── 比较选项卡支持 ──────────────────────────────────────────────────
    template <typename T>
    T ParseNum(std::string_view s)
    {
        T v{};
        std::from_chars(s.data(), s.data() + s.size(), v);
        return v;
    }

    // 将导出 CSV 中的每一秒行归并为每个 "category/name/metric" 的总计
    //（count/sum/min/max），以便在整个运行期间重新计算真实平均值。
    std::map<std::string, Stats> LoadCsvAggregate(const std::filesystem::path& file)
    {
        std::map<std::string, Stats> out;
        std::ifstream in(file);
        if (!in.is_open()) return out;

        std::string line;
        std::getline(in, line); // 表头：tick_ms,category,name,metric,count,sum_us,min_us,max_us
        while (std::getline(in, line)) {
            if (line.empty()) continue;
            std::string_view f[8];
            int n = 0;
            size_t start = 0;
            for (; n < 8; n++) {
                const size_t comma = line.find(',', start);
                if (comma == std::string::npos) { f[n++] = std::string_view(line).substr(start); break; }
                f[n] = std::string_view(line).substr(start, comma - start);
                start = comma + 1;
            }
            if (n < 8) continue;

            Stats s;
            s.count = ParseNum<uint32_t>(f[4]);
            if (s.count == 0) continue;
            s.sum_us = ParseNum<uint64_t>(f[5]);
            s.min_us = ParseNum<uint64_t>(f[6]);
            s.max_us = ParseNum<uint64_t>(f[7]);
            out[std::string(f[1]) + "/" + std::string(f[2]) + "/" + std::string(f[3])].Merge(s);
        }
        return out;
    }

}

void PerformanceWindow::Draw(IDirect3DDevice9* device)
{
    GWToolbox::SetProfilingEnabled(visible);
    if (!visible) {
        UnhookPresent();
        return;
    }
    HookPresent(device);

    static LARGE_INTEGER prev_frame_qpc = {};
    {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        if (prev_frame_qpc.QuadPart) {
            const auto frame_us = QpcToMicroseconds(now.QuadPart - prev_frame_qpc.QuadPart);
            if (frame_us > 0) acc_frame.Record(frame_us);
        }
        prev_frame_qpc = now;
    }

    uint64_t total_update_us = 0, total_draw_us = 0;

    for (const auto* m : GWToolbox::GetAllModules()) {
        const bool has_data = m->last_update_time_us_ || m->last_draw_time_us_ || !m->last_ui_message_times_us_.empty();
        if (!has_data) continue;
        auto found = acc_modules.find(m->Name());
        if (found == acc_modules.end()) found = acc_modules.emplace(m->Name(), ModuleStats{}).first;
        auto& ms = found->second;
        ms.update.Record(m->last_update_time_us_);
        ms.draw.Record(m->last_draw_time_us_);
        for (auto& [msg_id, time_us] : m->last_ui_message_times_us_) {
            if (time_us > 0)
                ms.ui_msgs[msg_id].Record(time_us);
        }
        m->last_ui_message_times_us_.clear();
        total_update_us += m->last_update_time_us_;
        total_draw_us += m->last_draw_time_us_;
    }
    if (total_update_us > 0) acc_tb_update.Record(total_update_us);
    if (total_draw_us > 0) acc_tb_draw.Record(total_draw_us);
    if (present_time_us > 0) acc_present.Record(present_time_us);

    const DWORD now = GetTickCount();
    if (window_start_tick == 0) window_start_tick = now;
    if (now - window_start_tick >= 1000) {
        FlushWindow(now);
        window_start_tick = now;
    }

    ImGui::SetNextWindowSize(ImVec2(560.f, 440.f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        ImGui::End();
        return;
    }

    struct DisplayEntry {
        const char* name;
        const ModuleStats* stats;
    };
    static std::vector<DisplayEntry> entries;
    entries.clear();
    for (const auto& [name, ms] : displayed_modules) {
        entries.push_back({name.c_str(), &ms});
    }

    if (ImGui::BeginTabBar("##perf_tabs")) {

        // ── 选项卡 1：模块（帧概览 + 各模块更新/绘制） ──────────
        if (ImGui::BeginTabItem("模块")) {

            if (displayed_frame.count > 0) {
                const auto& f = displayed_frame;
                const auto& tu = displayed_tb_update;
                const auto& td = displayed_tb_draw;
                const auto& p = displayed_present;
                const float fps = f.Avg() > 0 ? 1000000.f / f.Avg() : 0.f;

                if (ImGui::BeginTable("##frame_timing", 3,
                        ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp)) {
                    ImGui::TableSetupColumn("");
                    ImGui::TableSetupColumn("平均 (微秒)");
                    ImGui::TableSetupColumn("最大 (微秒)");
                    ImGui::TableHeadersRow();

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("帧 (%.1f FPS)", fps);
                    ImGui::TableNextColumn(); ImGui::Text("%llu", f.Avg());
                    ImGui::TableNextColumn(); ImGui::Text("%llu", f.max_us);

                    if (tu.count > 0) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::TextUnformatted("  工具箱更新");
                        ImGui::TableNextColumn(); ImGui::Text("%llu", tu.Avg());
                        ImGui::TableNextColumn(); ImGui::Text("%llu", tu.max_us);
                    }
                    if (td.count > 0) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::TextUnformatted("  工具箱绘制");
                        ImGui::TableNextColumn(); ImGui::Text("%llu", td.Avg());
                        ImGui::TableNextColumn(); ImGui::Text("%llu", td.max_us);
                    }
                    if (p.count > 0) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::TextUnformatted("  Present");
                        ImGui::TableNextColumn(); ImGui::Text("%llu", p.Avg());
                        ImGui::TableNextColumn(); ImGui::Text("%llu", p.max_us);
                    }

                    const uint64_t known_avg = tu.Avg() + td.Avg() + p.Avg();
                    const uint64_t gw_avg = f.Avg() > known_avg ? f.Avg() - known_avg : 0;
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::TextUnformatted("  激战");
                    ImGui::TableNextColumn(); ImGui::Text("~%llu", gw_avg);
                    ImGui::TableNextColumn();

                    ImGui::EndTable();
                }
                ImGui::Separator();
            }

            enum SortColMod : int { MCN, MCUpdMin, MCUpdAvg, MCUpdMax, MCDrawMin, MCDrawAvg, MCDrawMax };

            static std::vector<DisplayEntry> mod_entries;
            mod_entries = entries;

            if (ImGui::BeginTable("##module_timing", 7,
                    ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp
                    | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Sortable | ImGuiTableFlags_SortTristate,
                    ImGui::GetContentRegionAvail())) {
                ImGui::TableSetupColumn("模块",   ImGuiTableColumnFlags_DefaultSort, 0.f, MCN);
                ImGui::TableSetupColumn("更新最小",  ImGuiTableColumnFlags_NoSort,      0.f, MCUpdMin);
                ImGui::TableSetupColumn("更新平均",  0,                                 0.f, MCUpdAvg);
                ImGui::TableSetupColumn("更新最大",  0,                                 0.f, MCUpdMax);
                ImGui::TableSetupColumn("绘制最小", ImGuiTableColumnFlags_NoSort,      0.f, MCDrawMin);
                ImGui::TableSetupColumn("绘制平均", 0,                                 0.f, MCDrawAvg);
                ImGui::TableSetupColumn("绘制最大", 0,                                 0.f, MCDrawMax);
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableHeadersRow();

                if (const auto* ss = ImGui::TableGetSortSpecs(); ss && ss->SpecsCount > 0) {
                    const auto& spec = ss->Specs[0];
                    const bool asc = spec.SortDirection == ImGuiSortDirection_Ascending;
                    std::ranges::sort(mod_entries, [&](const auto& a, const auto& b) {
                        uint64_t va = 0, vb = 0;
                        switch (static_cast<SortColMod>(spec.ColumnUserID)) {
                            case MCN:       return asc ? strcmp(a.name, b.name) < 0 : strcmp(a.name, b.name) > 0;
                            case MCUpdAvg:  va = a.stats->update.Avg(); vb = b.stats->update.Avg(); break;
                            case MCUpdMax:  va = a.stats->update.max_us; vb = b.stats->update.max_us; break;
                            case MCDrawAvg: va = a.stats->draw.Avg(); vb = b.stats->draw.Avg(); break;
                            case MCDrawMax: va = a.stats->draw.max_us; vb = b.stats->draw.max_us; break;
                            default: return false;
                        }
                        return asc ? va < vb : va > vb;
                    });
                }

                // Every module has a row but only a dozen fit on screen; submitting all of them
                // made this window one of the most expensive things it was measuring.
                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(mod_entries.size()));
                while (clipper.Step()) {
                    for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
                        const auto& e = mod_entries[row];
                        const uint64_t total_avg = e.stats->update.Avg() + e.stats->draw.Avg();
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        if (total_avg >= static_cast<uint64_t>(settings.slow_threshold_us))
                            ImGui::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), "%s", e.name);
                        else
                            ImGui::TextUnformatted(e.name);
                        DrawStatsColumns(e.stats->update);
                        DrawStatsColumns(e.stats->draw);
                    }
                }
                ImGui::EndTable();
            }

            ImGui::EndTabItem();
        }

        // ── 选项卡 2：UI 消息（各模块、各消息 ID 的回调耗时） ──────
        if (ImGui::BeginTabItem("UI 消息")) {
            ImGui::TextUnformatted("各模块、各消息 ID 的回调耗时（微秒，最近 5 秒）。");
            ImGui::Separator();

            struct UIMessageEntry {
                const char* module_name;
                uint32_t msg_id;
                Stats stats;
            };

            enum SortColUI : int { UCMOD, UCMSG, UCMin, UCAvg, UCMax, UCCount };

            static std::vector<UIMessageEntry> ui_entries;
            ui_entries.clear();
            for (const auto& [name, ms] : displayed_modules) {
                for (const auto& [msg_id, s] : ms.ui_msgs) {
                    if (s.count > 0)
                        ui_entries.push_back({name.c_str(), msg_id, s});
                }
            }

            if (ui_entries.empty()) {
                ImGui::TextDisabled("尚无 UI 消息回调数据。消息触发后才会显示数据。");
            }
            else {
                if (ImGui::BeginTable("##ui_msg_timing", 6,
                        ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp
                        | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Sortable | ImGuiTableFlags_SortTristate,
                        ImGui::GetContentRegionAvail())) {
                    ImGui::TableSetupColumn("模块",  ImGuiTableColumnFlags_DefaultSort, 0.f, UCMOD);
                    ImGui::TableSetupColumn("消息 ID",  0,                                 0.f, UCMSG);
                    ImGui::TableSetupColumn("最小",     ImGuiTableColumnFlags_NoSort,      0.f, UCMin);
                    ImGui::TableSetupColumn("平均",     0,                                 0.f, UCAvg);
                    ImGui::TableSetupColumn("最大",     0,                                 0.f, UCMax);
                    ImGui::TableSetupColumn("调用次数",   0,                                 0.f, UCCount);
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableHeadersRow();

                    if (const auto* ss = ImGui::TableGetSortSpecs(); ss && ss->SpecsCount > 0) {
                        const auto& spec = ss->Specs[0];
                        const bool asc = spec.SortDirection == ImGuiSortDirection_Ascending;
                        std::ranges::sort(ui_entries, [&](const auto& a, const auto& b) {
                            uint64_t va = 0, vb = 0;
                            switch (static_cast<SortColUI>(spec.ColumnUserID)) {
                                case UCMOD:   return asc ? strcmp(a.module_name, b.module_name) < 0 : strcmp(a.module_name, b.module_name) > 0;
                                case UCMSG:   va = a.msg_id; vb = b.msg_id; break;
                                case UCAvg:   va = a.stats.Avg(); vb = b.stats.Avg(); break;
                                case UCMax:   va = a.stats.max_us; vb = b.stats.max_us; break;
                                case UCCount: va = a.stats.count; vb = b.stats.count; break;
                                default: return false;
                            }
                            return asc ? va < vb : va > vb;
                        });
                    }

                    ImGuiListClipper clipper;
                    clipper.Begin(static_cast<int>(ui_entries.size()));
                    while (clipper.Step()) {
                        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
                            const auto& e = ui_entries[row];
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            if (e.stats.Avg() >= static_cast<uint64_t>(settings.slow_threshold_us))
                                ImGui::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), "%s", e.module_name);
                            else
                                ImGui::TextUnformatted(e.module_name);
                            ImGui::TableNextColumn();
                            ImGui::Text("0x%04X", e.msg_id);
                            DrawStatsColumns(e.stats);
                            ImGui::TableNextColumn();
                            ImGui::Text("%u", e.stats.count);
                        }
                    }
                    ImGui::EndTable();
                }
            }

            ImGui::EndTabItem();
        }

        // ── 选项卡 3：比较（对比两个导出的 CSV，例如 MSVC vs clang） ──────────
        if (ImGui::BeginTabItem("比较")) {
            static std::vector<std::filesystem::path> csv_files;
            static std::vector<std::string> csv_names;
            static int sel_a = -1, sel_b = -1;
            static std::map<std::string, Stats> agg_a, agg_b;
            static std::string label_a, label_b;

            auto refresh_files = [] {
                csv_files.clear();
                csv_names.clear();
                std::error_code ec;
                for (const auto& entry : std::filesystem::directory_iterator(Resources::GetSettingsFolderPath(), ec)) {
                    if (!entry.is_regular_file(ec)) continue;
                    const auto& p = entry.path();
                    if (p.extension() == L".csv" && p.filename().wstring().starts_with(L"performance_log")) {
                        csv_files.push_back(p);
                        csv_names.push_back(p.filename().string());
                    }
                }
            };
            if (csv_files.empty()) refresh_files();

            ImGui::TextWrapped("比较两个导出 CSV 中各模块的耗时。%% 表示 B 相对于 A 的变化；负数（绿色）表示 B 更快。");
            if (ImGui::Button("刷新文件列表")) refresh_files();
            ImGui::SameLine();
            ImGui::TextDisabled("（找到 %d 个）", static_cast<int>(csv_files.size()));

            auto file_combo = [](const char* label, int& sel) {
                const char* preview = (sel >= 0 && sel < static_cast<int>(csv_names.size())) ? csv_names[sel].c_str() : "(无)";
                if (ImGui::BeginCombo(label, preview)) {
                    for (int i = 0; i < static_cast<int>(csv_names.size()); i++) {
                        const bool selected = sel == i;
                        if (ImGui::Selectable(csv_names[i].c_str(), selected)) sel = i;
                        if (selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            };
            file_combo("A##cmp_a", sel_a);
            file_combo("B##cmp_b", sel_b);

            if (ImGui::Button("加载 / 比较")) {
                agg_a.clear();
                agg_b.clear();
                label_a.clear();
                label_b.clear();
                if (sel_a >= 0 && sel_a < static_cast<int>(csv_files.size())) {
                    agg_a = LoadCsvAggregate(csv_files[sel_a]);
                    label_a = csv_names[sel_a];
                }
                if (sel_b >= 0 && sel_b < static_cast<int>(csv_files.size())) {
                    agg_b = LoadCsvAggregate(csv_files[sel_b]);
                    label_b = csv_names[sel_b];
                }
            }

            if (!agg_a.empty() || !agg_b.empty()) {
                ImGui::TextDisabled("A = %s    B = %s", label_a.empty() ? "-" : label_a.c_str(), label_b.empty() ? "-" : label_b.c_str());

                std::set<std::string> keys;
                for (const auto& [k, s] : agg_a) keys.insert(k);
                for (const auto& [k, s] : agg_b) keys.insert(k);

                if (ImGui::BeginTable("##cmp_table", 6,
                        ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY,
                        ImGui::GetContentRegionAvail())) {
                    ImGui::TableSetupColumn("分类/名称/指标");
                    ImGui::TableSetupColumn("A 平均");
                    ImGui::TableSetupColumn("B 平均");
                    ImGui::TableSetupColumn("平均 %");
                    ImGui::TableSetupColumn("A 最大");
                    ImGui::TableSetupColumn("B 最大");
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableHeadersRow();

                    for (const auto& key : keys) {
                        const auto it_a = agg_a.find(key);
                        const auto it_b = agg_b.find(key);
                        const uint64_t a_avg = it_a != agg_a.end() ? it_a->second.Avg() : 0;
                        const uint64_t b_avg = it_b != agg_b.end() ? it_b->second.Avg() : 0;
                        const uint64_t a_max = it_a != agg_a.end() ? it_a->second.max_us : 0;
                        const uint64_t b_max = it_b != agg_b.end() ? it_b->second.max_us : 0;

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::TextUnformatted(key.c_str());
                        ImGui::TableNextColumn(); ImGui::Text("%llu", a_avg);
                        ImGui::TableNextColumn(); ImGui::Text("%llu", b_avg);
                        ImGui::TableNextColumn();
                        if (a_avg > 0 && it_b != agg_b.end()) {
                            const double pct = (static_cast<double>(b_avg) - static_cast<double>(a_avg)) / static_cast<double>(a_avg) * 100.0;
                            const ImVec4 col = pct < 0 ? ImVec4(0.55f, 1.f, 0.55f, 1.f) : ImVec4(1.f, 0.45f, 0.45f, 1.f);
                            ImGui::TextColored(col, "%+.1f%%", pct);
                        }
                        else {
                            ImGui::TextUnformatted("-");
                        }
                        ImGui::TableNextColumn(); ImGui::Text("%llu", a_max);
                        ImGui::TableNextColumn(); ImGui::Text("%llu", b_max);
                    }
                    ImGui::EndTable();
                }
            }

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

void PerformanceWindow::Initialize()
{
    ToolboxWindow::Initialize();
    SettingsRegistry::Register(this, settings);
}

void PerformanceWindow::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxWindow::LoadSettings(doc, legacy);
    doc.GetStruct(Name(), settings);
}

void PerformanceWindow::SaveSettings(SettingsDoc& doc)
{
    ToolboxWindow::SaveSettings(doc);
    doc.SetStruct(Name(), settings);
}

void PerformanceWindow::Terminate()
{
    UnhookPresent();
    ToolboxWindow::Terminate();
}

void PerformanceWindow::DrawSettingsInternal()
{
    ImGui::InputInt("慢模块阈值（微秒）", &settings.slow_threshold_us, 100, 1000);
    settings.slow_threshold_us = std::max(settings.slow_threshold_us, 0);

    ImGui::Checkbox("将计时数据流式写入 CSV", &settings.stream_to_csv);
    ImGui::ShowHelp("性能窗口打开时，每秒计时数据追加到设置文件夹中的 performance_log_<编译器>.csv。\n"
                    "每个编译器写入自己的文件；在比较选项卡中加载两个文件可对比不同编译器（如 MSVC vs clang）的构建性能。");
}
