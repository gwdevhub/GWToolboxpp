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

// The functions used to allocate looks like:
// void *__thiscall ObjectPool::Alloc(ObjectPool *pool, int32_t typesize, const char *typename);
// So, the typename & typesize are passed at every allocs, similar to `MemAlloc`.
// It's worth nothing that the minimum typesize is 4, though Guild Wars doesn't assert it, it just
// ends up crashing. This is due to the freeList pointer lists that are 4 bytes each.
