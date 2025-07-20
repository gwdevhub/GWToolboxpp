#pragma once

#include <GWCA/Utilities/Export.h>

#include <GWCA/GameContainers/List.h>
#include <GWCA/GameContainers/Array.h>

namespace GW {
    GWCA_API uint32_t Hash(const void *data, size_t size);
    GWCA_API uint32_t Hash8(uint8_t data);
    GWCA_API uint32_t Hash16(uint16_t data);
    GWCA_API uint32_t Hash32(uint32_t data);
    GWCA_API uint32_t HashWString(const wchar_t* str, uint32_t maxLength = -1);

    template <typename T>
    struct THash {
        /* +h0000 */ TList<T> m_fullList;
        /* +h0008 */ uint32_t m_slotSize;
        /* +h000C */ Array<TList<T>> m_slotListArray;
        /* +h001C */ uint32_t m_mask;
        /* +h0020 */ uint32_t m_maxCount;
    };

    template <typename T>
    struct THashLink
    {
        /* +h0000 */ uint32_t hash;
        /* +h0004 */ T *elem;
        /* +h0008 */ uint32_t h0008;
        /* +h000C */ uint32_t h000C;
        /* +h0010 */ TLink<T> m_fullListLink;
        /* +h0018 */ TLink<T> m_slotListLink;
    };
}
