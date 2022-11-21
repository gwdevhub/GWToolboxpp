#include "stdafx.h"
#include <GWCA/Context/PreGameContext.h>

#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Scanner.h>

#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/MemoryMgr.h>

#include <Timer.h>

#include "LoginModule.h"

namespace {
    clock_t state_timestamp = 0;
    enum class LoginState {
        Idle,
        PendingLogin,
        FindCharacterIndex,
        SelectChar
    } state;

    uint32_t reroll_index_needed = 0;
    uint32_t reroll_index_current = 0;

    bool IsCharSelectReady() {
        GW::PreGameContext* pgc = GW::GetPreGameContext();
        if (!pgc || !pgc->chars.valid())
            return false;
        uint32_t ui_state = 10;
        GW::UI::SendUIMessage(GW::UI::UIMessage::kCheckUIState, 0, &ui_state);
        return ui_state == 2;
    }

    typedef wchar_t*(__cdecl* GetStringParameter_pt)(uint32_t param_id_plus_0x27);
    GetStringParameter_pt GetStringParameter_Func = 0;
    GetStringParameter_pt GetStringParameter_Ret = 0;

    // Always ensure character name has been pre-filled; this isn't actually used in practice as the security character feature is deprecated.
    // Prefilling it ensures that auto login can work without -charname argument being given.
    wchar_t* original_charname_parameter = nullptr;
    wchar_t* OnGetStringParameter(uint32_t param_id_plus_0x27) {
        GW::Hook::EnterHook();
        wchar_t* parameter_value = GetStringParameter_Ret(param_id_plus_0x27);
        if (param_id_plus_0x27 == 0x29) {
            // charname parameter
            original_charname_parameter = parameter_value;
            parameter_value = (wchar_t*)L"NA";
        }
        GW::Hook::LeaveHook();
        return parameter_value;
    }

    typedef void(__cdecl* PortalAccountLogin_pt)(uint32_t transaction_id,uint32_t* user_id,uint32_t* session_id,wchar_t* preselect_character, wchar_t* security_character);
    PortalAccountLogin_pt PortalAccountLogin_Func = 0;
    PortalAccountLogin_pt PortalAccountLogin_Ret = 0;

    // Ensure we're asking for a valid character on login if given as a parameter
    void OnPortalAccountLogin(uint32_t transaction_id, uint32_t* user_id, uint32_t* session_id, wchar_t* preselect_character, wchar_t* security_character) {
        GW::Hook::EnterHook();
        // Don't pre-select the character yet; we'll do this in the Update() loop after sucessful login
        preselect_character[0] = 0;
        PortalAccountLogin_Ret(transaction_id, user_id, session_id, preselect_character, security_character);
        state_timestamp = TIMER_INIT();
        state = LoginState::PendingLogin;
        GW::Hook::LeaveHook();
    }
}
void LoginModule::Initialize() {
    ToolboxModule::Initialize();

    state = LoginState::Idle;

    GetStringParameter_Func = (GetStringParameter_pt)GW::Scanner::FindAssertion("p:\\code\\gw\\param\\param.cpp", "string - PARAM_STRING_FIRST < (sizeof(s_strings) / sizeof((s_strings)[0]))", -0x13);
    GW::HookBase::CreateHook(GetStringParameter_Func, OnGetStringParameter, (void**)&GetStringParameter_Ret);
    if(GetStringParameter_Func)
        GW::HookBase::EnableHooks(GetStringParameter_Func);

    PortalAccountLogin_Func = (PortalAccountLogin_pt)GW::Scanner::Find("\xc7\x45\xe4\x38\x00\x00\x00\x89\x4d\xec", "xxxxxxxxxx", -0x2f);
    GW::HookBase::CreateHook(PortalAccountLogin_Func, OnPortalAccountLogin, (void**)&PortalAccountLogin_Ret);
    if (PortalAccountLogin_Func)
        GW::HookBase::EnableHooks(PortalAccountLogin_Func);
}
void LoginModule::Terminate()
{
    GW::HookBase::RemoveHook(GetStringParameter_Func);
    GW::HookBase::RemoveHook(PortalAccountLogin_Func);
}
void LoginModule::Update(float) {
    // This loop checks to see if the player has inputted a -charname argument previously.
    // If they have, then we use the user interface to manually navigate to a character
    // This is because the Portal login above would usually do it, but it overrides any reconnect dialogs.
    switch (state) {
    case LoginState::Idle:
        return;
    case LoginState::PendingLogin: {
        // No charname to switch to
        if (TIMER_DIFF(state_timestamp) > 5000) {
            state = LoginState::Idle;
            return;
        }
        if (IsCharSelectReady()) {
            state = LoginState::FindCharacterIndex;
            return;
        }
    } break;
    case LoginState::FindCharacterIndex: {
        // No charname to switch to
        if (!(original_charname_parameter && *original_charname_parameter)) {
            state = LoginState::Idle;
            return;
        }
        const auto pgc = GW::GetPreGameContext();
        for (size_t i = 0; i < pgc->chars.size(); i++) {
            if (wcscmp(pgc->chars[i].character_name, original_charname_parameter) == 0) {
                state_timestamp = TIMER_INIT();
                state = LoginState::SelectChar;
                reroll_index_needed = i;
                reroll_index_current = 0xffff;
                // Wipe out the command line parameter for GW here; its only relevent for the first login!
                *original_charname_parameter = 0;
                return;
            }
        }
        // Character no found
        state = LoginState::Idle;
        return;
    } break;
    case LoginState::SelectChar: {
        if (TIMER_DIFF(state_timestamp) > 250) {
            // This could be due to a reconnect dialog in the way, which is fine
            state = LoginState::Idle;
            return;
        }
        const auto pgc = GW::GetPreGameContext();
        if (pgc->index_1 == reroll_index_current)
            return; // Not moved yet
        HWND h = GW::MemoryMgr::GetGWWindowHandle();
        if (pgc->index_1 == reroll_index_needed) {
            // We're on the character that was asked for
            state = LoginState::Idle;
            return;
        }
        reroll_index_current = pgc->index_1;
        SendMessage(h, WM_KEYDOWN, VK_RIGHT, 0x014D0001);
        SendMessage(h, WM_KEYUP, VK_RIGHT, 0xC14D0001);
        return;
    } break;

    }

}

