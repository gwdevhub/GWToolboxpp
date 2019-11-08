#include "stdafx.h"

#include <GWCA/Managers/StoCMgr.h>

#include "FunModule.h"
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

void FunModule::Initialize() {
	ToolboxModule::Initialize();
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	GW::StoC::RegisterPacketCallback<GW::Packet::StoC::DisplayDialogue>(&DisplayDialogue_Entry,
		[this](GW::HookStatus* status, GW::Packet::StoC::DisplayDialogue* packet) -> void {
			DisplayDialogue(packet);
		});
}
void FunModule::Terminate() {
	GW::StoC::RemoveCallback<GW::Packet::StoC::DisplayDialogue>(&DisplayDialogue_Entry);
	if (mp3) delete mp3;
	CoUninitialize();
}
void FunModule::DisplayDialogue(GW::Packet::StoC::DisplayDialogue* packet) {
	for (uint8_t i = 0; kanaxai_dialogs[i] != 0; i++) {
		if (wmemcmp(packet->message, kanaxai_dialogs[i], 4) == 0)
			return PlayKanaxaiDialog(i);
	}
}
void FunModule::PlayKanaxaiDialog(uint8_t idx) {
	//Log::Log("Kanaxai dialog %d, %ls", idx, Resources::GetPath(kanaxai_audio_filenames[idx]).c_str());
	if (!mp3)
		mp3 = new Mp3();
	if (!mp3->Load(Resources::GetPath(kanaxai_audio_filenames[idx]).c_str()))
		return;
	mp3->Play();
}