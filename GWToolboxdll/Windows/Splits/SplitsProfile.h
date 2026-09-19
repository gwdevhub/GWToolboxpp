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
    // Fails the run if a Vanquish/Mission/Bonus goal's map is left while still Started (attempted and abandoned); same whole-run-fails semantics as stop_on_party_defeated.
    bool auto_fail_on_rezone    = true;
    bool auto_send_age          = false;

    // ---- Display settings --------------------------------------------------
    enum class TimeDisplay : uint8_t { Real = 0, Game = 1, Both = 2 };
    TimeDisplay time_display       = TimeDisplay::Game;
    bool        both_header_only   = false; // Both: show Real+Game in clock only, rows stay game-only
    // PB = fastest completed run; Average = mean of non-failed runs; SumOfBest = cumulative sum of each leg's fastest-ever segment (non-failed runs) — only the data source changes, same Ahead/Behind coloring throughout.
    enum class ComparisonMode : uint8_t { PB = 0, Average = 1, SumOfBest = 2 };
    ComparisonMode comparison_mode = ComparisonMode::PB;
    bool show_split_pb    = true;  // shows the cumulative-time-vs-comparison delta under the time column
    bool show_segment     = true;
    bool show_segment_pb  = true;  // shows the segment-vs-comparison-segment delta under the split column
    bool show_paused_time = false; // shows running total of manually-paused real time next to the clock
    bool show_recent_runs = false;
    // SC forces every goal to DisplayStyle::Dynamic regardless of its own field — SC's parallel/independent-start objectives don't fit PB/Average/Last-Run comparison math. Manual/Running leave this false.
    bool dynamic_by_default = false;
    // Running's zone-transition-chain behavior (movement-based autostart, ordered single-goal-per-leg picker, wrong-turn auto-fail) — read by name instead of comparing ActiveProfileIdx() to a raw index. Manual/SC leave this false.
    bool sequential_route = false;
    // After a run completes or fails, waits for the same re-entry signal that would start a fresh run, then resets and starts in one step — lets farming presets loop without a manual Reset. SC-only.
    bool auto_reset_on_complete = false;

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
