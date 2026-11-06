#pragma once
#include <GWCA/stdlib/GWCA.h>
#include <GWCA/Managers/ItemMgr.h>

namespace GW::Items {
    using ItemClickCallback = HookCallback<UI::UIPacket::kMouseAction*, Item*>;
    inline void RegisterItemClickCallback(HookEntry* entry, const ItemClickCallback& callback)
    {
        Client::Add({Client::Family::Item, entry}, callback,
            [&](auto invoke, void* context) { return RegisterItemClickCallback(entry, invoke, context); },
            [](HookEntry* entry, uint32_t) { RemoveItemClickCallbackNative(entry); }, false);
    }
    inline void RemoveItemClickCallback(HookEntry* entry)
    {
        RemoveItemClickCallbackNative(entry);
        Client::Forget(Client::Family::Item, entry);
    }
}
