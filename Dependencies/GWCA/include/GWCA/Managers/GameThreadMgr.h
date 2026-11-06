#pragma once
#include <GWCA/Runtime.h>

#include <GWCA/Utilities/Hook.h>
#include <GWCA/Utilities/Export.h>

#ifndef EXCEPT_EXPRESSION_LOOP
    #define EXCEPT_EXPRESSION_LOOP EXCEPTION_CONTINUE_SEARCH
#endif
namespace GW {

    struct Module;
    extern Module GameThreadModule;

    namespace GameThread {
        void EnableHooks();

        void ClearCallsNative();
#if !defined(GWCA_STDLIB)
        inline void ClearCalls() { ClearCallsNative(); }
#endif

        using NativeTaskCallback = void (__cdecl *)(void* context);
        bool Enqueue(NativeTaskCallback callback, void* context = nullptr, uint32_t force_enqueue = 0);

        using NativeGameThreadCallback = void (__cdecl *)(HookStatus* status, void* context);
        bool RegisterGameThreadCallback(HookEntry* entry, NativeGameThreadCallback callback,
                                                void* context = nullptr, int32_t altitude = 0x4000);

        bool RemoveGameThreadCallbackNative(HookEntry* entry);
#if !defined(GWCA_STDLIB)
        inline bool RemoveGameThreadCallback(HookEntry* entry) { return RemoveGameThreadCallbackNative(entry); }
#endif

        bool IsInGameThread();
    };
}

namespace GW::GameThread {
    extern "C" GWCA_API void __cdecl GW_GameThread_EnableHooks() noexcept;
    extern "C" GWCA_API void __cdecl GW_GameThread_ClearCallsNative() noexcept;
    extern "C" GWCA_API bool __cdecl GW_GameThread_Enqueue(NativeTaskCallback callback, void* context, uint32_t force_enqueue) noexcept;
    extern "C" GWCA_API bool __cdecl GW_GameThread_RegisterGameThreadCallback(HookEntry* entry, NativeGameThreadCallback callback, void* context, int32_t altitude) noexcept;
    extern "C" GWCA_API bool __cdecl GW_GameThread_RemoveGameThreadCallbackNative(HookEntry* entry) noexcept;
    extern "C" GWCA_API bool __cdecl GW_GameThread_IsInGameThread() noexcept;
}

#if !defined(GWCA_BUILD_EXPORTS)
namespace GW::GameThread {
    inline void EnableHooks()
    {
        return GWCA_CALL_C(GW_GameThread_EnableHooks);
    }

    inline void ClearCallsNative()
    {
        return GWCA_CALL_C(GW_GameThread_ClearCallsNative);
    }

    inline bool Enqueue(NativeTaskCallback callback, void* context, uint32_t force_enqueue)
    {
        return GWCA_CALL_C(GW_GameThread_Enqueue, callback, context, force_enqueue);
    }

    inline bool RegisterGameThreadCallback(HookEntry* entry, NativeGameThreadCallback callback, void* context, int32_t altitude)
    {
        return GWCA_CALL_C(GW_GameThread_RegisterGameThreadCallback, entry, callback, context, altitude);
    }

    inline bool RemoveGameThreadCallbackNative(HookEntry* entry)
    {
        return GWCA_CALL_C(GW_GameThread_RemoveGameThreadCallbackNative, entry);
    }

    inline bool IsInGameThread()
    {
        return GWCA_CALL_C(GW_GameThread_IsInGameThread);
    }
}
#endif
