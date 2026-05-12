#pragma once

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
    };
}
