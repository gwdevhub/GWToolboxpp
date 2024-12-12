#pragma once

#include <GWCA/GameContainers/List.h>
#include <GWCA/GameContainers/Array.h>

namespace GW {
    typedef uint32_t AgentID;
    typedef uint32_t PlayerID;
    typedef uint32_t HeroID;
    typedef uint32_t Profession;

    struct PlayerPartyMember { // total: 0xC/12
        /* +h0000 */ PlayerID login_number;
        /* +h0004 */ AgentID calledTargetId;
        /* +h0008 */ uint32_t state;

        bool connected() const { return (state & 1) > 0; }
        bool ticked()    const { return (state & 2) > 0; }
    };
    static_assert(sizeof(PlayerPartyMember) == 12, "struct PlayerPartyMember has incorrect size");

    struct HeroPartyMember { // total: 0x18/24
        /* +h0000 */ AgentID agent_id;
        /* +h0004 */ PlayerID owner_player_id;
        /* +h0008 */ HeroID hero_id;
        /* +h000C */ uint32_t h000C;
        /* +h0010 */ uint32_t h0010;
        /* +h0014 */ uint32_t level;
    };
    static_assert(sizeof(HeroPartyMember) == 24, "struct HeroPartyMember has incorrect size");

    struct HenchmanPartyMember { // total: 0x34/52
        /* +h0000 */ AgentID agent_id;
        /* +h0004 */ uint32_t h0004[10];
        /* +h002C */ Profession profession;
        /* +h0030 */ uint32_t level;
    };
    static_assert(sizeof(HenchmanPartyMember) == 52, "struct HenchmanPartyMember has incorrect size");

    typedef Array<HeroPartyMember> HeroPartyMemberArray;
    typedef Array<PlayerPartyMember> PlayerPartyMemberArray;
    typedef Array<HenchmanPartyMember> HenchmanPartyMemberArray;

    struct PartyInfo { // total: 0x84/132

        size_t GetPartySize() {
            return players.size() + henchmen.size() + heroes.size();
        }

        /* +h0000 */ uint32_t party_id;
        /* +h0004 */ Array<PlayerPartyMember> players;
        /* +h0014 */ Array<HenchmanPartyMember> henchmen;
        /* +h0024 */ Array<HeroPartyMember> heroes;
        /* +h0034 */ Array<AgentID> others; // agent id of allies, minions, pets.
        /* +h0044 */ uint32_t h0044[14];
        /* +h007C */ TLink<PartyInfo> invite_link;
    };
    static_assert(sizeof(PartyInfo) == 132, "struct PartyInfo has incorrect size");

    enum PartySearchType {
        PartySearchType_Hunting  = 0,
        PartySearchType_Mission  = 1,
        PartySearchType_Quest    = 2,
        PartySearchType_Trade    = 3,
        PartySearchType_Guild    = 4,
    };

    struct PartySearch {
        /* +h0000 */ uint32_t party_search_id;
        /* +h0004 */ uint32_t party_search_type;
        /* +h0008 */ uint32_t hardmode;
        /* +h000C */ uint32_t district;
        /* +h0010 */ uint32_t language;
        /* +h0014 */ uint32_t party_size;
        /* +h0018 */ uint32_t hero_count;
        /* +h001C */ wchar_t message[32];
        /* +h005C */ wchar_t party_leader[20];
        /* +h0084 */ Profession primary;
        /* +h0088 */ Profession secondary;
        /* +h008C */ uint32_t level;
        /* +h0090 */ uint32_t timestamp;
    };
}
