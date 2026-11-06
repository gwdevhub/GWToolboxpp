#pragma once
#include <GWCA/Runtime.h>
#include <GWCA/Utilities/Export.h>
#include <GWCA/GameContainers/List.h>
#include <GWCA/GameContainers/Array.h>

namespace GW {
    uint32_t Hash(const void* data, size_t size);
    uint32_t Hash8(uint8_t data);
    uint32_t Hash16(uint16_t data);
    uint32_t Hash32(uint32_t data);
    uint32_t HashWString(const wchar_t* str, uint32_t maxLength = -1);

    template <typename V>
    struct THashKeyVal {
        V m_value;

        THashKeyVal(const V& value) : m_value(value) {}
        uint32_t GetHash() const { return Hash(&m_value, sizeof(m_value)); }
        bool operator==(const THashKeyVal&) const = default;
    };

    template <typename V>
    struct THashKeyIdentity {
        V m_value;

        THashKeyIdentity(const V& value) : m_value(value) {}
        uint32_t GetHash() const { return static_cast<uint32_t>(m_value); }
        bool operator==(const THashKeyIdentity&) const = default;
    };

    // Embedded in each element T at THash::m_linkOffset.
    template <typename T>
    struct THashLink {
        /* +h0000 */ uint32_t m_hash;
        /* +h0004 */ TLink<T> m_linkToFull; // 0x08 bytes
        /* +h000C */ TLink<T> m_linkToSlot; // 0x08 bytes
    };
    static_assert(sizeof(THashLink<void*>) == 0x14, "THashLink has incorrect size");

    // K is the element's leading base class, as in Gw.exe's "Find(THashKeyVal<unsigned>(baseId))".
    template <typename T, typename K = THashKeyVal<uint32_t>>
    struct THash {
        /* +h0000 */ TList<T> m_fullList;                 // 0x0C bytes, links at m_linkOffset + 0x4
        /* +h000C */ uint32_t m_linkOffset;               // offset of THashLink<T> within T
        /* +h0010 */ BaseArray<TList<T>> m_slotListArray; // 0x0C bytes, links at m_linkOffset + 0xC
        /* +h001C */ uint32_t m_slotMask;
        /* +h0020 */ uint32_t m_slotMaxCount;             // slot count the table stops growing at

        class iterator {
        public:
            iterator(T* node, uint32_t link_offset) : current(node), link_offset(link_offset) {}

            T& operator*() const { return *current; }
            T* operator->() const { return current; }

            iterator& operator++() {
                current = GetLink(current, link_offset)->m_linkToFull.Next();
                return *this;
            }

            bool operator==(const iterator& other) const { return current == other.current; }
            bool operator!=(const iterator& other) const { return current != other.current; }

        private:
            T* current;
            uint32_t link_offset;
        };

        static THashLink<T>* GetLink(T* elem, uint32_t link_offset) {
            return reinterpret_cast<THashLink<T>*>(reinterpret_cast<uintptr_t>(elem) + link_offset);
        }
        THashLink<T>* GetLink(T* elem) const { return GetLink(elem, m_linkOffset); }

        static const K& KeyOf(const T* elem) { return *reinterpret_cast<const K*>(elem); }

        iterator begin() { return iterator(m_fullList.Get()->Next(), m_linkOffset); }
        iterator end() { return iterator(nullptr, m_linkOffset); }

        bool valid() const { return m_slotListArray.valid(); }
        uint32_t size() const { return m_slotListArray.m_size; } // slot count, not element count

        T* Find(const K& key) {
            if (!valid())
                return nullptr;
            const uint32_t hash = key.GetHash();
            TList<T>* slot_list = m_slotListArray.get(hash & m_slotMask);
            return slot_list ? FindFrom(key, hash, slot_list->Get()->Next()) : nullptr;
        }
        const T* Find(const K& key) const { return const_cast<THash*>(this)->Find(key); }

        // Keys need not be unique: Insert never checks for an existing one.
        T* FindNext(const K& key, const T* elem) {
            return elem ? FindFrom(key, key.GetHash(), GetLink(const_cast<T*>(elem))->m_linkToSlot.Next()) : nullptr;
        }
        const T* FindNext(const K& key, const T* elem) const { return const_cast<THash*>(this)->FindNext(key, elem); }

    private:
        T* FindFrom(const K& key, uint32_t hash, T* elem) {
            for (; elem; elem = GetLink(elem)->m_linkToSlot.Next()) {
                if (GetLink(elem)->m_hash == hash && KeyOf(elem) == key)
                    return elem;
            }
            return nullptr;
        }
    };
    static_assert(sizeof(THash<void*>) == 0x24, "THash has incorrect size");

    using CBaseHashLink = THashLink<uint8_t>;
    using CBaseHash = THash<uint8_t>;
}

namespace GW {
    extern "C" GWCA_API uint32_t __cdecl GW_Hash(const void* data, size_t size) noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_Hash8(uint8_t data) noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_Hash16(uint16_t data) noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_Hash32(uint32_t data) noexcept;
    extern "C" GWCA_API uint32_t __cdecl GW_HashWString(const wchar_t* str, uint32_t maxLength) noexcept;
}

#if !defined(GWCA_BUILD_EXPORTS)
namespace GW {
    inline uint32_t Hash(const void* data, size_t size)
    {
        return GWCA_CALL_C(GW_Hash, data, size);
    }

    inline uint32_t Hash8(uint8_t data)
    {
        return GWCA_CALL_C(GW_Hash8, data);
    }

    inline uint32_t Hash16(uint16_t data)
    {
        return GWCA_CALL_C(GW_Hash16, data);
    }

    inline uint32_t Hash32(uint32_t data)
    {
        return GWCA_CALL_C(GW_Hash32, data);
    }

    inline uint32_t HashWString(const wchar_t* str, uint32_t maxLength)
    {
        return GWCA_CALL_C(GW_HashWString, str, maxLength);
    }
}
#endif
