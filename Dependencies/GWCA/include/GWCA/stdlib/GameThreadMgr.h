#pragma once
#include <GWCA/stdlib/GWCA.h>
#include <GWCA/Managers/GameThreadMgr.h>

namespace GW::GameThread {
    using TaskCallback = std::function<void()>;
    using GameThreadCallback = HookCallback<>;
    inline void ClearCalls()
    {
        Client::Clearing guard;
        ClearCallsNative();
        Client::CancelTasks();
        Client::ForgetGameThread();
    }
    inline void Enqueue(std::function<void()> callback, bool force_enqueue = false)
    {
        Client::Enqueue(std::move(callback), [&](auto invoke, void* context) {
            return Enqueue(invoke, context, force_enqueue ? 1u : 0u);
        });
    }
    inline void RegisterGameThreadCallback(HookEntry* entry, const GameThreadCallback& callback, int altitude = 0x4000)
    {
        Client::Add({Client::Family::GameThread, entry}, callback,
            [&](auto invoke, void* context) { return RegisterGameThreadCallback(entry, invoke, context, altitude); },
            [](HookEntry* entry, uint32_t) { RemoveGameThreadCallbackNative(entry); }, true);
    }
    inline void RemoveGameThreadCallback(HookEntry* entry)
    {
        RemoveGameThreadCallbackNative(entry);
        Client::Forget(Client::Family::GameThread, entry);
    }
}
