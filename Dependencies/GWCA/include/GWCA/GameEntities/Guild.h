#pragma once

#include <GWCA/GameContainers/Array.h>

namespace GW {

    namespace Constants {
        enum class MapID : uint32_t;
    }
    struct GHKey {
        uint32_t k[4]{};

        explicit operator bool() const {
            return std::ranges::any_of(k, [](uint32_t i) { return i != 0; });
        }
    };
    struct GuildPlayer { // total: 0x174/372
        /* +h0000 */ void* vtable;
        /* +h0004 */ wchar_t *name_ptr; // ptr to invitedname, why? dunno
        /* +h0008 */ wchar_t invited_name[20]; // name of character that was invited in
        /* +h0030 */ wchar_t current_name[20]; // name of character currently being played
        /* +h0058 */ wchar_t inviter_name[20]; // name of character that invited player
        /* +h0080 */ uint32_t invite_time; // time in ms from game creation ??
        /* +h0084 */ wchar_t promoter_name[20]; // name of player that last modified rank
        /* +h00AC */ uint32_t h00AC[12];
        /* +h00DC */ uint32_t offline;
        /* +h00E0 */ uint32_t member_type;
        /* +h00E4 */ uint32_t status;
        /* +h00E8 */ uint32_t h00E8[35];
    };
    static_assert(sizeof(GuildPlayer) == 372, "struct GuildPlayer has incorrect size");

    typedef Array<GuildPlayer *> GuildRoster;

    struct GuildHistoryEvent { // total: 0x208/520
        /* +h0000 */ uint32_t time1; // Guessing one of these is time in ms
        /* +h0004 */ uint32_t time2;
        /* +h0008 */ wchar_t name[256]; // Name of added/kicked person, then the adder/kicker, they seem to be in the same array
    };
    static_assert(sizeof(GuildHistoryEvent) == 520, "struct GuildHistoryEvent has incorrect size");

    typedef Array<GuildHistoryEvent *> GuildHistory;

    struct CapeDesign { // total: 0x1C/28
        /* +h0000 */ uint32_t cape_bg_color;
        /* +h0004 */ uint32_t cape_detail_color;
        /* +h0008 */ uint32_t cape_emblem_color;
        /* +h000C */ uint32_t cape_shape;
        /* +h0010 */ uint32_t cape_detail;
        /* +h0014 */ uint32_t cape_emblem;
        /* +h0018 */ uint32_t cape_trim;
    };
    static_assert(sizeof(CapeDesign) == 0x1c, "struct CapeDesign has incorrect size");

    struct Guild { // total: 0xAC/172
        /* +h0000 */ GHKey key;
        /* +h0010 */ uint32_t h0010[5];
        /* +h0024 */ uint32_t index; // Same as PlayerGuildIndex
        /* +h0028 */ uint32_t rank;
        /* +h002C */ uint32_t features;
        /* +h0030 */ wchar_t name[32];
        /* +h0070 */ uint32_t rating;
        /* +h0074 */ uint32_t faction; // 0=kurzick, 1=luxon
        /* +h0078 */ uint32_t faction_point;
        /* +h007C */ uint32_t qualifier_point;
        /* +h0080 */ wchar_t tag[8];
        /* +h0090 */ CapeDesign cape;
    };
    static_assert(sizeof(Guild) == 172, "struct Guild has incorrect size");

    typedef Array<Guild *> GuildArray;

    struct TownAlliance { // total: 0x78/120
        /* +h0000 */ uint32_t rank;
        /* +h0004 */ uint32_t allegiance;
        /* +h0008 */ uint32_t faction;
        /* +h000C */ wchar_t name[32];
        /* +h004C */ wchar_t tag[5];
        /* +h0056 */ uint8_t _padding[2];
        /* +h0058 */ CapeDesign cape;
        /* +h0074 */ GW::Constants::MapID map_id;
    };
    static_assert(sizeof(TownAlliance) == 0x78, "struct TownAlliance has incorrect size");
}
