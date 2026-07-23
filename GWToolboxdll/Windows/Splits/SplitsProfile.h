#pragma once

#include <cstdint>
#include <string>
#include <Color.h>

class ToolboxIni;
class SettingsDoc;

// ---------------------------------------------------------------------------
// SplitsProfile — all per-profile settings: display, behaviour, timer rules.
// SplitsWindow owns an array of 3 of these (Manual/Running/SC) and hot-switches between them.
// ---------------------------------------------------------------------------
struct SplitsProfile {
    // Human-readable label shown in the profile switcher.
    std::string name;

    // ---- Behavioural flags -------------------------------------------------
    bool stop_on_party_defeated = true;
    // Fails the run if a VanquishComplete/MissionComplete/MissionBonus goal's target map
    // is left (rezoned out of) while that goal is still Started — i.e. attempted and
    // abandoned without completing it. Mirrors stop_on_party_defeated's semantics (whole
    // run fails), just a different detection condition.
    bool auto_fail_on_rezone    = true;
    bool auto_send_age          = false;

    // ---- Display settings --------------------------------------------------
    enum class TimeDisplay : uint8_t { Real = 0, Game = 1, Both = 2 };
    TimeDisplay time_display       = TimeDisplay::Game;
    bool        both_header_only   = false; // Both: show Real+Game in clock only, rows stay game-only
    // What the Ahead/Behind delta is computed against. PB = fastest completed run in history;
    // Average = mean of every non-failed run's time at that goal; LastRun = the most recent
    // attempt in history (whatever it reached, failed or not). Same color_pb_ahead/behind
    // pair is reused regardless of mode — only the comparison data source changes.
    enum class ComparisonMode : uint8_t { PB = 0, Average = 1, LastRun = 2 };
    ComparisonMode comparison_mode = ComparisonMode::PB;
    bool show_split_pb    = true;  // shows the cumulative-time-vs-comparison delta under the time column
    bool show_segment     = true;
    bool show_segment_pb  = true;  // shows the segment-vs-comparison-segment delta under the split column
    bool show_paused_time = false; // shows running total of manually-paused real time next to the clock
    // SC: every goal renders GoalEntry::DisplayStyle::Dynamic (Start/End/Duration) regardless
    // of its own display_style field, no PB/Average/Last-Run comparison — SC's whole point is
    // parallel/independent-start objectives (start_trigger), which that comparison math isn't
    // meaningful for. Manual/Running leave this false and stay Splits-style throughout.
    bool dynamic_by_default = false;

    Color color_completed = Colors::RGB(0,   255, 0  );   // goal label: done
    Color color_active    = Colors::RGB(255, 255, 255);   // goal label: current objective
    Color color_real_time = Colors::RGB(230, 230, 230);   // real time text; segment uses muted
    Color color_game_time = Colors::RGB(153, 217, 255);   // game time text; segment uses muted
    Color color_pb_ahead  = Colors::RGB(255, 217,   0);   // comparison delta: ahead (of PB/Average/Last Run per comparison_mode)
    Color color_pb_behind = Colors::RGB(255, 102, 102);   // comparison delta: behind; also failed goal label

    // ---- Goal list binding -------------------------------------------------
    // Manual/Running: remembers the last-used list so it reloads on profile switch.
    std::string last_list_name;

    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy, const char* section);
    void SaveSettings(SettingsDoc& doc, const char* section) const;
};

// ---------------------------------------------------------------------------
// Factory helpers — produce profiles with sensible per-mode defaults.
// ---------------------------------------------------------------------------
SplitsProfile MakeManualProfile();
SplitsProfile MakeRunningProfile();
SplitsProfile MakeSCProfile();
