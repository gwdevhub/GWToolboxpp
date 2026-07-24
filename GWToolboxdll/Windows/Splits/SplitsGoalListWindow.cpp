#include "stdafx.h"
#include <functional>
#include <memory>
#include <ImGuiAddons.h>
#include <Windows/Splits/SplitsGoalListWindow.h>
#include <Windows/Splits/SplitsProfile.h>
#include <Windows/SplitsWindow.h>
#include <Windows/Splits/GoalEntry.h>
#include <Windows/Splits/GoalList.h>
#include <Windows/Splits/GoalClock.h>
#include <Windows/Splits/SCPresets.h>
#include <Modules/Resources.h>
#include <Windows/SettingsWindow.h>
#include <Windows/InfoWindow.h>
#include <Utils/TextUtils.h>
#include <Utils/EncString.h>
#include <Utils/ToolboxUtils.h>

#include <GWCA/GameContainers/Array.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Constants/Constants.h>
#include <GWCA/Context/WorldContext.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Quest.h>
#include <GWCA/GameEntities/Title.h>

// Routed through kOpenWikiUrl (same mechanism the skill listing's Wiki button uses) rather than ShellExecute; enqueued since this fires from an ImGui render callback, not the game thread.
static void OpenGameIntegrationWikiPage()
{
    GW::GameThread::Enqueue([] {
        GW::UI::SendUIMessage(GW::UI::UIMessage::kOpenWikiUrl,
                               const_cast<char*>("https://wiki.guildwars.com/wiki/Guild_Wars_Wiki:Game_integration"));
    });
}

// ---------------------------------------------------------------------------
// Formatting helpers
// ---------------------------------------------------------------------------
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

static void DrawPBDelta(double actual, double pb_split, ImVec4 col_ahead, ImVec4 col_behind)
{
    if (std::isnan(pb_split) || std::isnan(actual)) return;
    const double delta = actual - pb_split;
    char dbuf[32];
    FormatTime(dbuf, sizeof(dbuf), std::abs(delta));
    ImGui::TextColored(delta < 0.0 ? col_ahead : col_behind, delta < 0.0 ? "-%s" : "+%s", dbuf);
}

// ---------------------------------------------------------------------------
// Main window Draw
// ---------------------------------------------------------------------------
void SplitsGoalListWindow::Draw(SplitsWindow& plugin)
{
    const bool is_open = ImGui::Begin(plugin.Name(), plugin.GetVisiblePtr(), plugin.GetWinFlags());

    if (!is_open) {
        ImGui::End();
        return;
    }

    // Resume modal
    if (plugin.HasPendingResume())
        ImGui::OpenPopup("Resume Run?");
    if (ImGui::BeginPopupModal("Resume Run?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Run '%s' was interrupted.", plugin.PendingResumeName());
        ImGui::Text("Resume from where you left off?");
        ImGui::Spacing();
        if (ImGui::Button("Resume",  {110, 0})) { plugin.ApplyResume();  ImGui::CloseCurrentPopup(); }
        ImGui::SameLine();
        if (ImGui::Button("Discard", {110, 0})) { plugin.DiscardResume(); ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }

    const GoalClock& clock = plugin.Clock();
    const GoalList*  list  = plugin.List();

    // Header clock — shows whichever time the profile is set to display, plus run controls.
    {
        const SplitsProfile& hp = plugin.ActiveProfile();
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
        const float start_x = min_x + (avail - total_w) * 0.5f;
        const float clamped_start_x = start_x < min_x ? min_x : start_x;

        if (plugin.NuzlockePointsEnabled()) {
            ImGui::SetCursorPosX(min_x);
            ImGui::TextColored({0.6f, 0.85f, 1.f, 1.f}, "Points: %d", plugin.NuzlockeTotalPoints());
            ImGui::SameLine(0, 0);
        }

        if (hp.show_paused_time) {
            char pbuf[32]; FormatTime(pbuf, sizeof(pbuf), plugin.TotalPausedReal());
            char ptext[48]; snprintf(ptext, sizeof(ptext), "Paused: %s", pbuf);
            const float pause_w = ImGui::CalcTextSize(ptext).x;
            const float pause_x = clamped_start_x - 12.f - pause_w;
            ImGui::SetCursorPosX(pause_x > min_x ? pause_x : min_x);
            ImGui::TextColored({1.f, 0.8f, 0.3f, 1.f}, "%s", ptext);
            ImGui::SameLine(0, 0);
        }

        const ImVec4 real_col = ImGui::ColorConvertU32ToFloat4(hp.color_real_time);
        const ImVec4 game_col = ImGui::ColorConvertU32ToFloat4(hp.color_game_time);
        ImGui::SetCursorPosX(clamped_start_x);
        if (td == TD::Both) {
            ImGui::TextColored(real_col, "Real: %s", tbuf_r);
            ImGui::SameLine(0, 0);
            ImGui::TextColored(game_col, "  Game: %s", tbuf_g);
        } else {
            const bool gt = (td == TD::Game);
            ImGui::TextColored(gt ? game_col : real_col, gt ? "Game: %s" : "Real: %s", gt ? tbuf_g : tbuf_r);
        }

        // Run controls + settings gear, right-aligned on the same row as the clock.
        const bool   running = plugin.Clock().IsRunning();
        const char*  lbl0    = running ? "Pause" : "Start";
        const float  fp_x    = ImGui::GetStyle().FramePadding.x;
        const float  sp      = ImGui::GetStyle().ItemSpacing.x;
        const float  bw0     = ImGui::CalcTextSize(lbl0).x          + fp_x * 2.f;
        const float  bw1     = ImGui::CalcTextSize("Reset").x        + fp_x * 2.f;
        const float  bw2     = ImGui::CalcTextSize("Split").x        + fp_x * 2.f;
        const float  bwg     = ImGui::CalcTextSize(ICON_FA_COGS).x  + fp_x * 2.f;
        const float  buttons_w = bw0 + bw1 + bw2 + bwg + sp * 3.f;
        const float  button_x  = min_x + avail - buttons_w;

        ImGui::SameLine(button_x);
        if (ImGui::Button(lbl0,          {bw0, 0})) plugin.StartRun();
        ImGui::SameLine(0, sp);
        if (ImGui::Button("Reset",       {bw1, 0})) plugin.ResetRun();
        ImGui::SameLine(0, sp);
        if (ImGui::Button("Split",       {bw2, 0})) plugin.TriggerManualSplit();
        ImGui::SameLine(0, sp);
        if (ImGui::Button(ICON_FA_COGS,  {bwg, 0}))
            SettingsWindow::Instance().NavigateToSection(plugin.SettingsName());
    }

    ImGui::Separator();

    if (plugin.RunComplete()) {
        const float avail = ImGui::GetContentRegionAvail().x;
        static const char* kMsg = "Run complete!";
        const float tw = ImGui::CalcTextSize(kMsg).x;
        const float cx = ImGui::GetCursorPosX() + (avail - tw) * 0.5f;
        ImGui::SetCursorPosX(cx > ImGui::GetCursorPosX() ? cx : ImGui::GetCursorPosX());
        ImGui::TextColored({0.4f, 1.f, 0.4f, 1.f}, "%s", kMsg);
        ImGui::Separator();
    }

    if (plugin.RunFailed()) {
        const float avail = ImGui::GetContentRegionAvail().x;
        static const char* kMsg = "Run failed";
        const float tw = ImGui::CalcTextSize(kMsg).x;
        const float cx = ImGui::GetCursorPosX() + (avail - tw) * 0.5f;
        ImGui::SetCursorPosX(cx > ImGui::GetCursorPosX() ? cx : ImGui::GetCursorPosX());
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(plugin.ActiveProfile().color_pb_behind), "%s", kMsg);
        ImGui::Separator();
    }

    // List picker — only visible when the run hasn't started so switching mid-run isn't possible.
    if (plugin.Clock().RealTime() == 0.0) {
        const char* current = (list && !list->name.empty()) ? list->name.c_str() : "(no list)";
        ImGui::SetNextItemWidth(-1.f);
        if (ImGui::BeginCombo("##list_picker", current)) {
            for (const auto& [name, path] : plugin.GetSavedLists()) {
                const bool selected = list && list->name == name;
                if (ImGui::Selectable(name.c_str(), selected))
                    plugin.LoadActiveList(path);
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::Separator();
    }

    if (!list || list->goals.empty()) {
        ImGui::TextDisabled("No goal list loaded. Open Settings to create one.");
        ImGui::Separator();
        plugin.DrawNuzlockeSection();
        ImGui::End();
        return;
    }

    // Skip headers when computing the first non-finished goal.
    int current_idx = -1;
    for (int i = 0; i < static_cast<int>(list->goals.size()); ++i) {
        if (list->goals[i].is_header) continue;
        const GoalStatus s = list->goals[i].status;
        if (s != GoalStatus::Completed && s != GoalStatus::Failed) { current_idx = i; break; }
    }

    // Manual/Running goals relay-start off the previous one, so this column never differs from it — SC's parallel start_trigger objectives (e.g. Deep rooms) are the only case where it can.
    SplitsProfile profile = plugin.ActiveProfile();
    const std::vector<double>& pb_real = plugin.CompareSplits();
    const std::vector<double>& pb_game = plugin.CompareSplitsGame();
    const auto nan = std::numeric_limits<double>::quiet_NaN();

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const float pad_x   = 2.f;
    const float pad_y   = 1.f;
    const float win_x   = ImGui::GetWindowPos().x;
    const float avail_w = ImGui::GetContentRegionAvail().x;

    // pb_idx counts only non-header goals; PB arrays are indexed the same way.
    int pb_idx = 0;

    for (int i = 0; i < static_cast<int>(list->goals.size()); ) {
        const auto& g = list->goals[i];

        if (g.is_header) {
            const bool hdr_open = DrawHeaderRow(*list, i, profile);
            ++i;
            if (!hdr_open) {
                // collapsed — skip until next header at same/shallower indent, advancing pb_idx to keep PB alignment
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

        DrawGoalRow(g, clock, i, i == current_idx,
                    pb_at_v(pb_real), pb_seg_v(pb_real),
                    pb_at_v(pb_game), pb_seg_v(pb_game),
                    profile);
        ++pb_idx;
        ++i;

        const float row_y1 = ImGui::GetCursorScreenPos().y + pad_y;
        draw_list->AddRect({win_x + pad_x, row_y0}, {win_x + avail_w - pad_x, row_y1},
                           IM_COL32(80, 80, 80, 140), 2.f);
    }

    ImGui::Separator();
    plugin.DrawNuzlockeSection();

    ImGui::End();
}

void SplitsGoalListWindow::DrawGoalRow(const GoalEntry& g, const GoalClock& /*clock*/,
                                        int /*index*/, bool is_current,
                                        double pb_split_real, double pb_seg_real,
                                        double pb_split_game, double pb_seg_game,
                                        const SplitsProfile& profile)
{
    const bool done = (g.status == GoalStatus::Completed || g.status == GoalStatus::Failed);

    ImVec4 color;
    if (g.status == GoalStatus::Failed)         color = ImGui::ColorConvertU32ToFloat4(profile.color_pb_behind);
    else if (g.status == GoalStatus::Completed) color = ImGui::ColorConvertU32ToFloat4(profile.color_completed);
    else if (is_current)                        color = ImGui::ColorConvertU32ToFloat4(profile.color_active);
    else                                        color = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);

    using TD = SplitsProfile::TimeDisplay;
    const auto td = profile.time_display;
    const bool show_real = (td != TD::Game) && !(td == TD::Both && profile.both_header_only);
    const bool show_game = (td != TD::Real);

    const ImVec4 real_col   = ImGui::ColorConvertU32ToFloat4(profile.color_real_time);
    const ImVec4 game_col   = ImGui::ColorConvertU32ToFloat4(profile.color_game_time);
    const ImVec4 ahead_col  = ImGui::ColorConvertU32ToFloat4(profile.color_pb_ahead);
    const ImVec4 behind_col = ImGui::ColorConvertU32ToFloat4(profile.color_pb_behind);
    auto muted = [](ImVec4 c) { return ImVec4{c.x, c.y, c.z, c.w * 0.55f}; };

    // SC forces Dynamic for every goal via profile.dynamic_by_default; a goal can still opt in individually outside SC via its own display_style.
    if (g.display_style == GoalEntry::DisplayStyle::Dynamic || profile.dynamic_by_default) {
        // Start/End/Duration for THIS goal alone, not "vs previous row" — needed for goals with an independent start_trigger (Quest pickup, Deep's parallel rooms). No PB/Average/Last-Run comparison: there's no meaningful history-relative segment for a start that isn't relay-locked to list order.
        const bool started = g.start_real_time >= 0.0;
        auto draw_or_dash = [&](const ImVec4& col, double t, bool valid) {
            char buf[32];
            if (valid) FormatTime(buf, sizeof(buf), t);
            else snprintf(buf, sizeof(buf), "--:--");
            ImGui::TextColored(col, "%s", buf);
        };

        if (!ImGui::BeginTable("##goalrow_dyn", 4, ImGuiTableFlags_None)) return;
        ImGui::TableSetupColumn("name",  ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("start", ImGuiTableColumnFlags_WidthFixed, 70.f);
        ImGui::TableSetupColumn("end",   ImGuiTableColumnFlags_WidthFixed, 70.f);
        ImGui::TableSetupColumn("dur",   ImGuiTableColumnFlags_WidthFixed, 70.f);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        if (g.indent > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + g.indent * 12.f);
        ImGui::TextColored(color, "%s", g.label.c_str());

        ImGui::TableSetColumnIndex(1); // Start
        if (show_real) draw_or_dash(real_col, g.start_real_time, started);
        if (show_game) draw_or_dash(game_col, g.start_game_time, started);

        ImGui::TableSetColumnIndex(2); // End
        if (show_real) draw_or_dash(real_col, g.split.real_time, done);
        if (show_game) draw_or_dash(game_col, g.split.game_time, done);

        ImGui::TableSetColumnIndex(3); // Duration = End - Start
        const bool have_duration = started && done;
        if (show_real) draw_or_dash(real_col, g.split.real_time - g.start_real_time, have_duration);
        if (show_game) draw_or_dash(game_col, g.split.game_time - g.start_game_time, have_duration);

        ImGui::EndTable();
        return;
    }

    if (!ImGui::BeginTable("##goalrow", 3, ImGuiTableFlags_None)) return;
    ImGui::TableSetupColumn("name",  ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("time",  ImGuiTableColumnFlags_WidthFixed, 110.f);
    ImGui::TableSetupColumn("seg",   ImGuiTableColumnFlags_WidthFixed, profile.show_segment ? 80.f : 0.f);
    ImGui::TableNextRow();

    ImGui::TableSetColumnIndex(0);
    if (g.indent > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + g.indent * 12.f);
    ImGui::TextColored(color, "%s", g.label.c_str());

    if (g.status != GoalStatus::Completed && g.trigger.type == GoalTrigger::Type::ReachTitleRank) {
        auto* title = GW::PlayerMgr::GetTitleTrack(g.trigger.title_id);
        if (title) {
            if (title->current_points == 0) {
                ImGui::TextDisabled("No progress detected");
            } else {
            const bool at_max = (title->points_needed_next_rank == 0xFFFFFFFF);
            char buf[96];
            if (title->is_percentage_based()) {
                // Points are percentage × 10 (e.g. 863 = 86.3%)
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
            } // end else (has progress)
        }
    }

    if (g.status != GoalStatus::Completed && g.trigger.type == GoalTrigger::Type::MobKill) {
        const uint32_t target = g.trigger.param2 > 0 ? g.trigger.param2 : 1;
        ImGui::TextDisabled("kills: %d / %u", g.trigger_progress, target);
    }

    {
        ImGui::TableSetColumnIndex(1);
        if (done) {
            if (show_real) {
                char buf[32]; FormatTime(buf, sizeof(buf), g.split.real_time);
                ImGui::TextColored(real_col, "%s", buf);
                if (profile.show_split_pb) DrawPBDelta(g.split.real_time, pb_split_real, ahead_col, behind_col);
            }
            if (show_game) {
                char buf[32]; FormatTime(buf, sizeof(buf), g.split.game_time);
                ImGui::TextColored(game_col, "%s", buf);
                if (profile.show_split_pb) DrawPBDelta(g.split.game_time, pb_split_game, ahead_col, behind_col);
            }
        } else if (is_current) {
            ImGui::TextDisabled("---");
        }
    }

    if (profile.show_segment) {
        ImGui::TableSetColumnIndex(2);
        if (done) {
            if (show_real) {
                char seg[32]; FormatTime(seg, sizeof(seg), g.split.segment_real);
                ImGui::TextColored(muted(real_col), "+%s", seg);
                if (profile.show_segment_pb) DrawPBDelta(g.split.segment_real, pb_seg_real, ahead_col, behind_col);
            }
            if (show_game) {
                char seg[32]; FormatTime(seg, sizeof(seg), g.split.segment_game);
                ImGui::TextColored(muted(game_col), "+%s", seg);
                if (profile.show_segment_pb) DrawPBDelta(g.split.segment_game, pb_seg_game, ahead_col, behind_col);
            }
        }
    }

    ImGui::EndTable();
}

bool SplitsGoalListWindow::DrawHeaderRow(const GoalList& list, int header_idx, const SplitsProfile& profile)
{
    const GoalEntry& h = list.goals[header_idx];

    using TD = SplitsProfile::TimeDisplay;
    const bool gt = (profile.time_display == TD::Game) ||
                    (profile.time_display == TD::Both && profile.both_header_only);

    // Derive timing and status from all non-header descendants.
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

    // Build label — use ### so the ID stays stable while the time text changes.
    char label[160];
    // List name is part of the ID so headers from different lists never share ImGui collapse state.
    if (end_t >= 0.0) {
        char tbuf[32]; FormatTime(tbuf, sizeof(tbuf), end_t);
        if (status == GoalStatus::Failed)
            snprintf(label, sizeof(label), "%s - %s [Failed]###%s_hdr%d", h.label.c_str(), tbuf, list.name.c_str(), header_idx);
        else
            snprintf(label, sizeof(label), "%s - %s###%s_hdr%d", h.label.c_str(), tbuf, list.name.c_str(), header_idx);
    } else {
        snprintf(label, sizeof(label), "%s###%s_hdr%d", h.label.c_str(), list.name.c_str(), header_idx);
    }

    // Tint the bar red for failed runs; everything else uses the toolbox theme colour.
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

void SplitsGoalListWindow::LoadSettings(SettingsDoc& /*doc*/, ToolboxIni* /*legacy*/) {}

void SplitsGoalListWindow::SaveSettings(SettingsDoc& /*doc*/) {}

// ---------------------------------------------------------------------------
// Settings panel
// ---------------------------------------------------------------------------
void SplitsGoalListWindow::DrawSettings(SplitsWindow& plugin)
{
    GoalList* list = plugin.List();
    if (list_name_buf_[0] == '\0' && !list->name.empty())
        snprintf(list_name_buf_, sizeof(list_name_buf_), "%s", list->name.c_str());

    DrawProfileSwitcher(plugin);
    ImGui::Separator();

    ImGui::Columns(3, "settings_cols", false);
    DrawTimeAndBehaviorColumn(plugin);
    ImGui::NextColumn();
    DrawKeybindsAndColorsColumn(plugin);
    ImGui::NextColumn();
    DrawGoalListManagementColumn(plugin);
    ImGui::Columns(1);
    ImGui::Separator();

    // SC's lists are tool-generated by the Dungeon/Elite Area presets and aren't meant for
    // hand-editing; Manual/Running lists are user-authored and fully editable. This is the one
    // place the two profile families genuinely diverge in shape, so it's a single top-level
    // branch into two self-contained methods rather than conditionals threaded through shared code.
    if (plugin.ActiveProfile().dynamic_by_default)
        DrawSCGoalsSummary(plugin);
    else
        DrawEditableGoalsList(plugin);

    ImGui::Separator();
    ImGui::TextUnformatted("Add Goal:");
    if (plugin.ActiveProfileIdx() == 1)
        DrawRunningLegPicker(plugin); // Running profile: combined from/to leg picker only.
    else
        DrawStandardAddGoalForm(plugin);
}

// ---------------------------------------------------------------------------
// DrawSettings sections — split out so each profile's UI lives in one named,
// self-contained method instead of conditionals scattered through a shared draw path.
// ---------------------------------------------------------------------------

void SplitsGoalListWindow::DrawProfileSwitcher(SplitsWindow& plugin)
{
    ImGui::TextUnformatted("Profile:");
    for (int i = 0; i < kProfileCount; ++i) {
        ImGui::SameLine();
        const bool active = (plugin.ActiveProfileIdx() == i);
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button,        ImGui::GetStyleColorVec4(ImGuiCol_Header));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_HeaderHovered));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
        }
        if (ImGui::Button(plugin.Profiles()[i].name.c_str())) {
            plugin.SwitchProfile(i);
            list_name_buf_[0] = '\0';
        }
        if (active) {
            ImGui::PopStyleColor(3);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Active — auto-loads on next session");
        }
    }
}

void SplitsGoalListWindow::DrawTimeAndBehaviorColumn(SplitsWindow& plugin)
{
    SplitsProfile& p = plugin.ActiveProfile();
    using TD = SplitsProfile::TimeDisplay;
    ImGui::TextUnformatted("Time:");
    ImGui::SameLine();
    if (ImGui::RadioButton("Game", p.time_display == TD::Game)) p.time_display = TD::Game;
    ImGui::SameLine();
    if (ImGui::RadioButton("Real", p.time_display == TD::Real)) p.time_display = TD::Real;
    ImGui::SameLine();
    if (ImGui::RadioButton("Both", p.time_display == TD::Both)) p.time_display = TD::Both;
    if (p.time_display == TD::Both) {
        ImGui::SameLine(0, 12.f);
        ImGui::Checkbox("Clock only", &p.both_header_only);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Show Real+Game in the header clock only;\ngoal rows display game time.");
    }
    // Comparison-vs-history is meaningless for SC (dynamic_by_default forces Start/End/Duration display, never a PB/Average/Last-Run delta) — hidden rather than left as dead controls.
    if (!p.dynamic_by_default) {
        ImGui::TextUnformatted("Compare vs:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.f);
        {
            using CM = SplitsProfile::ComparisonMode;
            static const char* cm_labels[] = { "PB", "Average", "Last Run" };
            int cm_idx = static_cast<int>(p.comparison_mode);
            if (ImGui::Combo("##cmpmode", &cm_idx, cm_labels, 3))
                p.comparison_mode = static_cast<CM>(cm_idx);
        }
        ImGui::Checkbox("Show total delta", &p.show_split_pb);
        ImGui::Checkbox("Show split column", &p.show_segment);
        if (p.show_segment) {
            ImGui::SameLine();
            ImGui::Checkbox("Show split delta", &p.show_segment_pb);
        }
    }
    ImGui::Checkbox("Auto /age on completion", &p.auto_send_age);
    ImGui::Checkbox("Show paused time",        &p.show_paused_time);

    ImGui::Spacing();
    ImGui::Checkbox("Stop on party wipe", &p.stop_on_party_defeated);
    // Label/tooltip just reflect what this profile's goals actually use; GoalEngine::Update() itself checks both trigger sets regardless of profile.
    const char* fail_label = p.dynamic_by_default
        ? "Auto-fail on rezone (Dungeon/Elite Area)"
        : "Auto-fail on rezone (VQ/Mission/Bonus)";
    const char* fail_tooltip = p.dynamic_by_default
        ? "Fails the run if you leave the current Dungeon/Elite Area goal's map\n"
          "while it's still in progress \xe2\x80\x94 mirrors OT's own ObjectiveSet::StopObjectives():\n"
          "any objective still in progress when you leave the area is marked failed,\n"
          "not just on a party wipe."
        : "Fails the run if you leave a Vanquish/Mission/Bonus goal's map\n"
          "while it's still in progress \xe2\x80\x94 i.e. you attempted it and\n"
          "left without finishing, rather than completing it first.";
    ImGui::Checkbox(fail_label, &p.auto_fail_on_rezone);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", fail_tooltip);
}

void SplitsGoalListWindow::DrawKeybindsAndColorsColumn(SplitsWindow& plugin)
{
    auto vk_name = [](int vk) -> const char* {
        if (vk <= 0) return "None";
        static char buf[32];
        const LONG scan = MapVirtualKeyA(static_cast<UINT>(vk), MAPVK_VK_TO_VSC) << 16;
        if (scan && GetKeyNameTextA(scan, buf, sizeof(buf)) > 0) return buf;
        snprintf(buf, sizeof(buf), "VK %d", vk);
        return buf;
    };

    static int* capturing_key    = nullptr;
    static bool capturing_active = false;

    auto draw_keybind = [&](const char* label, int& key) {
        char btn_lbl[64];
        snprintf(btn_lbl, sizeof(btn_lbl), "%s##kb_%s", vk_name(key), label);
        const bool is_capturing = (capturing_key == &key);
        if (is_capturing) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.3f, 0.3f, 1.f));
            ImGui::Button("Press key...");
            ImGui::PopStyleColor();
            for (int vk = 1; vk < 256; ++vk) {
                if (vk == VK_LBUTTON || vk == VK_RBUTTON || vk == VK_MBUTTON) continue;
                if (GetAsyncKeyState(vk) & 0x8000) {
                    key = (vk == VK_ESCAPE) ? 0 : vk;
                    capturing_key    = nullptr;
                    capturing_active = false;
                    break;
                }
            }
        } else {
            if (ImGui::Button(btn_lbl)) {
                capturing_key    = &key;
                capturing_active = true;
            }
        }
        ImGui::SameLine();
        if (ImGui::SmallButton(("x##clr_" + std::string(label)).c_str())) key = 0;
        ImGui::SameLine();
        ImGui::TextUnformatted(label);
    };

    SplitsProfile& p = plugin.ActiveProfile();

    ImGui::TextUnformatted("Keybinds:");
    draw_keybind("Start", plugin.KeyStart());
    draw_keybind("Reset", plugin.KeyReset());
    draw_keybind("Split", plugin.KeySplit());
    if (capturing_active)
        ImGui::TextDisabled("(Esc to clear)");

    ImGui::Spacing();
    ImGui::ColorButtonPicker("Completed##sc", &p.color_completed); ImGui::SameLine(); ImGui::TextUnformatted("Completed");
    ImGui::SameLine(0, 12.f);
    ImGui::ColorButtonPicker("Current##sc",   &p.color_active);    ImGui::SameLine(); ImGui::TextUnformatted("Current");
    ImGui::ColorButtonPicker("Real##sc",      &p.color_real_time); ImGui::SameLine(); ImGui::TextUnformatted("Real");
    ImGui::SameLine(0, 12.f);
    ImGui::ColorButtonPicker("Game##sc",      &p.color_game_time); ImGui::SameLine(); ImGui::TextUnformatted("Game");
    // Ahead is only ever drawn for the comparison delta, never in Dynamic mode, so hidden for SC. Behind stays visible either way since it also doubles as the Failed goal-label color.
    if (!p.dynamic_by_default) {
        ImGui::ColorButtonPicker("Ahead##sc", &p.color_pb_ahead); ImGui::SameLine(); ImGui::TextUnformatted("Ahead");
        ImGui::SameLine(0, 12.f);
    }
    ImGui::ColorButtonPicker("Behind##sc",    &p.color_pb_behind); ImGui::SameLine(); ImGui::TextUnformatted("Behind");
    ImGui::Spacing();
}

void SplitsGoalListWindow::DrawGoalListManagementColumn(SplitsWindow& plugin)
{
    GoalList* list = plugin.List();

    ImGui::TextUnformatted("Goal List");
    ImGui::SetNextItemWidth(130.f);
    ImGui::InputText("##listname", list_name_buf_, sizeof(list_name_buf_));
    ImGui::SameLine();
    if (ImGui::Button("New")) {
        plugin.NewActiveList(list_name_buf_);
        list = plugin.List();
    }
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
        list->name = list_name_buf_;
        plugin.SaveActiveList();
    }

    const auto saved = plugin.GetSavedLists();
    if (!saved.empty()) {
        ImGui::TextUnformatted("Load:");
        ImGui::Indent();
        for (const auto& [display, path] : saved) {
            const bool is_active = (display == list->name);
            if (is_active) {
                ImGui::PushStyleColor(ImGuiCol_Button,        ImGui::GetStyleColorVec4(ImGuiCol_Header));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_HeaderHovered));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
            }
            if (ImGui::Button(display.c_str())) {
                plugin.LoadActiveList(path);
                list = plugin.List();
                snprintf(list_name_buf_, sizeof(list_name_buf_), "%s", list->name.c_str());
            }
            if (is_active) {
                ImGui::PopStyleColor(3);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Currently loaded");
            }
        }
        ImGui::Unindent();
    }
    // SC's Dungeon/Elite Area presets aren't browsed here — built live from SCPresets on demand, never saved to disk.
}

void SplitsGoalListWindow::DrawSCGoalsSummary(SplitsWindow& plugin)
{
    // SC lists are a complete, tool-generated structure — editing by hand would just break the relay chain the picker built, and the dropdown only offers fresh Dungeon/Elite Area picks anyway. No editable rows here; the live Splits window shows the actual goals once running.
    GoalList* list = plugin.List();
    int goal_count = 0;
    for (const auto& g : list->goals) if (!g.is_header) ++goal_count;
    ImGui::Text("Goals: %d (%s)", goal_count, list->name.empty() ? "none loaded" : list->name.c_str());
}

void SplitsGoalListWindow::DrawEditableGoalsList(SplitsWindow& plugin)
{
    ImGui::TextUnformatted("Goals:");
    GoalList* list = plugin.List();
    bool erased = false;
    bool moved = false;
    int first_goal_idx = -1;
    for (int k = 0; k < static_cast<int>(list->goals.size()); ++k) {
        if (!list->goals[k].is_header) { first_goal_idx = k; break; }
    }
    for (int i = 0; i < static_cast<int>(list->goals.size()) && !erased && !moved; ++i) {
        auto& g = list->goals[i];
        ImGui::PushID(i);

        if (g.is_header) {
            ImGui::TextColored({1.f, 0.85f, 0.4f, 1.f}, "[HDR]");
        } else {
            const char* tname = "Man";
            switch (g.trigger.type) {
                case GoalTrigger::Type::MapEnter:         tname = "Map";  break;
                case GoalTrigger::Type::EnterExplorable:  tname = "Exp";  break;
                case GoalTrigger::Type::ExitExplorable:   tname = "Exit"; break;
                case GoalTrigger::Type::EnterOutpost:     tname = "Out";  break;
                case GoalTrigger::Type::VanquishComplete: tname = "VQ";   break;
                case GoalTrigger::Type::MissionComplete:  tname = g.trigger.hard_mode ? "HM"  : "Mis"; break;
                case GoalTrigger::Type::MissionBonus:     tname = g.trigger.hard_mode ? "HMB" : "Bon"; break;
                case GoalTrigger::Type::ReachLevel:       tname = "Lv";   break;
                default: break;
            }
            ImGui::TextDisabled("[%s]", tname);
        }
        ImGui::SameLine();
        // Visual indent hint
        for (int d = 0; d < g.indent; ++d) { ImGui::TextDisabled("|"); ImGui::SameLine(0, 2); }
        ImGui::TextUnformatted(g.label.c_str());
        ImGui::SameLine();

        // Indent controls
        if (g.indent > 0) {
            if (ImGui::SmallButton("<##ui")) { g.indent--; moved = true; }
        } else {
            ImGui::BeginDisabled(); ImGui::SmallButton("<##ui"); ImGui::EndDisabled();
        }
        ImGui::SameLine();
        if (ImGui::SmallButton(">##ui")) { g.indent++; moved = true; }
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

        // Only the first goal's start behavior is ever in question — every later goal relays immediately off the previous one's completion (start_trigger isn't set through this editor).
        if (i == first_goal_idx) {
            if (plugin.ActiveProfileIdx() == 1) {
                ImGui::TextDisabled("Autostart --> movement detected in an explorable");
            } else if (g.starts_immediately) {
                ImGui::TextDisabled("Autostart --> begins at run start");
            } else if (g.trigger.type == GoalTrigger::Type::MissionComplete ||
                       g.trigger.type == GoalTrigger::Type::MissionBonus ||
                       g.trigger.type == GoalTrigger::Type::VanquishComplete) {
                const std::string& map_name = Resources::GetMapName(g.trigger.map_id)->string();
                ImGui::TextDisabled("Autostart --> entering %s",
                    map_name.empty() ? "target map" : map_name.c_str());
            } else {
                ImGui::TextDisabled("Not an autostart goal \xe2\x80\x94 press Start manually");
            }
        }

        ImGui::PopID();
    }
}

void SplitsGoalListWindow::DrawStandardAddGoalForm(SplitsWindow& plugin)
{
    GoalList* list = plugin.List();
    SplitsProfile& p = plugin.ActiveProfile();

    // Sentinel -1 = "Header" (no trigger), -2 = "Quest" (adds a QuestPickup + QuestComplete pair from one label/ID entry).
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
    // SC is single-purpose (dungeons/elite areas only) — Manual's other trigger types aren't offered at all rather than shown-but-unused.
    static const TriggerOpt trigger_opts_sc[] = {
        { "Dungeons",    static_cast<int>(GoalTrigger::Type::DungeonReward) },
        { "Elite Areas", static_cast<int>(GoalTrigger::Type::ObjectiveDone) },
    };
    const bool is_sc = p.dynamic_by_default;
    const TriggerOpt* trigger_opts      = is_sc ? trigger_opts_sc : trigger_opts_full;
    int               trigger_opts_count = is_sc ? static_cast<int>(std::size(trigger_opts_sc))
                                                    : static_cast<int>(std::size(trigger_opts_full));
    // Default to a valid SC option the first time SC becomes active with a leftover Manual-only selection, otherwise nothing in the loop below matches and the combo shows the wrong label.
    if (is_sc && edit_trigger_type_ != trigger_opts_sc[0].type_int && edit_trigger_type_ != trigger_opts_sc[1].type_int)
        edit_trigger_type_ = trigger_opts_sc[0].type_int;
    const char* current_trigger_label = trigger_opts[0].label;
    for (int i = 0; i < trigger_opts_count; ++i)
        if (trigger_opts[i].type_int == edit_trigger_type_) { current_trigger_label = trigger_opts[i].label; break; }
    ImGui::SetNextItemWidth(220.f);
    if (ImGui::BeginCombo("Trigger##add", current_trigger_label)) {
        for (int i = 0; i < trigger_opts_count; ++i) {
            const bool selected = (trigger_opts[i].type_int == edit_trigger_type_);
            if (ImGui::Selectable(trigger_opts[i].label, selected))
                edit_trigger_type_ = trigger_opts[i].type_int;
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    const bool is_header_mode      = (edit_trigger_type_ == -1);
    const bool is_mission_batch    = (edit_trigger_type_ == static_cast<int>(GoalTrigger::Type::MissionComplete) ||
                                      edit_trigger_type_ == static_cast<int>(GoalTrigger::Type::MissionBonus));
    const bool is_explorable_batch = (edit_trigger_type_ == static_cast<int>(GoalTrigger::Type::MapEnter));
    const bool is_town_batch       = (edit_trigger_type_ == static_cast<int>(GoalTrigger::Type::EnterExplorable));
    const bool is_dungeon_batch    = (edit_trigger_type_ == static_cast<int>(GoalTrigger::Type::DungeonReward));
    const bool is_elite_area_batch = (edit_trigger_type_ == static_cast<int>(GoalTrigger::Type::ObjectiveDone));
    const bool is_title_picker     = (edit_trigger_type_ == static_cast<int>(GoalTrigger::Type::ReachTitleRank));
    const bool needs_level         = (edit_trigger_type_ == static_cast<int>(GoalTrigger::Type::ReachLevel));
    const bool is_quest_mode       = (edit_trigger_type_ == -2);
    const bool needs_quest_id      = is_quest_mode;
    const bool needs_skill_id      = (edit_trigger_type_ == static_cast<int>(GoalTrigger::Type::SkillLearnt));
    const bool needs_mob_id        = (edit_trigger_type_ == static_cast<int>(GoalTrigger::Type::MobKill));

    if (is_mission_batch) {
        DrawMissionBatchPicker(plugin);
    } else if (is_explorable_batch) {
        DrawExplorableBatchPicker(plugin);
    } else if (is_town_batch) {
        DrawTownBatchPicker(plugin);
    } else if (is_dungeon_batch) {
        DrawDungeonBatchPicker(plugin);
    } else if (is_elite_area_batch) {
        DrawEliteAreaBatchPicker(plugin);
    } else if (is_title_picker) {
        DrawTitlePicker(plugin);
    } else {
        ImGui::SetNextItemWidth(200.f);
        ImGui::InputText("Label##add", edit_label_, sizeof(edit_label_));
        if (needs_level) {
            ImGui::SetNextItemWidth(100.f);
            ImGui::InputInt("Level##add", &edit_level_);
            if (edit_level_ < 1)  edit_level_ = 1;
            if (edit_level_ > 20) edit_level_ = 20;
        }
        if (needs_quest_id) {
            ImGui::SetNextItemWidth(120.f);
            ImGui::InputInt("Quest ID##add", &edit_quest_id_);
            if (edit_quest_id_ < 0) edit_quest_id_ = 0;
        }
        if (needs_skill_id) {
            ImGui::SetNextItemWidth(120.f);
            ImGui::InputInt("Skill ID##add", &edit_skill_id_);
            if (edit_skill_id_ < 0) edit_skill_id_ = 0;
        }
        if (needs_mob_id) {
            ImGui::SetNextItemWidth(120.f);
            ImGui::InputInt("Model ID##add", &edit_mob_id_);
            if (edit_mob_id_ < 0) edit_mob_id_ = 0;
            ImGui::SetNextItemWidth(120.f);
            ImGui::InputInt("Kill Count##add", &edit_mob_kill_count_);
            if (edit_mob_kill_count_ < 1) edit_mob_kill_count_ = 1;
        }
        const bool can_add = edit_label_[0] != '\0';
        if (!can_add) ImGui::BeginDisabled();
        if (is_header_mode) {
            if (ImGui::Button("Add Header")) {
                GoalEntry hdr;
                hdr.is_header = true;
                hdr.label     = edit_label_;
                list->goals.push_back(std::move(hdr));
                edit_label_[0] = '\0';
            }
        } else if (is_quest_mode) {
            if (ImGui::Button("Add Goal")) {
                // Flat: two plain completion-triggered goals like every other Manual goal, so the list stays relay-chained/PB-comparable instead of mixing in Dynamic's look.
                GoalEntry pickup;
                pickup.label          = std::string(edit_label_) + " Pickup";
                pickup.trigger.type   = GoalTrigger::Type::QuestPickup;
                pickup.trigger.map_id = GW::Constants::MapID::None;
                pickup.trigger.param1 = static_cast<uint32_t>(edit_quest_id_);
                list->goals.push_back(std::move(pickup));

                GoalEntry complete;
                complete.label          = std::string(edit_label_) + " Complete";
                complete.trigger.type   = GoalTrigger::Type::QuestComplete;
                complete.trigger.map_id = GW::Constants::MapID::None;
                complete.trigger.param1 = static_cast<uint32_t>(edit_quest_id_);
                list->goals.push_back(std::move(complete));

                list->RenumberDuplicateLabels();
                edit_label_[0] = '\0';
            }
        } else {
            if (ImGui::Button("Add Goal")) {
                GoalEntry new_g;
                new_g.label          = edit_label_;
                new_g.trigger.type   = static_cast<GoalTrigger::Type>(edit_trigger_type_);
                new_g.trigger.map_id = GW::Constants::MapID::None;
                new_g.trigger.level  = edit_level_;
                if (needs_skill_id) new_g.trigger.param1 = static_cast<uint32_t>(edit_skill_id_);
                if (needs_mob_id) {
                    new_g.trigger.param1 = static_cast<uint32_t>(edit_mob_id_);
                    new_g.trigger.param2 = static_cast<uint32_t>(edit_mob_kill_count_);
                }
                list->goals.push_back(std::move(new_g));
                list->RenumberDuplicateLabels();
                edit_label_[0] = '\0';
            }
        }
        if (!can_add) ImGui::EndDisabled();

        if (is_quest_mode || needs_skill_id) {
            ImGui::SameLine();
            if (ImGui::Button("Find ID##wiki")) {
                OpenGameIntegrationWikiPage();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Opens the wiki's Quest/Skill ID lookup tables\n(no in-game ID list exists for either)");
            }
        }
        if (needs_mob_id) {
            ImGui::SameLine();
            if (ImGui::Button("Find ID##info")) {
                InfoWindow::Instance().visible = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Opens the Info window \xe2\x80\x94 target the mob in-game and\nread its Model ID off the Target section (no wiki needed).");
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Title picker
// ---------------------------------------------------------------------------
void SplitsGoalListWindow::DrawTitlePicker(SplitsWindow& plugin)
{
    using TitleID = GW::Constants::TitleID;

    struct TitleEntry { TitleID id; std::unique_ptr<GuiUtils::EncString> enc; std::string name; };
    // Built once, lazily: every non-deprecated title, name decoded from the game's own title data — same enumeration TitleTrackerWidget uses, sorted alphabetically instead of grouped.
    static std::vector<TitleEntry> s_titles;
    static bool s_titles_sorted = false;
    if (s_titles.empty()) {
        for (uint32_t i = 0; i <= static_cast<uint32_t>(TitleID::Codex); ++i) {
            const auto id = static_cast<TitleID>(i);
            if (GW::PlayerMgr::IsDeprecatedTitle(id)) continue;
            const auto* data = GW::PlayerMgr::GetTitleData(id);
            if (!data) { s_titles.clear(); break; } // title data not loaded yet at all — retry next frame
            TitleEntry e;
            e.id  = id;
            e.enc = std::make_unique<GuiUtils::EncString>(data->name_id);
            s_titles.push_back(std::move(e));
        }
    }
    // Names decode asynchronously — keep polling until every entry has one before sorting/displaying, so no two rows can show the same blank label at once (ImGui ID collision) mid-decode.
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
    ImGui::InputText("##titlefilter", title_filter_buf_, sizeof(title_filter_buf_));
    ImGui::SameLine(); if (ImGui::SmallButton("x##titlefx")) title_filter_buf_[0] = '\0';

    const bool searching = title_filter_buf_[0] != '\0';

    if (ImGui::BeginListBox("##titlelist", { -1.f, 180.f })) {
        for (int i = 0; i < NUM_TITLES; ++i) {
            const TitleEntry& e = s_titles[i];
            if (searching && !TextUtils::CaseInsensitiveContains(e.name, title_filter_buf_)) continue;
            ImGui::PushID(static_cast<int>(e.id));
            const bool selected = (edit_title_id_ == static_cast<int>(e.id));
            if (ImGui::Selectable(e.name.c_str(), selected)) {
                if (edit_title_id_ != static_cast<int>(e.id)) {
                    edit_title_id_ = static_cast<int>(e.id);
                    edit_title_rank_ = -1; // reset rank so it defaults to max for the new title
                }
            }
            if (selected) ImGui::SetItemDefaultFocus();
            ImGui::PopID();
        }
        ImGui::EndListBox();
    }

    const char* sel_title_name = nullptr;
    for (int i = 0; i < NUM_TITLES; ++i) {
        if (static_cast<int>(s_titles[i].id) == edit_title_id_) {
            sel_title_name = s_titles[i].name.c_str();
            break;
        }
    }

    auto* live_title = GW::PlayerMgr::GetTitleTrack(static_cast<TitleID>(edit_title_id_));
    // GetTitleTrack() returns null for a title with zero progress — that's not "disconnected", just unstarted. A goal can still target Rank 1; GoalEngine resolves the real tier-index anchor once progress starts.
    const int  total_ranks = live_title ? static_cast<int>(live_title->max_title_rank) : 0;
    const bool has_a_rank_to_pick = (live_title != nullptr) && total_ranks > 0;
    const bool can_add = (sel_title_name != nullptr);

    // Rank selector: max_title_rank = total ranks; max_title_tier_index = global flat index of rank-1 (anchor, only known once progress exists — see GoalEngine's ReachTitleRank match).
    if (sel_title_name && has_a_rank_to_pick) {
        const int max_tier_base = static_cast<int>(live_title->max_title_tier_index);
        const bool pct_title    = live_title->is_percentage_based();
        auto* wc = GW::GetWorldContext();

        if (edit_title_rank_ <= 0 || edit_title_rank_ > total_ranks)
            edit_title_rank_ = total_ranks;

        ImGui::Text("Target rank (1..%d):", total_ranks);
        if (ImGui::BeginListBox("##rankbox", { -1.f, 80.f })) {
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
                const bool rank_sel = (edit_title_rank_ == r);
                if (ImGui::Selectable(rank_label, rank_sel))
                    edit_title_rank_ = r;
                if (rank_sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndListBox();
        }
    } else if (sel_title_name) {
        edit_title_rank_ = 1; // no progress yet, so nothing to pick from — target Rank 1, the common case for a fresh title
        ImGui::TextDisabled("(no progress detected yet \xe2\x80\x94 will target Rank 1; reopen this once you've started the title to pick a different rank)");
    }

    if (!can_add) ImGui::BeginDisabled();
    if (ImGui::Button("Add Goal##titleadd")) {
        char label_buf[160];
        if (has_a_rank_to_pick)
            snprintf(label_buf, sizeof(label_buf), "%s r%d/%d", sel_title_name, edit_title_rank_, total_ranks);
        else
            snprintf(label_buf, sizeof(label_buf), "%s r%d", sel_title_name, edit_title_rank_);

        GoalEntry g;
        g.label            = label_buf;
        g.trigger.type     = GoalTrigger::Type::ReachTitleRank;
        g.trigger.title_id = static_cast<TitleID>(edit_title_id_);
        // Relative target rank (1-based), not a stored tier index — GoalEngine resolves the absolute anchor against the live Title* at match time (see its ReachTitleRank case for why).
        g.trigger.level    = edit_title_rank_;
        g.trigger.map_id   = GW::Constants::MapID::None;

        GoalList* list = plugin.List();
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

// ---------------------------------------------------------------------------
// Running profile: sequential split picker
// ---------------------------------------------------------------------------
void SplitsGoalListWindow::DrawRunningLegPicker(SplitsWindow& plugin)
{
    using MapID = GW::Constants::MapID;

    struct MapRow { int id; std::string name; };

    auto build_map_list = []() -> std::vector<MapRow> {
        std::unordered_map<uint32_t, int> seen;
        std::vector<MapRow> out;
        for (int id = 1; id < static_cast<int>(MapID::Count); ++id) {
            const auto mid = static_cast<MapID>(id);
            const auto* inf = GW::Map::GetMapInfo(mid);
            if (!inf || !inf->name_id) continue;
            if (inf->region != GW::Region_Presearing && !inf->GetIsOnWorldMap()) continue;
            if (seen.count(inf->name_id)) continue;
            seen[inf->name_id] = id;
            out.push_back({ id, Resources::GetMapName(mid)->string() });
        }
        std::sort(out.begin(), out.end(), [](const MapRow& a, const MapRow& b) {
            return a.name < b.name;
        });
        return out;
    };

    static std::vector<MapRow> s_map_list;
    if (s_map_list.empty()) s_map_list = build_map_list();

    const std::string sel_name = run_map_id_ > 0 ? Resources::GetMapName(static_cast<MapID>(run_map_id_))->string() : std::string{};
    const char* preview = run_map_id_ > 0 ? sel_name.c_str() : "(pick map)";
    ImGui::SetNextItemWidth(200.f);
    if (ImGui::BeginCombo("##runmap", preview)) {
        ImGui::SetNextItemWidth(-1.f);
        ImGui::InputText("##runmapflt", run_filter_, sizeof(run_filter_));
        for (const auto& row : s_map_list) {
            if (run_filter_[0] != '\0' && !TextUtils::CaseInsensitiveContains(row.name, run_filter_))
                continue;
            const bool sel = (row.id == run_map_id_);
            if (ImGui::Selectable(row.name.c_str(), sel)) {
                run_map_id_ = row.id;
                const auto nm = Resources::GetMapName(static_cast<MapID>(row.id))->string();
                snprintf(edit_label_, sizeof(edit_label_), "%s", nm.c_str());
                run_filter_[0] = '\0';
            }
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(180.f);
    ImGui::InputText("##runlabel", edit_label_, sizeof(edit_label_));

    const bool can_add = run_map_id_ > 0 && edit_label_[0] != '\0';
    if (!can_add) ImGui::BeginDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Add Split")) {
        GoalList* list = plugin.List();
        GoalEntry g;
        g.label          = edit_label_;
        g.trigger.type   = GoalTrigger::Type::MapEnter;
        g.trigger.map_id = static_cast<MapID>(run_map_id_);
        list->goals.push_back(std::move(g));
        list->RenumberDuplicateLabels();
        edit_label_[0] = '\0';
        run_map_id_     = 0;
    }
    if (!can_add) ImGui::EndDisabled();
    ImGui::TextDisabled("Fires on each explorable entry in order. Last split completes the run.");
}

namespace {
// Shared by DrawTownBatchPicker/DrawExplorableBatchPicker below: both pickers are
// "list every map matching a type filter, grouped into campaign tabs + Pre-Searing,
// with N per-row checkbox columns and a bulk Add" — the only real differences are which
// GW::RegionTypes count and how many/what columns are offered. Consolidated here instead
// of the two near-identical ~180-line copies this used to be (Town: Enter/Leave only,
// Explorable: Enter/VQ/Leave) so a future third column or map-type filter only needs to
// change one place.
struct BatchMapRow { int id; std::string name; GW::Region region; };

struct BatchColumn {
    uint8_t bit;
    GoalTrigger::Type trigger_type;
    const char* header;      // table column header, e.g. "Enter"
    const char* quick_label; // bulk-select button suffix, e.g. "En"
    const char* goal_prefix; // goal label prefix, e.g. "Enter "
};

constexpr GW::Constants::Campaign kBatchCampaigns[] = {
    GW::Constants::Campaign::Prophecies, GW::Constants::Campaign::Factions,
    GW::Constants::Campaign::Nightfall, GW::Constants::Campaign::EyeOfTheNorth,
};
constexpr const char* kBatchCampaignLabels[] = { "Prophecies", "Factions", "Nightfall", "EotN" };

// region_ok filters which maps are candidates (by campaign for the main tabs, by Presearing
// for that tab); type_ok additionally filters by map type and, on ties for the same name_id,
// keeps whichever candidate reports the lower rank (Town never sets rank, so ties keep
// whichever map id was encountered first, matching the original per-picker behavior).
std::vector<BatchMapRow> BuildBatchMapRows(
    const std::function<bool(const GW::AreaInfo&)>& region_ok,
    const std::function<bool(const GW::AreaInfo&, int&)>& type_ok)
{
    using MapID = GW::Constants::MapID;
    struct Cand { int id; int rank; GW::Region region; };
    std::unordered_map<uint32_t, Cand> best;
    for (int id = 1; id < static_cast<int>(MapID::Count); ++id) {
        const auto mid  = static_cast<MapID>(id);
        const auto* inf = GW::Map::GetMapInfo(mid);
        if (!inf || !inf->name_id || !region_ok(*inf)) continue;
        int rank = 0;
        if (!type_ok(*inf, rank)) continue;
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

void DrawMapBatchPicker(SplitsWindow& plugin, const char* id_prefix,
                        char* filter_buf, size_t filter_buf_size,
                        std::map<int, uint8_t>& checked,
                        const std::function<bool(const GW::AreaInfo&, int&)>& type_ok,
                        const std::vector<BatchColumn>& columns)
{
    using MapID = GW::Constants::MapID;
    using Camp  = GW::Constants::Campaign;

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
            if (ci) ImGui::SameLine(0, 10);
            const uint8_t bit = columns[ci].bit;
            char all_lbl[32];  snprintf(all_lbl, sizeof(all_lbl), "All %s",  columns[ci].quick_label);
            char none_lbl[32]; snprintf(none_lbl, sizeof(none_lbl), "None %s", columns[ci].quick_label);
            if (ImGui::SmallButton(all_lbl))  { for (auto* r : filtered) checked[r->id] |=  bit; }
            ImGui::SameLine();
            if (ImGui::SmallButton(none_lbl)) { for (auto* r : filtered) checked[r->id] &= ~bit; }
        }
    };
    auto draw_table = [&](const char* table_id, const std::vector<const BatchMapRow*>& filtered, bool show_region_header) {
        constexpr ImGuiTableFlags tflags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY;
        if (!ImGui::BeginTable(table_id, 1 + static_cast<int>(columns.size()), tflags, { -1.f, 220.f })) return;
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Map", ImGuiTableColumnFlags_WidthStretch);
        for (const auto& col : columns)
            ImGui::TableSetupColumn(col.header, ImGuiTableColumnFlags_WidthFixed, 40.f);
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
            uint8_t& bits = checked[r->id];
            ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(r->name.c_str());
            for (int ci = 0; ci < static_cast<int>(columns.size()); ++ci) {
                ImGui::TableSetColumnIndex(1 + ci);
                ImGui::PushID(ci);
                bool v = (bits & columns[ci].bit) != 0;
                if (ImGui::Checkbox("##v", &v)) bits = v ? (bits | columns[ci].bit) : (bits & ~columns[ci].bit);
                ImGui::PopID();
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    };

    char tabbar_id[32];   snprintf(tabbar_id, sizeof(tabbar_id), "##%s_campaigns", id_prefix);
    char table_id[32];    snprintf(table_id, sizeof(table_id), "##%stbl", id_prefix);
    char table_ps_id[32]; snprintf(table_ps_id, sizeof(table_ps_id), "##%stbl_ps", id_prefix);

    if (ImGui::BeginTabBar(tabbar_id)) {
        for (int ci = 0; ci < static_cast<int>(std::size(kBatchCampaigns)); ++ci) {
            if (!ImGui::BeginTabItem(kBatchCampaignLabels[ci])) continue;
            const Camp camp = kBatchCampaigns[ci];
            // Named locals, not chained directly into filter_rows(): that would bind filtered's pointers into a temporary that's destroyed at the end of the full expression (a real use-after-free).
            const auto rows = BuildBatchMapRows(
                [camp](const GW::AreaInfo& inf) { return inf.GetIsOnWorldMap() && inf.campaign == camp; },
                type_ok);
            const auto filtered = filter_rows(rows);
            draw_bulk_buttons(filtered);
            draw_table(table_id, filtered, /*show_region_header=*/true);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Pre-Searing")) {
            const auto ps_rows = BuildBatchMapRows(
                [](const GW::AreaInfo& inf) { return inf.region == GW::Region_Presearing; },
                type_ok);
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
        // Column array index is the tie-break so goals for the same map keep the original column order (e.g. Enter before Leave).
        std::sort(items.begin(), items.end(), [&](const Item& a, const Item& b) {
            if (a.camp   != b.camp)   return a.camp   < b.camp;
            if (a.region != b.region) return a.region < b.region;
            if (a.name   != b.name)   return a.name   < b.name;
            return (a.col - columns.data()) < (b.col - columns.data());
        });
        GoalList* list = plugin.List();
        for (const auto& item : items) {
            GoalEntry g;
            g.label          = std::string(item.col->goal_prefix) + item.name;
            g.trigger.type   = item.col->trigger_type;
            g.trigger.map_id = static_cast<MapID>(item.id);
            list->goals.push_back(std::move(g));
        }
        list->RenumberDuplicateLabels();
        checked.clear();
    }
    if (total == 0) ImGui::EndDisabled();
}
} // namespace

// ---------------------------------------------------------------------------
// Town batch picker
// ---------------------------------------------------------------------------
void SplitsGoalListWindow::DrawTownBatchPicker(SplitsWindow& plugin)
{
    static const BatchColumn kColumns[] = {
        { 1, GoalTrigger::Type::EnterOutpost, "Enter", "En", "Enter " },
        { 2, GoalTrigger::Type::ExitOutpost,  "Leave", "Lv", "Leave " },
    };
    static const GW::RegionType s_town_types[] = {
        GW::RegionType::City, GW::RegionType::Outpost, GW::RegionType::MissionOutpost,
        GW::RegionType::Challenge, GW::RegionType::Marketplace,
        GW::RegionType::HeroBattleOutpost, GW::RegionType::ZaishenBattle,
    };
    auto type_ok = [](const GW::AreaInfo& inf, int&) {
        for (auto t : s_town_types) if (inf.type == t) return true;
        return false;
    };
    DrawMapBatchPicker(plugin, "town", town_filter_buf_, sizeof(town_filter_buf_),
                       batch_town_checked_, type_ok,
                       std::vector<BatchColumn>(std::begin(kColumns), std::end(kColumns)));
}

// ---------------------------------------------------------------------------
// Explorable batch picker
// ---------------------------------------------------------------------------
void SplitsGoalListWindow::DrawExplorableBatchPicker(SplitsWindow& plugin)
{
    static const BatchColumn kColumns[] = {
        { 1, GoalTrigger::Type::EnterExplorable,  "Enter", "En", "Enter " },
        { 2, GoalTrigger::Type::VanquishComplete, "VQ",    "VQ", "VQ "    },
        { 4, GoalTrigger::Type::ExitExplorable,   "Leave", "Lv", "Leave " },
    };
    // Ties (same name_id) prefer ExplorableZone (rank 0) over MissionArea (rank 1).
    auto type_ok = [](const GW::AreaInfo& inf, int& rank) {
        if (inf.type != GW::RegionType::ExplorableZone && inf.type != GW::RegionType::MissionArea) return false;
        rank = (inf.type == GW::RegionType::ExplorableZone) ? 0 : 1;
        return true;
    };
    DrawMapBatchPicker(plugin, "exp", exp_filter_buf_, sizeof(exp_filter_buf_),
                       batch_exp_checked_, type_ok,
                       std::vector<BatchColumn>(std::begin(kColumns), std::end(kColumns)));
}

// ---------------------------------------------------------------------------
// Mission batch picker
// ---------------------------------------------------------------------------
void SplitsGoalListWindow::DrawMissionBatchPicker(SplitsWindow& plugin)
{
    using MapID = GW::Constants::MapID;
    using Camp  = GW::Constants::Campaign;

    struct MissionRow { int id; std::string name; uint32_t chron; };

    constexpr int NUM_CAMPS = 3; // EotN skipped (kBatchCampaigns/kBatchCampaignLabels defined above, shared with the Town/Explorable pickers)

    auto build_list = [](Camp camp) -> std::vector<MissionRow> {
        if (camp == Camp::Nightfall) {
            static const MapID order[] = {
                MapID::Chahbek_Village, MapID::Jokanur_Diggings, MapID::Blacktide_Den,
                MapID::Consulate_Docks, MapID::Venta_Cemetery, MapID::Kodonur_Crossroads,
                MapID::Pogahn_Passage, MapID::Rilohn_Refuge, MapID::Moddok_Crevice,
                MapID::Tihark_Orchard, MapID::Dasha_Vestibule, MapID::Dzagonur_Bastion,
                MapID::Grand_Court_of_Sebelkeh, MapID::Jennurs_Horde, MapID::Nundu_Bay,
                MapID::Gate_of_Desolation, MapID::Ruins_of_Morah, MapID::Gate_of_Pain,
                MapID::Gate_of_Madness, MapID::Abaddons_Gate,
            };
            std::vector<MissionRow> out;
            out.reserve(std::size(order));
            for (uint32_t i = 0; i < static_cast<uint32_t>(std::size(order)); ++i)
                out.push_back({ static_cast<int>(order[i]), Resources::GetMapName(order[i])->string(), i + 1 });
            return out;
        }
        if (camp == Camp::Factions) {
            static const MapID order[] = {
                MapID::Minister_Chos_Estate_outpost_mission, MapID::Zen_Daijun_outpost_mission,
                MapID::Vizunah_Square_mission, MapID::Nahpui_Quarter_outpost_mission,
                MapID::Tahnnakai_Temple_outpost_mission, MapID::Arborstone_outpost_mission,
                MapID::Boreas_Seabed_outpost_mission, MapID::Sunjiang_District_outpost_mission,
                MapID::The_Eternal_Grove_outpost_mission, MapID::Gyala_Hatchery_outpost_mission,
                MapID::Unwaking_Waters_Kurzick_outpost, MapID::Raisu_Palace_outpost_mission,
                MapID::Imperial_Sanctum_outpost_mission,
            };
            std::vector<MissionRow> out;
            out.reserve(std::size(order));
            for (uint32_t i = 0; i < static_cast<uint32_t>(std::size(order)); ++i)
                out.push_back({ static_cast<int>(order[i]), Resources::GetMapName(order[i])->string(), i + 1 });
            return out;
        }
        if (camp == Camp::Prophecies) {
            static const MapID order[] = {
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
            std::vector<MissionRow> out;
            out.reserve(std::size(order));
            for (uint32_t i = 0; i < static_cast<uint32_t>(std::size(order)); ++i)
                out.push_back({ static_cast<int>(order[i]), Resources::GetMapName(order[i])->string(), i + 1 });
            return out;
        }
        // Dynamic scan for other campaigns
        auto type_rank = [](GW::RegionType t) {
            switch (t) {
                case GW::RegionType::EotnMission:        return 0;
                case GW::RegionType::CooperativeMission: return 1;
                case GW::RegionType::EliteMission:       return 1;
                case GW::RegionType::MissionOutpost:     return 2;
                case GW::RegionType::Dungeon:            return 3;
                default:                                 return 99;
            }
        };
        struct Candidate { int id; uint32_t chron; int rank; };
        std::unordered_map<uint32_t, Candidate> best;
        for (int id = 1; id < static_cast<int>(MapID::Count); ++id) {
            const auto mid = static_cast<MapID>(id);
            const auto* info = GW::Map::GetMapInfo(mid);
            if (!info || !info->name_id || !info->GetIsOnWorldMap() || info->campaign != camp) continue;
            if (info->mission_chronology == 0) continue;
            int rank = type_rank(info->type);
            if (rank >= 99) continue;
            auto it = best.find(info->name_id);
            if (it == best.end() || rank < it->second.rank)
                best[info->name_id] = { id, info->mission_chronology, rank };
        }
        std::vector<MissionRow> out;
        out.reserve(best.size());
        for (const auto& kv : best)
            out.push_back({ kv.second.id, Resources::GetMapName(static_cast<MapID>(kv.second.id))->string(), kv.second.chron });
        std::sort(out.begin(), out.end(),
            [](const MissionRow& a, const MissionRow& b) { return a.chron < b.chron; });
        return out;
    };

    ImGui::TextColored({1.f, 0.8f, 0.2f, 1.f},
        "Note: Bonus is read from the mission-complete bitmask, which never clears once earned. "
        "On a character that's already earned a mission's bonus before, Bonus will always show "
        "complete on every later attempt of that mission, whether or not it's actually re-earned that run.");

    if (ImGui::BeginTabBar("##batch_campaigns")) {
        for (int ci = 0; ci < NUM_CAMPS; ++ci) {
            if (!ImGui::BeginTabItem(kBatchCampaignLabels[ci])) continue;
            const auto missions = build_list(kBatchCampaigns[ci]);

            if (ImGui::SmallButton("All M"))  { for (const auto& m : missions) batch_mis_checked_.insert(m.id); }
            ImGui::SameLine();
            if (ImGui::SmallButton("None M")) { for (const auto& m : missions) batch_mis_checked_.erase(m.id); }
            ImGui::SameLine(0, 16);
            if (ImGui::SmallButton("All B"))  { for (const auto& m : missions) batch_bon_checked_.insert(m.id); }
            ImGui::SameLine();
            if (ImGui::SmallButton("None B")) { for (const auto& m : missions) batch_bon_checked_.erase(m.id); }
            ImGui::SameLine(0, 16);
            ImGui::Checkbox("Hard Mode", &batch_hm_);

            constexpr ImGuiTableFlags tflags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY;
            if (ImGui::BeginTable("##missiontbl", 4, tflags, { -1.f, 220.f })) {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("#",       ImGuiTableColumnFlags_WidthFixed, 28.f);
                ImGui::TableSetupColumn("Mission", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("M",       ImGuiTableColumnFlags_WidthFixed, 28.f);
                ImGui::TableSetupColumn("B",       ImGuiTableColumnFlags_WidthFixed, 28.f);
                ImGui::TableHeadersRow();

                for (int mi = 0; mi < static_cast<int>(missions.size()); ++mi) {
                    const auto& row = missions[mi];
                    ImGui::PushID(row.id);
                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%d", mi + 1);
                    ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(row.name.c_str());
                    ImGui::TableSetColumnIndex(2);
                    bool mc = batch_mis_checked_.count(row.id) > 0;
                    if (ImGui::Checkbox("##m", &mc)) { if (mc) batch_mis_checked_.insert(row.id); else batch_mis_checked_.erase(row.id); }
                    ImGui::TableSetColumnIndex(3);
                    bool bc = batch_bon_checked_.count(row.id) > 0;
                    if (ImGui::Checkbox("##b", &bc)) { if (bc) batch_bon_checked_.insert(row.id); else batch_bon_checked_.erase(row.id); }
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    const int total = static_cast<int>(batch_mis_checked_.size() + batch_bon_checked_.size());
    if (total == 0) ImGui::BeginDisabled();
    char add_lbl[48];
    snprintf(add_lbl, sizeof(add_lbl), "Add %d Goal%s##batchadd", total, total == 1 ? "" : "s");
    if (ImGui::Button(add_lbl)) {
        struct BatchItem { int id; bool bonus; Camp camp; uint32_t chron; };
        std::vector<BatchItem> items;
        items.reserve(total);
        for (int id : batch_mis_checked_) {
            const auto* info = GW::Map::GetMapInfo(static_cast<MapID>(id));
            if (info) items.push_back({ id, false, info->campaign, info->mission_chronology });
        }
        for (int id : batch_bon_checked_) {
            const auto* info = GW::Map::GetMapInfo(static_cast<MapID>(id));
            if (info) items.push_back({ id, true, info->campaign, info->mission_chronology });
        }
        std::sort(items.begin(), items.end(), [](const BatchItem& a, const BatchItem& b) {
            if (a.camp  != b.camp)  return a.camp  < b.camp;
            if (a.chron != b.chron) return a.chron < b.chron;
            return !a.bonus;
        });
        GoalList* list = plugin.List();
        for (const auto& item : items) {
            GoalEntry g;
            g.label = Resources::GetMapName(static_cast<MapID>(item.id))->string();
            if (item.bonus) g.label += " - Bonus";
            if (batch_hm_)  g.label += " (HM)";
            g.trigger.type      = item.bonus ? GoalTrigger::Type::MissionBonus : GoalTrigger::Type::MissionComplete;
            g.trigger.map_id    = static_cast<MapID>(item.id);
            g.trigger.hard_mode = batch_hm_;
            list->goals.push_back(std::move(g));
        }
        list->RenumberDuplicateLabels();
        batch_mis_checked_.clear();
        batch_bon_checked_.clear();
    }
    if (total == 0) ImGui::EndDisabled();
}

// ---------------------------------------------------------------------------
// Dungeon batch picker (EotN's 20 regular dungeons only — the 5 pre-EotN elite areas each need their own finish condition, see DrawEliteAreaBatchPicker below).
// ---------------------------------------------------------------------------
void SplitsGoalListWindow::DrawDungeonBatchPicker(SplitsWindow& plugin)
{
    using MapID = GW::Constants::MapID;

    struct DungeonRow { int id; std::string name; const SCPresets::Dungeon* dungeon; };
    std::vector<DungeonRow> rows;
    rows.reserve(std::size(SCPresets::kDungeons));
    for (const auto& dungeon : SCPresets::kDungeons)
        rows.push_back({ static_cast<int>(dungeon.levels[0]), Resources::GetMapName(dungeon.levels[0])->string(), &dungeon });
    std::sort(rows.begin(), rows.end(), [](const DungeonRow& a, const DungeonRow& b) { return a.name < b.name; });

    std::vector<const DungeonRow*> filtered;
    for (const auto& r : rows) {
        if (dungeon_filter_buf_[0] != '\0' && !TextUtils::CaseInsensitiveContains(r.name, dungeon_filter_buf_))
            continue;
        filtered.push_back(&r);
    }

    if (plugin.ActiveProfile().dynamic_by_default) {
        // SC: single click = the full per-level breakdown, matching the generated preset exactly. No partial picks; rename before Save for something more specific (e.g. "2 Man CoF").
        ImGui::TextColored({1.f, 0.8f, 0.2f, 1.f},
            "Click a dungeon to start a new list with its full level breakdown. Rename it\n"
            "below (e.g. \"2 Man CoF\") before saving if you want something more specific.");
        ImGui::SetNextItemWidth(-1.f);
        ImGui::InputText("##dungeonfilter", dungeon_filter_buf_, sizeof(dungeon_filter_buf_));
        ImGui::SameLine(); if (ImGui::SmallButton("x##dungeonfx")) dungeon_filter_buf_[0] = '\0';

        ImGui::BeginChild("##dungeonlist_sc", { -1.f, 220.f }, true);
        for (const auto* r : filtered) {
            if (ImGui::Selectable(r->name.c_str())) {
                plugin.SetActiveList(SCPresets::BuildDungeonPresetList(*r->dungeon));
                snprintf(list_name_buf_, sizeof(list_name_buf_), "%s", r->name.c_str());
            }
        }
        ImGui::EndChild();
        return;
    }

    // Manual stays flat (unlike SC) — only the first level's map_id names the dungeon; the goal itself is one generic DungeonReward completion.
    ImGui::TextColored({1.f, 0.8f, 0.2f, 1.f},
        "Note: completion is a generic \"dungeon reward chest opened\" signal, not specific to "
        "this dungeon \xe2\x80\x94 correct as long as the list is run in order, same as any other "
        "sequential Manual goal.");

    ImGui::SetNextItemWidth(-1.f);
    ImGui::InputText("##dungeonfilter", dungeon_filter_buf_, sizeof(dungeon_filter_buf_));
    ImGui::SameLine(); if (ImGui::SmallButton("x##dungeonfx")) dungeon_filter_buf_[0] = '\0';

    if (ImGui::SmallButton("All"))  { for (const auto* r : filtered) batch_dungeon_checked_.insert(r->id); }
    ImGui::SameLine();
    if (ImGui::SmallButton("None")) { for (const auto* r : filtered) batch_dungeon_checked_.erase(r->id); }

    constexpr ImGuiTableFlags tflags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY;
    if (ImGui::BeginTable("##dungeontbl", 2, tflags, { -1.f, 220.f })) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Dungeon", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Add",     ImGuiTableColumnFlags_WidthFixed, 40.f);
        ImGui::TableHeadersRow();

        for (const auto* r : filtered) {
            ImGui::PushID(r->id);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(r->name.c_str());
            ImGui::TableSetColumnIndex(1);
            bool checked = batch_dungeon_checked_.count(r->id) > 0;
            if (ImGui::Checkbox("##d", &checked)) {
                if (checked) batch_dungeon_checked_.insert(r->id);
                else batch_dungeon_checked_.erase(r->id);
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    const int total = static_cast<int>(batch_dungeon_checked_.size());
    if (total == 0) ImGui::BeginDisabled();
    char add_lbl[48];
    snprintf(add_lbl, sizeof(add_lbl), "Add %d Goal%s##batchadddungeon", total, total == 1 ? "" : "s");
    if (ImGui::Button(add_lbl)) {
        GoalList* list = plugin.List();
        // Iterate the name-sorted rows, not the raw std::set (which orders by map_id), so multi-add lands in a predictable order.
        for (const auto& r : rows) {
            if (!batch_dungeon_checked_.count(r.id)) continue;
            GoalEntry g;
            g.label          = r.name;
            g.trigger.type   = GoalTrigger::Type::DungeonReward;
            g.trigger.map_id = static_cast<MapID>(r.id);
            list->goals.push_back(std::move(g));
        }
        list->RenumberDuplicateLabels();
        batch_dungeon_checked_.clear();
    }
    if (total == 0) ImGui::EndDisabled();
}

// ---------------------------------------------------------------------------
// Elite area batch picker — Fissure of Woe, Underworld, Urgoz's Warren, The Deep.
// ToPK deferred: its arenas are map-based (InstanceLoadInfo/CountdownStart), not an objective/door checklist, so it doesn't fit this picker's shape.
// Each checkpoint is reduced from OT's own preset definitions to "what fires when this segment is done": FoW/UW use their ObjectiveDone objective_id; Urgoz/Deep use whatever door/dialogue/message OT uses to start the *next* segment.
// "Area complete" is a derived state (all checkpoints done, any order) rather than a single trigger — DrawHeaderRow's existing all_done aggregation handles it for free; checking every checkpoint + Add wraps them in a header, a partial pick adds flat goals instead.
// ---------------------------------------------------------------------------
void SplitsGoalListWindow::DrawEliteAreaBatchPicker(SplitsWindow& plugin)
{
    struct AreaTab {
        const SCPresets::EliteArea* area;
        std::set<int>*              checked; // keyed by checkpoint index, not param1 — safe across mixed trigger types
    };
    const AreaTab areas[] = {
        { &SCPresets::kEliteAreas[0], &batch_fow_checked_   },
        { &SCPresets::kEliteAreas[1], &batch_uw_checked_    },
        { &SCPresets::kEliteAreas[2], &batch_urgoz_checked_ },
        { &SCPresets::kEliteAreas[3], &batch_deep_checked_  },
    };

    if (plugin.ActiveProfile().dynamic_by_default) {
        // SC: single click = every checkpoint, matching the generated preset exactly. No partial picks; rename before Save for something more specific.
        ImGui::TextColored({1.f, 0.8f, 0.2f, 1.f},
            "Click an area to start a new list with its full checkpoint set. Rename it below\n"
            "(e.g. \"2 Man FoW\") before saving if you want something more specific.");
        ImGui::BeginChild("##elitelist_sc", { -1.f, 220.f }, true);
        for (const auto& tab : areas) {
            if (ImGui::Selectable(tab.area->label)) {
                plugin.SetActiveList(SCPresets::BuildEliteAreaPresetList(*tab.area));
                snprintf(list_name_buf_, sizeof(list_name_buf_), "%s", tab.area->label);
            }
        }
        ImGui::EndChild();
        return;
    }

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
            if (ImGui::BeginTable("##elitetbl", 2, tflags, { -1.f, 220.f })) {
                ImGui::TableSetupColumn("Checkpoint", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Add",        ImGuiTableColumnFlags_WidthFixed, 40.f);
                ImGui::TableHeadersRow();

                for (size_t i = 0; i < area.count; ++i) {
                    ImGui::PushID(static_cast<int>(i));
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(area.checkpoints[i].name);
                    ImGui::TableSetColumnIndex(1);
                    bool checked = tab.checked->count(static_cast<int>(i)) > 0;
                    if (ImGui::Checkbox("##e", &checked)) {
                        if (checked) tab.checked->insert(static_cast<int>(i));
                        else tab.checked->erase(static_cast<int>(i));
                    }
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }

            const int total = static_cast<int>(tab.checked->size());
            if (total == 0) ImGui::BeginDisabled();
            char add_lbl[48];
            snprintf(add_lbl, sizeof(add_lbl), "Add %d Goal%s##batchaddelite", total, total == 1 ? "" : "s");
            if (ImGui::Button(add_lbl)) {
                GoalList* list = plugin.List();
                // Every checkpoint checked = the whole area, so wrap in a header; a partial pick is just flat goals.
                const bool all_checked = (tab.checked->size() == area.count);
                if (all_checked) {
                    GoalEntry hdr;
                    hdr.is_header      = true;
                    hdr.label          = area.label;
                    hdr.trigger.map_id = area.map_id; // read by ApplyTimerPolicy's autostart, not the engine
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
