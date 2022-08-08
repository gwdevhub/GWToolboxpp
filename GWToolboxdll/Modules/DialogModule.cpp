#include "stdafx.h"

#include <Utils/GuiUtils.h>

#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/AgentMgr.h>

#include <GWCA/Utilities/Scanner.h>
#include <GWCA/Utilities/Hooker.h>

#include <Logger.h>
#include <Modules/DialogModule.h>

#include <Timer.h>

namespace {
    GW::UI::UIInteractionCallback NPCDialogUICallback_Func = 0;
    GW::UI::UIInteractionCallback NPCDialogUICallback_Ret = 0;

    std::vector<GW::UI::DialogButtonInfo*> dialog_buttons;
    std::vector<GuiUtils::EncString*> dialog_button_messages;

    GW::UI::DialogBodyInfo dialog_info;
    GuiUtils::EncString dialog_body;



    DialogModule& Instance() {
        return DialogModule::Instance();
    }
    void OnDialogButtonAdded(GW::UI::DialogButtonInfo* wparam) {
        GW::UI::DialogButtonInfo* button_info = new GW::UI::DialogButtonInfo();
        memcpy(button_info, wparam, sizeof(*button_info));

        GuiUtils::EncString* button_message = new GuiUtils::EncString(button_info->message);
        button_info->message = (wchar_t*)button_message->encoded().data();

        dialog_button_messages.push_back(button_message);
        dialog_buttons.push_back(button_info);
    }
    // Parse any buttons held within the dialog body
    void OnDialogBodyDecoded(void*, wchar_t* decoded) {
        std::wregex button_regex(L"<a=([0-9]+)>([^<]+)<");
        std::wsmatch m;
        std::wstring subject(decoded);
        std::wstring msg;
        GW::UI::DialogButtonInfo embedded_button;
        embedded_button.dialog_id = 0;
        embedded_button.skill_id = 0xFFFFFFF;
        while (std::regex_search(subject, m, button_regex)) {
            if (!GuiUtils::ParseUInt(m[1].str().c_str(), &embedded_button.dialog_id)) {
                Log::ErrorW(L"Failed to parse dialog id for %s, %s", m[1].str().c_str(), m[2].str().c_str());
                return;
            }
            // Technically theres no encoded string for this button, wrap it to avoid issues later.
            msg = L"\x108\x107";
            msg += m[2].str();
            msg += L"\x1";

            embedded_button.message = msg.data();
            OnDialogButtonAdded(&embedded_button);
            subject = m.suffix().str();
        }
    }
    // Wipe dialog ready for new one
    void ResetDialog() {
        for (auto d : dialog_buttons) {
            delete d;
        }
        dialog_buttons.clear();
        for (auto d : dialog_button_messages) {
            delete d;
        }
        dialog_button_messages.clear();
        dialog_info.agent_id = 0;
    }
    void OnNPCDialogUICallback(GW::UI::InteractionMessage* message, void* wparam, void* lparam) {
        GW::HookBase::EnterHook();
        if (message->message_id == 0xb) {
            ResetDialog();
        }
        NPCDialogUICallback_Ret(message, wparam,lparam);
        GW::HookBase::LeaveHook();
    }
    void OnPostUIMessage(GW::HookStatus* status, GW::UI::UIMessage message_id, void* wparam, void*) {
        if (status->blocked) {
            // Blocked elsewhere.
            return;
        }
        switch (message_id) {
        case GW::UI::UIMessage::kDialogBody: {
            ResetDialog();
            memcpy(&dialog_info, wparam, sizeof(dialog_info));
            if (!dialog_info.message_enc)
                return; // Dialog closed.
            dialog_body.reset(dialog_info.message_enc);
            GW::UI::AsyncDecodeStr(dialog_info.message_enc, OnDialogBodyDecoded);
        } break;
        case GW::UI::UIMessage::kDialogButton: {
            OnDialogButtonAdded((GW::UI::DialogButtonInfo*)wparam);
        } break;
        }
    }
    bool IsDialogButtonAvailable(uint32_t dialog_id) {
        for (auto d : dialog_buttons) {
            if (d->dialog_id == dialog_id)
                return true;
        }
        return false;
    }
    void OnPreUIMessage(GW::HookStatus* status, GW::UI::UIMessage message_id, void* wparam, void*) {
        switch (message_id) {
        case GW::UI::UIMessage::kSendDialog: {
            if (!IsDialogButtonAvailable((uint32_t)wparam)) {
                status->blocked = true;
            }
            else {
                ResetDialog();
            }
        } break;
        }
    }

    GW::HookEntry dialog_hook;

    std::vector<std::pair<clock_t, uint32_t>> queued_dialogs_to_send;
}

void DialogModule::Initialize() {
    ToolboxModule::Initialize();
    const GW::UI::UIMessage dialog_ui_messages[] = {
        GW::UI::UIMessage::kSendDialog,
        GW::UI::UIMessage::kDialogBody,
        GW::UI::UIMessage::kDialogButton
    };
    for (auto message_id : dialog_ui_messages) {
        GW::UI::RegisterUIMessageCallback(&dialog_hook, message_id, OnPreUIMessage,-0x1);
        GW::UI::RegisterUIMessageCallback(&dialog_hook, message_id, OnPostUIMessage, 0x500);
    }
    // NB: Can also be found via floating dialogs array in memory. We're not using hooks for any of the other floating dialogs, but would be good to document later.
    NPCDialogUICallback_Func = (GW::UI::UIInteractionCallback)GW::Scanner::FindAssertion("p:\\code\\gw\\ui\\game\\gmnpc.cpp", "interactMsg.codedText && interactMsg.codedText[0]", -0xfb);
    if (NPCDialogUICallback_Func) {
        GW::HookBase::CreateHook(NPCDialogUICallback_Func, OnNPCDialogUICallback, (void**)&NPCDialogUICallback_Ret);
        GW::HookBase::EnableHooks(NPCDialogUICallback_Func);
    }
}
void DialogModule::SendDialog(uint32_t dialog_id) {
    queued_dialogs_to_send.push_back({ TIMER_INIT(),dialog_id });
}
void DialogModule::Update(float) {
    for (auto it = queued_dialogs_to_send.begin(); it != queued_dialogs_to_send.end();it++) {
        if (TIMER_DIFF(it->first) > 3000) {
            // NB: Show timeout error message?
            queued_dialogs_to_send.erase(it);
            break;
        }
        if (IsDialogButtonAvailable(it->second)) {
            GW::Agents::SendDialog(it->second);
            queued_dialogs_to_send.erase(it);
            break;
        }
    }
}
void DialogModule::Terminate() {
    GW::HookBase::RemoveHook(NPCDialogUICallback_Func);
}
const wchar_t* DialogModule::GetDialogBody()
{
    return dialog_body.encoded().c_str();
}
const std::vector<GuiUtils::EncString*>& DialogModule::GetDialogButtonMessages()
{
    return dialog_button_messages;
}
uint32_t DialogModule::GetDialogAgent()
{
    return dialog_info.agent_id;
}
const std::vector<GW::UI::DialogButtonInfo*>& DialogModule::GetDialogButtons()
{
    return dialog_buttons;
}