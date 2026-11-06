#pragma once
#include <GWCA/Runtime.h>

#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/List.h>
#include <GWCA/GameContainers/Hash.h>

#include <GWCA/Utilities/Export.h>

namespace GW {
    namespace Constants {
        enum class Language;
        enum class MapID : uint32_t;
        enum class InstanceType;
    }
    struct CharContext;
    CharContext* GetCharContext();

    struct ObserverMatch;

    struct ProgressBarContext {
        int32_t pips; //0x0000
        uint8_t color[4]; //0x0004  RGBA
        uint8_t background[4]; //0x0008 RGBA
        wchar_t* name_str_enc; //0x000C ? it is encoded string but it being name is a guess
        wchar_t* tooltop_str_enc; //0x0010 ? it is encoded string but it being the tooltip description is a guess
        Array<void*> sort_order; //0x0014 ? sort_order referenced in elements
        float progress_rate; //0x0024 ?
        float progress; //0x0028
        TLink<ProgressBarContext> next; //0x002C
        void* N000024BD; //0x0034 texture information (DXT1 referenced)
        char pad_0038[8]; //0x0038
    };
    static_assert(sizeof(ProgressBarContext) == 0x40);

    class ObservablePlayer
    {
    public:
        uint32_t team; //0x0000 char literal 'play' in PvE
        uint32_t player_number; //0x0004 ?
        char pad_0008[12]; //0x0008
        wchar_t name[20]; //0x0014
        char pad_003C[16]; //0x003C
        THashLink<ObservablePlayer> next; //0x004C
    }; //Size: 0x0060
    static_assert(sizeof(ObservablePlayer) == 0x60);

    struct CharContext { // total: 0x444
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
        /* +h0228 */ uint32_t h0228;
        /* +h022C */ uint32_t district_number;
        /* +h0230 */ GW::Constants::Language language;
        /* +h0234 */ GW::Constants::MapID observe_map_id;
        /* +h0238 */ GW::Constants::MapID current_map_id;
        /* +h023C */ Constants::InstanceType observe_map_type;
        /* +h0240 */ Constants::InstanceType current_map_type;
        /* +h0244 */ uint32_t h0244[5];
        /* +h0258 */ Array<ObserverMatch*> observer_matches;
        /* +h0268 */ uint32_t h0268[17];
        /* +h02AC */ uint32_t player_flags; // bitwise something
        /* +h02B0 */ uint32_t player_number;
        /* +h02B4 */ char h02B4[148]; //
        /* +h0348 */ Array<ProgressBarContext> progress_bars; //
        /* +h0358 */ TList<ProgressBarContext> progress_bar_list; //  sorted list, unsure what the sort order is
        /* +h0364 */ char h0364[72]; //
        /* +h03AC */ THash<ObservablePlayer> unk_player_agent_thing; //? is a hash map, assuming this is a map of observable players; key type unverified
        /* +h03D0 */ wchar_t player_email[0x40];
    };
    static_assert(sizeof(CharContext) == 0x450, "struct CharContext has incorrect size");
}

namespace GW {
    extern "C" GWCA_API CharContext* __cdecl GW_GetCharContext() noexcept;
}

#if !defined(GWCA_BUILD_EXPORTS)
namespace GW {
    inline CharContext* GetCharContext()
    {
        return GWCA_CALL_C(GW_GetCharContext);
    }
}
#endif
