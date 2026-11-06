#pragma once
#include <GWCA/Runtime.h>

namespace GW {

    struct HookEntry;
    struct Module;
    extern Module MemoryMgrModule;

    namespace MemoryMgr {

        uint32_t GetGWVersion();

        DWORD GetSkillTimer();

        bool GetPersonalDir(size_t buf_len, wchar_t* buf);

        HWND GetGWWindowHandle();

        // Allocates on the Guild Wars game heap, not yours: no RAII, must be freed manually. USE AT YOUR OWN RISK.
        void* MemAlloc(size_t size);
        void* MemRealloc(void* buf, size_t newSize);
        void MemFree(void* buf);

    };
}

namespace GW::MemoryMgr {
    extern "C" GWCA_API uint32_t __cdecl GW_MemoryMgr_GetGWVersion() noexcept;
    extern "C" GWCA_API DWORD __cdecl GW_MemoryMgr_GetSkillTimer() noexcept;
    extern "C" GWCA_API bool __cdecl GW_MemoryMgr_GetPersonalDir(size_t buf_len, wchar_t* buf) noexcept;
    extern "C" GWCA_API HWND __cdecl GW_MemoryMgr_GetGWWindowHandle() noexcept;
    extern "C" GWCA_API void* __cdecl GW_MemoryMgr_MemAlloc(size_t size) noexcept;
    extern "C" GWCA_API void* __cdecl GW_MemoryMgr_MemRealloc(void* buf, size_t newSize) noexcept;
    extern "C" GWCA_API void __cdecl GW_MemoryMgr_MemFree(void* buf) noexcept;
}

#if !defined(GWCA_BUILD_EXPORTS)
namespace GW::MemoryMgr {
    inline uint32_t GetGWVersion()
    {
        return GWCA_CALL_C(GW_MemoryMgr_GetGWVersion);
    }

    inline DWORD GetSkillTimer()
    {
        return GWCA_CALL_C(GW_MemoryMgr_GetSkillTimer);
    }

    inline bool GetPersonalDir(size_t buf_len, wchar_t* buf)
    {
        return GWCA_CALL_C(GW_MemoryMgr_GetPersonalDir, buf_len, buf);
    }

    inline HWND GetGWWindowHandle()
    {
        return GWCA_CALL_C(GW_MemoryMgr_GetGWWindowHandle);
    }

    inline void* MemAlloc(size_t size)
    {
        return GWCA_CALL_C(GW_MemoryMgr_MemAlloc, size);
    }

    inline void* MemRealloc(void* buf, size_t newSize)
    {
        return GWCA_CALL_C(GW_MemoryMgr_MemRealloc, buf, newSize);
    }

    inline void MemFree(void* buf)
    {
        return GWCA_CALL_C(GW_MemoryMgr_MemFree, buf);
    }
}
#endif
