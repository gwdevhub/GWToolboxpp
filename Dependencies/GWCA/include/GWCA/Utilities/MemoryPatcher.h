#pragma once
#include <GWCA/Runtime.h>

namespace GW {

    class MemoryPatcher {
    public:
        MemoryPatcher() = default;
        MemoryPatcher(const MemoryPatcher&) = delete;
        ~MemoryPatcher() {
            if (GetIsActive()) 
                Reset();
        };

        void Reset();
        bool IsValid() { return m_addr != nullptr; }
        void SetPatch(uintptr_t addr, const char* patch, size_t size);

        const void* GetAddress() { return m_addr; }

        // Use to redirect a CALL or JMP instruction to call a different function instead.
        bool SetRedirect(uintptr_t call_instruction_address, void* redirect_func);

        bool TogglePatch(bool flag);
        bool TogglePatch() { return TogglePatch(!m_active); };

        bool GetIsActive() { return m_active; };

        // Disconnect all patches from memory, restoring original values if applicable
        static void DisableHooks();
        // Connect any applicable patches that have been disconnected.
        static void EnableHooks();
    private:
        void       *m_addr = nullptr;
        uint8_t    *m_patch = nullptr;
        uint8_t    *m_backup = nullptr;
        size_t      m_size = 0;
        bool        m_active = false;

        void PatchActual(bool patch);
    };
}

namespace GW {
    extern "C" GWCA_API void __cdecl GW_MemoryPatcher_Reset(MemoryPatcher* self) noexcept;
    extern "C" GWCA_API void __cdecl GW_MemoryPatcher_SetPatch(MemoryPatcher* self, uintptr_t addr, const char* patch, size_t size) noexcept;
    extern "C" GWCA_API bool __cdecl GW_MemoryPatcher_SetRedirect(MemoryPatcher* self, uintptr_t call_instruction_address, void* redirect_func) noexcept;
    extern "C" GWCA_API bool __cdecl GW_MemoryPatcher_TogglePatch(MemoryPatcher* self, bool flag) noexcept;
    extern "C" GWCA_API void __cdecl GW_MemoryPatcher_DisableHooks() noexcept;
    extern "C" GWCA_API void __cdecl GW_MemoryPatcher_EnableHooks() noexcept;
}

#if !defined(GWCA_BUILD_EXPORTS)
namespace GW {
    inline void MemoryPatcher::Reset()
    {
        return GWCA_CALL_C(GW_MemoryPatcher_Reset, this);
    }

    inline void MemoryPatcher::SetPatch(uintptr_t addr, const char* patch, size_t size)
    {
        return GWCA_CALL_C(GW_MemoryPatcher_SetPatch, this, addr, patch, size);
    }

    inline bool MemoryPatcher::SetRedirect(uintptr_t call_instruction_address, void* redirect_func)
    {
        return GWCA_CALL_C(GW_MemoryPatcher_SetRedirect, this, call_instruction_address, redirect_func);
    }

    inline bool MemoryPatcher::TogglePatch(bool flag)
    {
        return GWCA_CALL_C(GW_MemoryPatcher_TogglePatch, this, flag);
    }

    inline void MemoryPatcher::DisableHooks()
    {
        return GWCA_CALL_C(GW_MemoryPatcher_DisableHooks);
    }

    inline void MemoryPatcher::EnableHooks()
    {
        return GWCA_CALL_C(GW_MemoryPatcher_EnableHooks);
    }
}
#endif
