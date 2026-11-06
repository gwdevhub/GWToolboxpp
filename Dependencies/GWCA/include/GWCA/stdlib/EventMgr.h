#pragma once
#include <GWCA/stdlib/GWCA.h>
#include <GWCA/Managers/EventMgr.h>

namespace GW::EventMgr {
    using EventCallback = HookCallback<EventID, void*, uint32_t>;
    inline void RegisterEventCallback(HookEntry* entry, EventID event, const EventCallback& callback, int altitude = -0x8000)
    {
        Client::Add({Client::Family::Event, entry, static_cast<uint32_t>(event)}, callback,
            [&](auto invoke, void* context) { return RegisterEventCallback(entry, event, invoke, context, altitude); },
            [](HookEntry* entry, uint32_t id) { RemoveEventCallbackNative(entry, static_cast<EventID>(id)); }, true);
    }
    inline void RemoveEventCallback(HookEntry* entry, EventID event = EventID::kNone)
    {
        RemoveEventCallbackNative(entry, event);
        Client::Forget(Client::Family::Event, entry, static_cast<uint32_t>(event), event == EventID::kNone);
    }
}
