#pragma once
#include <GWCA/stdlib/GWCA.h>
#include <GWCA/Managers/SkillbarMgr.h>

namespace GW::SkillbarMgr {
    using DecodeTemplateHeaderCallback = HookCallback<SkillTemplate*, void*, uint8_t*>;
    using GetAccountTemplateDataCallback = HookCallback<uint32_t, SkillTemplate*, int*, int*>;
    using UpdateTemplateDisplayCallback = HookCallback<uint32_t*>;
    using DecodeTemplateStringCallback = HookCallback<const wchar_t*, SkillTemplate*, int*, uint8_t*>;
    using LoadSkillTemplateCallback = HookCallback<uint32_t, SkillTemplate*>;
    inline void RegisterDecodeTemplateHeaderCallback(HookEntry* entry, const DecodeTemplateHeaderCallback& callback, int altitude = -0x8000)
    {
        Client::Add({Client::Family::DecodeHeader, entry}, callback,
            [&](auto invoke, void* context) { return RegisterDecodeTemplateHeaderCallback(entry, invoke, context, altitude); },
            [](HookEntry* entry, uint32_t) { RemoveTemplateCallbackNative(entry); }, false);
    }
    inline void RegisterGetAccountTemplateDataCallback(HookEntry* entry, const GetAccountTemplateDataCallback& callback, int altitude = -0x8000)
    {
        Client::Add({Client::Family::AccountTemplate, entry}, callback,
            [&](auto invoke, void* context) { return RegisterGetAccountTemplateDataCallback(entry, invoke, context, altitude); },
            [](HookEntry* entry, uint32_t) { RemoveTemplateCallbackNative(entry); }, false);
    }
    inline void RegisterUpdateTemplateDisplayCallback(HookEntry* entry, const UpdateTemplateDisplayCallback& callback, int altitude = -0x8000)
    {
        Client::Add({Client::Family::UpdateDisplay, entry}, callback,
            [&](auto invoke, void* context) { return RegisterUpdateTemplateDisplayCallback(entry, invoke, context, altitude); },
            [](HookEntry* entry, uint32_t) { RemoveTemplateCallbackNative(entry); }, false);
    }
    inline void RegisterDecodeTemplateStringCallback(HookEntry* entry, const DecodeTemplateStringCallback& callback, int altitude = -0x8000)
    {
        Client::Add({Client::Family::DecodeString, entry}, callback,
            [&](auto invoke, void* context) { return RegisterDecodeTemplateStringCallback(entry, invoke, context, altitude); },
            [](HookEntry* entry, uint32_t) { RemoveTemplateCallbackNative(entry); }, false);
    }
    inline void RegisterLoadSkillTemplateCallback(HookEntry* entry, const LoadSkillTemplateCallback& callback, int altitude = -0x8000)
    {
        Client::Add({Client::Family::LoadTemplate, entry}, callback,
            [&](auto invoke, void* context) { return RegisterLoadSkillTemplateCallback(entry, invoke, context, altitude); },
            [](HookEntry* entry, uint32_t) { RemoveTemplateCallbackNative(entry); }, false);
    }
    inline void RemoveTemplateCallback(HookEntry* entry)
    {
        RemoveTemplateCallbackNative(entry);
        for (auto family : {Client::Family::DecodeHeader, Client::Family::AccountTemplate, Client::Family::UpdateDisplay, Client::Family::DecodeString, Client::Family::LoadTemplate})
            Client::Forget(family, entry);
    }
}
