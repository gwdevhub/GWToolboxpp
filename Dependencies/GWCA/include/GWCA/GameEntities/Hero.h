#pragma once

#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/GamePos.h>

namespace GW {
    typedef uint32_t AgentID;

    enum class HeroBehavior : uint32_t {
        Fight, Guard, AvoidCombat
    };

    struct HeroFlag { // total: 0x20/36
        /* +h0000 */ uint32_t hero_id;
        /* +h0004 */ AgentID  agent_id;
        /* +h0008 */ uint32_t level;
        /* +h000C */ HeroBehavior hero_behavior;
        /* +h0010 */ Vec2f flag;
        /* +h0018 */ uint32_t h0018;
        /* +h001C */ uint32_t h001c;
        /* +h0020 */ AgentID locked_target_id;
    };
    static_assert(sizeof(HeroFlag) == 0x24, "struct HeroFlag has incorrect size");

    struct HeroInfo { // total: 0x78/120
        /* +h0000 */ uint32_t hero_id;
        /* +h0004 */ uint32_t agent_id;
        /* +h0008 */ uint32_t level;
        /* +h000C */ uint32_t primary; // Primary profession 0-10 (None,W,R,Mo,N,Me,E,A,Rt,P,D)
        /* +h0010 */ uint32_t secondary; // Primary profession 0-10 (None,W,R,Mo,N,Me,E,A,Rt,P,D)
        /* +h0014 */ uint32_t hero_file_id;
        /* +h0018 */ uint32_t model_file_id;
        /* +h001C */ uint8_t  h001C[52];
        /* +h0050 */ wchar_t  name[20];
    };
    static_assert(sizeof(HeroInfo) == 120, "struct HeroInfo has incorrect size");

    typedef Array<HeroFlag> HeroFlagArray;
    typedef Array<HeroInfo> HeroInfoArray;
}
