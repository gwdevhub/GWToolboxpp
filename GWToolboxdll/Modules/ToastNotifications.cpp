#include "stdafx.h"

#include "ToastNotifications.h"


#include <GWCA/Utilities/Hook.h>

#include <GWCA/Packets/Opcodes.h>
#include <GWCA/Packets/StoC.h>

#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Constants/Constants.h>


#include <wintoast/wintoastlib.cpp>
#include <Utils/GuiUtils.h>
#include <Utils/ToolboxUtils.h>
#include <Defines.h>

namespace {

    bool is_platform_compatible = false;

    bool show_notifications_when_focussed = false;
    bool show_notifications_when_in_background = true;
    bool show_notifications_when_minimised = true;
    bool show_notifications_when_in_outpost = true;
    bool show_notifications_when_in_explorable = true;

    bool show_notifications_on_whisper = true;
    bool show_notifications_on_guild_chat = false;
    bool show_notifications_on_ally_chat = false;

    // Pointers deleted in ChatSettings::Terminate
    std::map<std::wstring, ToastNotifications::Toast*> toasts;

    void OnWhisperToastActivated(const ToastNotifications::Toast* toast, bool activated) {
        if (!activated)
            return; // dismissed
        GuiUtils::FocusWindow();
        GW::GameThread::Enqueue([charname = toast->title]() {
            GW::UI::SendUIMessage(GW::UI::UIMessage::kOpenWhisper, (wchar_t*)charname.data());
            });
    }
    void OnGenericToastActivated(const ToastNotifications::Toast*, bool activated) {
        if (!activated)
            return; // dismissed
        GuiUtils::FocusWindow();
    }

    GW::HookEntry OnWhisper_Entry;
    void OnWhisper(GW::HookStatus*, const wchar_t* from, const wchar_t* msg)
    {
        if(show_notifications_on_whisper)
            ToastNotifications::SendToast(from, msg,OnWhisperToastActivated);
    }

    void TriggerToastCallback(const ToastNotifications::Toast* toast, bool result) {
        if (toast->callback)
            toast->callback(toast, result);
        // naughty but idc
        ToastNotifications::Toast* nonconst = (ToastNotifications::Toast*)toast;
        nonconst->callback = 0;
        nonconst->dismiss();
    }

    GW::HookEntry OnMessageGlobal_Entry;
    void OnToastMessageDecoded(void* callback_param, wchar_t* decoded) {
        wchar_t* title = (wchar_t*)callback_param;
        ToastNotifications::SendToast(title, decoded,OnGenericToastActivated);
        delete[] title;
    }
    void SendEncodedToastMessage(const wchar_t* title, const wchar_t* encoded_message) {
        if (!(encoded_message && encoded_message[0]))
            return;
        wchar_t* title_copy = new wchar_t[wcslen(title) + 1];
        wcscpy(title_copy, title);
        GW::UI::AsyncDecodeStr(encoded_message, OnToastMessageDecoded, title_copy);
    }
    void OnMessageGlobal(GW::HookStatus*, GW::Packet::StoC::MessageGlobal* packet) {
        const wchar_t* title = nullptr;
        switch (packet->channel) {
        case GW::Chat::Channel::CHANNEL_GUILD:
            if (show_notifications_on_guild_chat)
                title = L"Guild Chat";
            break;
        case GW::Chat::Channel::CHANNEL_ALLIANCE:
            if (show_notifications_on_ally_chat)
                title = L"Alliance Chat";
            break;
        }
        if (!title)
            return;
        const wchar_t* message_encoded = ToolboxUtils::GetMessageCore();
        const size_t msg_len = wcslen(packet->sender_name) + wcslen(packet->sender_guild) + wcslen(message_encoded) + 10;
        wchar_t* message_including_sender = new wchar_t[msg_len];
        int written = -1;
        if(packet->sender_guild[0])
            written = swprintf(message_including_sender, msg_len, L"\x108\x107%s [%s]: \x1\x2%s", packet->sender_name, packet->sender_guild, message_encoded);
        else
            written = swprintf(message_including_sender, msg_len, L"\x108\x107%s: \x1\x2%s", packet->sender_name, message_encoded);
        ASSERT(written != -1);
        SendEncodedToastMessage(title, message_including_sender);
        delete[] message_including_sender;
    }

} // namespace
ToastNotifications::Toast::Toast(std::wstring _title, std::wstring _message) : title(_title), message(_message) {};
ToastNotifications::Toast::~Toast() {
    TriggerToastCallback(this, false);
    if (toast_template)
        delete toast_template;
}
void ToastNotifications::Toast::toastActivated() const {  
    TriggerToastCallback(this, true);
}
void ToastNotifications::Toast::toastActivated(int) const {
    TriggerToastCallback(this, true);
}
void ToastNotifications::Toast::toastDismissed(WinToastDismissalReason) const {
    TriggerToastCallback(this, false);
};
void ToastNotifications::Toast::toastFailed() const {
    Log::Error("Failed to show toast");
    TriggerToastCallback(this, false);
}
bool ToastNotifications::Toast::send() {
    using namespace WinToastLib;
    auto instance = WinToast::instance();
    if (!instance->isCompatible())
        return false;
    if (!instance->isInitialized()) {
        instance->setAppName(L"Guild Wars");
        const auto version = GuiUtils::StringToWString(GWTOOLBOXDLL_VERSION);
        const auto aumi = WinToast::configureAUMI(L"gwtoolbox", L"GWToolbox++", L"GWToolbox++", version);
        instance->setAppUserModelId(aumi);
        if (!instance->initialize())
            return false;
    }
    if(!toast_template)
        toast_template = new WinToastTemplate(WinToastTemplate::WinToastTemplateType::Text02);
    toast_template->setTextField(title, WinToastTemplate::FirstLine);
    toast_template->setTextField(message, WinToastTemplate::SecondLine);
    dismiss();
    toast_id = instance->showToast(*toast_template, this);
    return toast_id != -1;
}
bool ToastNotifications::Toast::dismiss() {
    bool ok = toast_id == -1 ? true : WinToast::instance()->hideToast(toast_id);
    toast_id = -1;
    return ok;
}

ToastNotifications::Toast* ToastNotifications::SendToast(const wchar_t* title, const wchar_t* message, OnToastCallback callback, void* extra_args) {
    if (!IsCompatible())
        return nullptr;
    bool show_toast = false;
    auto whnd = GW::MemoryMgr::GetGWWindowHandle();
    ASSERT(whnd);
    if (IsIconic(whnd)) {
        show_toast = show_notifications_when_minimised;
        goto show_toast;
    }
    if (GetActiveWindow() != whnd) {
        show_toast = show_notifications_when_in_background;
        goto show_toast;
    }
    show_toast = show_notifications_when_focussed;

show_toast:
    if (!show_toast)
        return nullptr;

    GW::Constants::InstanceType instance_type = GW::Map::GetInstanceType();

    switch (instance_type) {
    case GW::Constants::InstanceType::Explorable:
        show_toast = show_notifications_when_in_explorable;
        break;
    case GW::Constants::InstanceType::Outpost:
        show_toast = show_notifications_when_in_outpost;
        break;
    }

    if (!show_toast)
        return nullptr;

    auto found = toasts.find(title);
    Toast* toast = found != toasts.end() ? found->second : 0;
    if (!toast) {
        toast = new Toast(title, message);
        toasts[toast->title] = toast;
    }
    toast->message = message;
    toast->callback = callback;
    toast->extra_args = extra_args;
    if (!toast->send()) {
        TriggerToastCallback(toast, false);
        return nullptr;
    }
    return toast;
}

void ToastNotifications::Initialize()
{
    ToolboxModule::Initialize();

    is_platform_compatible = WinToast::instance()->isCompatible();
    if (is_platform_compatible) {
        GW::Chat::RegisterWhisperCallback(&OnWhisper_Entry, OnWhisper);
    }
    GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::MessageGlobal>(&OnMessageGlobal_Entry, OnMessageGlobal);
}
bool ToastNotifications::IsCompatible() {
    return is_platform_compatible;
}

void ToastNotifications::Terminate()
{
    ToolboxModule::Terminate();
    GW::StoC::RemoveCallback<GW::Packet::StoC::MessageGlobal>(&OnMessageGlobal_Entry);
    GW::Chat::RemoveWhisperCallback(&OnWhisper_Entry);
    for (auto toast : toasts) {
        TriggerToastCallback(toast.second, false);
        delete toast.second;
    }
    toasts.clear();
}
void ToastNotifications::DrawSettingInternal()
{
    ToolboxModule::DrawSettingInternal();
    ImGui::TextDisabled("GWToolbox++ can send desktop notifications when you receive whispers from friends.");

    constexpr float checkbox_w = 150.f;

    ImGui::Text("Send a desktop notification on:");
    ImGui::Indent();
    ImGui::StartSpacedElements(checkbox_w);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Whisper", &show_notifications_on_whisper);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Guild Chat", &show_notifications_on_guild_chat);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Alliance Chat", &show_notifications_on_ally_chat);
    ImGui::Unindent();

    ImGui::Text("Show desktop notifications when Guild Wars is:");
    ImGui::Indent();
    ImGui::StartSpacedElements(checkbox_w);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Minimised", &show_notifications_when_minimised);
    ImGui::NextSpacedElement(); ImGui::Checkbox("In Background", &show_notifications_when_in_background);
    ImGui::NextSpacedElement(); ImGui::Checkbox("In Focus", &show_notifications_when_focussed);
    ImGui::NextSpacedElement(); ImGui::Checkbox("In Outpost", &show_notifications_when_in_outpost);
    ImGui::NextSpacedElement(); ImGui::Checkbox("In Explorable", &show_notifications_when_in_explorable);
    ImGui::Unindent();
}

void ToastNotifications::LoadSettings(CSimpleIniA* ini)
{
    ToolboxModule::LoadSettings(ini);

    show_notifications_when_minimised = ini->GetBoolValue(Name(), VAR_NAME(show_notifications_when_minimised), show_notifications_when_minimised);
    show_notifications_when_in_background = ini->GetBoolValue(Name(), VAR_NAME(show_notifications_when_in_background), show_notifications_when_in_background);
    show_notifications_when_focussed = ini->GetBoolValue(Name(), VAR_NAME(show_notifications_when_focussed), show_notifications_when_focussed);
    show_notifications_when_in_outpost = ini->GetBoolValue(Name(), VAR_NAME(show_notifications_when_in_outpost), show_notifications_when_in_outpost);
    show_notifications_when_in_explorable = ini->GetBoolValue(Name(), VAR_NAME(show_notifications_when_in_explorable), show_notifications_when_in_explorable);

    show_notifications_on_whisper = ini->GetBoolValue(Name(), VAR_NAME(show_notifications_on_whisper), show_notifications_on_whisper);
    show_notifications_on_guild_chat = ini->GetBoolValue(Name(), VAR_NAME(show_notifications_on_guild_chat), show_notifications_on_guild_chat);
    show_notifications_on_ally_chat = ini->GetBoolValue(Name(), VAR_NAME(show_notifications_on_ally_chat), show_notifications_on_ally_chat);
}

void ToastNotifications::SaveSettings(CSimpleIniA* ini)
{
    ToolboxModule::SaveSettings(ini);

    ini->SetBoolValue(Name(), VAR_NAME(show_notifications_when_minimised), show_notifications_when_minimised);
    ini->SetBoolValue(Name(), VAR_NAME(show_notifications_when_in_background), show_notifications_when_in_background);
    ini->SetBoolValue(Name(), VAR_NAME(show_notifications_when_focussed), show_notifications_when_focussed);
    ini->SetBoolValue(Name(), VAR_NAME(show_notifications_when_in_outpost), show_notifications_when_in_outpost);
    ini->SetBoolValue(Name(), VAR_NAME(show_notifications_when_in_explorable), show_notifications_when_in_explorable);

    ini->SetBoolValue(Name(), VAR_NAME(show_notifications_on_whisper), show_notifications_on_whisper);
    ini->SetBoolValue(Name(), VAR_NAME(show_notifications_on_guild_chat), show_notifications_on_guild_chat);
    ini->SetBoolValue(Name(), VAR_NAME(show_notifications_on_ally_chat), show_notifications_on_ally_chat);
}
