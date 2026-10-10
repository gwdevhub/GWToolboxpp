#include "stdafx.h"

#include "SplitsGoalListWindow.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/Context/WorldContext.h>

#include <GWCA/GameContainers/Array.h>

#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Quest.h>
#include <GWCA/GameEntities/Title.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <ImGuiAddons.h>
#include <Modules/Resources.h>
#include <Utils/EncString.h>
#include <Utils/GuiUtils.h>
#include <Utils/TextUtils.h>
#include <Utils/ToolboxUtils.h>
#include <Windows/SettingsWindow.h>
#include <Windows/Splits/GoalClock.h>
#include <Windows/Splits/GoalEntry.h>
#include <Windows/Splits/GoalList.h>
#include <Windows/Splits/RunHistory.h>
#include <Windows/Splits/SCPresets.h>
#include <Windows/Splits/SplitsProfile.h>
#include <Windows/SplitsWindow.h>

#include <algorithm>
#include <vector>
#include <utility>
#include <set>
#include <map>
#include <functional>
#include <memory>
#include <span>

// Editor/picker state for the settings UI; lives here rather than in the class, like other toolbox windows.
namespace {
    char edit_label[128]     = {};
    int  edit_trigger_type   = 0; // -1 = Header, -2 = Quest pair
    int  edit_level          = 1;
    char list_name_buf[64]   = {};

    char list_picker_filter_buf[128] = {};
    char load_list_filter_buf[128]   = {};
    char sc_preset_filter_buf[128]   = {};

    std::set<int>          batch_mis_checked;
    std::set<int>          batch_bon_checked;
    bool                   batch_hm = false;

    std::map<int, uint8_t> batch_exp_checked;
    // Running: add in click order (a route).
    std::vector<std::pair<int, uint8_t>> batch_exp_order;
    char                   exp_filter_buf[128] = {};

    std::map<int, uint8_t> batch_town_checked;
    std::vector<std::pair<int, uint8_t>> batch_town_order;
    char                   town_filter_buf[128] = {};

    std::set<int> batch_dungeon_checked;
    char          dungeon_filter_buf[128] = {};

    // Key by checkpoint index: Urgoz/Deep param1 can collide.
    std::set<int> batch_fow_checked;
    std::set<int> batch_uw_checked;
    std::set<int> batch_urgoz_checked;
    std::set<int> batch_deep_checked;

    int  edit_title_id       = 0xff;
    int  edit_title_rank     = -1; // 1-based rank; -1 = unset (max)
    char title_filter_buf[128] = {};

    int  edit_quest_id = 0;
    int  edit_skill_id = 0;

    char edit_mob_name[128] = {};
    int  edit_mob_kill_count = 1;

    // Area MapID as int; -1 = closed.
    int  sc_creating_new_area       = -1;
    char sc_create_new_name_buf[64] = {};

    // Cache so idle viewer not re-read disk each frame.
    bool                    show_run_history_viewer  = false;
    std::vector<RecentRun>  run_history_cache;
    bool                    run_history_cache_dirty  = true;
    std::set<int>           run_history_selected_attempts;
}

static void FormatTime(char* buf, int bufsz, double seconds)
{
    const int h  = static_cast<int>(seconds) / 3600;
    const int m  = (static_cast<int>(seconds) % 3600) / 60;
    const int s  = static_cast<int>(seconds) % 60;
    const int cs = static_cast<int>(seconds * 100.0) % 100;
    if (h > 0)
        snprintf(buf, bufsz, "%d:%02d:%02d.%02d", h, m, s, cs);
    else
        snprintf(buf, bufsz, "%02d:%02d.%02d", m, s, cs);
}

// Empty for runs saved without a start time (TimeToString(0) would print "now").
static void FormatDate(char* buf, size_t bufsz, int64_t utc)
{
    const std::string s = utc > 0 ? TextUtils::TimeToString(static_cast<time_t>(utc)) : std::string{};
    snprintf(buf, bufsz, "%s", s.c_str());
}

// Measure once per tier; widen only when clock need the digit.
static float TimeColumnWidth(int tier, bool with_sign)
{
    static float w[2][3] = {{-1.f, -1.f, -1.f}, {-1.f, -1.f, -1.f}};
    float& slot = w[with_sign ? 1 : 0][tier];
    if (slot < 0.f) {
        static const char* plain[3] = {"88:88.88", "8:88:88.88", "88:88:88.88"};
        static const char* signed_[3] = {"+88:88.88", "+8:88:88.88", "+88:88:88.88"};
        slot = ImGui::CalcTextSize(with_sign ? signed_[tier] : plain[tier]).x + 12.f * ImGui::FontScale();
    }
    return slot;
}

static void DrawPBDelta(double actual, double pb_split, ImVec4 col_ahead, ImVec4 col_behind)
{
    if (std::isnan(pb_split) || std::isnan(actual)) return;
    const double delta = actual - pb_split;
    char dbuf[32];
    FormatTime(dbuf, sizeof(dbuf), std::abs(delta));
    ImGui::TextColored(delta < 0.0 ? col_ahead : col_behind, delta < 0.0 ? "-%s" : "+%s", dbuf);
}

static double SegmentAt(const std::vector<double>& cumulative, int idx)
{
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    if (idx < 0 || idx >= static_cast<int>(cumulative.size())) return nan;
    const double cur = cumulative[static_cast<size_t>(idx)];
    if (std::isnan(cur)) return nan;
    if (idx == 0) return cur;
    const double prev = cumulative[static_cast<size_t>(idx - 1)];
    return std::isnan(prev) ? nan : cur - prev;
}


// A goal's own duration when its start is known (parallel/out-of-order lists), else the relay gap.
static double DurationAt(const ComparisonSplits& cmp, bool gt, int idx)
{
    const auto& ends   = gt ? cmp.game : cmp.real;
    const auto& starts = gt ? cmp.start_game : cmp.start_real;
    if (idx >= 0 && idx < static_cast<int>(ends.size()) && idx < static_cast<int>(starts.size())) {
        const double end = ends[static_cast<size_t>(idx)], start = starts[static_cast<size_t>(idx)];
        if (!std::isnan(end) && !std::isnan(start)) return end - start;
    }
    return SegmentAt(ends, idx);
}

// attempt_real/attempt_game only for Sum of Best; else nullptr.
static void DrawSplitsBreakdownRows(const GoalList& list, bool gt, const ComparisonSplits& cmp,
                                     const std::vector<int>* attempt_real, const std::vector<int>* attempt_game)
{
    int idx = 0;
    for (const auto& g : list.goals) {
        if (g.is_header) continue;
        if (idx >= static_cast<int>(cmp.real.size())) break;
        const double seg_real = DurationAt(cmp, false, idx);
        if (std::isnan(seg_real)) { ImGui::TextDisabled("%s: never reached", g.label.c_str()); ++idx; continue; }
        const double seg_game = DurationAt(cmp, true, idx);
        const bool have_game  = !std::isnan(seg_game);
        char tbuf[32];
        FormatTime(tbuf, sizeof(tbuf), (gt && have_game) ? seg_game : seg_real);
        int attempt = 0;
        if (gt && attempt_game && idx < static_cast<int>(attempt_game->size()))
            attempt = (*attempt_game)[static_cast<size_t>(idx)];
        else if (attempt_real && idx < static_cast<int>(attempt_real->size()))
            attempt = (*attempt_real)[static_cast<size_t>(idx)];
        if (attempt > 0)
            ImGui::Text("%s: %s (Attempt #%d)", g.label.c_str(), tbuf, attempt);
        else
            ImGui::Text("%s: %s", g.label.c_str(), tbuf);
        ++idx;
    }
}

// Per-zone time = latest end - earliest start of the goals directly under a header, so it doesn't
// depend on run order (DoA rotations). Failed runs count for any zone they fully completed.
static void DrawZoneTimes(const GoalList& list, const std::vector<RecentRun>& runs, const bool gt)
{
    struct Zone { std::string label; int first = 0; int count = 0; };
    std::vector<Zone> zones;
    int idx = 0;
    for (const auto& g : list.goals) {
        if (g.is_header) { zones.push_back({g.label, idx, 0}); continue; }
        if (!zones.empty()) ++zones.back().count;
        ++idx;
    }
    std::erase_if(zones, [](const Zone& z) { return z.count == 0; });
    if (zones.empty() || !ImGui::TreeNode("zone_times", "Zone times")) return;

    if (ImGui::BeginTable("##zonetimes", 4, ImGuiTableFlags_None)) {
        ImGui::TableSetupColumn("zone", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("best", ImGuiTableColumnFlags_WidthFixed, 80.f * ImGui::FontScale());
        ImGui::TableSetupColumn("avg",  ImGuiTableColumnFlags_WidthFixed, 80.f * ImGui::FontScale());
        ImGui::TableSetupColumn("runs", ImGuiTableColumnFlags_WidthFixed, 50.f * ImGui::FontScale());
        ImGui::TableHeadersRow();
        for (const auto& z : zones) {
            double best = std::numeric_limits<double>::infinity(), sum = 0.0;
            int n = 0;
            for (const auto& run : runs) {
                if (z.first + z.count > static_cast<int>(run.goals.size())) continue;
                double start = std::numeric_limits<double>::infinity(), end = 0.0;
                bool complete = true;
                for (int i = z.first; i < z.first + z.count; ++i) {
                    const auto& rg = run.goals[static_cast<size_t>(i)];
                    const double s = gt ? rg.start_game_time : rg.start_real_time;
                    if (!rg.completed || s < 0.0) { complete = false; break; }
                    start = std::min(start, s);
                    end   = std::max(end, gt ? rg.game_time : rg.real_time);
                }
                if (!complete) continue;
                best = std::min(best, end - start);
                sum += end - start;
                ++n;
            }
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(z.label.c_str());
            if (n == 0) { ImGui::TableSetColumnIndex(1); ImGui::TextDisabled("no full clear yet"); continue; }
            char buf[32];
            FormatTime(buf, sizeof(buf), best);
            ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(buf);
            FormatTime(buf, sizeof(buf), sum / n);
            ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(buf);
            ImGui::TableSetColumnIndex(3); ImGui::Text("%d", n);
        }
        ImGui::EndTable();
    }
    ImGui::TreePop();
}

// sob/pb needed only when show_gold_delta.
static void DrawAttemptGoalsTable(SplitsWindow& splits, const RecentRun& run, bool gt, bool show_gold_delta,
                                   const std::vector<double>* sob, const ComparisonSplits* pb,
                                   ImVec4 gold_col, ImVec4 ahead_col, ImVec4 behind_col)
{
    const int col_count = show_gold_delta ? 4 : 2;
    if (!ImGui::BeginTable("##attemptgoals", col_count, ImGuiTableFlags_None)) return;
    ImGui::TableSetupColumn("name", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("time", ImGuiTableColumnFlags_WidthFixed, 80.f * ImGui::FontScale());
    if (show_gold_delta) {
        ImGui::TableSetupColumn("gold",  ImGuiTableColumnFlags_WidthFixed, 90.f * ImGui::FontScale());
        ImGui::TableSetupColumn("delta", ImGuiTableColumnFlags_WidthFixed, 80.f * ImGui::FontScale());
    }
    double prev = 0.0;
    int idx = 0;
    for (const auto& g : run.goals) {
        const double cur = gt ? g.game_time : g.real_time;
        const ImVec4 col = g.completed
            ? ImGui::ColorConvertU32ToFloat4(splits.ColorCompleted())
            : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
        char gbuf[32];
        // Never reached: the run stored no time, so don't print a fake 00:00.00.
        if (!g.completed && cur <= 0.0) snprintf(gbuf, sizeof(gbuf), "--:--");
        else FormatTime(gbuf, sizeof(gbuf), cur);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextColored(col, "%s", g.label.c_str());
        ImGui::TableSetColumnIndex(1);
        ImGui::TextColored(col, "%s", gbuf);

        if (show_gold_delta && g.completed && sob && pb) {
            const double own_start = gt ? g.start_game_time : g.start_real_time;
            const double seg       = cur - (own_start >= 0.0 ? own_start : prev);
            const double gold_seg  = SegmentAt(*sob, idx);
            const double pb_seg    = DurationAt(*pb, gt, idx);
            if (!std::isnan(gold_seg)) {
                char goldbuf[32];
                FormatTime(goldbuf, sizeof(goldbuf), gold_seg);
                ImGui::TableSetColumnIndex(2);
                ImGui::TextColored(gold_col, "Gold %s", goldbuf);
            }
            if (!std::isnan(pb_seg)) {
                ImGui::TableSetColumnIndex(3);
                DrawPBDelta(seg, pb_seg, ahead_col, behind_col);
            }
        }
        prev = cur;
        ++idx;
    }
    ImGui::EndTable();
}

static void SetCheckbox(const char* label, std::set<int>& set, int id)
{
    bool on = set.contains(id);
    if (!ImGui::Checkbox(label, &on)) return;
    if (on) set.insert(id);
    else set.erase(id);
}

struct SearchComboEntry {
    std::string label;
    bool        selected = false;
};

// filter_buf caller-owned; cleared when popup closes.
static void SearchableCombo(const char* combo_id, const char* preview,
                             char* filter_buf, size_t filter_buf_size,
                             const std::vector<SearchComboEntry>& entries,
                             const std::function<void(size_t)>& on_select)
{
    ImGui::SetNextItemWidth(-1.f);
    if (!ImGui::BeginCombo(combo_id, preview)) {
        filter_buf[0] = '\0';
        return;
    }
    if (ImGui::IsWindowAppearing())
        ImGui::SetKeyboardFocusHere();
    ImGui::SetNextItemWidth(-1.f);
    ImGui::InputTextWithHint("##filter", "Search...", filter_buf, filter_buf_size);
    ImGui::Separator();
    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& e = entries[i];
        if (filter_buf[0] != '\0' && !TextUtils::CaseInsensitiveContains(e.label, filter_buf))
            continue;
        if (ImGui::Selectable(e.label.c_str(), e.selected)) {
            on_select(i);
            ImGui::CloseCurrentPopup();
        }
        if (e.selected)
            ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
}

// =============================================================================
// LIVE WINDOW DRAW
// =============================================================================

void SplitsGoalListWindow::Draw(SplitsWindow& splits)
{
    // Own window, drawn here so it stay open after Settings closes.
    DrawRunHistoryViewer(splits);

    const bool is_open = ImGui::Begin(splits.Name(), splits.GetVisiblePtr(), splits.GetWinFlags());

    if (!is_open) {
        ImGui::End();
        return;
    }

    const GoalClock& clock = splits.Clock();
    const GoalList*  list  = splits.List();

    {
        const SplitsProfile& hp = splits.ActiveProfile();
        using TD = SplitsProfile::TimeDisplay;
        const auto td = hp.time_display;
        const float avail = ImGui::GetContentRegionAvail().x;
        const float min_x = ImGui::GetCursorPosX();

        char tbuf_r[32], tbuf_g[32];
        FormatTime(tbuf_r, sizeof(tbuf_r), clock.RealTime());
        FormatTime(tbuf_g, sizeof(tbuf_g), clock.GameTime());

        float total_w;
        if (td == TD::Both) {
            total_w = ImGui::CalcTextSize("Real: ").x + ImGui::CalcTextSize(tbuf_r).x
                    + ImGui::CalcTextSize("  Game: ").x + ImGui::CalcTextSize(tbuf_g).x;
        } else {
            const bool gt = (td == TD::Game);
            const char* lbl = gt ? "Game: " : "Real: ";
            total_w = ImGui::CalcTextSize(lbl).x + ImGui::CalcTextSize(gt ? tbuf_g : tbuf_r).x;
        }

        // Here, not at buttons: clock text clamps against it.
        const bool   running   = splits.Clock().IsRunning();
        const char*  lbl0      = running ? "Pause" : "Start";
        const float  fp_x      = ImGui::GetStyle().FramePadding.x;
        const float  sp        = ImGui::GetStyle().ItemSpacing.x;
        const float  bw0       = ImGui::CalcTextSize(lbl0).x         + fp_x * 2.f;
        const float  bw1       = ImGui::CalcTextSize("Reset").x      + fp_x * 2.f;
        const float  bw2       = ImGui::CalcTextSize("Split").x      + fp_x * 2.f;
        const float  bwg       = ImGui::CalcTextSize(ICON_FA_COGS).x + fp_x * 2.f;
        const float  buttons_w = bw0 + bw1 + bw2 + bwg + sp * 3.f;
        const float  button_x  = min_x + avail - buttons_w;

        const float start_x = min_x + (avail - total_w) * 0.5f;
        float clamped_start_x = start_x < min_x ? min_x : start_x;
        // Keep clock text off buttons when window narrow.
        const float max_start_x = button_x - total_w - sp;
        if (clamped_start_x > max_start_x) clamped_start_x = max_start_x;
        if (clamped_start_x < min_x)       clamped_start_x = min_x;

        if (splits.NuzlockePointsEnabled()) {
            ImGui::SetCursorPosX(min_x);
            ImGui::TextColored({0.6f, 0.85f, 1.f, 1.f}, "Points: %d", splits.NuzlockeTotalPoints());
            ImGui::SameLine(0, 0);
        }

        if (hp.show_paused_time) {
            char pbuf[32]; FormatTime(pbuf, sizeof(pbuf), splits.TotalPausedReal());
            char ptext[48]; snprintf(ptext, sizeof(ptext), "Paused: %s", pbuf);
            const float pause_w = ImGui::CalcTextSize(ptext).x;
            const float pause_x = clamped_start_x - 12.f * ImGui::FontScale() - pause_w;
            ImGui::SetCursorPosX(pause_x > min_x ? pause_x : min_x);
            ImGui::TextColored({1.f, 0.8f, 0.3f, 1.f}, "%s", ptext);
            ImGui::SameLine(0, 0);
        }

        const ImVec4 real_col = ImGui::ColorConvertU32ToFloat4(splits.ColorRealTime());
        const ImVec4 game_col = ImGui::ColorConvertU32ToFloat4(splits.ColorGameTime());
        ImGui::SetCursorPosX(clamped_start_x);
        if (td == TD::Both) {
            ImGui::TextColored(real_col, "Real: %s", tbuf_r);
            ImGui::SameLine(0, 0);
            ImGui::TextColored(game_col, "  Game: %s", tbuf_g);
        } else {
            const bool gt = (td == TD::Game);
            ImGui::TextColored(gt ? game_col : real_col, gt ? "Game: %s" : "Real: %s", gt ? tbuf_g : tbuf_r);
        }

        ImGui::SameLine(button_x);
        if (ImGui::Button(lbl0,          {bw0, 0})) splits.StartRun();
        ImGui::SameLine(0, sp);
        if (ImGui::Button("Reset",       {bw1, 0})) splits.ResetRun();
        ImGui::SameLine(0, sp);
        if (ImGui::Button("Split",       {bw2, 0})) splits.TriggerManualSplit();
        ImGui::SameLine(0, sp);
        if (ImGui::Button(ICON_FA_COGS,  {bwg, 0}))
            SettingsWindow::Instance().NavigateToSection(splits.SettingsName());
    }

    ImGui::Separator();

    auto status_banner = [](const char* msg, const ImVec4& col) {
        const float x = ImGui::GetCursorPosX();
        ImGui::SetCursorPosX(std::max(x, x + (ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(msg).x) * 0.5f));
        ImGui::TextColored(col, "%s", msg);
        ImGui::Separator();
    };
    if (splits.RunComplete()) status_banner("Run complete!", {0.4f, 1.f, 0.4f, 1.f});
    if (splits.RunFailed())   status_banner("Run failed", ImGui::ColorConvertU32ToFloat4(splits.ColorPbBehind()));

    // Only before run starts: no list switch mid-run.
    if (splits.Clock().RealTime() == 0.0) {
        if (splits.ActiveProfile().dynamic_by_default) {
            DrawSCLoadCombo(splits);
            DrawSCCreateVariantModal(splits);
        } else {
            const char* current = (list && !list->name.empty()) ? list->name.c_str() : "(no list)";
            const auto& saved = splits.GetSavedLists();
            std::vector<SearchComboEntry> entries;
            entries.reserve(saved.size());
            for (const auto& [name, path] : saved)
                entries.push_back({ name, list && list->name == name });
            SearchableCombo("##list_picker", current, list_picker_filter_buf, sizeof(list_picker_filter_buf),
                            entries, [&](const size_t i) { splits.LoadActiveList(saved[i].second); });
        }
        ImGui::Separator();
    }

    if (!list || list->goals.empty()) {
        ImGui::TextDisabled("No goal list loaded. Open Settings to create one.");
        ImGui::Separator();
        splits.DrawNuzlockeSection();
        ImGui::End();
        return;
    }

    int current_idx = -1;
    for (int i = 0; i < static_cast<int>(list->goals.size()); ++i) {
        if (list->goals[i].is_header) continue;
        const GoalStatus s = list->goals[i].status;
        if (s != GoalStatus::Completed && s != GoalStatus::Failed) { current_idx = i; break; }
    }

    // Start column only differs for SC parallel starts.
    const SplitsProfile& profile = splits.ActiveProfile();
    const ComparisonSplits& cmp = splits.ActiveComparison();
    const auto nan = std::numeric_limits<double>::quiet_NaN();

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const float pad_x   = 2.f;
    const float pad_y   = 1.f;
    const float win_x   = ImGui::GetWindowPos().x;
    const float avail_w = ImGui::GetContentRegionAvail().x;

    int pb_idx = 0;

    for (int i = 0; i < static_cast<int>(list->goals.size()); ) {
        const auto& g = list->goals[i];

        if (g.is_header) {
            const bool hdr_open = DrawHeaderRow(*list, i, profile);
            ++i;
            if (!hdr_open) {
                // Collapsed: skip children, keep pb_idx aligned.
                while (i < static_cast<int>(list->goals.size())) {
                    const auto& child = list->goals[i];
                    if (child.is_header && child.indent <= g.indent) break;
                    if (!child.is_header) ++pb_idx;
                    ++i;
                }
            }
            continue;
        }

        auto pb_at_v = [&](const std::vector<double>& v) -> double {
            if (pb_idx < 0 || pb_idx >= static_cast<int>(v.size())) return nan;
            return v[static_cast<size_t>(pb_idx)];
        };
        auto pb_seg_v = [&](const std::vector<double>& v) -> double {
            const double cur = pb_at_v(v);
            if (std::isnan(cur)) return nan;
            if (pb_idx == 0) return cur;
            if (pb_idx - 1 >= static_cast<int>(v.size())) return nan;
            const double prev = v[static_cast<size_t>(pb_idx - 1)];
            return std::isnan(prev) ? nan : (cur - prev);
        };

        const float row_y0 = ImGui::GetCursorScreenPos().y - pad_y;

        DrawGoalRow(g, clock, i == current_idx,
                    pb_at_v(cmp.real), pb_seg_v(cmp.real),
                    pb_at_v(cmp.game), pb_seg_v(cmp.game),
                    pb_at_v(cmp.start_real), pb_at_v(cmp.start_game),
                    profile, splits);
        ++pb_idx;
        ++i;

        const float row_y1 = ImGui::GetCursorScreenPos().y + pad_y;
        draw_list->AddRect({win_x + pad_x, row_y0}, {win_x + avail_w - pad_x, row_y1},
                           IM_COL32(80, 80, 80, 140), 2.f);
    }

    ImGui::Separator();
    splits.DrawNuzlockeSection();
    DrawRecentRunsSection(splits);

    ImGui::End();
}

void SplitsGoalListWindow::DrawGoalRow(const GoalEntry& g, const GoalClock& clock, bool is_current,
                                        double pb_split_real, double pb_seg_real,
                                        double pb_split_game, double pb_seg_game,
                                        double pb_start_real, double pb_start_game,
                                        const SplitsProfile& profile, SplitsWindow& splits)
{
    const bool done = (g.status == GoalStatus::Completed || g.status == GoalStatus::Failed);

    ImVec4 color;
    if (g.status == GoalStatus::Failed)         color = ImGui::ColorConvertU32ToFloat4(splits.ColorPbBehind());
    else if (g.status == GoalStatus::Completed) color = ImGui::ColorConvertU32ToFloat4(splits.ColorCompleted());
    // Started too: several goals can be current.
    else if (is_current || g.status == GoalStatus::Started) color = ImGui::ColorConvertU32ToFloat4(splits.ColorActive());
    else                                        color = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);

    using TD = SplitsProfile::TimeDisplay;
    const auto td = profile.time_display;
    const bool show_real = (td != TD::Game) && !(td == TD::Both && profile.both_header_only);
    const bool show_game = (td != TD::Real);

    const ImVec4 real_col   = ImGui::ColorConvertU32ToFloat4(splits.ColorRealTime());
    const ImVec4 game_col   = ImGui::ColorConvertU32ToFloat4(splits.ColorGameTime());
    const ImVec4 ahead_col  = ImGui::ColorConvertU32ToFloat4(splits.ColorPbAhead());
    const ImVec4 behind_col = ImGui::ColorConvertU32ToFloat4(splits.ColorPbBehind());
    auto muted = [](ImVec4 c) { return ImVec4{c.x, c.y, c.z, c.w * 0.55f}; };

    auto time_cell = [&](double real_v, double game_v, bool valid, double pb_real, double pb_game, bool with_delta, bool as_segment) {
        auto line = [&](double v, ImVec4 col, double pb) {
            char buf[32];
            if (valid) FormatTime(buf, sizeof(buf), v);
            else snprintf(buf, sizeof(buf), "--:--");
            if (as_segment) ImGui::TextColored(muted(col), "+%s", buf);
            else            ImGui::TextColored(col, "%s", buf);
            if (valid && with_delta) DrawPBDelta(v, pb, ahead_col, behind_col);
        };
        if (show_real) line(real_v, real_col, pb_real);
        if (show_game) line(game_v, game_col, pb_game);
    };

    if (g.display_style == GoalEntry::DisplayStyle::Dynamic || profile.dynamic_by_default) {
        const bool started = g.start_real_time >= 0.0;

        const int hours = static_cast<int>(std::max(clock.RealTime(), clock.GameTime())) / 3600;
        const int tier  = hours >= 10 ? 2 : (hours >= 1 ? 1 : 0); // MM:SS.CC / H:MM:SS.CC / HH:MM:SS.CC
        const float time_w = TimeColumnWidth(tier, /*with_sign=*/false);
        const float dur_w  = TimeColumnWidth(tier, /*with_sign=*/true);

        if (!ImGui::BeginTable("##goalrow_dyn", 4, ImGuiTableFlags_None)) return;
        ImGui::TableSetupColumn("name",  ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("start", ImGuiTableColumnFlags_WidthFixed, time_w);
        ImGui::TableSetupColumn("end",   ImGuiTableColumnFlags_WidthFixed, time_w);
        ImGui::TableSetupColumn("dur",   ImGuiTableColumnFlags_WidthFixed, dur_w);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        if (g.indent > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + g.indent * 12.f * ImGui::FontScale());
        ImGui::TextColored(color, "%s", g.label.c_str());

        // Start delta SC only: Running relay, start = previous End.
        const bool show_start_delta = profile.dynamic_by_default && profile.show_split_pb;
        ImGui::TableSetColumnIndex(1);
        time_cell(g.start_real_time, g.start_game_time, started, pb_start_real, pb_start_game, show_start_delta, false);

        ImGui::TableSetColumnIndex(2);
        time_cell(g.split.real_time, g.split.game_time, done, pb_split_real, pb_split_game, profile.show_split_pb, false);

        // Duration is end - own start, so compare like with like; relay segment only when PB has no start (SoB, old runs).
        auto pb_duration = [](double pb_end, double pb_start, double pb_seg) {
            return std::isnan(pb_end) || std::isnan(pb_start) ? pb_seg : pb_end - pb_start;
        };
        ImGui::TableSetColumnIndex(3);
        time_cell(g.split.real_time - g.start_real_time, g.split.game_time - g.start_game_time, started && done,
                  pb_duration(pb_split_real, pb_start_real, pb_seg_real),
                  pb_duration(pb_split_game, pb_start_game, pb_seg_game), profile.show_segment_pb, false);

        ImGui::EndTable();
        return;
    }

    if (!ImGui::BeginTable("##goalrow", 3, ImGuiTableFlags_None)) return;
    ImGui::TableSetupColumn("name",  ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("time",  ImGuiTableColumnFlags_WidthFixed, 110.f * ImGui::FontScale());
    ImGui::TableSetupColumn("seg",   ImGuiTableColumnFlags_WidthFixed, (profile.show_segment ? 80.f : 0.f) * ImGui::FontScale());
    ImGui::TableNextRow();

    ImGui::TableSetColumnIndex(0);
    if (g.indent > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + g.indent * 12.f * ImGui::FontScale());
    ImGui::TextColored(color, "%s", g.label.c_str());

    if (g.trigger.type == GoalTrigger::Type::ReachTitleRank) {
        auto* title = GW::PlayerMgr::GetTitleTrack(g.trigger.title_id);
        if (title) {
            if (title->current_points == 0) {
                ImGui::TextDisabled("No progress detected");
            } else {
            const bool at_max = (title->points_needed_next_rank == 0xFFFFFFFF);
            char buf[96];
            if (title->is_percentage_based()) {
                // Points = percent x 10 (863 = 86.3%).
                const float cur  = title->current_points * 0.1f;
                const float next = at_max ? 100.0f : title->points_needed_next_rank * 0.1f;
                if (at_max)
                    snprintf(buf, sizeof(buf), "%.1f%% (Max)", cur);
                else
                    snprintf(buf, sizeof(buf), "%.1f%% / %.1f%%", cur, next);
            } else {
                auto* wc = GW::GetWorldContext();
                uint32_t cur_rank = 0;
                if (wc && title->current_title_tier_index < wc->title_tiers.size())
                    cur_rank = wc->title_tiers[title->current_title_tier_index].tier_number;
                const uint32_t max_rank = title->max_title_rank;
                if (at_max)
                    snprintf(buf, sizeof(buf), "rank %u/%u (Max)  %u pts",
                        cur_rank, max_rank, title->current_points);
                else
                    snprintf(buf, sizeof(buf), "rank %u/%u  %u / %u",
                        cur_rank, max_rank,
                        title->current_points, title->points_needed_next_rank);
            }
            ImGui::TextDisabled("%s", buf);
            }
        }
    }

    if (g.trigger.type == GoalTrigger::Type::MobKill) {
        const uint32_t target = g.trigger.param2 > 0 ? g.trigger.param2 : 1;
        ImGui::TextDisabled("kills: %d / %u", g.trigger_progress, target);
    }

    if (g.trigger.type == GoalTrigger::Type::SkillLearnt) {
        ImGui::TextDisabled("skill id: %u", g.trigger.param1);
    }

    if (g.trigger.type == GoalTrigger::Type::QuestPickup || g.trigger.type == GoalTrigger::Type::QuestComplete) {
        ImGui::TextDisabled("quest id: %u", g.trigger.param1);
    }

    ImGui::TableSetColumnIndex(1);
    if (done)            time_cell(g.split.real_time, g.split.game_time, true, pb_split_real, pb_split_game, profile.show_split_pb, false);
    else if (is_current) ImGui::TextDisabled("---");

    if (profile.show_segment && done) {
        ImGui::TableSetColumnIndex(2);
        time_cell(g.split.segment_real, g.split.segment_game, true, pb_seg_real, pb_seg_game, profile.show_segment_pb, true);
    }

    ImGui::EndTable();
}

bool SplitsGoalListWindow::DrawHeaderRow(const GoalList& list, int header_idx, const SplitsProfile& profile)
{
    const GoalEntry& h = list.goals[header_idx];

    using TD = SplitsProfile::TimeDisplay;
    const bool gt = (profile.time_display == TD::Game) ||
                    (profile.time_display == TD::Both && profile.both_header_only);

    double start_t = -1.0, end_t = -1.0;
    bool any_started = false, any_failed = false, all_done = true, has_any = false;

    for (int j = header_idx + 1; j < static_cast<int>(list.goals.size()); ++j) {
        const GoalEntry& child = list.goals[j];
        if (child.indent <= h.indent) break;
        if (child.is_header) continue;
        has_any = true;

        if (child.status == GoalStatus::Failed)  any_failed  = true;
        if (child.status == GoalStatus::Started)  any_started = true;
        if (child.status != GoalStatus::Completed && child.status != GoalStatus::Failed) all_done = false;

        const double cs = gt ? child.start_game_time : child.start_real_time;
        if (cs >= 0.0 && (start_t < 0.0 || cs < start_t)) start_t = cs;

        if (child.status == GoalStatus::Completed || child.status == GoalStatus::Failed) {
            const double ce = gt ? child.split.game_time : child.split.real_time;
            if (end_t < 0.0 || ce > end_t) end_t = ce;
        }
    }

    if (!has_any) all_done = false;
    GoalStatus status = GoalStatus::NotStarted;
    if (any_failed)                           status = GoalStatus::Failed;
    else if (all_done && has_any)             status = GoalStatus::Completed;
    else if (any_started || start_t >= 0.0)  status = GoalStatus::Started;

    char label[160];
    // ### + list name: stable ID, no shared collapse state across lists.
    if (end_t >= 0.0) {
        char tbuf[32]; FormatTime(tbuf, sizeof(tbuf), end_t);
        if (status == GoalStatus::Failed)
            snprintf(label, sizeof(label), "%s - %s [Failed]###%s_hdr%d", h.label.c_str(), tbuf, list.name.c_str(), header_idx);
        else
            snprintf(label, sizeof(label), "%s - %s###%s_hdr%d", h.label.c_str(), tbuf, list.name.c_str(), header_idx);
    } else {
        snprintf(label, sizeof(label), "%s###%s_hdr%d", h.label.c_str(), list.name.c_str(), header_idx);
    }

    const bool push_color = (status == GoalStatus::Failed);
    if (push_color) {
        ImGui::PushStyleColor(ImGuiCol_Header,        ImVec4(0.55f, 0.08f, 0.08f, 0.90f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.65f, 0.12f, 0.12f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,  ImVec4(0.75f, 0.18f, 0.18f, 1.00f));
    }

    const bool is_open = ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen);

    if (push_color) ImGui::PopStyleColor(3);

    return is_open;
}

void SplitsGoalListWindow::DrawRecentRunsSection(SplitsWindow& splits)
{
    const SplitsProfile& profile = splits.ActiveProfile();
    if (!profile.show_recent_runs) return;
    const auto& runs = splits.RecentRuns();
    if (runs.empty()) return;
    if (!ImGui::CollapsingHeader("Recent Runs")) return;

    const bool gt = (profile.time_display == SplitsProfile::TimeDisplay::Game);
    for (size_t i = 0; i < runs.size(); ++i) {
        const RecentRun& run = runs[i];
        ImGui::PushID(static_cast<int>(i));

        char tbuf[32];
        FormatTime(tbuf, sizeof(tbuf), run.total_real);
        char dbuf[32];
        FormatDate(dbuf, sizeof(dbuf), run.utc_start);
        char label[96];
        snprintf(label, sizeof(label), "%s  %s%s%s%s", tbuf, dbuf,
                 run.start_zone.empty() ? "" : "  ", run.start_zone.c_str(), run.failed ? "  [Failed]" : "");

        if (ImGui::CollapsingHeader(label))
            DrawAttemptGoalsTable(splits, run, gt, false, nullptr, nullptr, {}, {}, {});
        ImGui::PopID();
    }
}

void SplitsGoalListWindow::DrawRunHistoryViewer(SplitsWindow& splits)
{
    if (!show_run_history_viewer) return;

    if (run_history_cache_dirty) {
        run_history_cache = splits.LoadFullRunHistory();
        run_history_selected_attempts.clear();
        run_history_cache_dirty = false;
    }

    const GoalList* list = splits.List();
    char title[192];
    snprintf(title, sizeof(title), "Run History - %s###run_history_viewer",
        list && !list->name.empty() ? list->name.c_str() : "(no list)");
    ImGui::SetNextWindowSize({520.f * ImGui::FontScale(), 480.f * ImGui::FontScale()}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(title, &show_run_history_viewer)) {
        ImGui::End();
        return;
    }

    const bool gt = (splits.ActiveProfile().time_display == SplitsProfile::TimeDisplay::Game);
    const ImVec4 ahead_col  = ImGui::ColorConvertU32ToFloat4(splits.ColorPbAhead());
    const ImVec4 behind_col = ImGui::ColorConvertU32ToFloat4(splits.ColorPbBehind());
    const ImVec4 gold_col   = {1.0f, 0.85f, 0.0f, 1.0f}; // no ColorGold() setting yet; same gold as GWChrono

    auto request_delete = [this](const char* label, std::function<void()> action) {
        ImGui::ConfirmDialog(label, [this, action = std::move(action)](const bool confirmed, void*) {
            if (!confirmed) return;
            action();
            run_history_cache_dirty = true;
        });
    };

    if (splits.PBAttemptNumber() > 0 && !std::isnan(splits.PBTotalReal())) {
        char tbuf[32];
        FormatTime(tbuf, sizeof(tbuf), splits.PBTotalReal());
        if (list && ImGui::TreeNode("pb_breakdown", "PB \xe2\x80\x94 %s (Attempt #%d)", tbuf, splits.PBAttemptNumber())) {
            const auto& pb = splits.Comparison(SplitsProfile::ComparisonMode::PB);
            DrawSplitsBreakdownRows(*list, gt, pb, nullptr, nullptr);
            ImGui::TreePop();
        }
    } else {
        ImGui::TextDisabled("PB: none yet");
    }

    auto draw_breakdown_section = [&](const char* str_id, const char* name, const ComparisonSplits& cmp,
                                       const std::vector<int>* attempt_real, const std::vector<int>* attempt_game) {
        // Last non-NaN: arrays trail NaN past unreached legs.
        const auto& cumulative = gt ? cmp.game : cmp.real;
        const auto last = std::ranges::find_if(cumulative.rbegin(), cumulative.rend(), [](double v) { return !std::isnan(v); });
        const double total = last == cumulative.rend() ? std::numeric_limits<double>::quiet_NaN() : *last;
        if (std::isnan(total)) {
            ImGui::TextDisabled("%s: no data yet", name);
            return;
        }
        char tbuf[32];
        FormatTime(tbuf, sizeof(tbuf), total);
        if (ImGui::TreeNode(str_id, "%s \xe2\x80\x94 %s", name, tbuf)) {
            DrawSplitsBreakdownRows(*list, gt, cmp, attempt_real, attempt_game);
            ImGui::TreePop();
        }
    };
    if (list) {
        using CM = SplitsProfile::ComparisonMode;
        draw_breakdown_section("sob_breakdown", "Sum of Best", splits.Comparison(CM::SumOfBest),
                                &splits.BestSegRealAttempt(), &splits.BestSegGameAttempt());
        draw_breakdown_section("avg_breakdown", "Average", splits.Comparison(CM::Average), nullptr, nullptr);
        DrawZoneTimes(*list, run_history_cache, gt);
    }

    ImGui::Separator();

    const int selected_count = static_cast<int>(run_history_selected_attempts.size());
    if (selected_count == 0) ImGui::BeginDisabled();
    char sel_lbl[48];
    snprintf(sel_lbl, sizeof(sel_lbl), "Delete Selected (%d)", selected_count);
    if (ImGui::Button(sel_lbl)) {
        std::vector<int> attempts(run_history_selected_attempts.begin(), run_history_selected_attempts.end());
        char confirm_label[64];
        snprintf(confirm_label, sizeof(confirm_label), "Delete %d selected attempt%s? This cannot be undone.",
            selected_count, selected_count == 1 ? "" : "s");
        request_delete(confirm_label, [&splits, attempts] { splits.DeleteRunsFromHistory(attempts); });
    }
    if (selected_count == 0) ImGui::EndDisabled();
    ImGui::SameLine();
    if (run_history_cache.empty()) ImGui::BeginDisabled();
    if (ImGui::Button("Clear All History")) {
        request_delete("Delete ALL run history for this list? This cannot be undone.",
            [&splits] { splits.ClearRunHistory(); });
    }
    if (run_history_cache.empty()) ImGui::EndDisabled();


    ImGui::Separator();
    ImGui::BeginChild("##runhistlist", {0.f, 0.f}, true);
    for (const auto& run : run_history_cache) {
        ImGui::PushID(run.attempt_number);

        SetCheckbox("##sel", run_history_selected_attempts, run.attempt_number);
        ImGui::SameLine();

        char tbuf[32];
        FormatTime(tbuf, sizeof(tbuf), run.total_real); // real only: no per-run game total
        char dbuf[32];
        FormatDate(dbuf, sizeof(dbuf), run.utc_start);
        const bool is_pb = (run.attempt_number == splits.PBAttemptNumber());
        char label[128];
        snprintf(label, sizeof(label), "#%d  %s  %s%s%s%s%s", run.attempt_number, tbuf, dbuf,
            run.start_zone.empty() ? "" : "  ", run.start_zone.c_str(),
            run.failed ? "  [Failed]" : "", is_pb ? "  [PB]" : "");

        if (ImGui::CollapsingHeader(label)) {
            const auto& sob_cmp = splits.Comparison(SplitsProfile::ComparisonMode::SumOfBest);
            const auto& pb_cmp  = splits.Comparison(SplitsProfile::ComparisonMode::PB);
            const auto& sob = gt ? sob_cmp.game : sob_cmp.real;
            DrawAttemptGoalsTable(splits, run, gt, true, &sob, &pb_cmp, gold_col, ahead_col, behind_col);
        }
        ImGui::PopID();
    }
    ImGui::EndChild();

    ImGui::End();
}

// =============================================================================
// SETTINGS DISPATCH & COLUMNS
// =============================================================================

void SplitsGoalListWindow::DrawSettings(SplitsWindow& splits)
{
    GoalList* list = splits.List();
    if (list_name_buf[0] == '\0' && !list->name.empty())
        snprintf(list_name_buf, sizeof(list_name_buf), "%s", list->name.c_str());

    DrawProfileSwitcher(splits);
    ImGui::Separator();

    ImGui::Columns(3, "settings_cols", false);
    DrawTimeAndBehaviorColumn(splits);
    ImGui::NextColumn();
    DrawKeybindsAndColorsColumn(splits);
    ImGui::NextColumn();
    DrawGoalListManagementColumn(splits);
    ImGui::Columns(1);
    // Outside 3-column layout; see DrawSCCreateVariantModal.
    if (splits.ActiveProfile().dynamic_by_default)
        DrawSCCreateVariantModal(splits);
    ImGui::Separator();

    // SC lists tool-built, not hand-edited.
    if (splits.ActiveProfile().dynamic_by_default) {
        DrawSCGoalsSummary(splits);
        return;
    }
    DrawEditableGoalsList(splits);

    ImGui::Separator();
    ImGui::TextUnformatted("Add Goal:");
    DrawStandardAddGoalForm(splits);
}

void SplitsGoalListWindow::DrawProfileSwitcher(SplitsWindow& splits)
{
    ImGui::TextUnformatted("Profile:");
    for (int i = 0; i < kProfileCount; ++i) {
        ImGui::SameLine();
        const bool active = (splits.ActiveProfileIdx() == i);
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button,        ImGui::GetStyleColorVec4(ImGuiCol_Header));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_HeaderHovered));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
        }
        if (ImGui::Button(splits.Profiles()[i].name.c_str())) {
            splits.SwitchProfile(i);
            list_name_buf[0] = '\0';
        }
        if (active) {
            ImGui::PopStyleColor(3);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Active — auto-loads on next session");
        }
    }
}

void SplitsGoalListWindow::DrawTimeAndBehaviorColumn(SplitsWindow& splits)
{
    SplitsProfile& p = splits.ActiveProfile();
    using TD = SplitsProfile::TimeDisplay;
    ImGui::TextUnformatted("Time:");
    ImGui::SameLine();
    if (ImGui::RadioButton("Game", p.time_display == TD::Game)) p.time_display = TD::Game;
    ImGui::SameLine();
    if (ImGui::RadioButton("Real", p.time_display == TD::Real)) p.time_display = TD::Real;
    ImGui::SameLine();
    if (ImGui::RadioButton("Both", p.time_display == TD::Both)) p.time_display = TD::Both;
    if (p.time_display == TD::Both) {
        ImGui::SameLine(0, 12.f * ImGui::FontScale());
        ImGui::CheckboxWithHelp("Clock only", &p.both_header_only, "Show Real+Game in the header clock only;\ngoal rows display game time.");
    }
    ImGui::TextUnformatted("Compare vs:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(110.f * ImGui::FontScale());
    {
        using CM = SplitsProfile::ComparisonMode;
        static const char* cm_labels[] = { "PB", "Average", "Sum of Best" };
        int cm_idx = static_cast<int>(p.comparison_mode);
        if (ImGui::Combo("##cmpmode", &cm_idx, cm_labels, static_cast<int>(std::size(cm_labels))))
            p.comparison_mode = static_cast<CM>(cm_idx);
    }
    // Dynamic rows always show End + Duration: no split-column toggle.
    // Running legs are Dynamic too, hence sequential_route.
    if (!p.dynamic_by_default && !p.sequential_route) {
        ImGui::Checkbox("Show total delta", &p.show_split_pb);
        ImGui::Checkbox("Show split column", &p.show_segment);
        if (p.show_segment) {
            ImGui::SameLine();
            ImGui::Checkbox("Show split delta", &p.show_segment_pb);
        }
    } else {
        ImGui::Checkbox("Show total delta", &p.show_split_pb);
        ImGui::SameLine();
        ImGui::Checkbox("Show split delta", &p.show_segment_pb);
    }
    ImGui::Checkbox("Auto /age on completion", &p.auto_send_age);
    ImGui::Checkbox("Show paused time",        &p.show_paused_time);
    ImGui::Checkbox("Show recent runs",        &p.show_recent_runs);

    ImGui::Spacing();
    ImGui::Checkbox("Stop on party wipe", &p.stop_on_party_defeated);
    ImGui::CheckboxWithHelp("Auto-fail on leaving a zone early", &p.auto_fail_on_rezone,
                            "If you leave a zone before accomplishing all goals tied to that zone, the run will fail.");
}

void SplitsGoalListWindow::DrawKeybindsAndColorsColumn(SplitsWindow& splits)
{
    ImGui::TextUnformatted("Keybinds:");
    ImGui::TextDisabled("Bind /splits start, /splits split, /splits reset\nas Send Chat hotkeys in the Hotkeys window.");

    ImGui::Spacing();
    ImGui::PushID("splits_colors");
    ImGui::ColorButtonPicker("Completed color", &splits.ColorCompleted()); ImGui::SameLine(); ImGui::TextUnformatted("Completed");
    ImGui::SameLine(0, 12.f * ImGui::FontScale());
    ImGui::ColorButtonPicker("Current color",   &splits.ColorActive());    ImGui::SameLine(); ImGui::TextUnformatted("Current");
    ImGui::ColorButtonPicker("Real time color", &splits.ColorRealTime());  ImGui::SameLine(); ImGui::TextUnformatted("Real");
    ImGui::SameLine(0, 12.f * ImGui::FontScale());
    ImGui::ColorButtonPicker("Game time color", &splits.ColorGameTime());  ImGui::SameLine(); ImGui::TextUnformatted("Game");
    ImGui::ColorButtonPicker("Ahead color",     &splits.ColorPbAhead());   ImGui::SameLine(); ImGui::TextUnformatted("Ahead");
    ImGui::SameLine(0, 12.f * ImGui::FontScale());
    ImGui::ColorButtonPicker("Behind color",    &splits.ColorPbBehind());  ImGui::SameLine(); ImGui::TextUnformatted("Behind");
    ImGui::PopID();
    ImGui::Spacing();
}

void SplitsGoalListWindow::DrawGoalListManagementColumn(SplitsWindow& splits)
{
    GoalList* list = splits.List();

    ImGui::TextUnformatted("Goal List");
    ImGui::SetNextItemWidth(130.f * ImGui::FontScale());
    ImGui::InputText("##listname", list_name_buf, sizeof(list_name_buf));
    ImGui::SameLine();
    if (ImGui::Button("New")) splits.NewActiveList(list_name_buf);
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
        list->name = list_name_buf;
        splits.SaveActiveList();
    }

    if (splits.ActiveProfile().dynamic_by_default)
        DrawSCLoadCombo(splits);
    else
        DrawPlainLoadCombo(splits);

    if (ImGui::Button("Run History...")) {
        show_run_history_viewer = true;
        run_history_cache_dirty = true;
    }
}

void SplitsGoalListWindow::DrawPlainLoadCombo(SplitsWindow& splits)
{
    GoalList* list = splits.List();
    const auto& saved = splits.GetSavedLists();
    if (saved.empty()) return;

    ImGui::TextUnformatted("Load:");
    std::vector<SearchComboEntry> entries;
    entries.reserve(saved.size());
    for (const auto& [display, path] : saved)
        entries.push_back({ display, display == list->name });
    const char* current = list->name.empty() ? "(no list)" : list->name.c_str();
    SearchableCombo("##load_list", current, load_list_filter_buf, sizeof(load_list_filter_buf),
                    entries, [&](const size_t i) {
                        splits.LoadActiveList(saved[i].second);
                        snprintf(list_name_buf, sizeof(list_name_buf), "%s", list->name.c_str());
                    });
}

// =============================================================================
// GOAL LIST BODY & ADD-GOAL FORM
// =============================================================================

// Area first so own variants not buried among other areas.
namespace {
    struct SCDefaultOption { std::string label; std::function<GoalList()> build; };
    struct SCAreaGroup { std::string label; GW::Constants::MapID anchor_map_id; std::vector<SCDefaultOption> defaults; };

    std::vector<SCAreaGroup> BuildSCAreaGroups()
    {
        std::vector<SCAreaGroup> areas;
        areas.reserve(std::size(SCPresets::kDungeons) + std::size(SCPresets::kEliteAreas) + 2);

        for (const auto& dungeon : SCPresets::kDungeons) {
            areas.push_back({ dungeon.name, dungeon.levels[0], { { "Default", [dungeon] { return SCPresets::BuildDungeonPresetList(dungeon); } } } });
        }
        for (const auto& area : SCPresets::kEliteAreas)
            areas.push_back({ area.label, area.map_id, { { "Default", [area] { return SCPresets::BuildEliteAreaPresetList(area); } } } });

        areas.push_back({ "Domain of Anguish", GW::Constants::MapID::Domain_of_Anguish, { { "Default", [] { return SCPresets::BuildDoAPresetList(); } } } });
        areas.push_back({ "Tomb of the Primeval Kings", SCPresets::kToPKLevels[0], { { "Default", [] { return SCPresets::BuildToPKPresetList(); } } } });

        std::sort(areas.begin(), areas.end(), [](const auto& a, const auto& b) { return a.label < b.label; });
        return areas;
    }
}

void SplitsGoalListWindow::DrawSCLoadCombo(SplitsWindow& splits)
{
    GoalList* list = splits.List();

    ImGui::TextUnformatted("Load:");
    const char* current = (list && !list->name.empty()) ? list->name.c_str() : "(no list)";
    ImGui::SetNextItemWidth(-1.f);
    if (ImGui::BeginCombo("##sc_load", current)) {
        // Build only while open: drawn every frame.
        const std::vector<SCAreaGroup> areas = BuildSCAreaGroups();
        const auto& saved_by_area = splits.GetSavedListsByArea();
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(-1.f);
        ImGui::InputTextWithHint("##filter", "Search...", sc_preset_filter_buf, sizeof(sc_preset_filter_buf));
        ImGui::Separator();

        for (const auto& area : areas) {
            if (sc_preset_filter_buf[0] != '\0' && !TextUtils::CaseInsensitiveContains(area.label, sc_preset_filter_buf))
                continue;
            if (!ImGui::BeginMenu(area.label.c_str())) continue;

            for (const auto& d : area.defaults) {
                if (ImGui::Selectable(d.label.c_str())) {
                    splits.SetActiveList(d.build());
                    snprintf(list_name_buf, sizeof(list_name_buf), "%s", area.label.c_str());
                    ImGui::CloseCurrentPopup();
                }
            }

            if (const auto it = saved_by_area.find(area.anchor_map_id); it != saved_by_area.end() && !it->second.empty()) {
                ImGui::Separator();
                for (const auto& [name, path] : it->second) {
                    const bool selected = list && list->name == name;
                    if (ImGui::Selectable(name.c_str(), selected)) {
                        splits.LoadActiveList(path);
                        snprintf(list_name_buf, sizeof(list_name_buf), "%s", name.c_str());
                        ImGui::CloseCurrentPopup();
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::Separator();
            // Modal, not inline field: growing submenu fight BeginMenu hover, closed itself.
            if (ImGui::Selectable("+ Create New")) {
                sc_creating_new_area = static_cast<int>(area.anchor_map_id);
                sc_create_new_name_buf[0] = '\0';
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndMenu();
        }
        ImGui::EndCombo();
    } else {
        sc_preset_filter_buf[0] = '\0';
    }
    ImGui::SameLine();
    // Clear so auto-load anchor match see nothing and rebuild next entry.
    if (ImGui::SmallButton("Unset")) splits.NewActiveList("");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Clears the loaded list so the right one auto-loads next time you enter a tracked area, instead of loading one right now.");

}

// Split out: combo drawn in two places at once; each opening modal made it fail to render.
void SplitsGoalListWindow::DrawSCCreateVariantModal(SplitsWindow& splits)
{
    if (sc_creating_new_area == -1) return;

    static int s_open_requested_frame = -1;
    const int frame = ImGui::GetFrameCount();
    if (s_open_requested_frame != frame) {
        s_open_requested_frame = frame;
        ImGui::OpenPopup("Create SC Variant");
    }

    if (!ImGui::BeginPopupModal("Create SC Variant", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;

    const std::vector<SCAreaGroup> areas = BuildSCAreaGroups();
    const SCAreaGroup* target = nullptr;
    for (const auto& a : areas) {
        if (static_cast<int>(a.anchor_map_id) == sc_creating_new_area) { target = &a; break; }
    }
    if (!target) {
        // Stale: nothing to clone.
        sc_creating_new_area = -1;
        ImGui::CloseCurrentPopup();
    } else {
        ImGui::Text("New variant of: %s", target->label.c_str());
        ImGui::SetNextItemWidth(240.f * ImGui::FontScale());
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        const bool submit = ImGui::InputTextWithHint("##createname", "Variant name...",
            sc_create_new_name_buf, sizeof(sc_create_new_name_buf), ImGuiInputTextFlags_EnterReturnsTrue);
        const bool can_create = sc_create_new_name_buf[0] != '\0';
        if (!can_create) ImGui::BeginDisabled();
        if (ImGui::Button("Create", {110.f * ImGui::FontScale(), 0}) || (submit && can_create)) {
            GoalList clone = target->defaults.front().build();
            clone.name = sc_create_new_name_buf;
            splits.SetActiveList(std::move(clone));
            splits.SaveActiveList();
            snprintf(list_name_buf, sizeof(list_name_buf), "%s", sc_create_new_name_buf);
            sc_creating_new_area = -1;
            ImGui::CloseCurrentPopup();
        }
        if (!can_create) ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", {110.f * ImGui::FontScale(), 0})) {
            sc_creating_new_area = -1;
            ImGui::CloseCurrentPopup();
        }
    }
    ImGui::EndPopup();
}

void SplitsGoalListWindow::DrawSCGoalsSummary(SplitsWindow& splits)
{
    // Read-only: hand edits would break the built relay chain.
    GoalList* list = splits.List();
    const auto goal_count = static_cast<int>(std::ranges::count_if(list->goals, [](const GoalEntry& g) { return !g.is_header; }));
    ImGui::Text("Goals: %d (%s)", goal_count, list->name.empty() ? "none loaded" : list->name.c_str());
}

void SplitsGoalListWindow::DrawEditableGoalsList(SplitsWindow& splits)
{
    ImGui::TextUnformatted("Goals:");
    GoalList* list = splits.List();
    bool erased = false;
    bool moved = false;
    int first_goal_idx = -1;
    GW::Constants::MapID first_goal_header_map = GW::Constants::MapID::None;
    for (int k = 0; k < static_cast<int>(list->goals.size()); ++k) {
        if (list->goals[k].is_header) { first_goal_header_map = list->goals[k].trigger.map_id; continue; }
        first_goal_idx = k;
        break;
    }
    for (int i = 0; i < static_cast<int>(list->goals.size()) && !erased && !moved; ++i) {
        auto& g = list->goals[i];
        ImGui::PushID(i);

        if (g.is_header) {
            ImGui::TextColored({1.f, 0.85f, 0.4f, 1.f}, "[HDR]");
        } else {
            const char* tname = "Man";
            switch (g.trigger.type) {
                case GoalTrigger::Type::MapEnter:         tname = "Map";   break;
                case GoalTrigger::Type::EnterExplorable:  tname = "Exp";   break;
                case GoalTrigger::Type::ExitExplorable:   tname = "Exit";  break;
                case GoalTrigger::Type::EnterOutpost:     tname = "Out";   break;
                case GoalTrigger::Type::ExitOutpost:      tname = "OutX";  break;
                case GoalTrigger::Type::VanquishComplete: tname = "VQ";    break;
                case GoalTrigger::Type::MissionComplete:  tname = g.trigger.hard_mode ? "HM"  : "Mis"; break;
                case GoalTrigger::Type::MissionBonus:     tname = g.trigger.hard_mode ? "HMB" : "Bon"; break;
                case GoalTrigger::Type::ReachLevel:       tname = "Lv";    break;
                case GoalTrigger::Type::ReachTitleRank:   tname = "Title"; break;
                case GoalTrigger::Type::QuestPickup:      tname = "QuP";   break;
                case GoalTrigger::Type::QuestComplete:    tname = "QuC";   break;
                case GoalTrigger::Type::SkillLearnt:      tname = "Skl";   break;
                case GoalTrigger::Type::MobKill:          tname = "Mob";   break;
                case GoalTrigger::Type::DungeonReward:    tname = "Dgn";   break;
                case GoalTrigger::Type::ObjectiveDone:    tname = "Obj";   break;
                default: break;
            }
            ImGui::TextDisabled("[%s]", tname);
        }
        ImGui::SameLine();
        // No nested headers.
        if (!g.is_header && g.indent > 0) { ImGui::TextDisabled("|"); ImGui::SameLine(0, 2.f * ImGui::FontScale()); }
        ImGui::TextUnformatted(g.label.c_str());
        ImGui::SameLine();

        if (i > 0) {
            if (ImGui::SmallButton("^##up")) {
                std::swap(list->goals[static_cast<size_t>(i)], list->goals[static_cast<size_t>(i - 1)]);
                moved = true;
            }
        } else {
            ImGui::BeginDisabled();
            ImGui::SmallButton("^##up");
            ImGui::EndDisabled();
        }
        ImGui::SameLine();

        if (i + 1 < static_cast<int>(list->goals.size())) {
            if (ImGui::SmallButton("v##down")) {
                std::swap(list->goals[static_cast<size_t>(i)], list->goals[static_cast<size_t>(i + 1)]);
                moved = true;
            }
        } else {
            ImGui::BeginDisabled();
            ImGui::SmallButton("v##down");
            ImGui::EndDisabled();
        }
        ImGui::SameLine();

        if (ImGui::SmallButton("X")) {
            list->goals.erase(list->goals.begin() + i);
            list->RenumberDuplicateLabels();
            erased = true;
        }

        // Only first goal's start in question; rest relay.
        if (i == first_goal_idx) {
            using TT = GoalTrigger::Type;
            if (splits.ActiveProfile().sequential_route) {
                ImGui::TextDisabled("Autostart --> movement detected in an explorable");
            } else if (g.starts_immediately) {
                ImGui::TextDisabled("Autostart --> begins at run start");
            } else if (g.trigger.type == TT::MissionComplete || g.trigger.type == TT::MissionBonus ||
                       g.trigger.type == TT::VanquishComplete || g.trigger.type == TT::DungeonReward) {
                const std::string& map_name = Resources::GetMapName(g.trigger.map_id)->string();
                ImGui::TextDisabled("Autostart --> entering %s",
                    map_name.empty() ? "target map" : map_name.c_str());
            } else if (first_goal_header_map != GW::Constants::MapID::None) {
                const std::string& map_name = Resources::GetMapName(first_goal_header_map)->string();
                ImGui::TextDisabled("Autostart --> entering %s",
                    map_name.empty() ? "target map" : map_name.c_str());
            } else if (g.trigger.type == TT::ReachTitleRank) {
                ImGui::TextDisabled("Autostart --> starts on first title progress");
            } else if (g.trigger.type == TT::MobKill) {
                ImGui::TextDisabled("Autostart --> starts on first kill");
            } else if (g.trigger.type == TT::Manual) {
                ImGui::TextDisabled("Not an autostart goal \xe2\x80\x94 press Start manually");
            } else {
                ImGui::TextDisabled("Autostart --> starts once its trigger fires");
            }
        }

        ImGui::PopID();
    }
}

void SplitsGoalListWindow::DrawStandardAddGoalForm(SplitsWindow& splits)
{
    GoalList* list = splits.List();
    SplitsProfile& p = splits.ActiveProfile();

    struct TriggerOpt { const char* label; int type_int; };
    static const TriggerOpt trigger_opts_full[] = {
        { "Header",      -1 },
        { "Manual",      static_cast<int>(GoalTrigger::Type::Manual)          },
        { "Missions",    static_cast<int>(GoalTrigger::Type::MissionComplete)  },
        { "Explorables", static_cast<int>(GoalTrigger::Type::MapEnter)         },
        { "Towns",       static_cast<int>(GoalTrigger::Type::EnterExplorable)  },
        { "Titles",      static_cast<int>(GoalTrigger::Type::ReachTitleRank)   },
        { "Reach Level", static_cast<int>(GoalTrigger::Type::ReachLevel)       },
        { "Quest",       -2 },
        { "Skill Learnt",   static_cast<int>(GoalTrigger::Type::SkillLearnt)   },
        { "Mob Kill",       static_cast<int>(GoalTrigger::Type::MobKill)       },
        { "Dungeons",       static_cast<int>(GoalTrigger::Type::DungeonReward) },
        { "Elite Areas",    static_cast<int>(GoalTrigger::Type::ObjectiveDone) },
    };
    static const TriggerOpt trigger_opts_running[] = {
        { "Header",      -1 },
        { "Explorables", static_cast<int>(GoalTrigger::Type::MapEnter)        },
        { "Towns",       static_cast<int>(GoalTrigger::Type::EnterExplorable) },
    };
    const bool is_running = p.sequential_route;
    const TriggerOpt* trigger_opts      = is_running ? trigger_opts_running : trigger_opts_full;
    int               trigger_opts_count = is_running ? static_cast<int>(std::size(trigger_opts_running))
                                                      : static_cast<int>(std::size(trigger_opts_full));
    // Reset leftover Manual-only pick when Running active.
    auto is_valid_selection = [&](const TriggerOpt* opts, int count) {
        for (int i = 0; i < count; ++i)
            if (opts[i].type_int == edit_trigger_type) return true;
        return false;
    };
    if (is_running && !is_valid_selection(trigger_opts_running, static_cast<int>(std::size(trigger_opts_running))))
        edit_trigger_type = trigger_opts_running[1].type_int;
    const char* current_trigger_label = trigger_opts[0].label;
    for (int i = 0; i < trigger_opts_count; ++i)
        if (trigger_opts[i].type_int == edit_trigger_type) { current_trigger_label = trigger_opts[i].label; break; }
    ImGui::SetNextItemWidth(220.f * ImGui::FontScale());
    if (ImGui::BeginCombo("Trigger##add", current_trigger_label)) {
        for (int i = 0; i < trigger_opts_count; ++i) {
            const bool selected = (trigger_opts[i].type_int == edit_trigger_type);
            if (ImGui::Selectable(trigger_opts[i].label, selected))
                edit_trigger_type = trigger_opts[i].type_int;
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    const bool is_header_mode      = (edit_trigger_type == -1);
    const bool is_mission_batch    = (edit_trigger_type == static_cast<int>(GoalTrigger::Type::MissionComplete) ||
                                      edit_trigger_type == static_cast<int>(GoalTrigger::Type::MissionBonus));
    const bool is_explorable_batch = (edit_trigger_type == static_cast<int>(GoalTrigger::Type::MapEnter));
    const bool is_town_batch       = (edit_trigger_type == static_cast<int>(GoalTrigger::Type::EnterExplorable));
    const bool is_dungeon_batch    = (edit_trigger_type == static_cast<int>(GoalTrigger::Type::DungeonReward));
    const bool is_elite_area_batch = (edit_trigger_type == static_cast<int>(GoalTrigger::Type::ObjectiveDone));
    const bool is_title_picker     = (edit_trigger_type == static_cast<int>(GoalTrigger::Type::ReachTitleRank));
    const bool needs_level         = (edit_trigger_type == static_cast<int>(GoalTrigger::Type::ReachLevel));
    const bool is_quest_mode       = (edit_trigger_type == -2);
    const bool needs_quest_id      = is_quest_mode;
    const bool needs_skill_id      = (edit_trigger_type == static_cast<int>(GoalTrigger::Type::SkillLearnt));
    const bool needs_mob_id        = (edit_trigger_type == static_cast<int>(GoalTrigger::Type::MobKill));

    if (is_mission_batch) {
        DrawMissionBatchPicker(splits);
    } else if (is_explorable_batch) {
        DrawExplorableBatchPicker(splits);
    } else if (is_town_batch) {
        DrawTownBatchPicker(splits);
    } else if (is_dungeon_batch) {
        DrawDungeonBatchPicker(splits);
    } else if (is_elite_area_batch) {
        DrawEliteAreaBatchPicker(splits);
    } else if (is_title_picker) {
        DrawTitlePicker(splits);
    } else {
        ImGui::SetNextItemWidth(200.f * ImGui::FontScale());
        ImGui::InputText("Label##add", edit_label, sizeof(edit_label));
        if (needs_level) {
            ImGui::SetNextItemWidth(100.f * ImGui::FontScale());
            ImGui::InputInt("Level##add", &edit_level);
            if (edit_level < 1)  edit_level = 1;
            if (edit_level > 20) edit_level = 20;
        }
        if (needs_quest_id) {
            ImGui::SetNextItemWidth(120.f * ImGui::FontScale());
            ImGui::InputInt("Quest ID##add", &edit_quest_id);
            if (edit_quest_id < 0) edit_quest_id = 0;
        }
        if (needs_skill_id) {
            ImGui::SetNextItemWidth(120.f * ImGui::FontScale());
            ImGui::InputInt("Skill ID##add", &edit_skill_id);
            if (edit_skill_id < 0) edit_skill_id = 0;
        }
        if (needs_mob_id) {
            ImGui::SetNextItemWidth(200.f * ImGui::FontScale());
            ImGui::InputText("Mob Name##add", edit_mob_name, sizeof(edit_mob_name));
            ImGui::SetNextItemWidth(120.f * ImGui::FontScale());
            ImGui::InputInt("Kill Count##add", &edit_mob_kill_count);
            if (edit_mob_kill_count < 1) edit_mob_kill_count = 1;
        }
        const bool can_add = edit_label[0] != '\0';
        if (!can_add) ImGui::BeginDisabled();
        if (is_header_mode) {
            if (ImGui::Button("Add Header")) {
                GoalEntry hdr;
                hdr.is_header = true;
                hdr.label     = edit_label;
                list->goals.push_back(std::move(hdr));
                edit_label[0] = '\0';
            }
        } else if (is_quest_mode) {
            if (ImGui::Button("Add Goal")) {
                // Flat pair, not Dynamic: keep list relay/PB-comparable.
                for (const auto& [suffix, type] : {std::pair{" Pickup", GoalTrigger::Type::QuestPickup},
                                                   std::pair{" Complete", GoalTrigger::Type::QuestComplete}}) {
                    GoalEntry g;
                    g.label          = std::string(edit_label) + suffix;
                    g.trigger.type   = type;
                    g.trigger.param1 = static_cast<uint32_t>(edit_quest_id);
                    list->goals.push_back(std::move(g));
                }

                list->RenumberDuplicateLabels();
                edit_label[0] = '\0';
            }
        } else {
            if (ImGui::Button("Add Goal")) {
                GoalEntry new_g;
                new_g.label          = edit_label;
                new_g.trigger.type   = static_cast<GoalTrigger::Type>(edit_trigger_type);
                new_g.trigger.map_id = GW::Constants::MapID::None;
                new_g.trigger.level  = edit_level;
                if (needs_skill_id) new_g.trigger.param1 = static_cast<uint32_t>(edit_skill_id);
                if (needs_mob_id) {
                    new_g.trigger.pattern = TextUtils::StringToWString(edit_mob_name);
                    new_g.trigger.param2  = static_cast<uint32_t>(edit_mob_kill_count);
                }
                list->goals.push_back(std::move(new_g));
                list->RenumberDuplicateLabels();
                edit_label[0] = '\0';
            }
        }
        if (!can_add) ImGui::EndDisabled();

        if (is_quest_mode || needs_skill_id) {
            ImGui::SameLine();
            if (ImGui::Button("Find ID##wiki")) {
                GuiUtils::OpenWiki(L"Guild_Wars_Wiki:Game_integration");
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Opens the wiki's Quest/Skill ID lookup tables\n(no in-game ID list exists for either)");
            }
        }
        if (needs_mob_id) {
            ImGui::SameLine();
            ImGui::TextDisabled("(must match the name exactly, case-insensitive)");
        }
    }
}

// =============================================================================
// TRIGGER-TYPE PICKERS
// =============================================================================

void SplitsGoalListWindow::DrawTitlePicker(SplitsWindow& splits)
{
    using TitleID = GW::Constants::TitleID;

    struct TitleEntry { TitleID id; std::unique_ptr<GuiUtils::EncString> enc; std::string name; };
    // Built once, lazily (same set as TitleTrackerWidget).
    static std::vector<TitleEntry> s_titles;
    static bool s_titles_sorted = false;
    if (s_titles.empty()) {
        for (uint32_t i = 0; i <= static_cast<uint32_t>(TitleID::Codex); ++i) {
            const auto id = static_cast<TitleID>(i);
            if (GW::PlayerMgr::IsDeprecatedTitle(id)) continue;
            const auto* data = GW::PlayerMgr::GetTitleData(id);
            if (!data) { s_titles.clear(); break; } // title data not loaded: retry next frame
            TitleEntry e;
            e.id  = id;
            e.enc = std::make_unique<GuiUtils::EncString>(data->name_id);
            s_titles.push_back(std::move(e));
        }
    }
    // Wait for all names before sort/show: blank dupes collide ImGui IDs.
    if (!s_titles.empty() && !s_titles_sorted) {
        bool all_resolved = true;
        for (auto& e : s_titles) {
            if (e.name.empty()) {
                e.name = e.enc->string();
                if (e.name.empty()) all_resolved = false;
            }
        }
        if (all_resolved) {
            std::sort(s_titles.begin(), s_titles.end(), [](const TitleEntry& a, const TitleEntry& b) { return a.name < b.name; });
            s_titles_sorted = true;
        }
    }
    const int NUM_TITLES = s_titles_sorted ? static_cast<int>(s_titles.size()) : 0;

    ImGui::SetNextItemWidth(-1.f);
    ImGui::InputText("##titlefilter", title_filter_buf, sizeof(title_filter_buf));
    ImGui::SameLine(); if (ImGui::SmallButton("x##titlefx")) title_filter_buf[0] = '\0';

    const bool searching = title_filter_buf[0] != '\0';

    if (ImGui::BeginListBox("##titlelist", { -1.f, 180.f * ImGui::FontScale() })) {
        for (int i = 0; i < NUM_TITLES; ++i) {
            const TitleEntry& e = s_titles[i];
            if (searching && !TextUtils::CaseInsensitiveContains(e.name, title_filter_buf)) continue;
            ImGui::PushID(static_cast<int>(e.id));
            const bool selected = (edit_title_id == static_cast<int>(e.id));
            if (ImGui::Selectable(e.name.c_str(), selected)) {
                if (edit_title_id != static_cast<int>(e.id)) {
                    edit_title_id = static_cast<int>(e.id);
                    edit_title_rank = -1;
                }
            }
            if (selected) ImGui::SetItemDefaultFocus();
            ImGui::PopID();
        }
        ImGui::EndListBox();
    }

    const char* sel_title_name = nullptr;
    for (int i = 0; i < NUM_TITLES; ++i) {
        if (static_cast<int>(s_titles[i].id) == edit_title_id) {
            sel_title_name = s_titles[i].name.c_str();
            break;
        }
    }

    auto* live_title = GW::PlayerMgr::GetTitleTrack(static_cast<TitleID>(edit_title_id));
    // Null = no progress yet, not error. Rank 1 still valid.
    const int  total_ranks = live_title ? static_cast<int>(live_title->max_title_rank) : 0;
    const bool has_a_rank_to_pick = (live_title != nullptr) && total_ranks > 0;
    const bool can_add = (sel_title_name != nullptr);

    // max_title_tier_index = anchor for rank 1, only known once progress exist.
    if (sel_title_name && has_a_rank_to_pick) {
        const int max_tier_base = static_cast<int>(live_title->max_title_tier_index);
        const bool pct_title    = live_title->is_percentage_based();
        auto* wc = GW::GetWorldContext();

        if (edit_title_rank <= 0 || edit_title_rank > total_ranks)
            edit_title_rank = total_ranks;

        ImGui::Text("Target rank (1..%d):", total_ranks);
        if (ImGui::BeginListBox("##rankbox", { -1.f, 80.f * ImGui::FontScale() })) {
            for (int r = 1; r <= total_ranks; ++r) {
                char rank_label[48];
                const int tier_idx = max_tier_base + (r - 1);
                if (pct_title && wc && tier_idx < static_cast<int>(wc->title_tiers.size())) {
                    const float pct = wc->title_tiers[tier_idx].tier_number * 0.1f;
                    if (r == total_ranks)
                        snprintf(rank_label, sizeof(rank_label), "Rank %d — %.1f%% (Max)", r, pct);
                    else
                        snprintf(rank_label, sizeof(rank_label), "Rank %d — %.1f%%", r, pct);
                } else {
                    if (r == total_ranks)
                        snprintf(rank_label, sizeof(rank_label), "Rank %d (Max)", r);
                    else
                        snprintf(rank_label, sizeof(rank_label), "Rank %d", r);
                }
                const bool rank_sel = (edit_title_rank == r);
                if (ImGui::Selectable(rank_label, rank_sel))
                    edit_title_rank = r;
                if (rank_sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndListBox();
        }
    } else if (sel_title_name) {
        edit_title_rank = 1;
        ImGui::TextDisabled("(no progress detected yet \xe2\x80\x94 will target Rank 1; reopen this once you've started the title to pick a different rank)");
    }

    if (!can_add) ImGui::BeginDisabled();
    if (ImGui::Button("Add Goal##titleadd")) {
        char label_buf[160];
        if (has_a_rank_to_pick)
            snprintf(label_buf, sizeof(label_buf), "%s r%d/%d", sel_title_name, edit_title_rank, total_ranks);
        else
            snprintf(label_buf, sizeof(label_buf), "%s r%d", sel_title_name, edit_title_rank);

        GoalEntry g;
        g.label            = label_buf;
        g.trigger.type     = GoalTrigger::Type::ReachTitleRank;
        g.trigger.title_id = static_cast<TitleID>(edit_title_id);
        // 1-based rank, not tier index: anchor resolved live in GoalEngine.
        g.trigger.level    = edit_title_rank;
        g.trigger.map_id   = GW::Constants::MapID::None;

        GoalList* list = splits.List();
        const bool dup = std::any_of(list->goals.begin(), list->goals.end(),
            [&g](const GoalEntry& e) {
                return e.trigger.type == g.trigger.type &&
                       e.trigger.title_id == g.trigger.title_id &&
                       e.trigger.level == g.trigger.level;
            });
        if (!dup) list->goals.push_back(std::move(g));
    }
    if (!can_add) ImGui::EndDisabled();
}

namespace {
struct BatchMapRow { int id; std::string name; GW::Region region; };

struct BatchColumn {
    uint8_t bit;
    GoalTrigger::Type trigger_type;
    const char* header;
    const char* quick_label;
    const char* goal_prefix;
};

constexpr GW::Constants::Campaign kBatchCampaigns[] = {
    GW::Constants::Campaign::Prophecies, GW::Constants::Campaign::Factions,
    GW::Constants::Campaign::Nightfall, GW::Constants::Campaign::EyeOfTheNorth,
};
constexpr const char* kBatchCampaignLabels[] = { "Prophecies", "Factions", "Nightfall", "EotN" };

// rank break name_id ties (Town: first seen wins).
std::vector<BatchMapRow> BuildBatchMapRows(
    const std::function<bool(const GW::AreaInfo&)>& region_ok,
    const std::function<bool(const GW::AreaInfo&, int&)>& type_ok,
    bool& all_named)
{
    all_named = true;
    using MapID = GW::Constants::MapID;
    struct Cand { int id; int rank; GW::Region region; };
    std::unordered_map<uint32_t, Cand> best;
    for (int id = 1; id < static_cast<int>(MapID::Count); ++id) {
        const auto mid  = static_cast<MapID>(id);
        const auto* inf = GW::Map::GetMapInfo(mid);
        if (!inf || !inf->name_id || !region_ok(*inf)) continue;
        int rank = 0;
        if (!type_ok(*inf, rank)) continue;
        if (Resources::GetMapName(mid)->string().empty()) {
            all_named = false;
            continue;
        }
        auto it = best.find(inf->name_id);
        if (it == best.end() || rank < it->second.rank)
            best[inf->name_id] = { id, rank, inf->region };
    }
    std::vector<BatchMapRow> out;
    out.reserve(best.size());
    for (const auto& kv : best)
        out.push_back({ kv.second.id, Resources::GetMapName(static_cast<MapID>(kv.second.id))->string(), kv.second.region });
    std::sort(out.begin(), out.end(), [](const BatchMapRow& a, const BatchMapRow& b) {
        if (a.region != b.region) return a.region < b.region;
        return a.name < b.name;
    });
    return out;
}

void DrawMapBatchPicker(SplitsWindow& splits, const char* id_prefix,
                        char* filter_buf, size_t filter_buf_size,
                        std::map<int, uint8_t>& checked,
                        std::vector<std::pair<int, uint8_t>>& check_order,
                        const std::function<bool(const GW::AreaInfo&, int&)>& type_ok,
                        const std::span<const BatchColumn> columns)
{
    using MapID = GW::Constants::MapID;
    using Camp  = GW::Constants::Campaign;

    // Running: keep click order (a route), not region/name sort.
    const bool preserve_order = splits.ActiveProfile().sequential_route;

    auto set_bit = [&](int id, uint8_t bit, bool on) {
        uint8_t& bits = checked[id];
        const bool was_on = (bits & bit) != 0;
        if (on == was_on) return;
        if (on) { bits |= bit; check_order.push_back({ id, bit }); }
        else {
            bits &= ~bit;
            const auto it = std::find(check_order.begin(), check_order.end(), std::make_pair(id, bit));
            if (it != check_order.end()) check_order.erase(it);
        }
    };

    char filter_id[32]; snprintf(filter_id, sizeof(filter_id), "##%sfilter", id_prefix);
    char clear_id[32];  snprintf(clear_id, sizeof(clear_id), "x##%sfx", id_prefix);
    ImGui::SetNextItemWidth(-1.f);
    ImGui::InputText(filter_id, filter_buf, filter_buf_size);
    ImGui::SameLine(); if (ImGui::SmallButton(clear_id)) filter_buf[0] = '\0';

    auto filter_rows = [&](const std::vector<BatchMapRow>& rows) {
        std::vector<const BatchMapRow*> filtered;
        for (const auto& r : rows) {
            if (filter_buf[0] != '\0' && !TextUtils::CaseInsensitiveContains(r.name, filter_buf))
                continue;
            filtered.push_back(&r);
        }
        return filtered;
    };
    auto draw_bulk_buttons = [&](const std::vector<const BatchMapRow*>& filtered) {
        for (size_t ci = 0; ci < columns.size(); ++ci) {
            if (ci) ImGui::SameLine(0, 10.f * ImGui::FontScale());
            const uint8_t bit = columns[ci].bit;
            char all_lbl[32];  snprintf(all_lbl, sizeof(all_lbl), "All %s",  columns[ci].quick_label);
            char none_lbl[32]; snprintf(none_lbl, sizeof(none_lbl), "None %s", columns[ci].quick_label);
            if (ImGui::SmallButton(all_lbl))  { for (auto* r : filtered) set_bit(r->id, bit, true); }
            ImGui::SameLine();
            if (ImGui::SmallButton(none_lbl)) { for (auto* r : filtered) set_bit(r->id, bit, false); }
        }
    };
    auto draw_table = [&](const char* table_id, const std::vector<const BatchMapRow*>& filtered, bool show_region_header) {
        constexpr ImGuiTableFlags tflags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY;
        if (!ImGui::BeginTable(table_id, 1 + static_cast<int>(columns.size()), tflags, { -1.f, 220.f * ImGui::FontScale() })) return;
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Map", ImGuiTableColumnFlags_WidthStretch);
        for (const auto& col : columns)
            ImGui::TableSetupColumn(col.header, ImGuiTableColumnFlags_WidthFixed, 40.f * ImGui::FontScale());
        ImGui::TableHeadersRow();

        GW::Region prev_reg = static_cast<GW::Region>(0xFFFFFFFFu);
        for (const auto* r : filtered) {
            ImGui::PushID(r->id);
            if (show_region_header && filter_buf[0] == '\0' && r->region != prev_reg) {
                prev_reg = r->region;
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                const std::string& rname = Resources::GetRegionName(r->region)->string();
                if (!rname.empty()) ImGui::TextDisabled("%s", rname.c_str());
            }
            ImGui::TableNextRow();
            const uint8_t bits = checked[r->id];
            ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(r->name.c_str());
            for (int ci = 0; ci < static_cast<int>(columns.size()); ++ci) {
                ImGui::TableSetColumnIndex(1 + ci);
                ImGui::PushID(ci);
                bool v = (bits & columns[ci].bit) != 0;
                if (ImGui::Checkbox("##v", &v)) set_bit(r->id, columns[ci].bit, v);
                ImGui::PopID();
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    };

    char tabbar_id[32];   snprintf(tabbar_id, sizeof(tabbar_id), "##%s_campaigns", id_prefix);
    char table_id[32];    snprintf(table_id, sizeof(table_id), "##%stbl", id_prefix);
    char table_ps_id[32]; snprintf(table_ps_id, sizeof(table_ps_id), "##%stbl_ps", id_prefix);

    // AreaInfo fixed: cache, not rescan ~1300 maps each frame.
    // Rebuilt until every name has decoded, else maps whose names were still loading stay missing all session.
    struct CachedRows { std::vector<BatchMapRow> rows; bool complete = false; double built_at = -1.0; };
    static std::unordered_map<std::string, CachedRows> row_cache;
    auto cached_rows = [&](const std::string& key, const std::function<bool(const GW::AreaInfo&)>& region_ok) -> const std::vector<BatchMapRow>& {
        auto& entry = row_cache[key];
        // Retry throttled: a name that never decodes must not mean a full map scan every frame.
        if (!entry.complete && ImGui::GetTime() - entry.built_at >= 0.5) {
            entry.rows     = BuildBatchMapRows(region_ok, type_ok, entry.complete);
            entry.built_at = ImGui::GetTime();
        }
        return entry.rows;
    };

    if (ImGui::BeginTabBar(tabbar_id)) {
        for (int ci = 0; ci < static_cast<int>(std::size(kBatchCampaigns)); ++ci) {
            if (!ImGui::BeginTabItem(kBatchCampaignLabels[ci])) continue;
            const Camp camp = kBatchCampaigns[ci];
            const auto& rows = cached_rows(std::string(id_prefix) + "_" + std::to_string(ci),
                [camp](const GW::AreaInfo& inf) { return inf.GetIsOnWorldMap() && inf.campaign == camp; });
            const auto filtered = filter_rows(rows);
            draw_bulk_buttons(filtered);
            draw_table(table_id, filtered, /*show_region_header=*/true);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Pre-Searing")) {
            const auto& ps_rows = cached_rows(std::string(id_prefix) + "_ps",
                [](const GW::AreaInfo& inf) { return inf.region == GW::Region_Presearing; });
            const auto ps_filtered = filter_rows(ps_rows);
            draw_bulk_buttons(ps_filtered);
            draw_table(table_ps_id, ps_filtered, /*show_region_header=*/false);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    int total = 0;
    for (const auto& [id, bits] : checked)
        for (const auto& col : columns)
            if (bits & col.bit) ++total;

    char add_id[32]; snprintf(add_id, sizeof(add_id), "##%sadd", id_prefix);
    if (total == 0) ImGui::BeginDisabled();
    char add_lbl[64];
    snprintf(add_lbl, sizeof(add_lbl), "Add %d Goal%s%s", total, total == 1 ? "" : "s", add_id);
    if (ImGui::Button(add_lbl)) {
        struct Item { int id; const BatchColumn* col; Camp camp; GW::Region region; std::string name; };
        std::vector<Item> items;
        items.reserve(static_cast<size_t>(total));
        if (preserve_order) {
            for (const auto& [id, bit] : check_order) {
                const auto col = std::find_if(columns.begin(), columns.end(), [bit](const BatchColumn& c) { return c.bit == bit; });
                if (col == columns.end()) continue;
                const auto mid = static_cast<MapID>(id);
                const auto* inf = GW::Map::GetMapInfo(mid);
                items.push_back({ id, &*col, inf ? inf->campaign : Camp::Prophecies,
                                   inf ? inf->region : static_cast<GW::Region>(0),
                                   Resources::GetMapName(mid)->string() });
            }
        } else {
            for (const auto& [id, bits] : checked) {
                if (!bits) continue;
                const auto mid = static_cast<MapID>(id);
                const auto* inf = GW::Map::GetMapInfo(mid);
                const Camp c = inf ? inf->campaign : Camp::Prophecies;
                const GW::Region reg = inf ? inf->region : static_cast<GW::Region>(0);
                const std::string nm = Resources::GetMapName(mid)->string();
                for (const auto& col : columns)
                    if (bits & col.bit) items.push_back({ id, &col, c, reg, nm });
            }
            // Column index tie-break: Enter before Leave.
            std::sort(items.begin(), items.end(), [&](const Item& a, const Item& b) {
                if (a.camp   != b.camp)   return a.camp   < b.camp;
                if (a.region != b.region) return a.region < b.region;
                if (a.name   != b.name)   return a.name   < b.name;
                return (a.col - columns.data()) < (b.col - columns.data());
            });
        }
        GoalList* list = splits.List();
        for (const auto& item : items) {
            GoalEntry g;
            g.trigger.map_id = static_cast<MapID>(item.id);
            if (preserve_order) {
                // Enter = start_trigger, Exit = trigger: Duration = time spent.
                g.label                 = item.name;
                g.trigger.type          = item.col->trigger_type == GoalTrigger::Type::EnterExplorable
                                              ? GoalTrigger::Type::ExitExplorable : GoalTrigger::Type::ExitOutpost;
                g.start_trigger         = GoalTrigger{};
                g.start_trigger->type   = item.col->trigger_type;
                g.start_trigger->map_id = g.trigger.map_id;
                g.display_style         = GoalEntry::DisplayStyle::Dynamic;
            } else {
                g.label          = std::string(item.col->goal_prefix) + item.name;
                g.trigger.type   = item.col->trigger_type;
            }
            list->goals.push_back(std::move(g));
        }
        list->RenumberDuplicateLabels();
        check_order.clear();
        checked.clear();
    }
    if (total == 0) ImGui::EndDisabled();
}
} // namespace

void SplitsGoalListWindow::DrawTownBatchPicker(SplitsWindow& splits)
{
    static const BatchColumn kColumns[] = {
        { 1, GoalTrigger::Type::EnterOutpost, "Enter", "En", "Enter " },
        { 2, GoalTrigger::Type::ExitOutpost,  "Leave", "Lv", "Leave " },
    };
    // Running: one box = one start/trigger pair goal.
    static const BatchColumn kColumnsRunning[] = {
        { 1, GoalTrigger::Type::EnterOutpost, "Add", "Add", "" },
    };
    static const GW::RegionType s_town_types[] = {
        GW::RegionType::City, GW::RegionType::Outpost, GW::RegionType::MissionOutpost,
        GW::RegionType::Challenge, GW::RegionType::Marketplace,
        GW::RegionType::HeroBattleOutpost, GW::RegionType::ZaishenBattle,
    };
    auto type_ok = [](const GW::AreaInfo& inf, int&) { return std::ranges::contains(s_town_types, inf.type); };
    const bool is_running = splits.ActiveProfile().sequential_route;
    DrawMapBatchPicker(splits, "town", town_filter_buf, sizeof(town_filter_buf),
                       batch_town_checked, batch_town_order, type_ok,
                       is_running ? std::span<const BatchColumn>(kColumnsRunning) : std::span<const BatchColumn>(kColumns));
}

void SplitsGoalListWindow::DrawExplorableBatchPicker(SplitsWindow& splits)
{
    static const BatchColumn kColumns[] = {
        { 1, GoalTrigger::Type::EnterExplorable,  "Enter", "En", "Enter " },
        { 2, GoalTrigger::Type::VanquishComplete, "VQ",    "VQ", "VQ "    },
        { 4, GoalTrigger::Type::ExitExplorable,   "Leave", "Lv", "Leave " },
    };
    // Running: no VQ; one box = one pass-through leg.
    static const BatchColumn kColumnsRunning[] = {
        { 1, GoalTrigger::Type::EnterExplorable, "Add", "Add", "" },
    };
    // Ties prefer ExplorableZone over MissionArea.
    auto type_ok = [](const GW::AreaInfo& inf, int& rank) {
        if (inf.type != GW::RegionType::ExplorableZone && inf.type != GW::RegionType::MissionArea) return false;
        rank = (inf.type == GW::RegionType::ExplorableZone) ? 0 : 1;
        return true;
    };
    const bool is_running = splits.ActiveProfile().sequential_route;
    DrawMapBatchPicker(splits, "exp", exp_filter_buf, sizeof(exp_filter_buf),
                       batch_exp_checked, batch_exp_order, type_ok,
                       is_running ? std::span<const BatchColumn>(kColumnsRunning) : std::span<const BatchColumn>(kColumns));
}

void SplitsGoalListWindow::DrawMissionBatchPicker(SplitsWindow& splits)
{
    using MapID = GW::Constants::MapID;
    using Camp  = GW::Constants::Campaign;

    struct MissionRow { int id; std::string name; uint32_t chron; };

    constexpr int NUM_CAMPS = 3;

    // Story order, not map id.
    static constexpr MapID kNightfall[] = {
        MapID::Chahbek_Village, MapID::Jokanur_Diggings, MapID::Blacktide_Den,
        MapID::Consulate_Docks, MapID::Venta_Cemetery, MapID::Kodonur_Crossroads,
        MapID::Pogahn_Passage, MapID::Rilohn_Refuge, MapID::Moddok_Crevice,
        MapID::Tihark_Orchard, MapID::Dasha_Vestibule, MapID::Dzagonur_Bastion,
        MapID::Grand_Court_of_Sebelkeh, MapID::Jennurs_Horde, MapID::Nundu_Bay,
        MapID::Gate_of_Desolation, MapID::Ruins_of_Morah, MapID::Gate_of_Pain,
        MapID::Gate_of_Madness, MapID::Abaddons_Gate,
    };
    static constexpr MapID kFactions[] = {
        MapID::Minister_Chos_Estate_outpost_mission, MapID::Zen_Daijun_outpost_mission,
        MapID::Vizunah_Square_mission, MapID::Nahpui_Quarter_outpost_mission,
        MapID::Tahnnakai_Temple_outpost_mission, MapID::Arborstone_outpost_mission,
        MapID::Boreas_Seabed_outpost_mission, MapID::Sunjiang_District_outpost_mission,
        MapID::The_Eternal_Grove_outpost_mission, MapID::Gyala_Hatchery_outpost_mission,
        MapID::Unwaking_Waters_Kurzick_outpost, MapID::Raisu_Palace_outpost_mission,
        MapID::Imperial_Sanctum_outpost_mission,
    };
    static constexpr MapID kProphecies[] = {
        MapID::The_Great_Northern_Wall, MapID::Fort_Ranik, MapID::Ruins_of_Surmia,
        MapID::Nolani_Academy, MapID::Borlis_Pass, MapID::The_Frost_Gate,
        MapID::Gates_of_Kryta, MapID::DAlessio_Seaboard, MapID::Divinity_Coast,
        MapID::The_Wilds, MapID::Bloodstone_Fen, MapID::Aurora_Glade,
        MapID::Riverside_Province, MapID::Sanctum_Cay, MapID::Dunes_of_Despair,
        MapID::Thirsty_River, MapID::Elona_Reach, MapID::Augury_Rock_outpost,
        MapID::The_Dragons_Lair, MapID::Ice_Caves_of_Sorrow, MapID::Iron_Mines_of_Moladune,
        MapID::Thunderhead_Keep, MapID::Ring_of_Fire, MapID::Abaddons_Mouth,
        MapID::Hells_Precipice,
    };
    auto build_list = [](Camp camp) -> std::vector<MissionRow> {
        std::span<const MapID> order;
        switch (camp) {
            case Camp::Nightfall:  order = kNightfall;  break;
            case Camp::Factions:   order = kFactions;   break;
            case Camp::Prophecies: order = kProphecies; break;
            default: return {};
        }
        std::vector<MissionRow> out;
        out.reserve(order.size());
        for (uint32_t i = 0; i < static_cast<uint32_t>(order.size()); ++i)
            out.push_back({ static_cast<int>(order[i]), Resources::GetMapName(order[i])->string(), i + 1 });
        return out;
    };

    ImGui::TextColored({1.f, 0.8f, 0.2f, 1.f},
        "Note: Bonus is read from the mission-complete bitmask, which never clears once earned. "
        "On a character that's already earned a mission's bonus before, Bonus will always show "
        "complete on every later attempt of that mission, whether or not it's actually re-earned that run.");

    // Cache only once all names decoded, else blank sticks.
    static std::unordered_map<int, std::vector<MissionRow>> mission_cache;
    static std::vector<MissionRow> mission_scratch;
    auto cached_missions = [&](const Camp camp) -> const std::vector<MissionRow>& {
        const int key = static_cast<int>(camp);
        if (const auto it = mission_cache.find(key); it != mission_cache.end()) return it->second;
        auto rows = build_list(camp);
        if (std::ranges::any_of(rows, [](const MissionRow& r) { return r.name.empty(); })) {
            mission_scratch = std::move(rows);
            return mission_scratch;
        }
        return mission_cache.emplace(key, std::move(rows)).first->second;
    };

    if (ImGui::BeginTabBar("##batch_campaigns")) {
        for (int ci = 0; ci < NUM_CAMPS; ++ci) {
            if (!ImGui::BeginTabItem(kBatchCampaignLabels[ci])) continue;
            const auto& missions = cached_missions(kBatchCampaigns[ci]);

            if (ImGui::SmallButton("All M"))  { for (const auto& m : missions) batch_mis_checked.insert(m.id); }
            ImGui::SameLine();
            if (ImGui::SmallButton("None M")) { for (const auto& m : missions) batch_mis_checked.erase(m.id); }
            ImGui::SameLine(0, 16.f * ImGui::FontScale());
            if (ImGui::SmallButton("All B"))  { for (const auto& m : missions) batch_bon_checked.insert(m.id); }
            ImGui::SameLine();
            if (ImGui::SmallButton("None B")) { for (const auto& m : missions) batch_bon_checked.erase(m.id); }
            ImGui::SameLine(0, 16.f * ImGui::FontScale());
            ImGui::Checkbox("Hard Mode", &batch_hm);

            constexpr ImGuiTableFlags tflags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY;
            if (ImGui::BeginTable("##missiontbl", 4, tflags, { -1.f, 220.f * ImGui::FontScale() })) {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("#",       ImGuiTableColumnFlags_WidthFixed, 28.f * ImGui::FontScale());
                ImGui::TableSetupColumn("Mission", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("M",       ImGuiTableColumnFlags_WidthFixed, 28.f * ImGui::FontScale());
                ImGui::TableSetupColumn("B",       ImGuiTableColumnFlags_WidthFixed, 28.f * ImGui::FontScale());
                ImGui::TableHeadersRow();

                for (int mi = 0; mi < static_cast<int>(missions.size()); ++mi) {
                    const auto& row = missions[mi];
                    ImGui::PushID(row.id);
                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%d", mi + 1);
                    ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(row.name.c_str());
                    ImGui::TableSetColumnIndex(2);
                    SetCheckbox("##m", batch_mis_checked, row.id);
                    ImGui::TableSetColumnIndex(3);
                    SetCheckbox("##b", batch_bon_checked, row.id);
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    const int total = static_cast<int>(batch_mis_checked.size() + batch_bon_checked.size());
    if (total == 0) ImGui::BeginDisabled();
    char add_lbl[48];
    snprintf(add_lbl, sizeof(add_lbl), "Add %d Goal%s##batchadd", total, total == 1 ? "" : "s");
    if (ImGui::Button(add_lbl)) {
        struct BatchItem { int id; bool bonus; Camp camp; uint32_t chron; };
        std::vector<BatchItem> items;
        items.reserve(total);
        for (int id : batch_mis_checked) {
            const auto* info = GW::Map::GetMapInfo(static_cast<MapID>(id));
            if (info) items.push_back({ id, false, info->campaign, info->mission_chronology });
        }
        for (int id : batch_bon_checked) {
            const auto* info = GW::Map::GetMapInfo(static_cast<MapID>(id));
            if (info) items.push_back({ id, true, info->campaign, info->mission_chronology });
        }
        std::sort(items.begin(), items.end(), [](const BatchItem& a, const BatchItem& b) {
            if (a.camp  != b.camp)  return a.camp  < b.camp;
            if (a.chron != b.chron) return a.chron < b.chron;
            return a.bonus < b.bonus;
        });
        GoalList* list = splits.List();
        for (const auto& item : items) {
            GoalEntry g;
            g.label = Resources::GetMapName(static_cast<MapID>(item.id))->string();
            if (item.bonus) g.label += " - Bonus";
            if (batch_hm)  g.label += " (HM)";
            g.trigger.type      = item.bonus ? GoalTrigger::Type::MissionBonus : GoalTrigger::Type::MissionComplete;
            g.trigger.map_id    = static_cast<MapID>(item.id);
            g.trigger.hard_mode = batch_hm;
            list->goals.push_back(std::move(g));
        }
        list->RenumberDuplicateLabels();
        batch_mis_checked.clear();
        batch_bon_checked.clear();
    }
    if (total == 0) ImGui::EndDisabled();
}

void SplitsGoalListWindow::DrawDungeonBatchPicker(SplitsWindow& splits)
{
    using MapID = GW::Constants::MapID;

    struct DungeonRow { int id; std::string name; };
    std::vector<DungeonRow> rows;
    rows.reserve(std::size(SCPresets::kDungeons));
    for (const auto& dungeon : SCPresets::kDungeons)
        rows.push_back({ static_cast<int>(dungeon.levels[0]), dungeon.name });
    std::sort(rows.begin(), rows.end(), [](const DungeonRow& a, const DungeonRow& b) { return a.name < b.name; });

    std::vector<const DungeonRow*> filtered;
    for (const auto& r : rows) {
        if (dungeon_filter_buf[0] != '\0' && !TextUtils::CaseInsensitiveContains(r.name, dungeon_filter_buf))
            continue;
        filtered.push_back(&r);
    }

    // Manual flat: one generic DungeonReward per dungeon.
    ImGui::TextColored({1.f, 0.8f, 0.2f, 1.f},
        "Note: completion is a generic \"dungeon reward chest opened\" signal, not specific to "
        "this dungeon \xe2\x80\x94 correct as long as the list is run in order, same as any other "
        "sequential Manual goal.");

    ImGui::SetNextItemWidth(-1.f);
    ImGui::InputText("##dungeonfilter", dungeon_filter_buf, sizeof(dungeon_filter_buf));
    ImGui::SameLine(); if (ImGui::SmallButton("x##dungeonfx")) dungeon_filter_buf[0] = '\0';

    if (ImGui::SmallButton("All"))  { for (const auto* r : filtered) batch_dungeon_checked.insert(r->id); }
    ImGui::SameLine();
    if (ImGui::SmallButton("None")) { for (const auto* r : filtered) batch_dungeon_checked.erase(r->id); }

    constexpr ImGuiTableFlags tflags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY;
    if (ImGui::BeginTable("##dungeontbl", 2, tflags, { -1.f, 220.f * ImGui::FontScale() })) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Dungeon", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Add",     ImGuiTableColumnFlags_WidthFixed, 40.f * ImGui::FontScale());
        ImGui::TableHeadersRow();

        for (const auto* r : filtered) {
            ImGui::PushID(r->id);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(r->name.c_str());
            ImGui::TableSetColumnIndex(1);
            SetCheckbox("##d", batch_dungeon_checked, r->id);
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    const int total = static_cast<int>(batch_dungeon_checked.size());
    if (total == 0) ImGui::BeginDisabled();
    char add_lbl[48];
    snprintf(add_lbl, sizeof(add_lbl), "Add %d Goal%s##batchadddungeon", total, total == 1 ? "" : "s");
    if (ImGui::Button(add_lbl)) {
        GoalList* list = splits.List();
        // Iterate sorted rows, not set (map_id order).
        for (const auto& r : rows) {
            if (!batch_dungeon_checked.contains(r.id)) continue;
            GoalEntry g;
            g.label          = r.name;
            g.trigger.type   = GoalTrigger::Type::DungeonReward;
            g.trigger.map_id = static_cast<MapID>(r.id);
            list->goals.push_back(std::move(g));
        }
        list->RenumberDuplicateLabels();
        batch_dungeon_checked.clear();
    }
    if (total == 0) ImGui::EndDisabled();
}

// Area complete = all checkpoints done (header aggregate). Partial pick = flat goals.
void SplitsGoalListWindow::DrawEliteAreaBatchPicker(SplitsWindow& splits)
{
    struct AreaTab {
        const SCPresets::EliteArea* area;
        std::set<int>*              checked;
    };
    const AreaTab areas[] = {
        { &SCPresets::kEliteAreas[0], &batch_fow_checked   },
        { &SCPresets::kEliteAreas[1], &batch_uw_checked    },
        { &SCPresets::kEliteAreas[2], &batch_urgoz_checked },
        { &SCPresets::kEliteAreas[3], &batch_deep_checked  },
    };

    if (ImGui::BeginTabBar("##elite_areas")) {
        for (const auto& tab : areas) {
            const auto& area = *tab.area;
            if (!ImGui::BeginTabItem(area.label)) continue;

            ImGui::TextColored({1.f, 0.8f, 0.2f, 1.f},
                "For overall completion tracking (one header, complete once every checkpoint\n"
                "below is done \xe2\x80\x94 order doesn't matter), check all of them. Checking only some\n"
                "adds just those as individual flat goals, with no completion header.");

            if (ImGui::SmallButton("All"))  { for (size_t i = 0; i < area.count; ++i) tab.checked->insert(static_cast<int>(i)); }
            ImGui::SameLine();
            if (ImGui::SmallButton("None")) { tab.checked->clear(); }

            constexpr ImGuiTableFlags tflags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY;
            if (ImGui::BeginTable("##elitetbl", 2, tflags, { -1.f, 220.f * ImGui::FontScale() })) {
                ImGui::TableSetupColumn("Checkpoint", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Add",        ImGuiTableColumnFlags_WidthFixed, 40.f * ImGui::FontScale());
                ImGui::TableHeadersRow();

                for (size_t i = 0; i < area.count; ++i) {
                    ImGui::PushID(static_cast<int>(i));
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(area.checkpoints[i].name);
                    ImGui::TableSetColumnIndex(1);
                    SetCheckbox("##e", *tab.checked, static_cast<int>(i));
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }

            const int total = static_cast<int>(tab.checked->size());
            if (total == 0) ImGui::BeginDisabled();
            char add_lbl[48];
            snprintf(add_lbl, sizeof(add_lbl), "Add %d Goal%s##batchaddelite", total, total == 1 ? "" : "s");
            if (ImGui::Button(add_lbl)) {
                GoalList* list = splits.List();
                const bool all_checked = (tab.checked->size() == area.count);
                if (all_checked) {
                    GoalEntry hdr;
                    hdr.is_header      = true;
                    hdr.label          = area.label;
                    hdr.trigger.map_id = area.map_id; // ApplyTimerPolicy autostart
                    list->goals.push_back(std::move(hdr));
                }
                for (size_t i = 0; i < area.count; ++i) {
                    if (!tab.checked->count(static_cast<int>(i))) continue;
                    GoalEntry g = SCPresets::BuildCheckpointGoal(area.checkpoints[i], area.map_id);
                    g.indent    = all_checked ? 1 : 0;
                    list->goals.push_back(std::move(g));
                }
                list->RenumberDuplicateLabels();
                tab.checked->clear();
            }
            if (total == 0) ImGui::EndDisabled();

            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}
