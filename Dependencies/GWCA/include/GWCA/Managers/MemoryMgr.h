#pragma once

#include <GWCA/GameContainers/Array.h>

namespace GW {

    struct HookEntry;
    struct Module;
    extern Module MemoryMgrModule;

    namespace MemoryMgr {

        GWCA_API uint32_t GetGWVersion();

        GWCA_API DWORD GetSkillTimer();

        GWCA_API bool GetPersonalDir(size_t buf_len, wchar_t* buf);

        GWCA_API HWND GetGWWindowHandle();

        // You probably don't want to use these functions.  These are for allocating
        // memory on the Guild Wars game heap, rather than your own heap.  Memory allocated with
        // these functions cannot be used with RAII and must be manually freed.  USE AT YOUR OWN RISK.
        GWCA_API void* MemAlloc(size_t size);
        GWCA_API void* MemRealloc(void* buf, size_t newSize);
        GWCA_API void MemFree(void* buf);

        HMODULE GetModuleForPointer(void* ptr, bool refresh = false);

        void RemoveFreeLibraryCallback(GW::HookEntry*);
        void RegisterFreeLibraryCallback(GW::HookEntry*, std::function<void(HMODULE)>);

        // Appends an element to a GW-managed array, growing the buffer via MemRealloc if at capacity.
        // Returns a pointer to the newly appended element.
        template <typename T>
        T* AddToGuildWarsArray(GW::BaseArray<T>& arr, const T& element)
        {
            if (arr.m_size >= arr.m_capacity) {
                auto* new_buf = static_cast<T*>(MemRealloc(arr.m_buffer, (arr.m_size + 1) * sizeof(T)));
                GWCA_ASSERT(new_buf);
                arr.m_buffer = new_buf;
                arr.m_capacity++;
            }
            arr.m_buffer[arr.m_size] = element;
            return &arr.m_buffer[arr.m_size++];
        }

        // Removes the element at index from a GW-managed array by shifting remaining elements left.
        // Capacity is unchanged so future AddToGuildWarsArray calls reuse the slack without reallocating.
        template <typename T>
        void RemoveFromGwArray(GW::BaseArray<T>& arr, uint32_t index)
        {
            GWCA_ASSERT(index < arr.m_size);
            const auto remaining = arr.m_size - index - 1;
            if (remaining > 0)
                memmove(&arr.m_buffer[index], &arr.m_buffer[index + 1], remaining * sizeof(T));
            arr.m_size--;
        }
    };
}
