#include "stdafx.h"

#include <GWCA/Constants/QuestIDs.h>

#include <GWCA/GameEntities/Agent.h>

#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>


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

    uint32_t last_dialog_id = 0;

    void OnDialogButtonAdded(const GW::UI::DialogButtonInfo* wparam)
    {
        const auto button_info = new GW::UI::DialogButtonInfo();
        memcpy(button_info, wparam, sizeof(*button_info));

        const auto button_message = new GuiUtils::EncString(button_info->message);
        button_info->message = const_cast<wchar_t*>(button_message->encoded().data());

        dialog_button_messages.push_back(button_message);
        dialog_buttons.push_back(button_info);
    }

    // Parse any buttons held within the dialog body
    // ReSharper disable once CppParameterMayBeConst
    // ReSharper disable once CppParameterMayBeConstPtrOrRef
    void OnDialogBodyDecoded(void*, const wchar_t* decoded)
    {
        const std::wregex button_regex(L"<a=([0-9]+)>([^<]+)(<|$)");
        std::wsmatch m;
        std::wstring subject(decoded);
        GW::UI::DialogButtonInfo embedded_button{};
        embedded_button.dialog_id = 0;
        embedded_button.skill_id = 0xFFFFFFF;
        while (std::regex_search(subject, m, button_regex)) {
            if (!GuiUtils::ParseUInt(m[1].str().c_str(), &embedded_button.dialog_id)) {
                Log::ErrorW(L"Failed to parse dialog id for %s, %s", m[1].str().c_str(), m[2].str().c_str());
                return;
            }
            // Technically theres no encoded string for this button, wrap it to avoid issues later.
            std::wstring msg = L"\x108\x107";
            msg += m[2].str();
            msg += L"\x1";

            embedded_button.message = msg.data();
            OnDialogButtonAdded(&embedded_button);
            subject = m.suffix().str();
        }
    }

    // Wipe dialog ready for new one
    void ResetDialog()
    {
        for (const auto d : dialog_buttons) {
            delete d;
        }
        dialog_buttons.clear();
        for (const auto d : dialog_button_messages) {
            delete d;
        }
        dialog_button_messages.clear();
        dialog_body.reset(nullptr);
    }

    void OnNPCDialogUICallback(GW::UI::InteractionMessage* message, void* wparam, void* lparam)
    {
        GW::HookBase::EnterHook();
        if (message->message_id == GW::UI::UIMessage::kDestroyFrame) {
            ResetDialog();
            if (dialog_info.agent_id) {
                last_agent_id = dialog_info.agent_id;
            }
            dialog_info.agent_id = 0;
        }
        NPCDialogUICallback_Ret(message, wparam, lparam);
        GW::HookBase::LeaveHook();
    }

    void OnDialogClosedByServer()
    {
        if (queued_dialogs_to_send.empty()) {
            return;
        }
        const auto npc = GW::Agents::GetAgentByID(last_agent_id);
        const auto me =  npc ? GW::Agents::GetControlledCharacter() : nullptr;
        if (me && GetDistance(npc->pos, me->pos) < GW::Constants::Range::Area) {
            GW::Agents::InteractAgent(npc);
        }
    }

    bool IsDialogButtonAvailable(uint32_t dialog_id)
    {
        return !dialog_body.encoded().empty() && std::ranges::any_of(dialog_buttons, [dialog_id](const GW::UI::DialogButtonInfo* d) {
            return d->dialog_id == dialog_id;
        });
    }
}

void DialogModule::OnPreUIMessage(
    GW::HookStatus* status, const GW::UI::UIMessage message_id, void* wparam, void*)
{
    switch (message_id) {
        case GW::UI::UIMessage::kDialogBody: {
            const auto new_dialog_info = static_cast<GW::UI::DialogBodyInfo*>(wparam);
            if (!new_dialog_info->message_enc) {
                OnDialogClosedByServer();
            }
        }
        break;
        case GW::UI::UIMessage::kSendAgentDialog: {
            const auto dialog_id = reinterpret_cast<uint32_t>(wparam);
            if ((dialog_id & 0xff000000) != 0) {
                break; // Don't handle merchant interaction dialogs.
            }
            if (!IsDialogButtonAvailable(dialog_id)) {
                status->blocked = true;
            }
            else {
                ResetDialog();
            }
        }
        break;
    }
}

void DialogModule::OnPostUIMessage(const GW::HookStatus* status, const GW::UI::UIMessage message_id, void* wparam, void*)
{
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
        }
        break;
        case GW::UI::UIMessage::kDialogButton: {
            OnDialogButtonAdded(static_cast<GW::UI::DialogButtonInfo*>(wparam));
        }
        break;
        case GW::UI::UIMessage::kSendAgentDialog: {
            OnDialogSent(reinterpret_cast<uint32_t>(wparam));
        }
        break;
    }
}

void DialogModule::OnDialogSent(const uint32_t dialog_id)
{
    last_dialog_id = dialog_id;
    const auto queued_at = queued_dialogs_to_send.contains(dialog_id) ? queued_dialogs_to_send.at(dialog_id) : 0;
    queued_dialogs_to_send.erase(dialog_id);
    if (IsQuest(dialog_id)) {
        const auto quest_id = GetQuestID(dialog_id);
        switch (GetQuestDialogType(dialog_id)) {
            case QuestDialogType::TAKE:
            case QuestDialogType::REWARD:
                break;
            default:
                return;
        }
        for (auto it = queued_dialogs_to_send.begin(); it != queued_dialogs_to_send.end();) {
            const auto other_dialog_id = it->first;
            // make sure we don't delete dialogs for the same quest queued up earlier or later, e.g. separate reward dialog!
            if (GetQuestID(other_dialog_id) == quest_id && queued_at == it->second) {
                it = queued_dialogs_to_send.erase(it);
            }
            else {
                ++it;
            }
        }
    }
    if (IsUWTele(dialog_id)) {
        queued_dialogs_to_send.erase(GW::Constants::DialogID::UwTeleEnquire);
        queued_dialogs_to_send.erase(dialog_id - 0x7);
    }
}

void DialogModule::Initialize()
{
    ToolboxModule::Initialize();
    constexpr GW::UI::UIMessage dialog_ui_messages[] = {
        GW::UI::UIMessage::kSendAgentDialog,
        GW::UI::UIMessage::kDialogBody,
        GW::UI::UIMessage::kDialogButton
    };
    for (const auto message_id : dialog_ui_messages) {
        RegisterUIMessageCallback(&dialog_hook, message_id, OnPreUIMessage, -0x1);
        RegisterUIMessageCallback(&dialog_hook, message_id, OnPostUIMessage, 0x500);
    }
    // NB: Can also be found via floating dialogs array in memory. We're not using hooks for any of the other floating dialogs, but would be good to document later.
    NPCDialogUICallback_Func = reinterpret_cast<GW::UI::UIInteractionCallback>(GW::Scanner::FindAssertion(
        "p:\\code\\gw\\ui\\game\\gmnpc.cpp", "interactMsg.codedText && interactMsg.codedText[0]", -0xfb));
    if (NPCDialogUICallback_Func) {
        GW::HookBase::CreateHook((void**)&NPCDialogUICallback_Func, OnNPCDialogUICallback, reinterpret_cast<void**>(&NPCDialogUICallback_Ret));
        GW::HookBase::EnableHooks(NPCDialogUICallback_Func);
    }
}

void DialogModule::Terminate()
{
    ToolboxModule::Terminate();
    GW::UI::RemoveUIMessageCallback(&dialog_hook);
    GW::HookBase::RemoveHook(NPCDialogUICallback_Func);
}

void DialogModule::SendDialog(const uint32_t dialog_id, clock_t time)
{
    time = time ? time : TIMER_INIT();
    queued_dialogs_to_send[dialog_id] = time;

    if (IsQuest(dialog_id)) {
        const uint32_t quest_id = GetQuestID(dialog_id);
        switch (GetQuestDialogType(dialog_id)) {
            case QuestDialogType::TAKE: // Dialog is for taking a quest
                queued_dialogs_to_send[GetDialogIDForQuestDialogType(quest_id, QuestDialogType::ENQUIRE_NEXT)] = time;
                queued_dialogs_to_send[GetDialogIDForQuestDialogType(quest_id, QuestDialogType::ENQUIRE)] = time;
                break;
            case QuestDialogType::REWARD: // Dialog is for accepting a quest reward
                queued_dialogs_to_send[GetDialogIDForQuestDialogType(quest_id, QuestDialogType::ENQUIRE_NEXT)] = time;
                queued_dialogs_to_send[GetDialogIDForQuestDialogType(quest_id, QuestDialogType::ENQUIRE_REWARD)] = time;
                break;
            default:
                return;
        }
    }

    if ((dialog_id & 0xf84) == dialog_id && (dialog_id & 0xfff) >> 8 != 0) {
        // Dialog is for changing profession; queue up the enquire dialog option aswell
        const uint32_t profession_id = (dialog_id & 0xfff) >> 8;
        const uint32_t enquire_dialog_id = profession_id << 8 | 0x85;
        queued_dialogs_to_send[enquire_dialog_id] = time;
        return;
    }

    if (IsUWTele(dialog_id)) {
        // Reaper teleport dialog; queue up prerequisites.
        queued_dialogs_to_send[GW::Constants::DialogID::UwTeleEnquire] = time;
        queued_dialogs_to_send[dialog_id - 0x7] = time;
    }
}

void DialogModule::SendDialog(const uint32_t dialog_id)
{
    SendDialog(dialog_id, TIMER_INIT());
}

void DialogModule::SendDialogs(const std::initializer_list<uint32_t> dialog_ids)
{
    const auto timestamp = TIMER_INIT();
    for (const auto dialog_id : dialog_ids) {
        SendDialog(dialog_id, timestamp);
    }
}

void DialogModule::Update(float)
{
    for (auto it = queued_dialogs_to_send.begin(); it != queued_dialogs_to_send.end(); ++it) {
        if (TIMER_DIFF(it->second) > 3000) {
            // NB: Show timeout error message?
            queued_dialogs_to_send.erase(it);
            break;
        }

        std::set<uint32_t> illegalDialogs({
            GetDialogIDForQuestDialogType(static_cast<uint32_t>(GW::Constants::QuestID::Doa_ToTheRescue), QuestDialogType::REWARD),
            GetDialogIDForQuestDialogType(static_cast<uint32_t>(GW::Constants::QuestID::Doa_RiftBetweenUs), QuestDialogType::TAKE),
            GetDialogIDForQuestDialogType(static_cast<uint32_t>(GW::Constants::QuestID::Doa_FoundryBreakout), QuestDialogType::REWARD),
            GetDialogIDForQuestDialogType(static_cast<uint32_t>(GW::Constants::QuestID::UW_Chamber), QuestDialogType::TAKE),
            GetDialogIDForQuestDialogType(static_cast<uint32_t>(GW::Constants::QuestID::UW_Chamber), QuestDialogType::REWARD),
            GetDialogIDForQuestDialogType(static_cast<uint32_t>(GW::Constants::QuestID::UW_Wastes), QuestDialogType::TAKE),
            GetDialogIDForQuestDialogType(static_cast<uint32_t>(GW::Constants::QuestID::UW_Wastes), QuestDialogType::REWARD),
            GetDialogIDForQuestDialogType(static_cast<uint32_t>(GW::Constants::QuestID::UW_UWG), QuestDialogType::TAKE),
            GetDialogIDForQuestDialogType(static_cast<uint32_t>(GW::Constants::QuestID::UW_UWG), QuestDialogType::REWARD),
            GetDialogIDForQuestDialogType(static_cast<uint32_t>(GW::Constants::QuestID::UW_Mnt), QuestDialogType::TAKE),
            GetDialogIDForQuestDialogType(static_cast<uint32_t>(GW::Constants::QuestID::UW_Mnt), QuestDialogType::REWARD),
            GetDialogIDForQuestDialogType(static_cast<uint32_t>(GW::Constants::QuestID::UW_Pits), QuestDialogType::TAKE),
            GetDialogIDForQuestDialogType(static_cast<uint32_t>(GW::Constants::QuestID::UW_Pits), QuestDialogType::REWARD),
            GetDialogIDForQuestDialogType(static_cast<uint32_t>(GW::Constants::QuestID::UW_Planes), QuestDialogType::TAKE),
            GetDialogIDForQuestDialogType(static_cast<uint32_t>(GW::Constants::QuestID::UW_Planes), QuestDialogType::REWARD),
            GetDialogIDForQuestDialogType(static_cast<uint32_t>(GW::Constants::QuestID::UW_Pools), QuestDialogType::TAKE),
            GetDialogIDForQuestDialogType(static_cast<uint32_t>(GW::Constants::QuestID::UW_Pools), QuestDialogType::REWARD),
            GetDialogIDForQuestDialogType(static_cast<uint32_t>(GW::Constants::QuestID::UW_Escort), QuestDialogType::TAKE),
            GetDialogIDForQuestDialogType(static_cast<uint32_t>(GW::Constants::QuestID::UW_Escort), QuestDialogType::REWARD),
            GetDialogIDForQuestDialogType(static_cast<uint32_t>(GW::Constants::QuestID::UW_Restore), QuestDialogType::TAKE),
            GetDialogIDForQuestDialogType(static_cast<uint32_t>(GW::Constants::QuestID::UW_Restore), QuestDialogType::REWARD),
            GetDialogIDForQuestDialogType(static_cast<uint32_t>(GW::Constants::QuestID::UW_Vale), QuestDialogType::TAKE),
            GetDialogIDForQuestDialogType(static_cast<uint32_t>(GW::Constants::QuestID::UW_Vale), QuestDialogType::REWARD),
        });

        if (illegalDialogs.contains(it->first)) {
            GW::GameThread::Enqueue([it] {
                GW::Agents::SendDialogRaw(it->first);
            });
            queued_dialogs_to_send.erase(it);
            break;
        }

        if (!GetDialogAgent()) {
            continue;
        }
        const auto dialog_id = it->first;
        if (dialog_id == 0) {
            // If dialog queued is id 0, this means the player wants to take (or accept reward for) the first available quest.
            queued_dialogs_to_send.erase(it);
            AcceptFirstAvailableQuest() || AcceptFirstAvailableBounty();
            break;
        }
        if (IsDialogButtonAvailable(dialog_id)) {
            queued_dialogs_to_send.erase(it);
            GW::Agents::SendDialog(dialog_id);
            break;
        }
    }
}

const wchar_t* DialogModule::GetDialogBody()
{
    return dialog_body.encoded().c_str();
}

const std::vector<GuiUtils::EncString*>& DialogModule::GetDialogButtonMessages()
{
    return dialog_button_messages;
}

uint32_t DialogModule::LastDialogId() {
    return last_dialog_id;
}

uint32_t DialogModule::AcceptFirstAvailableQuest()
{
    if (dialog_buttons.empty()) {
        return 0;
    }
    std::vector<uint32_t> available_quests;
    for (const auto dialog_button : dialog_buttons) {
        const uint32_t dialog_id = dialog_button->dialog_id;
        if (!IsQuest(dialog_id)) {
            continue;
        }
        // Quest related dialog
        uint32_t quest_id = GetQuestID(dialog_id);
        switch (GetQuestDialogType(dialog_id)) {
            case QuestDialogType::TAKE:
            case QuestDialogType::ENQUIRE:
            case QuestDialogType::ENQUIRE_NEXT:
            case QuestDialogType::ENQUIRE_REWARD:
            case QuestDialogType::REWARD:
                available_quests.push_back(quest_id);
                break;
            default:
                break;
        }
    }

    const auto take_quest = [](const uint32_t quest_id) {
        SendDialogs({
            GetDialogIDForQuestDialogType(quest_id, QuestDialogType::TAKE),
            GetDialogIDForQuestDialogType(quest_id, QuestDialogType::REWARD)
        });
        return quest_id;
    };

    // restore -> escort -> uwg
    for (const auto quest_id : {GW::Constants::QuestID::UW_Restore, GW::Constants::QuestID::UW_Escort}) {
        const auto uquest_id = std::to_underlying(quest_id);
        if (std::ranges::contains(available_quests, uquest_id)) {
            return take_quest(uquest_id);
        }
    }
    if (!available_quests.empty()) {
        return take_quest(available_quests[0]);
    }
    return 0;
}

uint32_t DialogModule::AcceptFirstAvailableBounty() {
    if (dialog_buttons.empty()) {
        return 0;
    }
    
    // NB: Be careful - GW allows you to grab factions blessings more than once!

    const wchar_t* luxon_priest = L"\x3F69\xBAA2\xF307\x2CC1";
    const wchar_t* kurzick_priest = L"\x3E98\xDA05\xAA38\x45D";

    const auto agent_name = GW::Agents::GetAgentEncName(GetDialogAgent());
    if (!agent_name)
        return 0;

    const auto has_bounty = [](GW::Constants::SkillID skill_id) {
        return GW::Effects::GetPlayerEffectBySkillId(skill_id) != nullptr;
    };

    // NB: Be careful - GW allows you to grab factions blessings more than once!
    if (wcscmp(agent_name, luxon_priest) == 0) {
        if (has_bounty(GW::Constants::SkillID::Blessing_of_the_Luxons))
            return 0;
        SendDialogs({ 0x85, 0x86, 0x2, 0x84 });
        return 1;
    }
    if (wcscmp(agent_name, kurzick_priest) == 0) {
        if (has_bounty(GW::Constants::SkillID::Blessing_of_the_Kurzicks))
            return 0;
        // Interested > Thanks || Lets talk this over > Bribe > Thanks
        SendDialogs({ 0x85, 0x86, 0x81, 0x2, 0x84 });
        return 1;
    }
    // NB: For all other bounties, we don't need to check because the dialog will no longer be available.
    for (auto dialog : dialog_buttons) {
        if (dialog->skill_id != 0xffffffff) {
            SendDialog(dialog->dialog_id);
            return 1;
        }
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
