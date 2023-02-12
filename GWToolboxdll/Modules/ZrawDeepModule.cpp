#include "stdafx.h"

#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Player.h>
#include <GWCA/GameEntities/Agent.h>

#include <GWCA/Constants/AgentIDs.h>

#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>

#include <Defines.h>
#include <Logger.h>
#include <Timer.h>

#include <Modules/ZrawDeepModule.h>
#include <Modules/Resources.h>

namespace {
    const wchar_t* kanaxai_dialogs[] = {
        // Room 1-4 no dialog
        L"\x5336\xBEB8\x8555\x7267", // Room 5 "Fear not the darkness. It is already within you."
        L"\x5337\xAA3A\xE96F\x3E34", // Room 6 "Is it comforting to know the source of your fears? Or do you fear more now that you see them in front of you."
        // Room 7 no dialog
        L"\x5338\xFD69\xA162\x3A04", // Room 8 "Even if you banish me from your sight, I will remain in your mind."
        // Room 9 no dialog
        L"\x5339\xA7BA\xC67B\x5D81", // Room 10 "You mortals may be here to defeat me, but acknowledging my presence only makes the nightmare grow stronger."
        // Room 11 no dialog
        L"\x533A\xED06\x815D\x5FFB", // Room 12 "So, you have passed through the depths of the Jade Sea, and into the nightmare realm. It is too bad that I must send you back from whence you came."
        L"\x533B\xCAA6\xFDA9\x3277", // Room 13 "I am Kanaxai, creator of nightmares. Let me make yours into reality."
        L"\x533C\xDD33\xA330\x4E27", // Room 14 "I will fill your hearts with visions of horror and despair that will haunt you for all of your days."
        L"\x533D\x9EB1\x8BEE\x2637",     // Kanaxai "What gives you the right to enter my lair? I shall kill you for your audacity, after I destroy your mind with my horrifying visions, of course."
        0
    };
    const wchar_t* kanaxai_audio_filenames[] = {
        L"kanaxai\\room5.mp3",
        L"kanaxai\\room6.mp3",
        L"kanaxai\\room8.mp3",
        L"kanaxai\\room10.mp3",
        L"kanaxai\\room12.mp3",
        L"kanaxai\\room13.mp3",
        L"kanaxai\\room14.mp3",
        L"kanaxai\\kanaxai.mp3"
    };

    Mp3* mp3 = nullptr;

    bool enabled = false;
    bool transmo_team = true;
    bool rewrite_npc_dialogs = true;
    bool kanaxais_true_form = true;

    clock_t pending_transmog = 0;
    bool can_terminate = true;
    bool terminating = false;

    const wchar_t* GetRandomKanaxaiDialog() {
        return kanaxai_dialogs[rand() % 8];
    }
    void SetToRandomKanaxaiString(wchar_t* current) {
        wmemset(current, 0, wcslen(current));
        wcscpy(current, GetRandomKanaxaiDialog());
    }
    const bool IsKanaxai(uint32_t agent_type_or_player_number = 0) {
        if (agent_type_or_player_number & 0x20000000)
            agent_type_or_player_number = (agent_type_or_player_number ^ 0x20000000);
        return agent_type_or_player_number == GW::Constants::ModelID::Deep::Kanaxai;
    }
    const bool IsKanaxai(GW::Agent* agent) {
        GW::AgentLiving* a = agent ? agent->GetAsAgentLiving() : nullptr;
        return a && IsKanaxai(a->player_number);
    }

    static uint32_t kanaxai_agent_id = 0;
    const bool IsKanaxaiTransformed() {
        if (!kanaxai_agent_id) return false;
        GW::AgentLiving* kanaxai = (GW::AgentLiving*)GW::Agents::GetAgentByID(kanaxai_agent_id);
        if (!IsKanaxai(kanaxai)) return false;
        return (kanaxai->transmog_npc_id ^ 0x20000000) == GW::Constants::ModelID::Minipet::Gwen;
    }
    const bool IsWholePartyTransformed() {
        GW::PartyInfo* p = GW::PartyMgr::GetPartyInfo();
        GW::PlayerArray* players = p ? GW::Agents::GetPlayerArray() : nullptr;
        if (!players) return false;
        for (auto& player : p->players) {
            if (!player.login_number || player.login_number >= players->size()) continue;
            GW::Player* p2 = &players->at(player.login_number);
            if (!p2) continue;
            GW::AgentLiving* pa = (GW::AgentLiving*)GW::Agents::GetAgentByID(p2->agent_id);
            if (pa && pa->GetIsLivingType() && (pa->transmog_npc_id ^ 0x20000000) != GW::Constants::ModelID::Minipet::Kanaxai)
                return false;
        }
        return true;
    }
    void CmdDeep24h(const wchar_t* , int , LPWSTR* ) {
        ZrawDeepModule::Instance().SetEnabled(!enabled);
        Log::Info(enabled ? "24h Deep mode on!" : "24h Deep mode off :(");
    }
}
void ZrawDeepModule::SetEnabled(bool _enabled) {
    if (!terminating)
        enabled = _enabled;
    if (_enabled) {
        can_terminate = false;
        GW::StoC::RegisterPacketCallback<GW::Packet::StoC::DisplayDialogue>(&ZrawDeepModule_StoCs,
            [this](GW::HookStatus* status, GW::Packet::StoC::DisplayDialogue* packet) -> void {
                UNREFERENCED_PARAMETER(status);
                DisplayDialogue(packet);
            });
        GW::StoC::RegisterPacketCallback<GW::Packet::StoC::SpeechBubble>(&ZrawDeepModule_StoCs,
            [this](GW::HookStatus* status, GW::Packet::StoC::SpeechBubble* packet) -> void {
                UNREFERENCED_PARAMETER(status);
                if (!rewrite_npc_dialogs) return;
                SetToRandomKanaxaiString(packet->message);
            });
        GW::StoC::RegisterPacketCallback<GW::Packet::StoC::DialogBody>(&ZrawDeepModule_StoCs,
            [this](GW::HookStatus* status, GW::Packet::StoC::DialogBody* packet) -> void {
                UNREFERENCED_PARAMETER(status);
                if (!rewrite_npc_dialogs) return;
                SetToRandomKanaxaiString(packet->message);
            });
        GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GameSrvTransfer>(&ZrawDeepModule_StoCs,
            [this](GW::HookStatus* status, GW::Packet::StoC::GameSrvTransfer*) -> void {
                UNREFERENCED_PARAMETER(status);
                kanaxai_agent_id = 0;
            });
        GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentAdd>(&ZrawDeepModule_StoCs,
            [this](GW::HookStatus* status, GW::Packet::StoC::AgentAdd* packet) -> void {
                UNREFERENCED_PARAMETER(status);
                if (!enabled) return;
                if (IsKanaxai(packet->agent_type)) {
                    kanaxai_agent_id = packet->agent_id;
                    pending_transmog = clock();
                }
            });
        GW::StoC::RegisterPacketCallback<GW::Packet::StoC::DisplayCape>(&ZrawDeepModule_StoCs,
            [this](GW::HookStatus* status, GW::Packet::StoC::DisplayCape* packet) -> void {
                if (!enabled) return;
                GW::AgentLiving* a = (GW::AgentLiving*)GW::Agents::GetAgentByID(packet->agent_id);
                if (!a || !a->GetIsLivingType()) return;
                if (a->IsPlayer() || a->GetCanBeViewedInPartyWindow() || IsKanaxai(a)) {
                    status->blocked = true;
                    pending_transmog = -500;
                }
            });
        GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentModel>(&ZrawDeepModule_StoCs,
            [this](GW::HookStatus* status, GW::Packet::StoC::AgentModel* packet) -> void {
                if (!enabled) return;
                GW::AgentLiving* a = (GW::AgentLiving*)GW::Agents::GetAgentByID(packet->agent_id);
                if (!a || !a->GetIsLivingType()) return;
                if (a->IsPlayer() || a->GetCanBeViewedInPartyWindow() || IsKanaxai(a)) {
                    status->blocked = true;
                    pending_transmog = -500;
                }
            });

        GW::AgentArray* agents = GW::Agents::GetAgentArray();
        if (agents) {
            for (auto* agent : *agents) {
                if (IsKanaxai(agent)) {
                    kanaxai_agent_id = agent->agent_id;
                    break;
                }
            }
        }
    }
    else {
        GW::StoC::RemoveCallback<GW::Packet::StoC::DisplayDialogue>(&ZrawDeepModule_StoCs);
        GW::StoC::RemoveCallback<GW::Packet::StoC::SpeechBubble>(&ZrawDeepModule_StoCs);
        GW::StoC::RemoveCallback<GW::Packet::StoC::DialogBody>(&ZrawDeepModule_StoCs);
        GW::StoC::RemoveCallback<GW::Packet::StoC::InstanceLoadInfo>(&ZrawDeepModule_StoCs);
        GW::StoC::RemoveCallback<GW::Packet::StoC::AgentAdd>(&ZrawDeepModule_StoCs);
        GW::StoC::RemoveCallback<GW::Packet::StoC::DisplayCape>(&ZrawDeepModule_StoCs);
        GW::StoC::RemoveCallback<GW::Packet::StoC::AgentModel>(&ZrawDeepModule_StoCs);
    }
    pending_transmog = -500;
}
void ZrawDeepModule::Terminate() {
    GW::Chat::DeleteCommand(L"deep24h");
    GW::Chat::DeleteCommand(L"24hdeep");
    SetEnabled(false);
    delete mp3;
    mp3 = nullptr;
    CoUninitialize();
}
bool ZrawDeepModule::CanTerminate() { return can_terminate; }
bool ZrawDeepModule::IsEnabled() { return enabled; }
bool ZrawDeepModule::HasSettings() { return enabled; }
void ZrawDeepModule::Initialize() {
    ToolboxModule::Initialize();
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    SetEnabled(enabled);
    GW::Chat::CreateCommand(L"deep24h", CmdDeep24h);
    GW::Chat::CreateCommand(L"24hdeep", CmdDeep24h);
}
void ZrawDeepModule::DrawSettingInternal() {
    ImGui::TextDisabled("Use chat command /deep24h to toggle this module on or off at any time");
    if(ImGui::Checkbox("Kanaxai makes you and your team stunningly attractive",&transmo_team))
        pending_transmog = -500;
    ImGui::Checkbox("Kanaxai infiltrates the minds of NPCs",&rewrite_npc_dialogs);
    if(ImGui::Checkbox("Kanaxai shows his true form", &kanaxais_true_form))
        pending_transmog = -500;
}
void ZrawDeepModule::SetTransmogs() {
    if (!GW::Map::GetIsMapLoaded())
        return;
    if (!GW::PartyMgr::GetIsPartyLoaded())
        return;
    if (pending_transmog == 0 || TIMER_DIFF(pending_transmog) < 500)
        return;
    pending_transmog = 0;
    const bool transmo_kanaxai_ = !terminating && enabled && kanaxais_true_form;
    const bool transmo_team_ = !terminating && enabled && transmo_team;
    if (transmo_team_) {
        if(!IsWholePartyTransformed())
            GW::Chat::SendChat('/', "transmoparty kanaxai 34");
    }
    else {
        GW::Chat::SendChat('/', "transmoparty reset");
    }
    if (transmo_kanaxai_) {
        if (kanaxai_agent_id && !IsKanaxaiTransformed()) {
            char buf[128] = { 0 };
            snprintf(buf, 128, "transmoagent %d gwenpre", kanaxai_agent_id);
            GW::Chat::SendChat('/', buf);
        }
    }
    else {
        if (kanaxai_agent_id && IsKanaxaiTransformed()) {
            char buf[128] = { 0 };
            snprintf(buf, 128, "transmoagent %d reset", kanaxai_agent_id);
            GW::Chat::SendChat('/', buf);
        }
    }
    if (!can_terminate && !transmo_kanaxai_ && !transmo_team_) {
        GW::GameThread::Enqueue([this]() {
            can_terminate = true;
            });
    }
}
void ZrawDeepModule::Update(float) {
    if (pending_transmog)
        SetTransmogs();
}
void ZrawDeepModule::SignalTerminate() {
    terminating = true;
    SetEnabled(false);
}
void ZrawDeepModule::SaveSettings(ToolboxIni* ini) {
    ToolboxModule::SaveSettings(ini);

    ini->SetBoolValue(Name(), VAR_NAME(enabled), enabled);
    ini->SetBoolValue(Name(), VAR_NAME(transmo_team), transmo_team);
    ini->SetBoolValue(Name(), VAR_NAME(rewrite_npc_dialogs), rewrite_npc_dialogs);
    ini->SetBoolValue(Name(), VAR_NAME(kanaxais_true_form), kanaxais_true_form);
}
void ZrawDeepModule::LoadSettings(ToolboxIni* ini) {
    ToolboxModule::LoadSettings(ini);

    enabled = ini->GetBoolValue(Name(), VAR_NAME(enabled), enabled);
    transmo_team = ini->GetBoolValue(Name(), VAR_NAME(transmo_team), transmo_team);
    rewrite_npc_dialogs = ini->GetBoolValue(Name(), VAR_NAME(rewrite_npc_dialogs), rewrite_npc_dialogs);
    kanaxais_true_form = ini->GetBoolValue(Name(), VAR_NAME(kanaxais_true_form), kanaxais_true_form);
}
void ZrawDeepModule::DisplayDialogue(GW::Packet::StoC::DisplayDialogue* packet) {
    for (uint8_t i = 0; kanaxai_dialogs[i] != 0; i++) {
        if (wmemcmp(packet->message, kanaxai_dialogs[i], 4) == 0)
            return PlayKanaxaiDialog(i);
    }
    // Not kanaxai? make it so!
    if(rewrite_npc_dialogs)
        SetToRandomKanaxaiString(packet->message);
}
void ZrawDeepModule::PlayKanaxaiDialog(uint8_t idx) {
    if (!mp3)
        mp3 = new Mp3();
    if (!mp3->Load(Resources::GetPath(kanaxai_audio_filenames[idx]).c_str()))
        return;
    mp3->Play();
}
