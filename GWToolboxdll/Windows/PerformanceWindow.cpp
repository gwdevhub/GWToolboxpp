#include "stdafx.h"

#include <GWCA/Utilities/Hooker.h>

#include <GWToolbox.h>
#include <Defines.h>
#include <Windows/PerformanceWindow.h>

namespace {
    int slow_threshold_us = 1000;

    // QPC helper
    uint64_t QpcToMicroseconds(LONGLONG ticks)
    {
        static LARGE_INTEGER freq = [] { LARGE_INTEGER f; QueryPerformanceFrequency(&f); return f; }();
        return static_cast<uint64_t>(ticks * 1000000 / freq.QuadPart);
    }

    // Present hook
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
    };

    // Ring buffer of 1-second snapshots
    Stats hist_frame[WINDOW_SECONDS], hist_tb_update[WINDOW_SECONDS], hist_tb_draw[WINDOW_SECONDS], hist_present[WINDOW_SECONDS];
    std::map<std::string, ModuleStats> hist_modules[WINDOW_SECONDS];
    int hist_index = 0;

    // Displayed stats (merged from ring buffer)
    Stats displayed_frame, displayed_tb_update, displayed_tb_draw, displayed_present;
    std::map<std::string, ModuleStats> displayed_modules;

    // Accumulating stats (current 1s window)
    Stats acc_frame, acc_tb_update, acc_tb_draw, acc_present;
    std::map<std::string, ModuleStats> acc_modules;
    DWORD window_start_tick = 0;

    void FlushWindow()
    {
        // Store current 1s snapshot into ring buffer
        hist_frame[hist_index] = acc_frame;
        hist_tb_update[hist_index] = acc_tb_update;
        hist_tb_draw[hist_index] = acc_tb_draw;
        hist_present[hist_index] = acc_present;
        hist_modules[hist_index] = acc_modules;
        hist_index = (hist_index + 1) % WINDOW_SECONDS;

        // Merge all snapshots in the ring buffer
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
            }
        }

        // Reset accumulators for next 1s window
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

}

void PerformanceWindow::Draw(IDirect3DDevice9* device)
{
    GWToolbox::SetProfilingEnabled(visible);
    if (!visible) {
        UnhookPresent();
        return;
    }
    HookPresent(device);

    // Frame period tracking
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

    // Accumulate per-module times
    uint64_t total_update_us = 0, total_draw_us = 0;

    for (const auto* m : GWToolbox::GetAllModules()) {
        if (m->last_update_time_us_ == 0 && m->last_draw_time_us_ == 0) continue;
        auto& ms = acc_modules[m->Name()];
        ms.update.Record(m->last_update_time_us_);
        ms.draw.Record(m->last_draw_time_us_);
        total_update_us += m->last_update_time_us_;
        total_draw_us += m->last_draw_time_us_;
    }
    if (total_update_us > 0) acc_tb_update.Record(total_update_us);
    if (total_draw_us > 0) acc_tb_draw.Record(total_draw_us);
    if (present_time_us > 0) acc_present.Record(present_time_us);

    // Flush every 1s
    const DWORD now = GetTickCount();
    if (window_start_tick == 0) window_start_tick = now;
    if (now - window_start_tick >= 1000) {
        FlushWindow();
        window_start_tick = now;
    }

    ImGui::SetNextWindowSize(ImVec2(500.f, 400.f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        ImGui::End();
        return;
    }

    // Frame overview (single thread: update → render → present)
    if (displayed_frame.count > 0) {
        const auto& f = displayed_frame;
        const auto& tu = displayed_tb_update;
        const auto& td = displayed_tb_draw;
        const auto& p = displayed_present;

        const float fps = f.Avg() > 0 ? 1000000.f / f.Avg() : 0.f;

        if (ImGui::BeginTable("##frame_timing", 3,
                ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("");
            ImGui::TableSetupColumn("avg (us)");
            ImGui::TableSetupColumn("max (us)");
            ImGui::TableHeadersRow();

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Frame (%.1f fps)", fps);
            ImGui::TableNextColumn(); ImGui::Text("%llu", f.Avg());
            ImGui::TableNextColumn(); ImGui::Text("%llu", f.max_us);

            if (tu.count > 0) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::TextUnformatted("  TB Update");
                ImGui::TableNextColumn(); ImGui::Text("%llu", tu.Avg());
                ImGui::TableNextColumn(); ImGui::Text("%llu", tu.max_us);
            }

            if (td.count > 0) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::TextUnformatted("  TB Draw");
                ImGui::TableNextColumn(); ImGui::Text("%llu", td.Avg());
                ImGui::TableNextColumn(); ImGui::Text("%llu", td.max_us);
            }

            if (p.count > 0) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::TextUnformatted("  Present");
                ImGui::TableNextColumn(); ImGui::Text("%llu", p.Avg());
                ImGui::TableNextColumn(); ImGui::Text("%llu", p.max_us);
            }

            const uint64_t tb_total_avg = tu.Avg() + td.Avg();
            const uint64_t known_avg = tb_total_avg + p.Avg();
            const uint64_t gw_avg = f.Avg() > known_avg ? f.Avg() - known_avg : 0;
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextUnformatted("  GW");
            ImGui::TableNextColumn(); ImGui::Text("~%llu", gw_avg);
            ImGui::TableNextColumn();

            ImGui::EndTable();
        }
    }

    ImGui::Separator();

    // Per-module breakdown
    enum SortCol : int { Col_Name, Col_UpdMin, Col_UpdAvg, Col_UpdMax, Col_DrawMin, Col_DrawAvg, Col_DrawMax };

    struct DisplayEntry {
        const char* name;
        ModuleStats stats;
    };
    static std::vector<DisplayEntry> entries;
    entries.clear();

    for (const auto& [name, ms] : displayed_modules) {
        entries.push_back({name.c_str(), ms});
    }

    if (ImGui::BeginTable("##module_timing", 7,
            ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp
            | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Sortable | ImGuiTableFlags_SortTristate,
            ImGui::GetContentRegionAvail())) {
        ImGui::TableSetupColumn("Module",      ImGuiTableColumnFlags_DefaultSort, 0.f, Col_Name);
        ImGui::TableSetupColumn("Upd min",     ImGuiTableColumnFlags_NoSort,      0.f, Col_UpdMin);
        ImGui::TableSetupColumn("Upd avg",     0,                                 0.f, Col_UpdAvg);
        ImGui::TableSetupColumn("Upd max",     0,                                 0.f, Col_UpdMax);
        ImGui::TableSetupColumn("Draw min",    ImGuiTableColumnFlags_NoSort,      0.f, Col_DrawMin);
        ImGui::TableSetupColumn("Draw avg",    0,                                 0.f, Col_DrawAvg);
        ImGui::TableSetupColumn("Draw max",    0,                                 0.f, Col_DrawMax);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        if (const auto* sort_specs = ImGui::TableGetSortSpecs()) {
            if (sort_specs->SpecsCount > 0) {
                const auto& spec = sort_specs->Specs[0];
                const bool ascending = spec.SortDirection == ImGuiSortDirection_Ascending;
                std::ranges::sort(entries, [&](const auto& a, const auto& b) {
                    uint64_t va = 0, vb = 0;
                    switch (static_cast<SortCol>(spec.ColumnUserID)) {
                        case Col_Name:    return ascending ? strcmp(a.name, b.name) < 0 : strcmp(a.name, b.name) > 0;
                        case Col_UpdAvg:  va = a.stats.update.Avg(); vb = b.stats.update.Avg(); break;
                        case Col_UpdMax:  va = a.stats.update.max_us; vb = b.stats.update.max_us; break;
                        case Col_DrawAvg: va = a.stats.draw.Avg(); vb = b.stats.draw.Avg(); break;
                        case Col_DrawMax: va = a.stats.draw.max_us; vb = b.stats.draw.max_us; break;
                        default: return false;
                    }
                    return ascending ? va < vb : va > vb;
                });
            }
        }

        for (const auto& e : entries) {
            const uint64_t total_avg = e.stats.update.Avg() + e.stats.draw.Avg();
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (total_avg >= static_cast<uint64_t>(slow_threshold_us))
                ImGui::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), "%s", e.name);
            else
                ImGui::TextUnformatted(e.name);
            DrawStatsColumns(e.stats.update);
            DrawStatsColumns(e.stats.draw);
        }
        ImGui::EndTable();
    }

    ImGui::End();
}

void PerformanceWindow::Terminate()
{
    UnhookPresent();
    ToolboxWindow::Terminate();
}

void PerformanceWindow::LoadSettings(ToolboxIni* ini)
{
    ToolboxWindow::LoadSettings(ini);
    LOAD_UINT(slow_threshold_us);
}

void PerformanceWindow::SaveSettings(ToolboxIni* ini)
{
    ToolboxWindow::SaveSettings(ini);
    SAVE_UINT(slow_threshold_us);
}

void PerformanceWindow::DrawSettingsInternal()
{
    ImGui::InputInt("Slow module threshold (us)", &slow_threshold_us, 100, 1000);
    if (slow_threshold_us < 0) slow_threshold_us = 0;
}
