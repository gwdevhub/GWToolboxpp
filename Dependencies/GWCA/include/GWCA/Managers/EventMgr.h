#pragma once

#include <GWCA/Utilities/Hook.h>
#include <GWCA/Utilities/Export.h>

namespace GW {

    struct Module;
    extern Module EventMgrModule;

    namespace EventMgr {
        // Index into the game's m_handler array (EvtDispatch.cpp asserts id < EVENTS, which is 0x2f).
        // Names below are read off Gw.exe: EvtOs.cpp translates each window message into one of these,
        // EvtQueue.cpp drains them into the dispatcher, and every id the game itself registers a handler
        // for was matched to that handler. Ids absent here are dispatched by the game but not yet identified.
        enum class EventID {
            kAppActivated = 0x0,        // WM_ACTIVATEAPP; packet is one dword, non-zero when activated
            kFrameTick = 0x1,           // once per frame from the pump, packet is the elapsed milliseconds
            kRenderFinished = 0x2,      // once per frame after the render, packet is one dword
            kCharTyped = 0x3,           // WM_CHAR: {character, repeat, modifier keys}
            kCursorVisibility = 0x5,    // packet is one dword, non-zero to show the cursor
            kShutdown = 0x6,            // posting this latches the event queue closed, so nothing dispatches after it
            kScreenSizeChanged = 0x7,   // {new width, new height, old width, old height}, raised inside the queue drain
            kDriverError = 0x8,         // the game's own handler logs "Driver internal error encountered." and quits
            kRecvPing = kDriverError,   // legacy GWCA name for 0x8; the game does not use this id for pings
            kDpiChanged = 0xa,          // WM_DPICHANGED: {x dpi, y dpi}
            kGamepadAttached = 0xb,     // the four below come from the same controller poll in EvtOs.cpp
            kGamepadState = 0xc,        // one dword
            kGamepadAxis = 0xd,         // four dwords
            kGamepadButtonDown = 0xe,   // {button, state}
            kGamepadButtonUp = 0xf,     // {button, state}
            kCharTypedInjected = 0x13,  // same shape as kCharTyped, from the game's own WM_APP+3 injection
            kKeyDown = 0x1a,            // {key, repeat, modifier keys}; the engine records the key as held here
            kKeyUp = 0x1b,              // same shape; the engine clears the held bit here
            kFocusLost = 0x1c,          // WM_KILLFOCUS, no packet
            kMouseMoveRelative = 0x1d,  // {button, 0, x, y, button mask, modifier keys} while the mouse is captured
            kMouseButtonDown = 0x1e,    // same shape as kMouseMoveRelative
            kMouseLeave = 0x1f,         // WM_MOUSELEAVE, no packet
            kMouseMove = 0x20,          // same shape as kMouseMoveRelative, only sent when the position changed
            kMouseButtonUp = 0x21,      // same shape; also synthesised for every held button when capture is lost
            kMouseWheel = 0x22,         // same shape, with the wheel delta in place of the button
            kWindowMoved = 0x23,        // WM_MOVE: {x, y}
            // A shared channel: eight of the game's own subsystems register here, so filter on FriendEventType.
            kFriend = 0x24,
            kService = 0x25,            // same packet shape as kFriend; FrApi forwards it as UI message 0x50
            kServiceAlt = 0x26,         // same again, forwarded as UI message 0x51
            kFocusGained = 0x27,        // WM_SETFOCUS, no packet
            kWindowResized = 0x28,      // WM_SIZE: {width, height}
            kTouchDown = 0x2c,          // the three touch ids carry {index, 0, x, y, touch mask}
            kTouchMove = 0x2d,
            kTouchUp = 0x2e,            // also synthesised for every held touch when the sequence is cancelled

            // AddHandler rejects 0x2a outright, and the dispatcher drops anything >= 0x2f, so 0x2f and
            // 0x30 are engine-internal: 0x30 is what makes the queue raise kScreenSizeChanged.
            kReserved = 0x2a,

            kNone = 0xffff
        };

        // First dword of a kFriend packet, which is what the game's own handler switches over.
        // These are the message ids FriendApi.cpp sends to the login frame, so they share that numbering.
        enum class FriendEventType {
            kStatusChanged = 0x26,
            kLocationChanged = 0x28,
            kFriendAddedOrRemoved = 0x2c
        };

        // (status, event_id, packet_bytes, packet_bytes_len). Altitude <= 0 runs before the game handles it, > 0 after.
        typedef HookCallback<EventID, void*,uint32_t> EventCallback;

        GWCA_API void RegisterEventCallback(
            HookEntry *entry,
            EventID event_id,
            const EventCallback& callback,
            int altitude = -0x8000);

        GWCA_API void RemoveEventCallback(
            HookEntry *entry,
            EventID event_id = EventID::kNone);

        // Took a length all along; the two-parameter declaration this replaces never had a definition to link against.
        GWCA_API bool SendEventMessage(EventID event_id, void* packet_bytes, uint32_t packet_bytes_len);
    };
}
