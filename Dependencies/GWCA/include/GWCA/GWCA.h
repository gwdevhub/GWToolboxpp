#pragma once
#include <GWCA/Runtime.h>
#include "Utilities/Export.h"

/**********************************************************************************\

_____ _    _ _____   ___
|  __ \ |  | /  __ \ / _ \   _     _
| |  \/ |  | | /  \// /_\ \_| |_ _| |_
| | __| |/\| | |    |  _  |_   _|_   _|
| |_\ \  /\  / \__/\| | | | |_|   |_|
\____/\/  \/ \____/\_| |_/


Created by KAOS (a.k.a. 4D 1) and HasKha for use in GWToolbox++

Credits to:

Sune C (a.k.a. Harboe) and ACB -
Most low level ground work of API via the GWCA and GWLP:R
projects as well as getting the gamerevision community rolling.
Much help from following these two gentlemen's breadcrumbs.

Miracle444 -
GWA2, as well as helping me really understand how the game handles
its memory information.

d8rken -
Zraw menu/api which acted as a gateway to get a grasp of cpp as well
as directx.

HasKha -
For getting me to not make everything singletons :P

Jon - 
Current maintainer of the project, rework and rewrite since 2022

Dubble -
Packaging for distribution, cmake
\**********************************************************************************/

namespace GW {
    bool Initialize();
    void DisableHooks();
    void EnableHooks();
    void TerminateNative();
#if !defined(GWCA_STDLIB)
    inline void Terminate() { TerminateNative(); }
#endif
}

namespace GW {
    extern "C" GWCA_API bool __cdecl GW_Initialize() noexcept;
    extern "C" GWCA_API void __cdecl GW_DisableHooks() noexcept;
    extern "C" GWCA_API void __cdecl GW_EnableHooks() noexcept;
    extern "C" GWCA_API void __cdecl GW_TerminateNative() noexcept;
}

#if !defined(GWCA_BUILD_EXPORTS)
namespace GW {
    inline bool Initialize()
    {
        return GWCA_CALL_C(GW_Initialize);
    }

    inline void DisableHooks()
    {
        return GWCA_CALL_C(GW_DisableHooks);
    }

    inline void EnableHooks()
    {
        return GWCA_CALL_C(GW_EnableHooks);
    }

    inline void TerminateNative()
    {
        return GWCA_CALL_C(GW_TerminateNative);
    }
}
#endif
