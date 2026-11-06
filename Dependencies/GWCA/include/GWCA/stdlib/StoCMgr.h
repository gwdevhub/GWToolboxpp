#pragma once
#include <GWCA/stdlib/GWCA.h>
#include <GWCA/Managers/StoCMgr.h>

namespace GW::StoC {
    using PacketCallback = HookCallback<Packet::StoC::PacketBase*>;
    inline bool RegisterPacketCallback(HookEntry* entry, uint32_t header, const PacketCallback& callback, int altitude = -0x8000)
    {
        return Client::Add({Client::Family::StoC, entry, header}, callback,
            [&](auto invoke, void* context) { return RegisterPacketCallback(entry, header, invoke, context, altitude); },
            [](HookEntry* entry, uint32_t id) { RemoveCallbackNative(id, entry); }, true);
    }
    inline bool RegisterPostPacketCallback(HookEntry* entry, uint32_t header, const PacketCallback& callback)
    {
        return RegisterPacketCallback(entry, header, callback, 0x8000);
    }
    template <typename T>
    bool RegisterPacketCallback(HookEntry* entry, const HookCallback<T*>& callback, int altitude = -0x8000)
    {
        if (!callback) return false;
        return RegisterPacketCallback(entry, Packet::StoC::Packet<T>::STATIC_HEADER,
            [callback](HookStatus* status, Packet::StoC::PacketBase* packet) { callback(status, static_cast<T*>(packet)); }, altitude);
    }
    template <typename T>
    bool RegisterPostPacketCallback(HookEntry* entry, const HookCallback<T*>& callback)
    {
        return RegisterPacketCallback<T>(entry, callback, 0x8000);
    }
    inline size_t RemoveCallback(uint32_t header, HookEntry* entry)
    {
        const auto result = RemoveCallbackNative(header, entry);
        Client::Forget(Client::Family::StoC, entry, header, false);
        return result;
    }
    inline size_t RemoveCallbacks(HookEntry* entry)
    {
        const auto result = RemoveCallbacksNative(entry);
        Client::Forget(Client::Family::StoC, entry);
        return result;
    }
    inline void RemovePostCallback(uint32_t header, HookEntry* entry) { (void)RemoveCallback(header, entry); }
    template <typename T> void RemoveCallback(HookEntry* entry) { (void)RemoveCallback(Packet::StoC::Packet<T>::STATIC_HEADER, entry); }
    template <typename T> void RemovePostCallback(HookEntry* entry) { RemoveCallback<T>(entry); }
}
