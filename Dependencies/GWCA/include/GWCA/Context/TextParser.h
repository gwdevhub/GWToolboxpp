#pragma once

#include <GWCA/GameContainers/Array.h>

namespace GW {
    namespace Constants {
        enum class Language;
    }
    struct TextCache {
        /* +h0000 */ uint32_t h0000;
    };

    struct SubStruct1 {
        /* +h0000 */ uint32_t h0000;
    };

    // Allocated @0078C243
    struct SubStructUnk { // total: 0x54/84
        /* +h0000 */ uint32_t AsyncDecodeStr_Callback;
        /* +h0004 */ uint32_t AsyncDecodeStr_Param;
        /* +h0008 */ uint32_t buffer_used; // if it's 1 then uses s1 & if it's 0 uses s2.
        /* +h000C */ Array<wchar_t> s1;
        /* +h001C */ Array<wchar_t> s2;
        /* +h002C */ uint32_t h002C;
        /* +h0030 */ uint32_t h0030; // tell us how many string will be enqueue before decoding.
        /* +h0034 */ uint32_t h0034; // @0078B990
        /* +h0038 */ uint8_t h0038[28];
    };

    struct TextParser {
        /* +h0000 */ uint32_t h0000[8];
        /* +h0020 */ wchar_t *dec_start;
        /* +h0024 */ wchar_t *dec_end;
        /* +h0028 */ uint32_t substitute_1; // related to h0020 & h0024
        /* +h002C */ uint32_t substitute_2; // related to h0020 & h0024
        /* +h0030 */ TextCache *cache;
        /* +h0034 */ uint32_t h0034[75];
        /* +h0160 */ uint32_t h0160; // @0078BEF5
        /* +h0164 */ uint32_t h0164;
        /* +h0168 */ uint32_t h0168; // set to 0 @0078BF34
        /* +h016C */ uint32_t h016C[5];
        /* +h0180 */ SubStruct1 *sub_struct;
        /* +h0184 */ uint32_t h0184[19];
        /* +h01d0 */ GW::Constants::Language language_id;
    };

}
