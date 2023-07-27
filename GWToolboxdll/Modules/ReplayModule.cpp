#include "stdafx.h"

#include <GWCA/Utilities/Scanner.h>
#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/MemoryPatcher.h>

#include <GWCA/Managers/MemoryMgr.h>

#include <Modules/ReplayModule.h>
#include <Logger.h>
#include <Timer.h>

namespace GW {
    namespace Constants {
        enum class MapID;
    }
}

namespace {
    typedef void(__cdecl *SendPacket_pt)(
        uintptr_t context, uint32_t size, void* packet);
    SendPacket_pt SendPacket_Func = 0;
    SendPacket_pt RetSendPacket = 0;

    uintptr_t CtoGS_Addr = 0;

    GW::MemoryPatcher CtoGS_Patch;

    struct Recorded_Packet {
        uint32_t size = 0;
        void* packet = nullptr;
        time_t timestamp_offset = 0;
        Recorded_Packet(uint32_t _size, void* _packet) {
            packet = malloc(_size);
            memcpy(packet, _packet, _size);
            size = _size;
        }
        ~Recorded_Packet() {
            free(packet);
        }
    };

    struct Recording {
        time_t start_timestamp = 0;
        GW::Constants::MapID map_id = (GW::Constants::MapID)0;
        std::vector<Recorded_Packet*> recorded_ctogs_packets;
        std::vector<Recorded_Packet*> recorded_gstoc_packets;
        ~Recording() {
            for (const auto p : recorded_ctogs_packets) {
                delete p;
            }
            recorded_ctogs_packets.clear();
            for (const auto p : recorded_gstoc_packets) {
                delete p;
            }
            recorded_gstoc_packets.clear();
        }
        Recording(GW::Constants::MapID _map_id) : map_id(_map_id) {
            
        }
        // Returns recorded packet ptr
        Recorded_Packet* AddCtoGS(uint32_t size, void* packet) {
            if(!start_timestamp)
                start_timestamp = time(NULL);
            auto cpy = new Recorded_Packet(size, packet);
            cpy->timestamp_offset = time(NULL) - start_timestamp;
            recorded_ctogs_packets.push_back(cpy);
            return cpy;
        }
        // Returns recorded packet ptr
        Recorded_Packet* AddGStoC(uint32_t size, void* packet) {
            if(!start_timestamp)
                start_timestamp = time(NULL);
            auto cpy = new Recorded_Packet(size, packet);
            cpy->timestamp_offset = time(NULL) - start_timestamp;
            recorded_gstoc_packets.push_back(cpy);
        }
    };

    Recording* current_recording = nullptr;

    void OnSendPacket(uintptr_t context, uint32_t size, void* packet) {
        GW::Hook::EnterHook();
        RetSendPacket(context, size, packet);
        if (!(current_recording && context == CtoGS_Addr))
            return GW::Hook::LeaveHook();


            
    }



    bool StartRecording() {
        if (!(CtoGS_Addr && SendPacket_Func))
            return false;
        CtoGS_Patch.TogglePatch(true);
        GW::Hook::EnableHooks(SendPacket_Func);



        return true;
    }
    bool StopRecording() {
        GW::Hook::DisableHooks(SendPacket_Func);
        CtoGS_Patch.TogglePatch(false);
    }
}

void ReplayModule::Initialize() {
    ToolboxModule::Initialize();

    uintptr_t address = GW::Scanner::FindAssertion("p:\\code\\gw\\net\\cli\\gcgamecmd.cpp","No valid case for switch variable 'code'", -0x32);
    if (address) {
        CtoGS_Addr = *(uintptr_t *)address;
        address = GW::Scanner::FindAssertion("p:\\code\\net\\msg\\msgconn.cpp", "bytes >= sizeof(dword)", -0x67);
        if (address) {
            SendPacket_Func = (SendPacket_pt)address;
            GW::Hook::CreateHook(SendPacket_Func, OnSendPacket, (void**)&RetSendPacket);
            CtoGS_Patch.SetPatch(address + 0xA7, "\x79\x14", 2);
        }
    }
}
