#pragma once
#include <GWCA/stdlib/detail/Support.h>
#include <GWCA/Managers/ChatMgr.h>

namespace GW::Chat {
    using ChatLinkCallback = HookCallback<const wchar_t*, const wchar_t*>;
}
