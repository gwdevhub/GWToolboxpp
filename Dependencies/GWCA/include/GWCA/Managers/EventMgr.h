#pragma once

#include <GWCA/Utilities/Hook.h>
#include <GWCA/Utilities/Export.h>

namespace GW {

    struct Module;
    extern Module EventMgrModule;

    namespace EventMgr {
        enum class EventID {
            kRecvPing = 0x8,
            kSendFriendState = 0x26,
            kRecvFriendState = 0x2c

        };

        typedef HookCallback<EventID, void*, uint32_t> EventCallback;
        GWCA_API void RegisterEventCallback(
            HookEntry *entry,
            EventID event_id,
            const EventCallback& callback,
            int altitude = -0x8000);

        GWCA_API void RemoveEventCallback(
            HookEntry *entry);
    };
}
