#pragma once
#include <GWCA/stdlib/GWCA.h>
#include <GWCA/Managers/UIMgr.h>

namespace GW::UI {
    using KeyCallback = HookCallback<uint32_t>;
    using UIMessageCallback = HookCallback<UIMessage, void*, void*>;
    using FrameUIMessageCallback = HookCallback<const Frame*, UIMessage, void*, void*>;
    using CreateUIComponentCallback = std::function<void(CreateUIComponentPacket*)>;
    inline void RegisterKeydownCallback(HookEntry* entry, const KeyCallback& callback)
    {
        Client::Add({Client::Family::Frame, entry, static_cast<uint32_t>(UIMessage::kKeyDown)}, callback,
            [&](auto invoke, void* context) { return RegisterKeydownCallback(entry, invoke, context); },
            [](HookEntry* entry, uint32_t) { RemoveFrameUIMessageCallbackNative(entry); }, false);
    }
    inline void RegisterKeyupCallback(HookEntry* entry, const KeyCallback& callback)
    {
        Client::Add({Client::Family::Frame, entry, static_cast<uint32_t>(UIMessage::kKeyUp)}, callback,
            [&](auto invoke, void* context) { return RegisterKeyupCallback(entry, invoke, context); },
            [](HookEntry* entry, uint32_t) { RemoveFrameUIMessageCallbackNative(entry); }, false);
    }
    inline void RemoveKeydownCallback(HookEntry* entry)
    {
        RemoveKeydownCallbackNative(entry);
        Client::Forget(Client::Family::Frame, entry);
    }
    inline void RemoveKeyupCallback(HookEntry* entry)
    {
        RemoveKeyupCallbackNative(entry);
        Client::Forget(Client::Family::Frame, entry);
    }
    inline void RegisterUIMessageCallback(HookEntry* entry, UIMessage message, const UIMessageCallback& callback, int altitude = -0x8000)
    {
        Client::Add({Client::Family::UI, entry, static_cast<uint32_t>(message)}, callback,
            [&](auto invoke, void* context) { return RegisterUIMessageCallback(entry, message, invoke, context, altitude); },
            [](HookEntry* entry, uint32_t id) { RemoveUIMessageCallbackNative(entry, static_cast<UIMessage>(id)); }, true);
    }
    inline void RemoveUIMessageCallback(HookEntry* entry, UIMessage message = UIMessage::kNone)
    {
        RemoveUIMessageCallbackNative(entry, message);
        Client::Forget(Client::Family::UI, entry, static_cast<uint32_t>(message), message == UIMessage::kNone);
    }
    inline void RegisterFrameUIMessageCallback(HookEntry* entry, UIMessage message, const FrameUIMessageCallback& callback, int altitude = -0x8000)
    {
        Client::Add({Client::Family::Frame, entry, static_cast<uint32_t>(message)}, callback,
            [&](auto invoke, void* context) { return RegisterFrameUIMessageCallback(entry, message, invoke, context, altitude); },
            [](HookEntry* entry, uint32_t) { RemoveFrameUIMessageCallbackNative(entry); }, false);
    }
    inline void RemoveFrameUIMessageCallback(HookEntry* entry)
    {
        RemoveFrameUIMessageCallbackNative(entry);
        Client::Forget(Client::Family::Frame, entry);
    }
    inline void RegisterCreateUIComponentCallback(HookEntry* entry, const CreateUIComponentCallback& callback, int altitude = -0x8000)
    {
        Client::Add({Client::Family::Component, entry}, callback,
            [&](auto invoke, void* context) { return RegisterCreateUIComponentCallback(entry, invoke, context, altitude); },
            [](HookEntry* entry, uint32_t) { RemoveCreateUIComponentCallbackNative(entry); }, true);
    }
    inline void RemoveCreateUIComponentCallback(HookEntry* entry)
    {
        RemoveCreateUIComponentCallbackNative(entry);
        Client::Forget(Client::Family::Component, entry);
    }
}
