#pragma once
#include <GWCA/Utilities/Export.h>
#include <GWCA/GameContainers/List.h>
#include <GWCA/GameContainers/Array.h>

namespace GW {
    GWCA_API uint32_t Hash(const void* data, size_t size);
    GWCA_API uint32_t Hash8(uint8_t data);
    GWCA_API uint32_t Hash16(uint16_t data);
    GWCA_API uint32_t Hash32(uint32_t data);
    GWCA_API uint32_t HashWString(const wchar_t* str, uint32_t maxLength = -1);

    template <typename T>
    struct THashLink;

    template <typename T>
    struct THash {
        /* +h0000 */ TList<T> m_fullList;                 // 0x0C bytes
        /* +h000C */ uint32_t m_slotSize;
        /* +h0010 */ BaseArray<TList<T>> m_slotListArray; // 0x0C bytes
        /* +h001C */ uint32_t m_mask;
        /* +h0020 */ uint32_t m_maxCount;

        class iterator {
        public:
            explicit iterator(TLink<T>* link) : current(link) {}

            THashLink<T>& operator*() {
                return *reinterpret_cast<THashLink<T>*>(
                    reinterpret_cast<uintptr_t>(current)
                    - offsetof(THashLink<T>, m_fullListLink)
                    );
            }

            THashLink<T>* operator->() { return &operator*(); }

            iterator& operator++() {
                current = current->NextLink();
                return *this;
            }

            bool operator==(const iterator& other) const { return current == other.current; }
            bool operator!=(const iterator& other) const { return current != other.current; }

        private:
            TLink<T>* current;
        };

        iterator begin() const {
            return iterator(m_fullList.Get()->NextLink());
        }

        iterator end() const {
            return iterator(m_fullList.Get());
        }

        bool valid() const { return m_slotListArray.valid(); }
        uint32_t size() const { return m_slotListArray.m_size; }
    };
    static_assert(sizeof(THash<void*>) == 0x24, "THash has incorrect size");

    template <typename T>
    struct THashLink {
        /* +h0000 */ uint32_t hash;
        /* +h0004 */ T* elem;
        /* +h0008 */ uint32_t h0008;
        /* +h000C */ uint32_t h000C;
        /* +h0010 */ TLink<T> m_fullListLink;  // 0x08 bytes
        /* +h0018 */ TLink<T> m_slotListLink;  // 0x08 bytes
    };
    static_assert(sizeof(THashLink<void*>) == 0x20, "THashLink has incorrect size");
}