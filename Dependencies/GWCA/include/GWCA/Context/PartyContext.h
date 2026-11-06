#pragma once
#include <GWCA/Runtime.h>

#include <GWCA/GameContainers/List.h>
#include <GWCA/GameContainers/Array.h>
#include <GWCA/Utilities/Export.h>

namespace GW {
    struct PartyContext;
    PartyContext* GetPartyContext();

    struct PartyInfo;
    struct PartySearch;

    struct PartySearchContext {
        uint32_t h0000;
        uint32_t h0004;
        uint32_t h0008;
        uint32_t h000c;
        uint32_t flags;
    };

    struct PartyContext { // total: 0x58/88
        /* +h0000 */ uint32_t h0000;
        /* +h0004 */ Array<void*> h0004;
        /* +h0014 */ uint32_t flag;
        /* +h0018 */ uint32_t h0018;
        /* +h001C */ TList<PartyInfo> requests;
        /* +h0028 */ uint32_t requests_count;
        /* +h002C */ TList<PartyInfo> sending;
        /* +h0038 */ uint32_t sending_count;
        /* +h003C */ uint32_t h003C;
        /* +h0040 */ Array<PartyInfo*> parties;
        /* +h0050 */ uint32_t h0050;
        /* +h0054 */ PartyInfo* player_party; // Players party
        /* +h0058 */ uint32_t h0058[17];
        /* +h009C */ uint32_t my_party_search_id;
        /* +h00A0 */ uint32_t h00A0[8];
        /* +h00C0 */ Array<PartySearch*> party_search;

        bool InHardMode() const { return (flag & 0x10) > 0; }
        bool IsDefeated() const { return (flag & 0x20) > 0; }
        bool IsPartyLeader() const { return (flag >> 0x7) & 1; }
    };
}

namespace GW {
    extern "C" GWCA_API PartyContext* __cdecl GW_GetPartyContext() noexcept;
}

#if !defined(GWCA_BUILD_EXPORTS)
namespace GW {
    inline PartyContext* GetPartyContext()
    {
        return GWCA_CALL_C(GW_GetPartyContext);
    }
}
#endif
