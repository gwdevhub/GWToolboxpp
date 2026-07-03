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
    getbool("game_time_explorable_only", game_time_explorable_only);
    getbool("stop_on_party_defeated",    stop_on_party_defeated);
    getbool("auto_send_age",             auto_send_age);
    getbool("simple_order",              simple_order);
    getbool("use_game_time",             use_game_time);
    getbool("show_segment",              show_segment);
    getbool("show_segment_pb",           show_segment_pb);
    getbool("show_paused_time",          show_paused_time);

    for (auto [key, field] : {
        std::pair{"color_completed", &color_completed},
        {"color_active",   &color_active},
        {"color_inactive", &color_inactive},
        {"color_failed",   &color_failed},
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
    doc.Set(section, "game_time_explorable_only", game_time_explorable_only);
    doc.Set(section, "stop_on_party_defeated",    stop_on_party_defeated);
    doc.Set(section, "auto_send_age",             auto_send_age);
    doc.Set(section, "simple_order",              simple_order);
    doc.Set(section, "use_game_time",             use_game_time);
    doc.Set(section, "show_segment",              show_segment);
    doc.Set(section, "show_segment_pb",           show_segment_pb);
    doc.Set(section, "show_paused_time",          show_paused_time);
    doc.Set(section, "color_completed",           color_completed);
    doc.Set(section, "color_active",              color_active);
    doc.Set(section, "color_inactive",            color_inactive);
    doc.Set(section, "color_failed",              color_failed);
    doc.Set(section, "last_list_name",            last_list_name);
}

// ---------------------------------------------------------------------------
SplitsProfile MakeManualProfile()
{
    SplitsProfile p;
    p.name                     = "Manual";
    p.game_time_explorable_only = false;
    p.stop_on_party_defeated   = true;
    p.auto_send_age            = false;
    p.simple_order             = true;
    return p;
}

SplitsProfile MakeRunningProfile()
{
    SplitsProfile p;
    p.name                      = "Running";
    p.game_time_explorable_only = false;
    p.stop_on_party_defeated    = false;
    p.auto_send_age             = false;
    p.show_segment              = true;   // "Split" column
    return p;
}
