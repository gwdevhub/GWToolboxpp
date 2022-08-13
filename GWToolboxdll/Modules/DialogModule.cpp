#include "stdafx.h"

#include <GWCA/Constants/QuestIDs.h>

#include <GWCA/GameEntities/Agent.h>

#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/AgentMgr.h>

#include <GWCA/Utilities/Scanner.h>
#include <GWCA/Utilities/Hooker.h>

#include <Utils/GuiUtils.h>
#include <Modules/DialogModule.h>
#include <Logger.h>
#include <Timer.h>

namespace {
    GW::UI::UIInteractionCallback NPCDialogUICallback_Func = nullptr;
    GW::UI::UIInteractionCallback NPCDialogUICallback_Ret = nullptr;

    std::vector<GW::UI::DialogButtonInfo*> dialog_buttons;
    std::vector<GuiUtils::EncString*> dialog_button_messages;

    GW::UI::DialogBodyInfo dialog_info;
    uint32_t last_agent_id = 0;
    GuiUtils::EncString dialog_body;

    GW::HookEntry dialog_hook;

    std::map<uint32_t, clock_t> queued_dialogs_to_send;

    void OnDialogButtonAdded(GW::UI::DialogButtonInfo* wparam) {
        const auto button_info = new GW::UI::DialogButtonInfo();
        memcpy(button_info, wparam, sizeof(*button_info));

        const auto button_message = new GuiUtils::EncString(button_info->message);
        button_info->message = const_cast<wchar_t*>(button_message->encoded().data());

        dialog_button_messages.push_back(button_message);
        dialog_buttons.push_back(button_info);
    }
    // Parse any buttons held within the dialog body
    void OnDialogBodyDecoded(void*, wchar_t* decoded) {
        const std::wregex button_regex(L"<a=([0-9]+)>([^<]+)<");
        std::wsmatch m;
        std::wstring subject(decoded);
        std::wstring msg;
        GW::UI::DialogButtonInfo embedded_button{};
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
        for (const auto d : dialog_buttons) {
            delete d;
        }
        dialog_buttons.clear();
        for (const auto d : dialog_button_messages) {
            delete d;
        }
        dialog_button_messages.clear();
    }
    void OnNPCDialogUICallback(GW::UI::InteractionMessage* message, void* wparam, void* lparam) {
        GW::HookBase::EnterHook();
        if (message->message_id == 0xb) {
            ResetDialog();
            if(dialog_info.agent_id)
                last_agent_id = dialog_info.agent_id;
            dialog_info.agent_id = 0;
        }
        NPCDialogUICallback_Ret(message, wparam,lparam);
        GW::HookBase::LeaveHook();
    }
    void OnDialogClosedByServer() {
        if (queued_dialogs_to_send.empty())
            return;
        GW::Agent* npc = GW::Agents::GetAgentByID(last_agent_id);
        GW::Agent* me = GW::Agents::GetPlayer();
        if (npc && me && GW::GetDistance(npc->pos, me->pos) < GW::Constants::Range::Area)
            GW::Agents::GoNPC(npc);
    }
    void OnPostUIMessage(GW::HookStatus* status, GW::UI::UIMessage message_id, void* wparam, void*) {
        if (status->blocked) {
            // Blocked elsewhere.
            return;
        }
        switch (message_id) {
        case GW::UI::UIMessage::kDialogBody: {
            ResetDialog();
            const auto new_dialog_info = static_cast<GW::UI::DialogBodyInfo*>(wparam);
            if (!new_dialog_info->message_enc) {
                return; // Dialog closed.
            }
            memcpy(&dialog_info, wparam, sizeof(dialog_info));
            dialog_body.reset(dialog_info.message_enc);
            GW::UI::AsyncDecodeStr(dialog_info.message_enc, OnDialogBodyDecoded);
        } break;
        case GW::UI::UIMessage::kDialogButton: {
            OnDialogButtonAdded(static_cast<GW::UI::DialogButtonInfo*>(wparam));
        } break;
        }
    }
    bool IsDialogButtonAvailable(uint32_t dialog_id) {
        return std::ranges::any_of(dialog_buttons, [dialog_id](const GW::UI::DialogButtonInfo* d) {
                return d->dialog_id == dialog_id;
            });
    }
    void OnPreUIMessage(GW::HookStatus* status, GW::UI::UIMessage message_id, void* wparam, void*) {
        switch (message_id) {
        case GW::UI::UIMessage::kDialogBody: {
            const auto new_dialog_info = static_cast<GW::UI::DialogBodyInfo*>(wparam);
            if (!new_dialog_info->message_enc) {
                OnDialogClosedByServer();
            }
        } break;
        case GW::UI::UIMessage::kSendDialog: {
            const auto dialog_id = reinterpret_cast<uint32_t>(wparam);
            if ((dialog_id & 0xff000000) != 0)
                break; // Don't handle merchant interaction dialogs.
            if (!IsDialogButtonAvailable(dialog_id)) {
                status->blocked = true;
            }
            else {
                ResetDialog();
            }
        } break;
        }
    }
}

void DialogModule::Initialize() {
    ToolboxModule::Initialize();
    constexpr GW::UI::UIMessage dialog_ui_messages[] = {
        GW::UI::UIMessage::kSendDialog,
        GW::UI::UIMessage::kDialogBody,
        GW::UI::UIMessage::kDialogButton
    };
    for (const auto message_id : dialog_ui_messages) {
        GW::UI::RegisterUIMessageCallback(&dialog_hook, message_id, OnPreUIMessage,-0x1);
        GW::UI::RegisterUIMessageCallback(&dialog_hook, message_id, OnPostUIMessage, 0x500);
    }
    // NB: Can also be found via floating dialogs array in memory. We're not using hooks for any of the other floating dialogs, but would be good to document later.
    NPCDialogUICallback_Func = reinterpret_cast<GW::UI::UIInteractionCallback>(GW::Scanner::FindAssertion(
        R"(p:\code\gw\ui\game\gmnpc.cpp)", "interactMsg.codedText && interactMsg.codedText[0]", -0xfb));
    if (NPCDialogUICallback_Func) {
        GW::HookBase::CreateHook(NPCDialogUICallback_Func, OnNPCDialogUICallback, reinterpret_cast<void**>(&NPCDialogUICallback_Ret));
        GW::HookBase::EnableHooks(NPCDialogUICallback_Func);
    }
}
void DialogModule::SendDialog(uint32_t dialog_id) {
    const bool already_queued = queued_dialogs_to_send.contains(dialog_id);
    queued_dialogs_to_send[dialog_id] = TIMER_INIT();
    if (already_queued)
        return; // Don't redo any logic.

    if ((dialog_id & 0x800000) != 0) {
        // Quest related dialog
        const uint32_t quest_id = (dialog_id ^ 0x800000) >> 8;
        switch ((dialog_id & 0xf)) {
        case 1: // Dialog is for taking a quest
            SendDialog(quest_id << 8 | 0x800003);
            break;
        case 7: // Dialog is for accepting a quest reward
            SendDialog(quest_id << 8 | 0x800006);
            break;
        }
        return;
    }
    if ((dialog_id & 0xf84) == dialog_id && ((dialog_id & 0xfff) >> 8) != 0) {
        // Dialog is for changing profession; queue up the enquire dialog option aswell
        const uint32_t profession_id = (dialog_id & 0xfff) >> 8;
        const uint32_t enquire_dialog_id = (profession_id << 8) | 0x85;
        SendDialog(enquire_dialog_id);
        return;
    }
    switch (dialog_id) {
    case GW::Constants::DialogID::UwTeleLab:
    case GW::Constants::DialogID::UwTeleVale:
    case GW::Constants::DialogID::UwTelePits:
    case GW::Constants::DialogID::UwTelePools:
    case GW::Constants::DialogID::UwTelePlanes:
    case GW::Constants::DialogID::UwTeleWastes:
    case GW::Constants::DialogID::UwTeleMnt: {
        const auto dialog_agent = GW::Agents::GetAgentByID(GetDialogAgent());
        if (dialog_agent 
            && dialog_agent->type == static_cast<uint32_t>(GW::Constants::AgentType::Living)
            && dialog_agent->GetAsAgentLiving()->player_number == GW::Constants::ModelID::UW::Reapers) {
            // Reaper teleport dialog; queue up prerequisites.
            SendDialog(0x7f);
            SendDialog(dialog_id - 0x7);
        }
    } break;
        
    }
}

void DialogModule::SendDialogs(std::initializer_list<uint32_t> dialog_ids) {
    for (const auto dialog_id : dialog_ids) {
        SendDialog(dialog_id);
    }
}

void DialogModule::Update(float) {
    for (auto it = queued_dialogs_to_send.begin(); it != queued_dialogs_to_send.end(); it++) {
        if (TIMER_DIFF(it->second) > 3000) {
            // NB: Show timeout error message?
            queued_dialogs_to_send.erase(it);
            break;
        }
        if (!GetDialogAgent())
            continue;
        if (it->first == 0) {
            // If dialog queued is id 0, this means sue player wants to take (or accept) the first available quest.
            AcceptFirstAvailableQuest();
            queued_dialogs_to_send.erase(it);
            break;
        }
        if (IsDialogButtonAvailable(it->first)) {
            GW::Agents::SendDialog(it->first);
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

uint32_t DialogModule::AcceptFirstAvailableQuest() {
    if (dialog_buttons.empty()) return 0;
    std::vector<uint32_t> available_quests;
    for (const auto dialog_button : dialog_buttons) {
        const uint32_t dialog_id = dialog_button->dialog_id;
        if ((dialog_id & 0x800000) == 0)
            continue;
        // Quest related dialog
        uint32_t quest_id = (dialog_id ^ 0x800000) >> 8;
        switch ((dialog_id & 0xff)) {
        case 1: // Dialog is for taking a quest
        case 3: // Dialog is for quest enquiry
        case 6: // Dialog is for enquiring about a quest reward
        case 7: // Dialog is for accepting a quest reward
            available_quests.push_back(quest_id);
            break;
        default:
            break;
        }
    }
    for (const auto quest_id : available_quests) {
        if (quest_id == static_cast<uint32_t>(GW::Constants::QuestID::UW_UWG) 
            && available_quests.size() > 1) {
            // skip unless it's the only available dialog - certain quests almost always want to be taken last
            continue;
        }
        SendDialog((quest_id << 8) | 0x800001); // Take quest
        SendDialog((quest_id << 8) | 0x800007); // Accept reward
        return quest_id;
    }
    return 0;
}

uint32_t DialogModule::GetDialogAgent()
{
    return dialog_info.agent_id;
}
const std::vector<GW::UI::DialogButtonInfo*>& DialogModule::GetDialogButtons()
{
    return dialog_buttons;
}
