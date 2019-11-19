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

static const wchar_t* kanaxai_audio_filenames[] = {
	L"kanaxai\\room5.mp3",
	L"kanaxai\\room6.mp3",
	L"kanaxai\\room8.mp3",
	L"kanaxai\\room10.mp3",
	L"kanaxai\\room12.mp3",
	L"kanaxai\\room13.mp3",
	L"kanaxai\\room14.mp3",
	L"kanaxai\\kanaxai.mp3"
};
static const wchar_t* kanaxai_dialogs[] = {
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
void ZrawDeepModule::Initialize() {
	ToolboxModule::Initialize();
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	GW::StoC::RegisterPacketCallback<GW::Packet::StoC::DisplayDialogue>(&ZrawDeepModule_StoCs,
		[this](GW::HookStatus* status, GW::Packet::StoC::DisplayDialogue* packet) -> void {
			if (!valid_for_map) return;
			DisplayDialogue(packet);
		});
	GW::StoC::RegisterPacketCallback<GW::Packet::StoC::InstanceLoadInfo>(&ZrawDeepModule_StoCs,
		[this](GW::HookStatus* status, GW::Packet::StoC::InstanceLoadInfo* packet) -> void {
			Reset();
		});
	GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GuildGeneral>(&ZrawDeepModule_StoCs,
		[this](GW::HookStatus* status, GW::Packet::StoC::GuildGeneral* packet) -> void {
			Reset();
		});
	/*GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentAdd>(&ZrawDeepModule_StoCs,
		[this](GW::HookStatus* status, GW::Packet::StoC::AgentAdd* packet) -> void {
			if (!valid_for_map) return;
			if(packet->agent_type & 0x30000000)
				pending_transmog = clock();
		});*/
	GW::StoC::RegisterPacketCallback<GW::Packet::StoC::DisplayCape>(&ZrawDeepModule_StoCs,
		[this](GW::HookStatus* status, GW::Packet::StoC::DisplayCape* packet) -> void {
			if (!valid_for_map) return;
			GW::Agent* a = GW::Agents::GetAgentByID(packet->agent_id);
			if (a->IsPlayer() || a->GetCanBeViewedInPartyWindow())
				status->blocked = true;
			pending_transmog = clock();
		});
	GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentModel>(&ZrawDeepModule_StoCs,
		[this](GW::HookStatus* status, GW::Packet::StoC::AgentModel* packet) -> void {
			if (!valid_for_map) return;
			GW::Agent* a = GW::Agents::GetAgentByID(packet->agent_id);
			if (a->IsPlayer() || a->GetCanBeViewedInPartyWindow())
				status->blocked = true;
			pending_transmog = clock();
		});
	/*GW::StoC::RegisterPacketCallback<GW::Packet::StoC::InstanceLoadInfo>(&ZrawDeepModule_StoCs,
		[this](GW::HookStatus* status, GW::Packet::StoC::InstanceLoadInfo* packet) -> void {
			if (!valid_for_map) return;
			pending_transmog = true;
		});*/
}
void ZrawDeepModule::SetValidForMap(bool v) {
	if (v == valid_for_map)
		return;
	valid_for_map = v;
	GW::GameThread::Enqueue([this]() {
		Log::Info(valid_for_map ? "24h Deep Active!" : "24h Deep Inactive :(");
		});
	if (valid_for_map && GW::Map::GetIsMapLoaded())
		pending_transmog = clock();
}
void ZrawDeepModule::SetTransmogs(bool enabled) {
	if (!GW::Map::GetIsMapLoaded())
		return;
	if (!GW::PartyMgr::GetIsPartyLoaded())
		return;
	//if (!pending_transmog || clock() - pending_transmog < (clock_t)(CLOCKS_PER_SEC * 5))
	//	return; // 2s delay.
	pending_transmog = 0;
	if (!enabled)
		GW::Chat::SendChat('/', "transmoparty reset");
	else
		GW::Chat::SendChat('/', "transmoparty kanaxai 34");
}
void ZrawDeepModule::Update(float delta) {
	if(pending_transmog)
		SetTransmogs(valid_for_map);
	if (checked_enabled)
		return;
	if (disabled) {
		SetValidForMap(false);
		checked_enabled = true;
		return;
	}
	if(GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading)
		return;
	if (!GW::PartyMgr::GetIsPartyLoaded())
		return;
	if (GW::Map::GetMapID() != GW::Constants::MapID::The_Deep) {
		SetValidForMap(false);
		checked_enabled = true;
		return;
	}
	GW::PartyInfo* party = GW::PartyMgr::GetPartyInfo();
	if (!party)
		return;
	GW::GuildArray guilds = GW::GuildMgr::GetGuildArray();
	if (!guilds.valid())
		return;
	bool found_guild = false;
	for (auto guild : guilds) {
		if (!guild) continue;
		if (found_guild || wcscmp(guild->tag, L"Zraw") == 0) found_guild = true;
		if (found_guild || wcscmp(guild->tag, L"SNOW") == 0) found_guild = true;
		if (found_guild || wcscmp(guild->tag, L"IT") == 0) found_guild = true;
	}
	SetValidForMap(found_guild);
	checked_enabled = true;
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
}
void ZrawDeepModule::PlayKanaxaiDialog(uint8_t idx) {
	if (!mp3)
		mp3 = new Mp3();
	if (!mp3->Load(Resources::GetPath(kanaxai_audio_filenames[idx]).c_str()))
		return;
	mp3->Play();
}