#include "stdafx.h"
#include "SplitsProfile.h"

#include <Color.h>

// ---------------------------------------------------------------------------
void SplitsProfile::LoadSettings(SettingsDoc& doc, ToolboxIni* ini, const char* section)
{
    auto getbool = [&](const char* key, bool& field) {
        if (!doc.Get(section, key, field))
            field = ini->GetBoolValue(section, key, field);
    };
    getbool("stop_on_party_defeated",    stop_on_party_defeated);
    getbool("auto_send_age",             auto_send_age);
    {
        auto v = static_cast<uint8_t>(time_display);
        if (!doc.Get(section, "time_display", v)) {
            const bool legacy_game = ini->GetBoolValue(section, "use_game_time", time_display == TimeDisplay::Game);
            v = legacy_game ? static_cast<uint8_t>(TimeDisplay::Game) : static_cast<uint8_t>(TimeDisplay::Real);
        }
        if (v <= 2) time_display = static_cast<TimeDisplay>(v);
    }
    getbool("both_header_only",          both_header_only);
    getbool("show_split_pb",             show_split_pb);
    getbool("show_segment",              show_segment);
    getbool("show_segment_pb",           show_segment_pb);
    getbool("show_paused_time",          show_paused_time);

    for (auto [key, field] : {
        std::pair{"color_completed",  &color_completed},
        {"color_active",    &color_active},
        {"color_real_time", &color_real_time},
        {"color_game_time", &color_game_time},
        {"color_pb_ahead",  &color_pb_ahead},
        {"color_pb_behind", &color_pb_behind},
    }) {
        if (!doc.Get(section, key, *field))
            *field = Colors::Load(ini, section, key, *field);
    }

    if (!doc.Get(section, "last_list_name", last_list_name)) {
        const char* v = ini->GetValue(section, "last_list_name", "");
        last_list_name = v ? v : "";
    }
}

void SplitsProfile::SaveSettings(SettingsDoc& doc, const char* section) const
{
    doc.Set(section, "stop_on_party_defeated",    stop_on_party_defeated);
    doc.Set(section, "auto_send_age",             auto_send_age);
    doc.Set(section, "time_display",              static_cast<uint8_t>(time_display));
    doc.Set(section, "both_header_only",   both_header_only);
    doc.Set(section, "show_split_pb",      show_split_pb);
    doc.Set(section, "show_segment",       show_segment);
    doc.Set(section, "show_segment_pb",           show_segment_pb);
    doc.Set(section, "show_paused_time",          show_paused_time);
    doc.Set(section, "color_completed",  color_completed);
    doc.Set(section, "color_active",     color_active);
    doc.Set(section, "color_real_time",  color_real_time);
    doc.Set(section, "color_game_time",  color_game_time);
    doc.Set(section, "color_pb_ahead",   color_pb_ahead);
    doc.Set(section, "color_pb_behind",  color_pb_behind);
    doc.Set(section, "last_list_name",            last_list_name);
}

// ---------------------------------------------------------------------------
SplitsProfile MakeManualProfile()
{
    SplitsProfile p;
    p.name                   = "Manual";
    p.stop_on_party_defeated = true;
    p.auto_send_age          = false;
    return p;
}

SplitsProfile MakeRunningProfile()
{
    SplitsProfile p;
    p.name                   = "Running";
    p.stop_on_party_defeated = false;
    p.auto_send_age             = false;
    p.show_segment              = true;   // "Split" column
    return p;
}
