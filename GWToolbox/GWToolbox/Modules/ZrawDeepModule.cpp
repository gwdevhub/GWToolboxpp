#include "stdafx.h"

#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Player.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Guild.h>

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Maps.h>

#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/GuildMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>

#include "ZrawDeepModule.h"
#include "Modules/Resources.h"

#include "logger.h"

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
        L"\x533D\x9EB1\x8BEE\x2637",	 // Kanaxai "What gives you the right to enter my lair? I shall kill you for your audacity, after I destroy your mind with my horrifying visions, of course."
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
    const wchar_t* GetRandomKanaxaiDialog() {
        return kanaxai_dialogs[rand() % 8];
    }
    void SetToRandomKanaxaiString(wchar_t* current) {
        wmemset(current, 0, wcslen(current));
        wcscpy(current, GetRandomKanaxaiDialog());
    }
}
void ZrawDeepModule::Initialize() {
	ToolboxModule::Initialize();
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	GW::StoC::RegisterPacketCallback<GW::Packet::StoC::DisplayDialogue>(&ZrawDeepModule_StoCs,
		[this](GW::HookStatus* status, GW::Packet::StoC::DisplayDialogue* packet) -> void {
            if (!enabled) return;
			DisplayDialogue(packet);
		});
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::SpeechBubble>(&ZrawDeepModule_StoCs,
        [this](GW::HookStatus* status, GW::Packet::StoC::SpeechBubble* packet) -> void {
            if (!enabled || !rewrite_npc_dialogs) return;
            SetToRandomKanaxaiString(packet->message);
        });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::DialogBody>(&ZrawDeepModule_StoCs,
        [this](GW::HookStatus* status, GW::Packet::StoC::DialogBody* packet) -> void {
            if (!enabled || !rewrite_npc_dialogs) return;
            SetToRandomKanaxaiString(packet->message);
        });
	GW::StoC::RegisterPacketCallback<GW::Packet::StoC::InstanceLoadInfo>(&ZrawDeepModule_StoCs,
		[this](GW::HookStatus* status, GW::Packet::StoC::InstanceLoadInfo* packet) -> void {
            if (!enabled) return;
			Reset();
		});
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentAdd>(&ZrawDeepModule_StoCs,
        [this](GW::HookStatus* status, GW::Packet::StoC::AgentAdd* packet) -> void {
            if (!enabled) return;
            Reset();
            if ((packet->agent_type ^ 0x20000000) == GW::Constants::ModelID::Deep::KanaxaiAspect) {
                kanaxai_agent_id = packet->agent_id;
                pending_transmog = clock();
            }            
        });
	GW::StoC::RegisterPacketCallback<GW::Packet::StoC::DisplayCape>(&ZrawDeepModule_StoCs,
		[this](GW::HookStatus* status, GW::Packet::StoC::DisplayCape* packet) -> void {
			if (!enabled) return;
			GW::Agent* a = GW::Agents::GetAgentByID(packet->agent_id);
			if (a->IsPlayer() || a->GetCanBeViewedInPartyWindow())
				status->blocked = true;
			pending_transmog = clock();
		});
	GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentModel>(&ZrawDeepModule_StoCs,
		[this](GW::HookStatus* status, GW::Packet::StoC::AgentModel* packet) -> void {
            if (!enabled) return;
			GW::Agent* a = GW::Agents::GetAgentByID(packet->agent_id);
            if (!a) return;
			if (a->IsPlayer() || a->GetCanBeViewedInPartyWindow() || a->player_number == GW::Constants::ModelID::Deep::KanaxaiAspect)
				status->blocked = true;
			pending_transmog = clock();
		});
    GW::Chat::CreateCommand(L"deep24h", [this](const wchar_t* message, int argc, LPWSTR* argv) -> void {
        Toggle();
        GW::GameThread::Enqueue([this]() {
            Log::Info(enabled ? "Zraw 24h Deep mode on!" : "Zraw 24h Deep mode off :(");
            });
        pending_transmog = clock();
        });
}
void ZrawDeepModule::DrawSettingInternal() {
    ImGui::TextDisabled("Use chat command /deep24h to toggle this module on or off at any time");
    if (ImGui::Checkbox("Enabled",&enabled)) {
        Reset();
        pending_transmog = clock();
    }
    if (!enabled)
        return;
    if(ImGui::Checkbox("Kanaxai gives your team visions in explorable areas",&transmo_team_in_explorable))
        pending_transmog = clock();
    ImGui::Checkbox("Kanaxai infiltrates the minds of NPCs",&rewrite_npc_dialogs);
    if(ImGui::Checkbox("Kanaxai shows his true form", &kanaxais_true_form))
        pending_transmog = clock();
}
void ZrawDeepModule::SetTransmogs() {
	if (!GW::Map::GetIsMapLoaded())
		return;
	if (!GW::PartyMgr::GetIsPartyLoaded())
		return;
    if (clock() - CLOCKS_PER_SEC < pending_transmog)
        return;
	pending_transmog = 0;
    if (!enabled)
        GW::Chat::SendChat('/', "transmoparty reset");
    else
	    GW::Chat::SendChat('/', "transmoparty kanaxai 34");
    if (kanaxai_agent_id) {
        GW::Agent* kanaxai = GW::Agents::GetAgentByID(kanaxai_agent_id);
        if (kanaxai && kanaxai->player_number == GW::Constants::ModelID::Deep::KanaxaiAspect) {
            if (kanaxais_true_form && (kanaxai->transmog_npc_id ^ 0x20000000) != GW::Constants::ModelID::Minipet::GwenDoll) {
                char buf[128] = { 0 };
                snprintf(buf, 128, "transmoagent %d gwenchan", kanaxai_agent_id);
                GW::Chat::SendChat('/', buf);
            }
            if (!kanaxais_true_form && (kanaxai->transmog_npc_id ^ 0x20000000) == GW::Constants::ModelID::Minipet::GwenDoll) {
                char buf[128] = { 0 };
                snprintf(buf, 128, "transmoagent %d reset", kanaxai_agent_id);
                GW::Chat::SendChat('/', buf);
            }
        }
    }
}
void ZrawDeepModule::Update(float delta) {
    if (pending_transmog)
        SetTransmogs();
}
void ZrawDeepModule::Terminate() {
	GW::StoC::RemoveCallback<GW::Packet::StoC::DisplayDialogue>(&ZrawDeepModule_StoCs);
	if (mp3) delete mp3;
	CoUninitialize();
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