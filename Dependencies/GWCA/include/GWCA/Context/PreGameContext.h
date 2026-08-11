#pragma once

#include <GWCA/GameContainers/Array.h>
#include <GWCA/Utilities/Export.h>

namespace GW {
    struct PreGameContext;
    struct CharacterInformation;
    GWCA_API PreGameContext* GetPreGameContext();
    GWCA_API Array<CharacterInformation>* GetAvailableChars();

    struct LoginCharacter {
        uint32_t unk0; // Some kind of function call
        wchar_t character_name[20];
    };

    // Character information available at login screen
    struct CharacterInformation {
        /* +h0000 */ uint32_t h0000[2];
        /* +h0008 */ uint32_t uuid[4];
        /* +h0018 */ wchar_t name[20];
        /* +h0040 */ uint32_t props[17];

        uint32_t GetMapId() const { return (props[0] >> 16) & 0xFFFF; }
        uint32_t GetPrimaryProfession() const { return (props[2] >> 20) & 0xF; }
        uint32_t GetSecondaryProfession() const { return (props[7] >> 10) & 0xF; }
        uint32_t GetCampaign() const { return props[7] & 0xF; }
        uint32_t GetLevel() const { return (props[7] >> 4) & 0x3F; }
        bool IsPvP() const { return ((props[7] >> 9) & 0x1) == 0x1; }
    };
    struct PreGameContext {
        /* +h0000 */ uint32_t frame_id;
        /* +h0004 */ uint32_t h0004[72];
        /* +h0124 */ uint32_t chosen_character_index;
        /* +h0128 */ uint32_t h0128[6];
        /* +h0140 */ uint32_t index_1;
        /* +h0144 */ uint32_t index_2;
        /* +h0148 */ GW::Array<LoginCharacter> chars;
    };
}
// C Interop API
extern "C" {
    GWCA_API void* GetAvailableChars();
}
