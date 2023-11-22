#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/Context/PreGameContext.h>

#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Scanner.h>

#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/MemoryMgr.h>

#include <Timer.h>

#include "LoginModule.h"

namespace {
    clock_t state_timestamp = 0;
    uint32_t char_sort_order = std::numeric_limits<uint32_t>::max();

    void SetCharSortOrder(uint32_t order)
    {
        const auto current_sort_order = GetPreference(GW::UI::EnumPreference::CharSortOrder);
        if (char_sort_order == std::numeric_limits<uint32_t>::max()) {
            char_sort_order = current_sort_order;
        }
        if (current_sort_order != order && order != std::numeric_limits<uint32_t>::max()) {
            SetPreference(GW::UI::EnumPreference::CharSortOrder, order);
        }
    }

    enum class LoginState {
        Idle,
        PendingLogin,
        FindCharacterIndex,
        SelectChar
    } state;

    uint32_t reroll_index_needed = 0;
    uint32_t reroll_index_current = 0;

    bool IsCharSelectReady()
    {
        const GW::PreGameContext* pgc = GW::GetPreGameContext();
        if (!pgc || !pgc->chars.valid()) {
            return false;
        }
        uint32_t ui_state = 10;
        SendUIMessage(GW::UI::UIMessage::kCheckUIState, nullptr, &ui_state);
        return ui_state == 2;
    }

    using GetStringParameter_pt = wchar_t*(__cdecl*)(uint32_t param_id_plus_0x27);
    GetStringParameter_pt GetStringParameter_Func = nullptr;
    GetStringParameter_pt GetStringParameter_Ret = nullptr;

    // Always ensure character name has been pre-filled; this isn't actually used in practice as the security character feature is deprecated.
    // Prefilling it ensures that auto login can work without -charname argument being given.
    wchar_t* original_charname_parameter = nullptr;

    wchar_t* OnGetStringParameter(const uint32_t param_id_plus_0x27)
    {
        GW::Hook::EnterHook();
        wchar_t* parameter_value = GetStringParameter_Ret(param_id_plus_0x27);
        if (param_id_plus_0x27 == 0x26) {
            // charname parameter
            original_charname_parameter = parameter_value;
            parameter_value = const_cast<wchar_t*>(L"NA");
        }
        Log::Info("GetStringParameter_Ret %p = %ls", param_id_plus_0x27, parameter_value);
        GW::Hook::LeaveHook();
        return parameter_value;
    }

    using PortalAccountLogin_pt = void(__cdecl*)(uint32_t transaction_id, uint32_t* user_id, uint32_t* session_id, wchar_t* preselect_character);
    PortalAccountLogin_pt PortalAccountLogin_Func = nullptr;
    PortalAccountLogin_pt PortalAccountLogin_Ret = nullptr;

    // Ensure we're asking for a valid character on login if given as a parameter
    void OnPortalAccountLogin(const uint32_t transaction_id, uint32_t* user_id, uint32_t* session_id, wchar_t* preselect_character)
    {
        GW::Hook::EnterHook();
        // Don't pre-select the character yet; we'll do this in the Update() loop after successful login
        preselect_character[0] = 0;
        PortalAccountLogin_Ret(transaction_id, user_id, session_id, preselect_character);
        state_timestamp = TIMER_INIT();
        state = LoginState::PendingLogin;
        GW::Hook::LeaveHook();
    }

    void InitialiationFailure(const char* msg = nullptr)
    {
        Log::Error(msg ? msg : "Failed to initialise LoginModule");
#if _DEBUG
        ASSERT(false);
#endif
    }
}

void LoginModule::Initialize()
{
    ToolboxModule::Initialize();

    state = LoginState::Idle;

    PortalAccountLogin_Func = (PortalAccountLogin_pt)GW::Scanner::Find("\xc7\x45\xe8\x38\x00\x00\x00\x89\x4d\xf0", "xxxxxxxxxx", -0x2b);
    if (!PortalAccountLogin_Func) {
        return InitialiationFailure("Failed to initialize PortalAccountLogin_Func");
    }

    GetStringParameter_Func = (GetStringParameter_pt)GW::Scanner::FindAssertion("p:\\code\\gw\\param\\param.cpp", "string - PARAM_STRING_FIRST < (sizeof(s_strings) / sizeof((s_strings)[0]))", -0x13);
    if (!GetStringParameter_Func) {
        return InitialiationFailure("Failed to initialize GetStringParameter_Func");
    }
    {
        int res = GW::HookBase::CreateHook(PortalAccountLogin_Func, OnPortalAccountLogin, (void**)&PortalAccountLogin_Ret);
        if (res == -1) {
            return InitialiationFailure("Failed to hook PortalAccountLogin_Func");
        }

        res = GW::HookBase::CreateHook(GetStringParameter_Func, OnGetStringParameter, (void**)&GetStringParameter_Ret);
        if (res == -1) {
            return InitialiationFailure("Failed to hook GetStringParameter_Func");
        }

        GW::HookBase::EnableHooks(PortalAccountLogin_Func);
        GW::HookBase::EnableHooks(GetStringParameter_Func);
    }
}

void LoginModule::Terminate()
{
    GW::HookBase::RemoveHook(GetStringParameter_Func);
    GW::HookBase::RemoveHook(PortalAccountLogin_Func);
}

void LoginModule::Update(float)
{
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
            if (original_charname_parameter != nullptr && *original_charname_parameter != '\0') {
                // we want to pre-select a character, set order to alphabetical so we don't get stuck on empty char slots when using arrow keys
                SetCharSortOrder(static_cast<uint32_t>(GW::Constants::Preference::CharSortOrder::Alphabetize));
            }
            if (IsCharSelectReady()) {
                state = LoginState::FindCharacterIndex;
            }
        }
        break;
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
                    // Wipe out the command line parameter for GW here; its only relevant for the first login!
                    *original_charname_parameter = '\0';
                    return;
                }
            }
            // Character no found
            state = LoginState::Idle;
        }
        break;
        case LoginState::SelectChar: {
            if (TIMER_DIFF(state_timestamp) > 250) {
                // This could be due to a reconnect dialog in the way, which is fine
                state = LoginState::Idle;
                return;
            }
            const auto pgc = GW::GetPreGameContext();
            if (pgc->index_1 == reroll_index_current) {
                return; // Not moved yet
            }
            const HWND h = GW::MemoryMgr::GetGWWindowHandle();
            if (pgc->index_1 == reroll_index_needed) {
                // We're on the character that was asked for
                state = LoginState::Idle;
                SetCharSortOrder(char_sort_order);
                return;
            }
            reroll_index_current = pgc->index_1;
            SendMessage(h, WM_KEYDOWN, VK_RIGHT, 0x014D0001);
            SendMessage(h, WM_KEYUP, VK_RIGHT, 0xC14D0001);
        }
        break;
    }
}
