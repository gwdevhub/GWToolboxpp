#pragma once

#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/GamePos.h>

namespace GW {
    typedef uint32_t AgentID;
    namespace Constants {
        enum HeroID : uint32_t;
        enum class Profession : uint32_t;
    }

    enum class HeroBehavior : uint32_t {
        Fight, Guard, AvoidCombat
    };

    struct HeroFlag { // total: 0x20/36
        /* +h0000 */ GW::Constants::HeroID hero_id;
        /* +h0004 */ AgentID  agent_id;
        /* +h0008 */ uint32_t inventory_id;
        /* +h000C */ HeroBehavior hero_behavior;
        /* +h0010 */ Vec2f flag;
        /* +h0018 */ uint32_t h0018;
        /* +h001C */ uint32_t h001c;
        /* +h0020 */ AgentID locked_target_id;
    };
    static_assert(sizeof(HeroFlag) == 0x24, "struct HeroFlag has incorrect size");

    struct HeroInfo { // total: 0x9C/156
        /* +h0000 */ GW::Constants::HeroID hero_id;
        /* +h0004 */ uint32_t agent_id;
        /* +h0008 */ uint32_t level;
        /* +h000C */ GW::Constants::Profession primary;
        /* +h0010 */ GW::Constants::Profession secondary;
        /* +h0014 */ uint32_t hero_file_id;
        /* +h0018 */ uint32_t model_file_id;
        /* +h001C */ uint32_t h001C;
        /* +h0020 */ uint32_t h0020;
        /* +h0024 */ uint32_t h0024;
        /* +h0028 */ uint32_t h0028;
        /* +h002C */ uint32_t h002C;
        /* +h0030 */ uint32_t h0030;
        /* +h0034 */ uint32_t h0034;
        /* +h0038 */ uint32_t h0038;
        /* +h003C */ uint32_t h003C;
        /* +h0040 */ uint32_t h0040;
        /* +h0044 */ uint32_t h0044;
        /* +h0048 */ uint32_t h0048;
        /* +h004C */ uint32_t h004C;
        /* +h0050 */ wchar_t  name[20];
        /* +h0078 */ uint32_t h0078;
        /* +h007C */ uint32_t h007C;
        /* +h0080 */ uint32_t h0080;
        /* +h0084 */ uint32_t h0084;
        /* +h0088 */ uint32_t h0088;
        /* +h008C */ uint32_t h008C;
        /* +h0090 */ uint32_t h0090;
        /* +h0094 */ uint32_t h0094;
        /* +h0098 */ uint32_t h0098;
    };
    static_assert(sizeof(HeroInfo) == 0x9C, "struct HeroInfo has incorrect size");

    struct HeroConstData { // total: 0x18/24
        /* +h0000 */ uint32_t h0000;
        /* +h0004 */ uint32_t h0004;
        /* +h0008 */ uint32_t h0008;
        /* +h000C */ uint32_t name_id;
        /* +h0010 */ uint32_t h0010;
        /* +h0014 */ uint32_t description_id;
    };
    static_assert(sizeof(HeroConstData) == 0x18, "struct HeroConstData has incorrect size");

    typedef Array<HeroFlag> HeroFlagArray;
    typedef Array<HeroInfo> HeroInfoArray;
}
