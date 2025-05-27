#pragma once

#include <GWCA/GameContainers/Array.h>

namespace GW {

    namespace Constants {
        enum class Profession : uint32_t;
    }

    struct CharAdjustment {
        char hue;
        char saturation;
        char lightness;
        char scale;  // percent
    };

    struct NPC { // total: 0x30/48
        /* +h0000 */ uint32_t model_file_id;
        /* +h0004 */ uint32_t skin_file_id;
		/* +h0008 */ CharAdjustment visual_adjustment; // may be overridden
        /* +h000C */ uint32_t appearance;
        /* +h0010 */ uint32_t npc_flags; // uses CHAR_CLASS_FLAG_* constants
        /* +h0014 */ GW::Constants::Profession primary;
        /* +h0018 */ GW::Constants::Profession secondary;
        /* +h001C */ uint8_t  default_level;
        // +h001D    uint8_t  padding;
        // +h001E    uint16_t padding;
        /* +h0020 */ wchar_t *name_enc;
        /* +h0024 */ uint32_t *model_files;
        /* +h0028 */ uint32_t files_count; // length of ModelFile
        /* +h002C */ uint32_t files_capacity; // capacity of ModelFile

        inline bool IsHenchman() { return (npc_flags & 0x10) != 0; }
        inline bool IsHero() { return (npc_flags & 0x20) != 0; }
        inline bool IsSpirit() { return (npc_flags & 0x4000) != 0; }
        inline bool IsMinion() { return (npc_flags & 0x100) != 0; }
        inline bool IsPet() { return (npc_flags & 0xD) != 0; }
    };
    static_assert(sizeof(NPC) == 0x30, "struct NPC has incorrect size");

    typedef Array<NPC> NPCArray;
}
