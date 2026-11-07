#pragma once

#include <glaze/glaze.hpp>

#include <string>

struct SplitsProfile {
    std::string name;

    bool stop_on_party_defeated = true;
    // Leave VQ/Mission/Bonus map unfinished = fail run.
    bool auto_fail_on_rezone    = true;
    bool auto_send_age          = false;

    enum class TimeDisplay : int { Real = 0, Game = 1, Both = 2 };
    TimeDisplay time_display       = TimeDisplay::Game;
    bool        both_header_only   = false; // Both: clock show both, rows game only
    // PB fastest. Average mean. SumOfBest best legs added. Failed runs skipped.
    enum class ComparisonMode : int { PB = 0, Average = 1, SumOfBest = 2 };
    ComparisonMode comparison_mode = ComparisonMode::PB;
    // SC: also gate Start delta. Start and End both pace.
    bool show_split_pb    = true;
    bool show_segment     = true;
    bool show_segment_pb  = true;
    bool show_paused_time = false;
    bool show_recent_runs = true;
    // SC: all rows Dynamic. Parallel starts break PB math.
    bool dynamic_by_default = false;
    // Running: move autostart, one goal per leg, wrong turn fail.
    bool sequential_route = false;
    // SC: after done/fail, next area entry reset + start. Farm loop.
    bool auto_reset_on_complete = false;

    // Reload on profile switch.
    std::string last_list_name;
};

// Saved fields only: name and the profile-identity flags (dynamic_by_default, sequential_route,
// auto_reset_on_complete) come from the Make*Profile factories, never from settings.
template <>
struct glz::meta<SplitsProfile> {
    using T = SplitsProfile;
    static constexpr auto value = glz::object(
        "stop_on_party_defeated", &T::stop_on_party_defeated,
        "auto_fail_on_rezone",    &T::auto_fail_on_rezone,
        "auto_send_age",          &T::auto_send_age,
        "time_display",           &T::time_display,
        "both_header_only",       &T::both_header_only,
        "comparison_mode",        &T::comparison_mode,
        "show_split_pb",          &T::show_split_pb,
        "show_segment",           &T::show_segment,
        "show_segment_pb",        &T::show_segment_pb,
        "show_paused_time",       &T::show_paused_time,
        "show_recent_runs",       &T::show_recent_runs,
        "last_list_name",         &T::last_list_name);
};

SplitsProfile MakeManualProfile();
SplitsProfile MakeRunningProfile();
SplitsProfile MakeSCProfile();
