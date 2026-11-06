#pragma once
#include <GWCA/Runtime.h>

#include <GWCA/Utilities/Hook.h>
#include <GWCA/Utilities/Export.h>

namespace GW {

    // StoC Manager. See https://github.com/GameRevision/GWLP-R/wiki/GStoC for some already explored packets.

    namespace Packet {
        namespace StoC {
            struct PacketBase;

            template <typename T>
            struct Packet;
        }
    }
    
    struct Module;
    extern Module StoCModule;

    namespace StoC {
        using NativePacketCallback = void (__cdecl *)(HookStatus* status, Packet::StoC::PacketBase* packet, void* context);
        // Altitude <= 0 fires before the packet is processed, > 0 after it.
        bool RegisterPacketCallback(
            HookEntry *entry,
            uint32_t header,
            NativePacketCallback callback,
            void* context = nullptr,
            int32_t altitude = -0x8000);

        // @Deprecated: use RegisterPacketCallback with a positive altitude instead.
        bool RegisterPostPacketCallback(
            HookEntry* entry,
            uint32_t header,
            NativePacketCallback callback, void* context = nullptr);

        size_t RemoveCallbackNative(uint32_t header, HookEntry *entry);
        size_t RemoveCallbacksNative(HookEntry *entry);

#if !defined(GWCA_STDLIB)
        inline size_t RemoveCallback(uint32_t header, HookEntry* entry) { return RemoveCallbackNative(header, entry); }
        inline size_t RemoveCallbacks(HookEntry* entry) { return RemoveCallbacksNative(entry); }
        template <typename T>
        void RemoveCallback(HookEntry* entry) {
            uint32_t header = Packet::StoC::Packet<T>::STATIC_HEADER;
            RemoveCallback(header, entry);
        }

        // @Deprecated: use RegisterPacketCallback with a positive altitude instead.
        inline void RemovePostCallback(uint32_t header, HookEntry* entry) { (void)RemoveCallbackNative(header, entry); }

        template <typename T>
        void RemovePostCallback(HookEntry* entry) {
            uint32_t header = Packet::StoC::Packet<T>::STATIC_HEADER;
            RemovePostCallback(header, entry);
        }
#endif

        bool EmulatePacket(Packet::StoC::PacketBase *packet);

        template <typename T>
        bool EmulatePacket(Packet::StoC::Packet<T> *packet) {
            packet->header = Packet::StoC::Packet<T>::STATIC_HEADER;
            return EmulatePacket((Packet::StoC::PacketBase *)packet);
        }
    };
}

namespace GW::StoC {
    extern "C" GWCA_API bool __cdecl GW_StoC_RegisterPacketCallback(HookEntry* entry, uint32_t header, NativePacketCallback callback, void* context, int32_t altitude) noexcept;
    extern "C" GWCA_API bool __cdecl GW_StoC_RegisterPostPacketCallback(HookEntry* entry, uint32_t header, NativePacketCallback callback, void* context) noexcept;
    extern "C" GWCA_API size_t __cdecl GW_StoC_RemoveCallbackNative(uint32_t header, HookEntry* entry) noexcept;
    extern "C" GWCA_API size_t __cdecl GW_StoC_RemoveCallbacksNative(HookEntry* entry) noexcept;
    extern "C" GWCA_API bool __cdecl GW_StoC_EmulatePacket(Packet::StoC::PacketBase* packet) noexcept;
}

#if !defined(GWCA_BUILD_EXPORTS)
namespace GW::StoC {
    inline bool RegisterPacketCallback(HookEntry* entry, uint32_t header, NativePacketCallback callback, void* context, int32_t altitude)
    {
        return GWCA_CALL_C(GW_StoC_RegisterPacketCallback, entry, header, callback, context, altitude);
    }

    inline bool RegisterPostPacketCallback(HookEntry* entry, uint32_t header, NativePacketCallback callback, void* context)
    {
        return GWCA_CALL_C(GW_StoC_RegisterPostPacketCallback, entry, header, callback, context);
    }

    inline size_t RemoveCallbackNative(uint32_t header, HookEntry* entry)
    {
        return GWCA_CALL_C(GW_StoC_RemoveCallbackNative, header, entry);
    }

    inline size_t RemoveCallbacksNative(HookEntry* entry)
    {
        return GWCA_CALL_C(GW_StoC_RemoveCallbacksNative, entry);
    }

    inline bool EmulatePacket(Packet::StoC::PacketBase* packet)
    {
        return GWCA_CALL_C(GW_StoC_EmulatePacket, packet);
    }
}
#endif
