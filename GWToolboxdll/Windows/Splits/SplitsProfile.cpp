#include "stdafx.h"
#include "SplitsProfile.h"

namespace {
struct BoolField { const char* key; bool SplitsProfile::* member; };
// Shared by LoadSettings/SaveSettings — one table instead of each key listed out separately on both sides.
constexpr BoolField kBoolFields[] = {
    {"stop_on_party_defeated", &SplitsProfile::stop_on_party_defeated},
    {"auto_fail_on_rezone",    &SplitsProfile::auto_fail_on_rezone},
    {"auto_send_age",          &SplitsProfile::auto_send_age},
    {"both_header_only",       &SplitsProfile::both_header_only},
    {"show_split_pb",          &SplitsProfile::show_split_pb},
    {"show_segment",           &SplitsProfile::show_segment},
    {"show_segment_pb",        &SplitsProfile::show_segment_pb},
    {"show_paused_time",       &SplitsProfile::show_paused_time},
    {"show_recent_runs",       &SplitsProfile::show_recent_runs},
    {"dynamic_by_default",     &SplitsProfile::dynamic_by_default},
    {"sequential_route",       &SplitsProfile::sequential_route},
    {"auto_reset_on_complete", &SplitsProfile::auto_reset_on_complete},
};
} // namespace

// ---------------------------------------------------------------------------
void SplitsProfile::LoadSettings(SettingsDoc& doc, ToolboxIni* ini, const char* section)
{
    for (const auto& f : kBoolFields) {
        bool& field = this->*f.member;
        if (!doc.Get(section, f.key, field))
            field = ini->GetBoolValue(section, f.key, field);
    }
    {
        auto v = static_cast<uint8_t>(time_display);
        if (!doc.Get(section, "time_display", v)) {
            const bool legacy_game = ini->GetBoolValue(section, "use_game_time", time_display == TimeDisplay::Game);
            v = legacy_game ? static_cast<uint8_t>(TimeDisplay::Game) : static_cast<uint8_t>(TimeDisplay::Real);
        }
        if (v <= 2) time_display = static_cast<TimeDisplay>(v);
    }
    {
        auto v = static_cast<uint8_t>(comparison_mode);
        if (!doc.Get(section, "comparison_mode", v))
            v = static_cast<uint8_t>(ini->GetLongValue(section, "comparison_mode", v));
        if (v <= 2) comparison_mode = static_cast<ComparisonMode>(v);
    }
    if (!doc.Get(section, "last_list_name", last_list_name)) {
        const char* v = ini->GetValue(section, "last_list_name", "");
        last_list_name = v ? v : "";
    }
}

void SplitsProfile::SaveSettings(SettingsDoc& doc, const char* section) const
{
    for (const auto& f : kBoolFields)
        doc.Set(section, f.key, this->*f.member);
    doc.Set(section, "time_display",    static_cast<uint8_t>(time_display));
    doc.Set(section, "comparison_mode", static_cast<uint8_t>(comparison_mode));
    doc.Set(section, "last_list_name",  last_list_name);
}

// ---------------------------------------------------------------------------
SplitsProfile MakeManualProfile()
{
    SplitsProfile p;
    p.name                   = "Manual";
    p.stop_on_party_defeated = true;
    p.auto_fail_on_rezone    = true;
    p.auto_send_age          = false;
    return p;
}

SplitsProfile MakeRunningProfile()
{
    SplitsProfile p;
    p.name                   = "Running";
    p.stop_on_party_defeated = false;
    p.auto_fail_on_rezone    = true;
    p.auto_send_age             = false;
    p.show_segment              = true;   // "Split" column
    p.sequential_route          = true;
    return p;
}

SplitsProfile MakeSCProfile()
{
    SplitsProfile p;
    p.name                   = "SC";
    p.stop_on_party_defeated = true;
    p.auto_fail_on_rezone    = true;
    p.auto_send_age          = false;
    p.dynamic_by_default     = true; // Start/End/Duration throughout — see field comment
    p.auto_reset_on_complete = true;
    return p;
}
