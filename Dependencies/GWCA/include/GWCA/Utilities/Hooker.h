#pragma once

#include <GWCA/Utilities/Export.h>

namespace GW {
    namespace Hook {
        GWCA_API void Initialize();
        GWCA_API void Deinitialize();

        // static void EnqueueHook(HookBase* base);
        // static void RemoveHook(HookBase* base);

        GWCA_API void EnableHooks(void* target = NULL);
        GWCA_API void DisableHooks(void* target = NULL);

        GWCA_API int CreateHook(void** target, void* detour, void** trampoline);
        GWCA_API void RemoveHook(void* target);

        GWCA_API void EnterHook();
        GWCA_API void LeaveHook();
        GWCA_API int  GetInHookCount();
    }
}
