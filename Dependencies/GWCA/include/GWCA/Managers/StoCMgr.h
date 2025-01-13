#pragma once

#include <GWCA/Utilities/Hook.h>
#include <GWCA/Utilities/Export.h>

namespace GW {

    /*
     StoC Manager
     See https://github.com/GameRevision/GWLP-R/wiki/GStoC for some already explored packets.
    */

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
        typedef HookCallback<Packet::StoC::PacketBase *> PacketCallback;
        // Register a function to be called when a packet is received.
        // An altitude of 0 or less will be triggered before the packet is processed.
        // An altitude greater than 0 will be triggered after the packet has been processed.
        GWCA_API bool RegisterPacketCallback(
            HookEntry *entry,
            uint32_t header,
            const PacketCallback& callback,
            int altitude = -0x8000);

        // @Deprecated: use RegisterPacketCallback with a positive altitude instead.
        GWCA_API bool RegisterPostPacketCallback(
            HookEntry* entry,
            uint32_t header,
            const PacketCallback& callback);

        /* Use this to add handlers to the stocmgr, primary function. */
        template <typename T>
        bool RegisterPacketCallback(HookEntry *entry, const HookCallback<T*>& handler, int altitude = -0x8000) {
            uint32_t header = Packet::StoC::Packet<T>::STATIC_HEADER;
            return RegisterPacketCallback(entry, header,
                                          [handler](HookStatus *status, Packet::StoC::PacketBase *pak) -> void {
                                              return handler(status, static_cast<T *>(pak));
                                          }, altitude);
        }


        // @Deprecated: use RegisterPacketCallback with a positive altitude instead.
        template <typename T>
        bool RegisterPostPacketCallback(HookEntry* entry, const HookCallback<T*>& handler) {
            uint32_t header = Packet::StoC::Packet<T>::STATIC_HEADER;
            return RegisterPostPacketCallback(entry, header,
                                              [handler](HookStatus* status, Packet::StoC::PacketBase* pak) -> void {
                                                  return handler(status, static_cast<T*>(pak));
                                              });
        }

        GWCA_API size_t RemoveCallback(uint32_t header, HookEntry *entry);
        GWCA_API size_t RemoveCallbacks(HookEntry *entry);

        template <typename T>
        void RemoveCallback(HookEntry* entry) {
            uint32_t header = Packet::StoC::Packet<T>::STATIC_HEADER;
            RemoveCallback(header, entry);
        }

        // @Deprecated: use RegisterPacketCallback with a positive altitude instead.
        GWCA_API void RemovePostCallback(uint32_t header, HookEntry* entry);

        template <typename T>
        void RemovePostCallback(HookEntry* entry) {
            uint32_t header = Packet::StoC::Packet<T>::STATIC_HEADER;
            RemovePostCallback(header, entry);
        }

        GWCA_API bool EmulatePacket(Packet::StoC::PacketBase *packet);

        template <typename T>
        bool EmulatePacket(Packet::StoC::Packet<T> *packet) {
            packet->header = Packet::StoC::Packet<T>::STATIC_HEADER;
            return EmulatePacket((Packet::StoC::PacketBase *)packet);
        }
    };
}
