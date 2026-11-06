#pragma once
#include <GWCA/stdlib/GWCA.h>
#include <GWCA/Managers/FriendListMgr.h>

namespace GW::FriendListMgr {
    using FriendStatusCallback = HookCallback<const Friend*, const Friend*>;
    inline void RegisterFriendStatusCallback(HookEntry* entry, const FriendStatusCallback& callback)
    {
        Client::Add({Client::Family::Friend, entry}, callback,
            [&](auto invoke, void* context) { return RegisterFriendStatusCallback(entry, invoke, context); },
            [](HookEntry* entry, uint32_t) { RemoveFriendStatusCallbackNative(entry); }, true);
    }
    inline void RemoveFriendStatusCallback(HookEntry* entry)
    {
        RemoveFriendStatusCallbackNative(entry);
        Client::Forget(Client::Family::Friend, entry);
    }
}
