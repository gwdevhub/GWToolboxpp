#pragma once
#include <GWCA/Utilities/Export.h>

namespace GW {
    struct GameContext;
    GWCA_API GameContext* GetGameContext();

    struct Cinematic;
    struct MapContext;
    struct TextParser;
    struct CharContext;
    struct ItemContext;
    struct AgentContext;
    struct GuildContext;
    struct PartyContext;
    struct TradeContext;
    struct WorldContext;
    struct GadgetContext;
    struct AccountContext;

    struct GameContext {
        /* +h0000 */ void* h0000;
        /* +h0004 */ void* h0004;
        /* +h0008 */ AgentContext* agent; // Most functions that access are prefixed with Agent.
        /* +h000C */ void* h000C;
        /* +h0010 */ void* h0010;
        /* +h0014 */ MapContext* map; // Static object/collision data
        /* +h0018 */ TextParser *text_parser;
        /* +h001C */ void* h001C;
        /* +h0020 */ uint32_t some_number; // 0x30 for me at the moment.
        /* +h0024 */ void* h0024;
        /* +h0028 */ AccountContext* account;
        /* +h002C */ WorldContext* world; // Best name to fit it that I can think of.
        /* +h0030 */ Cinematic *cinematic;
        /* +h0034 */ void* h0034;
        /* +h0038 */ GadgetContext* gadget;
        /* +h003C */ GuildContext* guild;
        /* +h0040 */ ItemContext* items;
        /* +h0044 */ CharContext* character;
        /* +h0048 */ void* h0048;
        /* +h004C */ PartyContext* party;
        /* +h0050 */ void* h0050;
        /* +h0054 */ void* h0054;
        /* +h0058 */ TradeContext* trade;
    };
}
