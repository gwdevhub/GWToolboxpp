#include "stdafx.h"

#include <GWCA/Context/PreGameContext.h>

#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Scanner.h>

#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>

#include "LoginModule.h"
#include <Utils/ToolboxUtils.h>
#include <Defines.h>

namespace {
    using GetStringParameter_pt = wchar_t*(__cdecl*)(uint32_t param_id);
    GetStringParameter_pt GetStringParameter_Func = nullptr;
    GetStringParameter_pt GetStringParameter_Ret = nullptr;

    // Always ensure character name has been pre-filled; this isn't actually used in practice as the security character feature is deprecated.
    // Prefilling it ensures that auto login can work without -charname argument being given.
    wchar_t* original_charname_parameter = nullptr;
    wchar_t* OnGetStringParameter(const uint32_t param_id)
    {
        GW::Hook::EnterHook();
        wchar_t* parameter_value = GetStringParameter_Ret(param_id);
        wchar_t* cmp = nullptr;
        [[maybe_unused]] const bool ok = GW::UI::GetCommandLinePref(L"character", &cmp);
        DEBUG_ASSERT(ok);
        if (cmp && cmp == parameter_value && wcslen(cmp) > 0) {
            // charname parameter
            original_charname_parameter = parameter_value;
            parameter_value = const_cast<wchar_t*>(L" ");
        }
        //Log::Info("GetStringParameter_Ret %p = %ls", param_id, parameter_value);
        GW::Hook::LeaveHook();
        return parameter_value;
    }
    GW::HookEntry UIMessage_HookEntry;
    void OnPostUIMessage(GW::HookStatus*,GW::UI::UIMessage, void*, void*) {
        GW::GameThread::Enqueue([]() {
            original_charname_parameter && *original_charname_parameter&& GW::LoginMgr::SelectCharacterToPlay(original_charname_parameter, false);
        },true);

    }
}

void LoginModule::Initialize()
{
    ToolboxModule::Initialize();
    GW::UI::RegisterUIMessageCallback(&UIMessage_HookEntry,GW::UI::UIMessage::kSetPreGameContext_Value1, OnPostUIMessage, 0x8000);
    GetStringParameter_Func = (GetStringParameter_pt)GW::Scanner::ToFunctionStart(GW::Scanner::FindAssertion("Param.cpp", "string - PARAM_STRING_FIRST < (sizeof(s_strings) / sizeof((s_strings)[0]))", 0, 0));
    DEBUG_ASSERT(GetStringParameter_Func);
    if (GetStringParameter_Func) {
        GW::Hook::CreateHook((void**)&GetStringParameter_Func, OnGetStringParameter, (void**)&GetStringParameter_Ret);
        GW::Hook::EnableHooks(GetStringParameter_Func);
    }
}

void LoginModule::Terminate()
{
    if(GetStringParameter_Func) GW::Hook::RemoveHook(GetStringParameter_Func);
    GW::UI::RemoveUIMessageCallback(&UIMessage_HookEntry);
    if(GetStringParameter_Func)
        GW::Hook::RemoveHook(GetStringParameter_Func);
}
