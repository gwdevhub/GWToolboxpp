#pragma once

#include <string>
#include <Color.h>

class ToolboxIni;
class SettingsDoc;

// ---------------------------------------------------------------------------
// SplitsProfile — all per-profile settings: display, behaviour, timer rules.
// SplitsWindow owns an array of 3 of these and hot-switches between them.
// ---------------------------------------------------------------------------
struct SplitsProfile {
    // Human-readable label shown in the profile switcher.
    std::string name;

    // ---- Timer rules -------------------------------------------------------
    bool game_time_explorable_only = false;

    // ---- Behavioural flags -------------------------------------------------
    bool stop_on_party_defeated = true;
    bool auto_send_age          = false;

    // ---- Display settings --------------------------------------------------
    bool show_start_time     = false;
    bool use_game_time       = true;  // true = show game time column; false = show real time column
    bool show_segment        = true;
    bool segment_is_duration = false;
    bool show_paused_time    = false; // shows running total of manually-paused real time next to the clock

    Color color_completed = Colors::RGB(0,   255, 0  );
    Color color_active    = Colors::RGB(255, 255, 255);
    Color color_inactive  = Colors::ARGB(255, 128, 128, 128);
    Color color_failed    = Colors::RGB(255, 0,   0  );

    // ---- Goal list binding -------------------------------------------------
    // Manual/Running: remembers the last-used list so it reloads on profile switch.
    std::string last_list_name;

    void LoadSettings(SettingsDoc& doc, ToolboxIni* legacy, const char* section);
    void SaveSettings(SettingsDoc& doc, const char* section) const;
};

// ---------------------------------------------------------------------------
// Factory helpers — produce profiles with sensible per-mode defaults.
// ---------------------------------------------------------------------------
SplitsProfile MakeSCProfile();
SplitsProfile MakeManualProfile();
SplitsProfile MakeRunningProfile();
