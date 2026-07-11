#pragma once

#include <cstdint>
#include <string>
#include <Color.h>

class ToolboxIni;
class SettingsDoc;

// ---------------------------------------------------------------------------
// SplitsProfile — all per-profile settings: display, behaviour, timer rules.
// SplitsWindow owns an array of 2 of these and hot-switches between them.
// ---------------------------------------------------------------------------
struct SplitsProfile {
    // Human-readable label shown in the profile switcher.
    std::string name;

    // ---- Behavioural flags -------------------------------------------------
    bool stop_on_party_defeated = true;
    bool auto_send_age          = false;

    // ---- Display settings --------------------------------------------------
    enum class TimeDisplay : uint8_t { Real = 0, Game = 1, Both = 2 };
    TimeDisplay time_display       = TimeDisplay::Game;
    bool        both_header_only   = false; // Both: show Real+Game in clock only, rows stay game-only
    bool show_split_pb    = true;  // shows the cumulative-time-vs-PB delta under the time column
    bool show_segment     = true;
    bool show_segment_pb  = true;  // shows the segment-vs-PB-segment delta under the split column
    bool show_paused_time = false; // shows running total of manually-paused real time next to the clock

    Color color_completed = Colors::RGB(0,   255, 0  );   // goal label: done
    Color color_active    = Colors::RGB(255, 255, 255);   // goal label: current objective
    Color color_real_time = Colors::RGB(230, 230, 230);   // real time text; segment uses muted
    Color color_game_time = Colors::RGB(153, 217, 255);   // game time text; segment uses muted
    Color color_pb_ahead  = Colors::RGB(255, 217,   0);   // PB delta: ahead
    Color color_pb_behind = Colors::RGB(255, 102, 102);   // PB delta: behind; also failed goal label

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
