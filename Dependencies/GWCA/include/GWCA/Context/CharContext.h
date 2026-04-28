#pragma once

#include <GWCA/GameContainers/Array.h>
#include <GWCA/Utilities/Export.h>

namespace GW {
    namespace Constants {
        enum class Language;
        enum class MapID : uint32_t;
        enum class InstanceType;
    }
    struct CharContext;
    GWCA_API CharContext* GetCharContext();

    struct ObserverMatch;

    struct ProgressBarContext {
        int     pips;
        uint8_t color[4];      // RGBA
        uint8_t background[4]; // RGBA
        int     unk[7];
        float   progress;      // 0 ... 1
        // possibly more
    };

    struct CharContext { // total: 0x434
        /* +h0000 */ Array<void*> h0000;
        /* +h0010 */ uint32_t h0010;
        /* +h0014 */ Array<void*> h0014;
        /* +h0024 */ uint32_t h0024[4];
        /* +h0034 */ Array<void*> h0034;
        /* +h0044 */ Array<void*> h0044;
        /* +h0054 */ uint32_t h0054[4]; // load head variables
        /* +h0064 */ uint32_t player_uuid[4]; // uuid
        /* +h0074 */ wchar_t player_name[0x14];
        /* +h009C */ uint32_t h009C[20];
        /* +h00EC */ Array<void*> h00EC;
        /* +h00FC */ uint32_t h00FC[37]; // 40
        /* +h0190 */ uint32_t world_flags;
        /* +h0194 */ uint32_t token1; // world id
        /* +h0198 */ GW::Constants::MapID map_id;
        /* +h019C */ uint32_t is_explorable;
        /* +h01A0 */ uint8_t host[0x18];
        /* +h01B8 */ uint32_t token2; // player id
        /* +h01BC */ uint32_t h01BC[27];
        /* +h0228 */ uint32_t district_number;
        /* +h022C */ GW::Constants::Language language;
        /* +h0230 */ GW::Constants::MapID observe_map_id;
        /* +h0234 */ GW::Constants::MapID current_map_id;
        /* +h0238 */ Constants::InstanceType observe_map_type;
        /* +h023C */ Constants::InstanceType current_map_type;
        /* +h0240 */ uint32_t h0240[5];
        /* +h0254 */ Array<ObserverMatch*> observer_matches;
        /* +h0264 */ uint32_t h0264[17];
        /* +h02A8 */ uint32_t player_flags; // bitwise something
        /* +h02AC */ uint32_t player_number;
        /* +h02B0 */ uint32_t h02B0[40];
        /* +h0350 */ ProgressBarContext* progress_bar; // seems to never be nullptr
        /* +h0354 */ uint32_t h0354[27];
        /* +h03C0 */ wchar_t player_email[0x40];
    };
    static_assert(sizeof(CharContext) == 0x440, "struct CharContext has incorrect size");
}
