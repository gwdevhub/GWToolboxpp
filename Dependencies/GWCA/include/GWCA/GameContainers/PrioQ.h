#pragma once

namespace GW {
    template <typename T>
    struct PrioQ {
        /* +h0000 */ uint32_t m_LinkOffset;
        /* +h0004 */ Array<T> m_Nodes;
    };
    static_assert(sizeof(PrioQ<int*>) == 0x14, "struct PrioQ has incorrect size");

    template <typename T>
    struct PrioQLink {
        /* +h0000*/
        /* +h0000*/ void* vtable;
        /* +h0004*/ PrioQ<T>* pq;
        /* +h0008*/ uint32_t posIndex;
        /* +h000C*/ float rank; // This is value used to order the PrioQ nodes. For a min-heap (e.g., A*), it put rank as -rank.
    };
    static_assert(sizeof(PrioQLink<int*>) == 0x10, "struct PrioQLink has incorrect size");
}
