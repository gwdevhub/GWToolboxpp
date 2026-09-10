#pragma once

namespace GW {
    struct ObjectPoolBlock {
        /* +h0000 */ ObjectPoolBlock* next;
        /* +h0004 */ uint32_t h0004;
    };
    static_assert(sizeof(ObjectPoolBlock) == 0x8, "struct ObjectPoolBlock has incorrect size");

    struct ObjectPool {
        /* +h0000 */ void** freeList;         // This is a singly linked list of free object. The last object freed is always the pointer.
        /* +h0004 */ ObjectPoolBlock* blocks; // This is a singly linked list of blocks that were allocated. Every blocks will generally be for many elements.
        /* +h0008 */ uint32_t count;
    };
    static_assert(sizeof(ObjectPool) == 0xC, "struct ObjectPool has incorrect size");
}

// ObjectPool::Alloc(pool, typesize, typename) passes type info per alloc; minimum typesize is 4 (freeList pointers) or it crashes.
