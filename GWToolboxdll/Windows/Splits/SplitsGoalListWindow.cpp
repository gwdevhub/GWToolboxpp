#include "stdafx.h"
#include <ImGuiAddons.h>
#include <Windows/Splits/SplitsGoalListWindow.h>
#include <Windows/Splits/SplitsProfile.h>
#include <Windows/SplitsWindow.h>
#include <Windows/Splits/GoalEntry.h>
#include <Windows/Splits/GoalList.h>
#include <Windows/Splits/GoalClock.h>
#include <Windows/Splits/MapNames.h>

#include <GWCA/GameContainers/Array.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Constants/Constants.h>
#include <GWCA/Context/WorldContext.h>
#include <GWCA/GameEntities/Quest.h>
#include <GWCA/GameEntities/Title.h>

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

static void DrawPBDelta(double actual, double pb_split)
{
    if (std::isnan(pb_split) || std::isnan(actual)) return;
    const double delta = actual - pb_split;
    char dbuf[32];
    FormatTime(dbuf, sizeof(dbuf), std::abs(delta));
    if (delta < 0.0)
        ImGui::TextColored({1.f, 0.85f, 0.0f, 1.f}, "-%s", dbuf);
    else
        ImGui::TextColored({1.f, 0.4f, 0.4f, 1.f}, "+%s", dbuf);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static const char* CampaignName(GW::Constants::Campaign c)
{
    switch (c) {
        case GW::Constants::Campaign::Prophecies:    return "Prophecies";
        case GW::Constants::Campaign::Factions:      return "Factions";
        case GW::Constants::Campaign::Nightfall:     return "Nightfall";
        case GW::Constants::Campaign::EyeOfTheNorth: return "Eye of the North";
        default: return nullptr;
    }
}

static const char* RegionName(GW::Region r)
{
    switch (r) {
        case GW::Region_Ascalon:             return "Ascalon";
        case GW::Region_NorthernShiverpeaks: return "Northern Shiverpeaks";
        case GW::Region_Kryta:               return "Kryta";
        case GW::Region_Maguuma:             return "Maguuma Jungle";
        case GW::Region_CrystalDesert:       return "Crystal Desert";
        case GW::Region_FissureOfWoe:        return "Southern Shiverpeaks";
        case GW::Region_ShingJea:            return "Shing Jea Island";
        case GW::Region_Kaineng:             return "Kaineng City";
        case GW::Region_Kurzick:             return "Echovald Forest";
        case GW::Region_Luxon:               return "The Jade Sea";
        case GW::Region_Istan:               return "Istan";
        case GW::Region_Kourna:              return "Kourna";
        case GW::Region_Vaabi:               return "Vabbi";
        case GW::Region_Desolation:          return "The Desolation";
        case GW::Region_DomainOfAnguish:     return "Realm of Torment";
        case GW::Region_TarnishedCoast:      return "Tarnished Coast";
        case GW::Region_DepthsOfTyria:       return "Depths of Tyria";
        case GW::Region_FarShiverpeaks:      return "Far Shiverpeaks";
        case GW::Region_CharrHomelands:      return "Charr Homelands";
        case GW::Region_Presearing:          return "Pre-Searing";
        default: return nullptr;
    }
}

// ---------------------------------------------------------------------------
// Main window Draw
// ---------------------------------------------------------------------------
void SplitsGoalListWindow::Draw(SplitsWindow& plugin)
{
    const bool is_open = ImGui::Begin(plugin.Name(), plugin.GetVisiblePtr(), plugin.GetWinFlags());

    // Title bar buttons
    {
        const bool   running = plugin.Clock().IsRunning();
        const char*  lbl0    = running ? "Pause" : "Start";
        const float  fp_x    = ImGui::GetStyle().FramePadding.x;
        const float  sp      = ImGui::GetStyle().ItemSpacing.x;
        const float  bh      = ImGui::GetFrameHeight() - 4.f;
        const float  bw0     = ImGui::CalcTextSize(lbl0).x    + fp_x * 2.f;
        const float  bw1     = ImGui::CalcTextSize("Reset").x  + fp_x * 2.f;
        const float  bw2     = ImGui::CalcTextSize("Split").x  + fp_x * 2.f;
        const float  total_w = bw0 + bw1 + bw2 + sp * 2.f;

        const ImVec2 win_pos  = ImGui::GetWindowPos();
        const float  win_w    = ImGui::GetWindowWidth();
        const float  title_h  = ImGui::GetFrameHeight();

        const ImVec2 saved_cursor = ImGui::GetCursorPos();
        ImGui::PushClipRect(win_pos, {win_pos.x + win_w, win_pos.y + title_h}, false);
        ImGui::SetCursorScreenPos({win_pos.x + win_w - total_w - 8.f, win_pos.y + 2.f});

        if (ImGui::Button(lbl0,    {bw0, bh})) plugin.StartRun();
        ImGui::SameLine(0, sp);
        if (ImGui::Button("Reset", {bw1, bh})) plugin.ResetRun();
        ImGui::SameLine(0, sp);
        if (ImGui::Button("Split", {bw2, bh})) plugin.TriggerManualSplit();

        ImGui::PopClipRect();
        ImGui::SetCursorPos(saved_cursor);
    }

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

    // Header clock — shows whichever time the profile is set to display.
    {
        const SplitsProfile& hp = plugin.ActiveProfile();
        const bool gt = hp.use_game_time;
        char tbuf[32];
        FormatTime(tbuf, sizeof(tbuf), gt ? clock.GameTime() : clock.RealTime());
        const char* label = gt ? "Game: " : "Real: ";
        const ImVec4 tcol = gt ? ImVec4{0.6f, 0.85f, 1.f, 1.f} : ImVec4{0.9f, 0.9f, 0.9f, 1.f};

        const float avail   = ImGui::GetContentRegionAvail().x;
        const float total_w = ImGui::CalcTextSize(label).x + ImGui::CalcTextSize(tbuf).x;
        const float start_x = ImGui::GetCursorPosX() + (avail - total_w) * 0.5f;
        const float min_x   = ImGui::GetCursorPosX();
        const float clamped_start_x = start_x < min_x ? min_x : start_x;

        if (hp.show_paused_time) {
            char pbuf[32]; FormatTime(pbuf, sizeof(pbuf), plugin.TotalPausedReal());
            char ptext[48]; snprintf(ptext, sizeof(ptext), "Paused: %s", pbuf);
            const float pause_w = ImGui::CalcTextSize(ptext).x;
            const float pause_x = clamped_start_x - 12.f - pause_w;
            ImGui::SetCursorPosX(pause_x > min_x ? pause_x : min_x);
            ImGui::TextColored({1.f, 0.8f, 0.3f, 1.f}, "%s", ptext);
            ImGui::SameLine(0, 0);
        }

        ImGui::SetCursorPosX(clamped_start_x);
        ImGui::TextColored(tcol, "%s%s", label, tbuf);
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
        ImGui::TextColored({1.f, 0.4f, 0.4f, 1.f}, "%s", kMsg);
        ImGui::Separator();
    }

    if (!list || list->goals.empty()) {
        if (plugin.ActiveProfileIdx() == 0)
            ImGui::TextDisabled("Enter DoA, FoW, UW, Deep, Urgoz or a Dungeon to begin");
        else
            ImGui::TextDisabled("No goal list loaded. Open Settings to create one.");
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

    const SplitsProfile& profile = plugin.ActiveProfile();
    const std::vector<double>& pb = profile.use_game_time ? plugin.PBSplitsGame() : plugin.PBSplits();
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

        auto pb_at = [&]() -> double {
            if (pb_idx < 0 || pb_idx >= static_cast<int>(pb.size())) return nan;
            return pb[static_cast<size_t>(pb_idx)];
        };

        const float row_y0 = ImGui::GetCursorScreenPos().y - pad_y;

        DrawGoalRow(g, clock, i, i == current_idx, pb_at(), profile);
        ++pb_idx;
        ++i;

        const float row_y1 = ImGui::GetCursorScreenPos().y + pad_y;
        draw_list->AddRect({win_x + pad_x, row_y0}, {win_x + avail_w - pad_x, row_y1},
                           IM_COL32(80, 80, 80, 140), 2.f);
    }

    ImGui::End();
}

void SplitsGoalListWindow::DrawGoalRow(const GoalEntry& g, const GoalClock& /*clock*/,
                                        int /*index*/, bool is_current, double pb_split,
                                        const SplitsProfile& profile)
{
    const bool done = (g.status == GoalStatus::Completed || g.status == GoalStatus::Failed);

    ImVec4 color;
    if (g.status == GoalStatus::Failed)         color = ImGui::ColorConvertU32ToFloat4(profile.color_failed);
    else if (g.status == GoalStatus::Completed) color = ImGui::ColorConvertU32ToFloat4(profile.color_completed);
    else if (is_current)                        color = ImGui::ColorConvertU32ToFloat4(profile.color_active);
    else                                        color = ImGui::ColorConvertU32ToFloat4(profile.color_inactive);

    const bool gt = profile.use_game_time;

    if (!ImGui::BeginTable("##goalrow", 4, ImGuiTableFlags_None)) return;
    ImGui::TableSetupColumn("name",  ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("start", ImGuiTableColumnFlags_WidthFixed, profile.show_start_time ? 90.f : 0.f);
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

    if (profile.show_start_time) {
        ImGui::TableSetColumnIndex(1);
        const double st = gt ? g.start_game_time : g.start_real_time;
        if (st >= 0.0) {
            char buf[32]; FormatTime(buf, sizeof(buf), st);
            ImGui::TextDisabled("%s", buf);
        } else {
            ImGui::TextDisabled("--:--");
        }
    }

    {
        ImGui::TableSetColumnIndex(2);
        if (done) {
            const double t = gt ? g.split.game_time : g.split.real_time;
            const ImVec4 tc = gt ? ImVec4{0.6f, 0.85f, 1.f, 1.f} : ImVec4{0.9f, 0.9f, 0.9f, 1.f};
            char buf[32]; FormatTime(buf, sizeof(buf), t);
            ImGui::TextColored(tc, "%s", buf);
            DrawPBDelta(t, pb_split);
        } else if (is_current) {
            ImGui::TextDisabled("---");
        }
    }

    if (profile.show_segment) {
        ImGui::TableSetColumnIndex(3);
        if (done) {
            char seg[32];
            if (gt) {
                if (profile.segment_is_duration && g.start_game_time >= 0.0)
                    FormatTime(seg, sizeof(seg), g.split.game_time - g.start_game_time);
                else
                    FormatTime(seg, sizeof(seg), g.split.segment_game);
            } else {
                if (profile.segment_is_duration && g.start_real_time >= 0.0)
                    FormatTime(seg, sizeof(seg), g.split.real_time - g.start_real_time);
                else
                    FormatTime(seg, sizeof(seg), g.split.segment_real);
            }
            ImGui::TextDisabled("+%s", seg);
        }
    }

    ImGui::EndTable();
}

bool SplitsGoalListWindow::DrawHeaderRow(const GoalList& list, int header_idx, const SplitsProfile& profile)
{
    const GoalEntry& h = list.goals[header_idx];

    const bool gt = profile.use_game_time;

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
    // Include list name in the ID so headers from different lists never share ImGui collapse state.
    // First render of a freshly loaded list always hits DefaultOpen (expanded).
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

    GoalList* list = plugin.List();
    if (list_name_buf_[0] == '\0' && !list->name.empty())
        snprintf(list_name_buf_, sizeof(list_name_buf_), "%s", list->name.c_str());

    // Profile switcher
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
            list = plugin.List();
        }
        if (active) {
            ImGui::PopStyleColor(3);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Active — auto-loads on next session");
        }
    }
    ImGui::Separator();

    ImGui::Columns(3, "settings_cols", false);

    // Col 0: display toggles + PB basis + behavior flags
    SplitsProfile& p = plugin.ActiveProfile();
    ImGui::Checkbox("Show start time column", &p.show_start_time);
    ImGui::TextUnformatted("Time:");
    ImGui::SameLine();
    if (ImGui::RadioButton("Game", p.use_game_time))  p.use_game_time = true;
    ImGui::SameLine();
    if (ImGui::RadioButton("Real", !p.use_game_time)) p.use_game_time = false;
    ImGui::Checkbox("Show split column",      &p.show_segment);
    if (p.show_segment) {
        ImGui::SameLine();
        if (ImGui::RadioButton("End-Start##seg",   p.segment_is_duration))  p.segment_is_duration = true;
        ImGui::SameLine();
        if (ImGui::RadioButton("Since last##seg", !p.segment_is_duration)) p.segment_is_duration = false;
    }
    ImGui::Checkbox("Auto /age on completion", &p.auto_send_age);
    ImGui::Checkbox("Show paused time",        &p.show_paused_time);

    ImGui::Spacing();
    ImGui::Checkbox("Stop on party wipe",         &p.stop_on_party_defeated);
    if (plugin.ActiveProfileIdx() != 0)
        ImGui::Checkbox("Game time: explorable only", &p.game_time_explorable_only);

    ImGui::NextColumn();

    // Col 1: keybinds + row colors
    ImGui::TextUnformatted("Keybinds:");
    draw_keybind("Start", plugin.KeyStart());
    draw_keybind("Reset", plugin.KeyReset());
    draw_keybind("Split", plugin.KeySplit());
    if (capturing_active)
        ImGui::TextDisabled("(Esc to clear)");

    ImGui::Spacing();
    ImGui::ColorButtonPicker("Completed##sc", &p.color_completed); ImGui::SameLine(); ImGui::TextUnformatted("Done");
    ImGui::SameLine(0, 12.f);
    ImGui::ColorButtonPicker("Active##sc",    &p.color_active);    ImGui::SameLine(); ImGui::TextUnformatted("Active");
    ImGui::ColorButtonPicker("Inactive##sc",  &p.color_inactive);  ImGui::SameLine(); ImGui::TextUnformatted("Inactive");
    ImGui::SameLine(0, 12.f);
    ImGui::ColorButtonPicker("Failed##sc",    &p.color_failed);    ImGui::SameLine(); ImGui::TextUnformatted("Failed");
    ImGui::Spacing();

    ImGui::NextColumn();

    // Col 2: list management — hidden for auto-load-preset profiles (SC)
    if (plugin.ActiveProfileIdx() != 0) {
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

    }

    ImGui::Columns(1);
    ImGui::Separator();

    if (plugin.ActiveProfileIdx() != 0) {
        // Existing goals list
        ImGui::TextUnformatted("Goals:");
        list = plugin.List();
        bool erased = false;
        bool moved = false;
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
            ImGui::PopID();
        }

        ImGui::Separator();

        // Add goal / header form
        ImGui::TextUnformatted("Add Goal:");

        if (plugin.ActiveProfileIdx() == 2) {
            // Running profile: combined from/to leg picker only.
            DrawRunningLegPicker(plugin);
        } else {
        // Trigger type combo — sentinel -1 means "Header (no trigger)".
        struct TriggerOpt { const char* label; int type_int; };
        static const TriggerOpt trigger_opts_full[] = {
            { "Header",      -1 },
            { "Manual",      static_cast<int>(GoalTrigger::Type::Manual)          },
            { "Missions",    static_cast<int>(GoalTrigger::Type::MissionComplete)  },
            { "Explorables", static_cast<int>(GoalTrigger::Type::MapEnter)         },
            { "Towns",       static_cast<int>(GoalTrigger::Type::EnterExplorable)  },
            { "Titles",      static_cast<int>(GoalTrigger::Type::ReachTitleRank)   },
            { "Reach Level", static_cast<int>(GoalTrigger::Type::ReachLevel)       },
        };
        const TriggerOpt* trigger_opts      = trigger_opts_full;
        int               trigger_opts_count = 7;
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
        const bool is_title_picker     = (edit_trigger_type_ == static_cast<int>(GoalTrigger::Type::ReachTitleRank));
        const bool needs_level         = (edit_trigger_type_ == static_cast<int>(GoalTrigger::Type::ReachLevel));

        if (is_mission_batch) {
            DrawMissionBatchPicker(plugin);
        } else if (is_explorable_batch) {
            DrawExplorableBatchPicker(plugin);
        } else if (is_town_batch) {
            DrawTownBatchPicker(plugin);
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
            } else {
                if (ImGui::Button("Add Goal")) {
                    GoalEntry new_g;
                    new_g.label          = edit_label_;
                    new_g.trigger.type   = static_cast<GoalTrigger::Type>(edit_trigger_type_);
                    new_g.trigger.map_id = GW::Constants::MapID::None;
                    new_g.trigger.level  = edit_level_;
                    list->goals.push_back(std::move(new_g));
                    list->RenumberDuplicateLabels();
                    edit_label_[0] = '\0';
                }
            }
            if (!can_add) ImGui::EndDisabled();
        }
        } // end else (non-Running add form)
    }
}

// ---------------------------------------------------------------------------
// Title picker
// ---------------------------------------------------------------------------
void SplitsGoalListWindow::DrawTitlePicker(SplitsWindow& plugin)
{
    using TitleID = GW::Constants::TitleID;

    struct TitleEntry { const char* name; TitleID id; const char* group; };
    static const TitleEntry s_titles[] = {
        { "Cartographer (Tyria)",          TitleID::TyrianCarto,          "Character \xe2\x80\x94 Core" },
        { "Cartographer (Cantha)",         TitleID::CanthanCarto,         "Character \xe2\x80\x94 Core" },
        { "Cartographer (Elona)",          TitleID::ElonianCarto,         "Character \xe2\x80\x94 Core" },
        { "Drunkard",                      TitleID::Drunkard,             "Character \xe2\x80\x94 Core" },
        { "Guardian (Tyria)",              TitleID::GuardianTyria,        "Character \xe2\x80\x94 Core" },
        { "Guardian (Cantha)",             TitleID::GuardianCantha,       "Character \xe2\x80\x94 Core" },
        { "Guardian (Elona)",              TitleID::GuardianElona,        "Character \xe2\x80\x94 Core" },
        { "Maxed Titles",                  TitleID::KoaBD,                "Character \xe2\x80\x94 Core" },
        { "Party Animal",                  TitleID::Party,                "Character \xe2\x80\x94 Core" },
        { "Protector (Tyria)",             TitleID::ProtectorTyria,       "Character \xe2\x80\x94 Core" },
        { "Protector (Cantha)",            TitleID::ProtectorCantha,      "Character \xe2\x80\x94 Core" },
        { "Protector (Elona)",             TitleID::ProtectorElona,       "Character \xe2\x80\x94 Core" },
        { "Skill Hunter (Tyria)",          TitleID::SkillHunterTyria,     "Character \xe2\x80\x94 Core" },
        { "Skill Hunter (Cantha)",         TitleID::SkillHunterCantha,    "Character \xe2\x80\x94 Core" },
        { "Skill Hunter (Elona)",          TitleID::SkillHunterElona,     "Character \xe2\x80\x94 Core" },
        { "Survivor",                      TitleID::Survivor,             "Character \xe2\x80\x94 Core" },
        { "Sweet Tooth",                   TitleID::Sweets,               "Character \xe2\x80\x94 Core" },
        { "Vanquisher (Tyria)",            TitleID::VanquisherTyria,      "Character \xe2\x80\x94 Core" },
        { "Vanquisher (Cantha)",           TitleID::VanquisherCantha,     "Character \xe2\x80\x94 Core" },
        { "Vanquisher (Elona)",            TitleID::VanquisherElona,      "Character \xe2\x80\x94 Core" },
        { "Legendary Defender of Ascalon", TitleID::LDoA,                 "Character \xe2\x80\x94 Prophecies" },
        { "Lightbringer",                  TitleID::Lightbringer,         "Character \xe2\x80\x94 Nightfall" },
        { "Sunspear",                      TitleID::Sunspear,             "Character \xe2\x80\x94 Nightfall" },
        { "Asura",                         TitleID::Asuran,               "Character \xe2\x80\x94 Eye of the North" },
        { "Deldrimor",                     TitleID::Deldrimor,            "Character \xe2\x80\x94 Eye of the North" },
        { "Ebon Vanguard",                 TitleID::Vanguard,             "Character \xe2\x80\x94 Eye of the North" },
        { "Master of the North",           TitleID::MasterOfTheNorth,     "Character \xe2\x80\x94 Eye of the North" },
        { "Norn",                          TitleID::Norn,                 "Character \xe2\x80\x94 Eye of the North" },
        { "Legendary Cartographer",        TitleID::LegendaryCarto,       "Character \xe2\x80\x94 All Campaigns" },
        { "Legendary Guardian",            TitleID::LegendaryGuardian,    "Character \xe2\x80\x94 All Campaigns" },
        { "Legendary Skill Hunter",        TitleID::LegendarySkillHunter, "Character \xe2\x80\x94 All Campaigns" },
        { "Legendary Vanquisher",          TitleID::LegendaryVanquisher,  "Character \xe2\x80\x94 All Campaigns" },
        { "Champion",                      TitleID::Champion,             "Account \xe2\x80\x94 Core" },
        { "Codex",                         TitleID::Codex,                "Account \xe2\x80\x94 Core" },
        { "Gamer",                         TitleID::Gamer,                "Account \xe2\x80\x94 Core" },
        { "Gladiator",                     TitleID::Gladiator,            "Account \xe2\x80\x94 Core" },
        { "Hero",                          TitleID::Hero,                 "Account \xe2\x80\x94 Core" },
        { "Lucky",                         TitleID::Lucky,                "Account \xe2\x80\x94 Core" },
        { "Treasure Hunter",               TitleID::TreasureHunter,       "Account \xe2\x80\x94 Core" },
        { "Unlucky",                       TitleID::Unlucky,              "Account \xe2\x80\x94 Core" },
        { "Wisdom",                        TitleID::Wisdom,               "Account \xe2\x80\x94 Core" },
        { "Zaishen",                       TitleID::Zaishen,              "Account \xe2\x80\x94 Core" },
        { "Kurzick",                       TitleID::Kurzick,              "Account \xe2\x80\x94 Factions" },
        { "Luxon",                         TitleID::Luxon,                "Account \xe2\x80\x94 Factions" },
        { "Commander",                     TitleID::Commander,            "Account \xe2\x80\x94 Nightfall / EotN" },
    };
    constexpr int NUM_TITLES = static_cast<int>(sizeof(s_titles) / sizeof(s_titles[0]));

    ImGui::SetNextItemWidth(-1.f);
    ImGui::InputText("##titlefilter", title_filter_buf_, sizeof(title_filter_buf_));
    ImGui::SameLine(); if (ImGui::SmallButton("x##titlefx")) title_filter_buf_[0] = '\0';

    const char* prev_group = nullptr;
    const bool  searching  = title_filter_buf_[0] != '\0';

    if (ImGui::BeginListBox("##titlelist", { -1.f, 180.f })) {
        for (int i = 0; i < NUM_TITLES; ++i) {
            const TitleEntry& e = s_titles[i];
            if (searching) {
                auto it = std::search(e.name, e.name + strlen(e.name),
                    title_filter_buf_, title_filter_buf_ + strlen(title_filter_buf_),
                    [](char a, char b) { return tolower((unsigned char)a) == tolower((unsigned char)b); });
                if (it == e.name + strlen(e.name)) continue;
            }
            if (!searching && e.group != prev_group) {
                prev_group = e.group;
                ImGui::SeparatorText(e.group);
            }
            const bool selected = (edit_title_id_ == static_cast<int>(e.id));
            ImGui::Indent(searching ? 0.f : 8.f);
            if (ImGui::Selectable(e.name, selected)) {
                if (edit_title_id_ != static_cast<int>(e.id)) {
                    edit_title_id_ = static_cast<int>(e.id);
                    edit_title_rank_ = -1; // reset rank so it defaults to max for the new title
                }
            }
            ImGui::Unindent(searching ? 0.f : 8.f);
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndListBox();
    }

    const char* sel_title_name = nullptr;
    for (int i = 0; i < NUM_TITLES; ++i) {
        if (static_cast<int>(s_titles[i].id) == edit_title_id_) {
            sel_title_name = s_titles[i].name;
            break;
        }
    }

    auto* live_title = GW::PlayerMgr::GetTitleTrack(static_cast<TitleID>(edit_title_id_));
    const bool can_add = (sel_title_name != nullptr);

    // Rank selector.
    // max_title_rank = total ranks for this title.
    // max_title_tier_index = global flat index of rank-1 tier (anchor).
    // Rank R global index = max_title_tier_index + (R - 1).
    if (can_add && live_title) {
        const int total_ranks   = static_cast<int>(live_title->max_title_rank);
        const int max_tier_base = static_cast<int>(live_title->max_title_tier_index);
        const bool pct_title    = live_title->is_percentage_based();
        auto* wc = GW::GetWorldContext();

        if (total_ranks > 0) {
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
        }
    } else if (can_add) {
        ImGui::TextDisabled("(connect to game to see rank list)");
    }

    if (!can_add) ImGui::BeginDisabled();
    if (ImGui::Button("Add Goal##titleadd")) {
        const int total_ranks   = live_title && live_title->max_title_rank > 0
                                    ? static_cast<int>(live_title->max_title_rank) : edit_title_rank_;
        const int max_tier_base = live_title ? static_cast<int>(live_title->max_title_tier_index) : 0;
        const int global_tier_idx = max_tier_base + (edit_title_rank_ - 1);

        char label_buf[160];
        snprintf(label_buf, sizeof(label_buf), "%s r%d/%d", sel_title_name, edit_title_rank_, total_ranks);

        GoalEntry g;
        g.label            = label_buf;
        g.trigger.type     = GoalTrigger::Type::ReachTitleRank;
        g.trigger.title_id = static_cast<TitleID>(edit_title_id_);
        g.trigger.level    = global_tier_idx;
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
// Running profile: combined from/to leg picker
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
            const bool is_exp  = (inf->type == GW::RegionType::ExplorableZone ||
                                   inf->type == GW::RegionType::MissionArea);
            const bool is_town = (inf->type == GW::RegionType::City          ||
                                   inf->type == GW::RegionType::Outpost       ||
                                   inf->type == GW::RegionType::MissionOutpost);
            if (!is_exp && !is_town) continue;
            if (seen.count(inf->name_id)) continue;
            seen[inf->name_id] = id;
            out.push_back({ id, MapNames::Get(mid) });
        }
        std::sort(out.begin(), out.end(), [](const MapRow& a, const MapRow& b) {
            return a.name < b.name;
        });
        return out;
    };

    static std::vector<MapRow> s_map_list;
    if (s_map_list.empty()) s_map_list = build_map_list();

    // Single map picker — each split fires when entering this map.
    const std::string sel_name = run_map_id_ > 0 ? MapNames::Get(static_cast<MapID>(run_map_id_)) : std::string{};
    const char* preview = run_map_id_ > 0 ? sel_name.c_str() : "(pick map)";
    ImGui::SetNextItemWidth(200.f);
    if (ImGui::BeginCombo("Map##runmap", preview)) {
        ImGui::SetNextItemWidth(-1.f);
        ImGui::InputText("##runmapflt", run_filter_, sizeof(run_filter_));
        const std::string_view filt(run_filter_);
        for (const auto& row : s_map_list) {
            if (!filt.empty()) {
                bool match = false;
                for (size_t i = 0; i + filt.size() <= row.name.size(); ++i)
                    if (_strnicmp(row.name.c_str() + i, filt.data(), filt.size()) == 0) { match = true; break; }
                if (!match) continue;
            }
            const bool sel = (row.id == run_map_id_);
            if (ImGui::Selectable(row.name.c_str(), sel)) run_map_id_ = row.id;
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    // Auto-fill label from map name.
    if (run_map_id_ > 0 && edit_label_[0] == '\0') {
        const std::string fn = MapNames::Get(static_cast<MapID>(run_map_id_));
        snprintf(edit_label_, sizeof(edit_label_), "%s", fn.c_str());
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(180.f);
    ImGui::InputText("Label##runlabel", edit_label_, sizeof(edit_label_));

    const bool can_add = run_map_id_ > 0 && edit_label_[0] != '\0';
    if (!can_add) ImGui::BeginDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Add Split")) {
        GoalList* list = plugin.List();
        GoalEntry g;
        g.label = edit_label_;
        GoalTrigger st;
        st.type   = GoalTrigger::Type::MapEnter;
        st.map_id = static_cast<MapID>(run_map_id_);
        g.start_trigger          = st;
        g.auto_complete_previous = 1;
        list->goals.push_back(std::move(g));
        list->RenumberDuplicateLabels();
        edit_label_[0] = '\0';
        run_map_id_    = 0;
    }
    ImGui::SameLine();
    if (ImGui::Button("Add End")) {
        GoalList* list = plugin.List();
        GoalEntry g;
        g.label              = edit_label_;
        g.trigger.type       = GoalTrigger::Type::MapEnter;
        g.trigger.map_id     = static_cast<MapID>(run_map_id_);
        g.auto_complete_previous = 1; // closes the last running leg before this goal fires
        list->goals.push_back(std::move(g));
        list->RenumberDuplicateLabels();
        edit_label_[0] = '\0';
        run_map_id_    = 0;
    }
    if (!can_add) ImGui::EndDisabled();
    ImGui::TextDisabled("Add Split: fires on entry, ends previous leg.  Add End: destination closes the run.");
}

// ---------------------------------------------------------------------------
// Town batch picker
// ---------------------------------------------------------------------------
void SplitsGoalListWindow::DrawTownBatchPicker(SplitsWindow& plugin)
{
    using MapID = GW::Constants::MapID;
    using Camp  = GW::Constants::Campaign;

    static const GW::RegionType s_town_types[] = {
        GW::RegionType::City,
        GW::RegionType::Outpost,
        GW::RegionType::MissionOutpost,
        GW::RegionType::Challenge,
        GW::RegionType::Marketplace,
        GW::RegionType::HeroBattleOutpost,
        GW::RegionType::ZaishenBattle,
    };
    static const Camp s_camps[]       = { Camp::Prophecies, Camp::Factions, Camp::Nightfall, Camp::EyeOfTheNorth };
    static const char* s_camp_labels[] = { "Prophecies", "Factions", "Nightfall", "EotN" };
    constexpr int NUM_CAMPS = 4;

    struct TownRow { int id; std::string name; GW::Region region; };

    auto build_town_list = [](Camp camp) -> std::vector<TownRow> {
        struct Cand { int id; GW::Region region; };
        std::unordered_map<uint32_t, Cand> best;
        for (int id = 1; id < static_cast<int>(MapID::Count); ++id) {
            const auto mid  = static_cast<MapID>(id);
            const auto* inf = GW::Map::GetMapInfo(mid);
            if (!inf || !inf->name_id || !inf->GetIsOnWorldMap() || inf->campaign != camp) continue;
            bool is_town = false;
            for (auto t : s_town_types) if (inf->type == t) { is_town = true; break; }
            if (!is_town) continue;
            if (best.find(inf->name_id) == best.end())
                best[inf->name_id] = { id, inf->region };
        }
        std::vector<TownRow> out;
        out.reserve(best.size());
        for (const auto& kv : best)
            out.push_back({ kv.second.id, MapNames::Get(static_cast<MapID>(kv.second.id)), kv.second.region });
        std::sort(out.begin(), out.end(), [](const TownRow& a, const TownRow& b) {
            if (a.region != b.region) return a.region < b.region;
            return a.name < b.name;
        });
        return out;
    };

    ImGui::SetNextItemWidth(-1.f);
    ImGui::InputText("##townfilter", town_filter_buf_, sizeof(town_filter_buf_));
    ImGui::SameLine(); if (ImGui::SmallButton("x##townfx")) town_filter_buf_[0] = '\0';

    constexpr uint8_t BIT_ENTER = 1;
    constexpr uint8_t BIT_LEAVE = 2;

    if (ImGui::BeginTabBar("##town_campaigns")) {
        for (int ci = 0; ci < NUM_CAMPS; ++ci) {
            if (!ImGui::BeginTabItem(s_camp_labels[ci])) continue;
            const auto rows = build_town_list(s_camps[ci]);

            std::vector<const TownRow*> filtered;
            for (const auto& r : rows) {
                if (town_filter_buf_[0] != '\0') {
                    auto it = std::search(r.name.begin(), r.name.end(),
                        town_filter_buf_, town_filter_buf_ + strlen(town_filter_buf_),
                        [](char a, char b) { return tolower((unsigned char)a) == tolower((unsigned char)b); });
                    if (it == r.name.end()) continue;
                }
                filtered.push_back(&r);
            }

            if (ImGui::SmallButton("All En"))  { for (auto* r : filtered) batch_town_checked_[r->id] |=  BIT_ENTER; }
            ImGui::SameLine();
            if (ImGui::SmallButton("None En")) { for (auto* r : filtered) batch_town_checked_[r->id] &= ~BIT_ENTER; }
            ImGui::SameLine(0, 10);
            if (ImGui::SmallButton("All Lv"))  { for (auto* r : filtered) batch_town_checked_[r->id] |=  BIT_LEAVE; }
            ImGui::SameLine();
            if (ImGui::SmallButton("None Lv")) { for (auto* r : filtered) batch_town_checked_[r->id] &= ~BIT_LEAVE; }

            constexpr ImGuiTableFlags tflags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY;
            if (ImGui::BeginTable("##towntbl", 4, tflags, { -1.f, 220.f })) {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("Town / Outpost", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Enter",          ImGuiTableColumnFlags_WidthFixed, 40.f);
                ImGui::TableSetupColumn("Leave",          ImGuiTableColumnFlags_WidthFixed, 40.f);
                ImGui::TableSetupColumn("##cur",          ImGuiTableColumnFlags_WidthFixed, 24.f);
                ImGui::TableHeadersRow();

                GW::Region prev_reg = static_cast<GW::Region>(0xFFFFFFFFu);
                for (const auto* r : filtered) {
                    ImGui::PushID(r->id);
                    if (town_filter_buf_[0] == '\0' && r->region != prev_reg) {
                        prev_reg = r->region;
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        const char* rname = RegionName(r->region);
                        if (rname) ImGui::TextDisabled("%s", rname);
                    }
                    ImGui::TableNextRow();
                    uint8_t& bits = batch_town_checked_[r->id];

                    ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(r->name.c_str());
                    ImGui::TableSetColumnIndex(1);
                    bool en = (bits & BIT_ENTER) != 0;
                    if (ImGui::Checkbox("##en", &en)) bits = en ? (bits | BIT_ENTER) : (bits & ~BIT_ENTER);
                    ImGui::TableSetColumnIndex(2);
                    bool lv = (bits & BIT_LEAVE) != 0;
                    if (ImGui::Checkbox("##lv", &lv)) bits = lv ? (bits | BIT_LEAVE) : (bits & ~BIT_LEAVE);
                    ImGui::TableSetColumnIndex(3);
                    if (ImGui::SmallButton("@")) {
                        const int cur = static_cast<int>(plugin.CurrentMap());
                        if (cur == r->id) bits |= (BIT_ENTER | BIT_LEAVE);
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Current map?");
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Pre-Searing")) {
            auto build_ps_town_list = []() -> std::vector<TownRow> {
                std::unordered_map<uint32_t, int> best;
                for (int id = 1; id < static_cast<int>(MapID::Count); ++id) {
                    const auto mid = static_cast<MapID>(id);
                    const auto* inf = GW::Map::GetMapInfo(mid);
                    if (!inf || !inf->name_id || inf->region != GW::Region_Presearing) continue;
                    bool is_town = false;
                    for (auto t : s_town_types) if (inf->type == t) { is_town = true; break; }
                    if (!is_town) continue;
                    if (best.find(inf->name_id) == best.end()) best[inf->name_id] = id;
                }
                std::vector<TownRow> out;
                out.reserve(best.size());
                for (const auto& kv : best)
                    out.push_back({ kv.second, MapNames::Get(static_cast<MapID>(kv.second)), GW::Region_Presearing });
                std::sort(out.begin(), out.end(), [](const TownRow& a, const TownRow& b) { return a.name < b.name; });
                return out;
            };
            const auto ps_rows = build_ps_town_list();

            std::vector<const TownRow*> ps_filtered;
            for (const auto& r : ps_rows) {
                if (town_filter_buf_[0] != '\0') {
                    auto it = std::search(r.name.begin(), r.name.end(),
                        town_filter_buf_, town_filter_buf_ + strlen(town_filter_buf_),
                        [](char a, char b) { return tolower((unsigned char)a) == tolower((unsigned char)b); });
                    if (it == r.name.end()) continue;
                }
                ps_filtered.push_back(&r);
            }

            if (ImGui::SmallButton("All En"))  { for (auto* r : ps_filtered) batch_town_checked_[r->id] |=  BIT_ENTER; }
            ImGui::SameLine();
            if (ImGui::SmallButton("None En")) { for (auto* r : ps_filtered) batch_town_checked_[r->id] &= ~BIT_ENTER; }
            ImGui::SameLine(0, 10);
            if (ImGui::SmallButton("All Lv"))  { for (auto* r : ps_filtered) batch_town_checked_[r->id] |=  BIT_LEAVE; }
            ImGui::SameLine();
            if (ImGui::SmallButton("None Lv")) { for (auto* r : ps_filtered) batch_town_checked_[r->id] &= ~BIT_LEAVE; }

            constexpr ImGuiTableFlags ps_tflags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY;
            if (ImGui::BeginTable("##towntbl_ps", 4, ps_tflags, { -1.f, 220.f })) {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("Town / Outpost", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Enter",          ImGuiTableColumnFlags_WidthFixed, 40.f);
                ImGui::TableSetupColumn("Leave",          ImGuiTableColumnFlags_WidthFixed, 40.f);
                ImGui::TableSetupColumn("##cur",          ImGuiTableColumnFlags_WidthFixed, 24.f);
                ImGui::TableHeadersRow();

                for (const auto* r : ps_filtered) {
                    ImGui::PushID(r->id);
                    ImGui::TableNextRow();
                    uint8_t& bits = batch_town_checked_[r->id];

                    ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(r->name.c_str());
                    ImGui::TableSetColumnIndex(1);
                    bool en = (bits & BIT_ENTER) != 0;
                    if (ImGui::Checkbox("##en", &en)) bits = en ? (bits | BIT_ENTER) : (bits & ~BIT_ENTER);
                    ImGui::TableSetColumnIndex(2);
                    bool lv = (bits & BIT_LEAVE) != 0;
                    if (ImGui::Checkbox("##lv", &lv)) bits = lv ? (bits | BIT_LEAVE) : (bits & ~BIT_LEAVE);
                    ImGui::TableSetColumnIndex(3);
                    if (ImGui::SmallButton("@")) {
                        const int cur = static_cast<int>(plugin.CurrentMap());
                        if (cur == r->id) bits |= (BIT_ENTER | BIT_LEAVE);
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Current map?");
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    int total = 0;
    for (const auto& [id, bits] : batch_town_checked_) {
        if (bits & BIT_ENTER) ++total;
        if (bits & BIT_LEAVE) ++total;
    }

    if (total == 0) ImGui::BeginDisabled();
    char add_lbl[48];
    snprintf(add_lbl, sizeof(add_lbl), "Add %d Goal%s##townadd", total, total == 1 ? "" : "s");
    if (ImGui::Button(add_lbl)) {
        GoalList* list = plugin.List();
        struct Item { int id; GoalTrigger::Type type; Camp camp; GW::Region region; std::string name; };
        std::vector<Item> items;
        items.reserve(total);
        for (const auto& [id, bits] : batch_town_checked_) {
            if (!bits) continue;
            const auto mid = static_cast<MapID>(id);
            const auto* inf = GW::Map::GetMapInfo(mid);
            const Camp c = inf ? inf->campaign : Camp::Prophecies;
            const GW::Region reg = inf ? inf->region : static_cast<GW::Region>(0);
            const std::string nm = MapNames::Get(mid);
            if (bits & BIT_ENTER) items.push_back({ id, GoalTrigger::Type::MapEnter,    c, reg, nm });
            if (bits & BIT_LEAVE) items.push_back({ id, GoalTrigger::Type::ExitOutpost, c, reg, nm });
        }
        std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
            if (a.camp   != b.camp)   return a.camp   < b.camp;
            if (a.region != b.region) return a.region < b.region;
            if (a.name   != b.name)   return a.name   < b.name;
            return a.type < b.type;
        });
        for (const auto& item : items) {
            GoalEntry g;
            g.label = (item.type == GoalTrigger::Type::ExitOutpost) ? "Leave " + item.name : "Enter " + item.name;
            g.trigger.type   = item.type;
            g.trigger.map_id = static_cast<MapID>(item.id);
            list->goals.push_back(std::move(g));
        }
        list->RenumberDuplicateLabels();
        batch_town_checked_.clear();
    }
    if (total == 0) ImGui::EndDisabled();
}

// ---------------------------------------------------------------------------
// Explorable batch picker
// ---------------------------------------------------------------------------
void SplitsGoalListWindow::DrawExplorableBatchPicker(SplitsWindow& plugin)
{
    using MapID = GW::Constants::MapID;
    using Camp  = GW::Constants::Campaign;

    static const Camp s_camps[]       = { Camp::Prophecies, Camp::Factions, Camp::Nightfall, Camp::EyeOfTheNorth };
    static const char* s_camp_labels[] = { "Prophecies", "Factions", "Nightfall", "EotN" };
    constexpr int NUM_CAMPS = 4;

    struct ExpRow { int id; std::string name; GW::Region region; };

    auto build_exp_list = [](Camp camp) -> std::vector<ExpRow> {
        struct Cand { int id; int type_rank; GW::Region region; };
        std::unordered_map<uint32_t, Cand> best;
        for (int id = 1; id < static_cast<int>(MapID::Count); ++id) {
            const auto mid = static_cast<MapID>(id);
            const auto* info = GW::Map::GetMapInfo(mid);
            if (!info || !info->name_id || !info->GetIsOnWorldMap() || info->campaign != camp) continue;
            if (info->type != GW::RegionType::ExplorableZone && info->type != GW::RegionType::MissionArea) continue;
            const int rank = (info->type == GW::RegionType::ExplorableZone) ? 0 : 1;
            auto it = best.find(info->name_id);
            if (it == best.end() || rank < it->second.type_rank)
                best[info->name_id] = { id, rank, info->region };
        }
        std::vector<ExpRow> out;
        out.reserve(best.size());
        for (const auto& kv : best)
            out.push_back({ kv.second.id, MapNames::Get(static_cast<MapID>(kv.second.id)), kv.second.region });
        std::sort(out.begin(), out.end(), [](const ExpRow& a, const ExpRow& b) {
            if (a.region != b.region) return a.region < b.region;
            return a.name < b.name;
        });
        return out;
    };

    ImGui::SetNextItemWidth(-1.f);
    ImGui::InputText("##expfilter", exp_filter_buf_, sizeof(exp_filter_buf_));
    ImGui::SameLine(); if (ImGui::SmallButton("x##expfx")) exp_filter_buf_[0] = '\0';

    constexpr uint8_t BIT_ENTER = 1;
    constexpr uint8_t BIT_VQ    = 2;
    constexpr uint8_t BIT_LEAVE = 4;

    if (ImGui::BeginTabBar("##exp_campaigns")) {
        for (int ci = 0; ci < NUM_CAMPS; ++ci) {
            if (!ImGui::BeginTabItem(s_camp_labels[ci])) continue;
            const auto rows = build_exp_list(s_camps[ci]);

            std::vector<const ExpRow*> filtered;
            for (const auto& r : rows) {
                if (exp_filter_buf_[0] != '\0') {
                    auto it = std::search(r.name.begin(), r.name.end(),
                        exp_filter_buf_, exp_filter_buf_ + strlen(exp_filter_buf_),
                        [](char a, char b) { return tolower((unsigned char)a) == tolower((unsigned char)b); });
                    if (it == r.name.end()) continue;
                }
                filtered.push_back(&r);
            }

            if (ImGui::SmallButton("All En"))  { for (auto* r : filtered) batch_exp_checked_[r->id] |=  BIT_ENTER; }
            ImGui::SameLine();
            if (ImGui::SmallButton("None En")) { for (auto* r : filtered) batch_exp_checked_[r->id] &= ~BIT_ENTER; }
            ImGui::SameLine(0, 10);
            if (ImGui::SmallButton("All VQ"))  { for (auto* r : filtered) batch_exp_checked_[r->id] |=  BIT_VQ; }
            ImGui::SameLine();
            if (ImGui::SmallButton("None VQ")) { for (auto* r : filtered) batch_exp_checked_[r->id] &= ~BIT_VQ; }
            ImGui::SameLine(0, 10);
            if (ImGui::SmallButton("All Lv"))  { for (auto* r : filtered) batch_exp_checked_[r->id] |=  BIT_LEAVE; }
            ImGui::SameLine();
            if (ImGui::SmallButton("None Lv")) { for (auto* r : filtered) batch_exp_checked_[r->id] &= ~BIT_LEAVE; }

            constexpr ImGuiTableFlags tflags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY;
            if (ImGui::BeginTable("##exptbl", 5, tflags, { -1.f, 220.f })) {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("Explorable", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Enter",      ImGuiTableColumnFlags_WidthFixed, 40.f);
                ImGui::TableSetupColumn("VQ",         ImGuiTableColumnFlags_WidthFixed, 30.f);
                ImGui::TableSetupColumn("Leave",      ImGuiTableColumnFlags_WidthFixed, 40.f);
                ImGui::TableSetupColumn("##cur",      ImGuiTableColumnFlags_WidthFixed, 24.f);
                ImGui::TableHeadersRow();

                GW::Region prev_reg = static_cast<GW::Region>(0xFFFFFFFFu);
                for (const auto* r : filtered) {
                    ImGui::PushID(r->id);
                    if (exp_filter_buf_[0] == '\0' && r->region != prev_reg) {
                        prev_reg = r->region;
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        const char* rname = RegionName(r->region);
                        if (rname) ImGui::TextDisabled("%s", rname);
                    }
                    ImGui::TableNextRow();
                    uint8_t& bits = batch_exp_checked_[r->id];

                    ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(r->name.c_str());
                    ImGui::TableSetColumnIndex(1);
                    bool en = (bits & BIT_ENTER) != 0;
                    if (ImGui::Checkbox("##en", &en)) bits = en ? (bits | BIT_ENTER) : (bits & ~BIT_ENTER);
                    ImGui::TableSetColumnIndex(2);
                    bool vq = (bits & BIT_VQ) != 0;
                    if (ImGui::Checkbox("##vq", &vq)) bits = vq ? (bits | BIT_VQ) : (bits & ~BIT_VQ);
                    ImGui::TableSetColumnIndex(3);
                    bool lv = (bits & BIT_LEAVE) != 0;
                    if (ImGui::Checkbox("##lv", &lv)) bits = lv ? (bits | BIT_LEAVE) : (bits & ~BIT_LEAVE);
                    ImGui::TableSetColumnIndex(4);
                    if (ImGui::SmallButton("@")) {
                        const int cur = static_cast<int>(plugin.CurrentMap());
                        if (cur == r->id) bits |= (BIT_ENTER | BIT_VQ | BIT_LEAVE);
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Current map?");
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Pre-Searing")) {
            auto build_ps_exp_list = []() -> std::vector<ExpRow> {
                std::unordered_map<uint32_t, int> best;
                for (int id = 1; id < static_cast<int>(MapID::Count); ++id) {
                    const auto mid = static_cast<MapID>(id);
                    const auto* inf = GW::Map::GetMapInfo(mid);
                    if (!inf || !inf->name_id || inf->region != GW::Region_Presearing) continue;
                    if (inf->type != GW::RegionType::ExplorableZone && inf->type != GW::RegionType::MissionArea) continue;
                    if (best.find(inf->name_id) == best.end()) best[inf->name_id] = id;
                }
                std::vector<ExpRow> out;
                out.reserve(best.size());
                for (const auto& kv : best)
                    out.push_back({ kv.second, MapNames::Get(static_cast<MapID>(kv.second)), GW::Region_Presearing });
                std::sort(out.begin(), out.end(), [](const ExpRow& a, const ExpRow& b) { return a.name < b.name; });
                return out;
            };
            const auto ps_rows = build_ps_exp_list();

            std::vector<const ExpRow*> ps_filtered;
            for (const auto& r : ps_rows) {
                if (exp_filter_buf_[0] != '\0') {
                    auto it = std::search(r.name.begin(), r.name.end(),
                        exp_filter_buf_, exp_filter_buf_ + strlen(exp_filter_buf_),
                        [](char a, char b) { return tolower((unsigned char)a) == tolower((unsigned char)b); });
                    if (it == r.name.end()) continue;
                }
                ps_filtered.push_back(&r);
            }

            if (ImGui::SmallButton("All En"))  { for (auto* r : ps_filtered) batch_exp_checked_[r->id] |=  BIT_ENTER; }
            ImGui::SameLine();
            if (ImGui::SmallButton("None En")) { for (auto* r : ps_filtered) batch_exp_checked_[r->id] &= ~BIT_ENTER; }
            ImGui::SameLine(0, 10);
            if (ImGui::SmallButton("All Lv"))  { for (auto* r : ps_filtered) batch_exp_checked_[r->id] |=  BIT_LEAVE; }
            ImGui::SameLine();
            if (ImGui::SmallButton("None Lv")) { for (auto* r : ps_filtered) batch_exp_checked_[r->id] &= ~BIT_LEAVE; }

            constexpr ImGuiTableFlags ps_tflags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY;
            if (ImGui::BeginTable("##exptbl_ps", 4, ps_tflags, { -1.f, 220.f })) {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("Explorable", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Enter",      ImGuiTableColumnFlags_WidthFixed, 40.f);
                ImGui::TableSetupColumn("Leave",      ImGuiTableColumnFlags_WidthFixed, 40.f);
                ImGui::TableSetupColumn("##cur",      ImGuiTableColumnFlags_WidthFixed, 24.f);
                ImGui::TableHeadersRow();

                for (const auto* r : ps_filtered) {
                    ImGui::PushID(r->id);
                    ImGui::TableNextRow();
                    uint8_t& bits = batch_exp_checked_[r->id];

                    ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(r->name.c_str());
                    ImGui::TableSetColumnIndex(1);
                    bool en = (bits & BIT_ENTER) != 0;
                    if (ImGui::Checkbox("##en", &en)) bits = en ? (bits | BIT_ENTER) : (bits & ~BIT_ENTER);
                    ImGui::TableSetColumnIndex(2);
                    bool lv = (bits & BIT_LEAVE) != 0;
                    if (ImGui::Checkbox("##lv", &lv)) bits = lv ? (bits | BIT_LEAVE) : (bits & ~BIT_LEAVE);
                    ImGui::TableSetColumnIndex(3);
                    if (ImGui::SmallButton("@")) {
                        const int cur = static_cast<int>(plugin.CurrentMap());
                        if (cur == r->id) bits |= (BIT_ENTER | BIT_LEAVE);
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Current map?");
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    int total = 0;
    for (const auto& [id, bits] : batch_exp_checked_) {
        if (bits & BIT_ENTER) ++total;
        if (bits & BIT_VQ)    ++total;
        if (bits & BIT_LEAVE) ++total;
    }

    if (total == 0) ImGui::BeginDisabled();
    char add_lbl[48];
    snprintf(add_lbl, sizeof(add_lbl), "Add %d Goal%s##expadd", total, total == 1 ? "" : "s");
    if (ImGui::Button(add_lbl)) {
        struct Item { int id; GoalTrigger::Type type; Camp camp; GW::Region region; std::string name; };
        std::vector<Item> items;
        items.reserve(total);
        for (const auto& [id, bits] : batch_exp_checked_) {
            if (!bits) continue;
            const auto mid = static_cast<MapID>(id);
            const auto* inf = GW::Map::GetMapInfo(mid);
            const Camp c = inf ? inf->campaign : Camp::Prophecies;
            const GW::Region reg = inf ? inf->region : static_cast<GW::Region>(0);
            const std::string nm = MapNames::Get(mid);
            if (bits & BIT_ENTER) items.push_back({ id, GoalTrigger::Type::MapEnter,         c, reg, nm });
            if (bits & BIT_VQ)    items.push_back({ id, GoalTrigger::Type::VanquishComplete,  c, reg, nm });
            if (bits & BIT_LEAVE) items.push_back({ id, GoalTrigger::Type::ExitExplorable,    c, reg, nm });
        }
        auto type_order = [](GoalTrigger::Type t) {
            switch (t) {
                case GoalTrigger::Type::MapEnter:        return 0;
                case GoalTrigger::Type::VanquishComplete:return 1;
                case GoalTrigger::Type::ExitExplorable:  return 2;
                default: return 3;
            }
        };
        std::sort(items.begin(), items.end(), [&](const Item& a, const Item& b) {
            if (a.camp   != b.camp)   return a.camp   < b.camp;
            if (a.region != b.region) return a.region < b.region;
            if (a.name   != b.name)   return a.name   < b.name;
            return type_order(a.type) < type_order(b.type);
        });
        GoalList* list = plugin.List();
        for (const auto& item : items) {
            GoalEntry g;
            if (item.type == GoalTrigger::Type::MapEnter)               g.label = "Enter " + item.name;
            else if (item.type == GoalTrigger::Type::VanquishComplete)  g.label = "VQ " + item.name;
            else if (item.type == GoalTrigger::Type::ExitExplorable)    g.label = "Leave " + item.name;
            else                                                         g.label = item.name;
            g.trigger.type   = item.type;
            g.trigger.map_id = static_cast<MapID>(item.id);
            list->goals.push_back(std::move(g));
        }
        list->RenumberDuplicateLabels();
        batch_exp_checked_.clear();
    }
    if (total == 0) ImGui::EndDisabled();
}

// ---------------------------------------------------------------------------
// Mission batch picker
// ---------------------------------------------------------------------------
void SplitsGoalListWindow::DrawMissionBatchPicker(SplitsWindow& plugin)
{
    using MapID = GW::Constants::MapID;
    using Camp  = GW::Constants::Campaign;

    struct MissionRow { int id; std::string name; uint32_t chron; };

    static const Camp s_camps[]       = { Camp::Prophecies, Camp::Factions, Camp::Nightfall, Camp::EyeOfTheNorth };
    static const char* s_camp_labels[] = { "Prophecies", "Factions", "Nightfall", "EotN" };
    constexpr int NUM_CAMPS = 3; // EotN skipped

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
                out.push_back({ static_cast<int>(order[i]), MapNames::Get(order[i]), i + 1 });
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
                out.push_back({ static_cast<int>(order[i]), MapNames::Get(order[i]), i + 1 });
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
                out.push_back({ static_cast<int>(order[i]), MapNames::Get(order[i]), i + 1 });
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
            out.push_back({ kv.second.id, MapNames::Get(static_cast<MapID>(kv.second.id)), kv.second.chron });
        std::sort(out.begin(), out.end(),
            [](const MissionRow& a, const MissionRow& b) { return a.chron < b.chron; });
        return out;
    };

    if (ImGui::BeginTabBar("##batch_campaigns")) {
        for (int ci = 0; ci < NUM_CAMPS; ++ci) {
            if (!ImGui::BeginTabItem(s_camp_labels[ci])) continue;
            const auto missions = build_list(s_camps[ci]);

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
            g.label = MapNames::Get(static_cast<MapID>(item.id));
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
