#pragma once
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
    GWCA_API bool Initialize();
    GWCA_API void DisableHooks();
    GWCA_API void EnableHooks();
    GWCA_API void Terminate();
}
