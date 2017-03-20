#include "GameSettings.h"

#include <GWCA\Managers\ChatMgr.h>
#include <GWCA\Managers\PartyMgr.h>

#include <logger.h>
#include "GuiUtils.h"
#include <GWToolbox.h>
#include <Timer.h>

void GameSettings::Initialize() {
	ToolboxModule::Initialize();
	patches.push_back(new GW::MemoryPatcher((void*)0x0067D7C8,
		(BYTE*)"\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 16));
	patches.push_back(new GW::MemoryPatcher((void*)0x0067D320, (BYTE*)"\xEB", 1));
	patches.push_back(new GW::MemoryPatcher((void*)0x0067D33D, (BYTE*)"\xEB", 1));

	BYTE* a = (BYTE*)"\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90";
	patches.push_back(new GW::MemoryPatcher((void*)0x00669846, a, 10));
	patches.push_back(new GW::MemoryPatcher((void*)0x00669892, a, 10));
	patches.push_back(new GW::MemoryPatcher((void*)0x006698CE, a, 10));
	patches.push_back(new GW::MemoryPatcher((void*)0x0067D5D6, a, 10));
	patches.push_back(new GW::MemoryPatcher((void*)0x0067D622, a, 10));
	patches.push_back(new GW::MemoryPatcher((void*)0x0067D65E, a, 10));
}

void GameSettings::Terminate() {
	ToolboxModule::Terminate();
}

void GameSettings::ChatEvent(DWORD id, DWORD type, wchar_t* info, void* unk) {
	if (type == 0x29) {
		static wchar_t last_name[64] = L"";
		static clock_t timer = TIMER_INIT();
		if (TIMER_DIFF(timer) < 500 && wcscmp(last_name, info) == 0) {
			GW::PlayerArray players = GW::Agents::GetPlayerArray();
			if (players.valid()) {
				for (unsigned i = 0; i < players.size(); ++i) {
					GW::Player& player = players[i];
					wchar_t* name = player.Name;
					if (player.AgentID > 0
						&& name != nullptr
						&& wcscmp(info, name) == 0) {
						GW::Agents::ChangeTarget(players[i].AgentID);
					}
				}
			}
		} else {
			timer = TIMER_INIT();
			wcscpy_s(last_name, info);
		}
	}
}

void GameSettings::LoadSettings(CSimpleIni* ini) {
	ToolboxModule::LoadSettings(ini);
	borderless_window = ini->GetBoolValue(Name(), "borderlesswindow", false);
	open_template_links = ini->GetBoolValue(Name(), "openlinks", true);
	tick_is_toggle = ini->GetBoolValue(Name(), "tick_is_toggle", true);
	select_with_chat_doubleclick = ini->GetBoolValue(Name(), "select_with_chat_doubleclick", true);

	ApplyBorderless(borderless_window);
	if (open_template_links) GW::Chat::SetOpenLinks(open_template_links);
	if (tick_is_toggle) GW::PartyMgr::SetTickToggle();
	if (select_with_chat_doubleclick) GW::Chat::SetChatEventCallback(&ChatEvent);
}

void GameSettings::SaveSettings(CSimpleIni* ini) {
	ToolboxModule::SaveSettings(ini);
	ini->SetBoolValue(Name(), "borderlesswindow", borderless_window);
	ini->SetBoolValue(Name(), "openlinks", open_template_links);
	ini->SetBoolValue(Name(), "tick_is_toggle", tick_is_toggle);
	ini->SetBoolValue(Name(), "select_with_chat_doubleclick", select_with_chat_doubleclick);
}

void GameSettings::DrawSettingInternal() {
	DrawBorderlessSetting();

	if (ImGui::Checkbox("Open web links from templates", &open_template_links)) {
		GW::Chat::SetOpenLinks(open_template_links);
	}
	ImGui::ShowHelp("Clicking on template that has a URL as name will open that URL in your browser");

	if (ImGui::Checkbox("Tick is a toggle", &tick_is_toggle)) {
		if (tick_is_toggle) {
			GW::PartyMgr::SetTickToggle();
		} else {
			GW::PartyMgr::RestoreTickToggle();
		}
	}
	ImGui::ShowHelp("Ticking in party window will work as a toggle instead of opening the menu");

	if (ImGui::Checkbox("Target with double-click on message author", &select_with_chat_doubleclick)) {
		GW::Chat::SetChatEventCallback(select_with_chat_doubleclick ? 
			&ChatEvent : [](DWORD, DWORD, wchar_t*, void*) {});
	}
	ImGui::ShowHelp("Double clicking on the author of a message in chat will target the player");
}

void GameSettings::DrawBorderlessSetting() {
	if (ImGui::Checkbox("Borderless Window", &borderless_window)) {
		ApplyBorderless(borderless_window);
	}
}

void GameSettings::ApplyBorderless(bool value) {
	borderless_window = value;
	DWORD current_style = GetWindowLong(GW::MemoryMgr::GetGWWindowHandle(), GWL_STYLE);

	if ((current_style & WS_POPUP) != 0) { // borderless or fullscreen
		if ((current_style & WS_MAXIMIZEBOX) == 0) { // fullscreen
			if (value) {
				Log::Info("Please enable Borderless while in Windowed mode");
				value = false;
			}
			return;
		}
	}

	DWORD style = value ? WS_POPUP | WS_VISIBLE | WS_MINIMIZEBOX | WS_CLIPSIBLINGS | WS_MAXIMIZEBOX
		: WS_SIZEBOX | WS_SYSMENU | WS_CAPTION | WS_VISIBLE | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_CLIPSIBLINGS;

	//printf("old 0x%X, new 0x%X\n", current_style, style);

	if (current_style != style) {
		for (GW::MemoryPatcher* patch : patches) patch->TooglePatch(value);

		SetWindowLong(GW::MemoryMgr::GetGWWindowHandle(), GWL_STYLE, style);

		if (value) {
			int width = GetSystemMetrics(SM_CXSCREEN);
			int height = GetSystemMetrics(SM_CYSCREEN);
			MoveWindow(GW::MemoryMgr::GetGWWindowHandle(), 0, 0, width, height, TRUE);
		} else {
			RECT size;
			SystemParametersInfoW(SPI_GETWORKAREA, 0, &size, 0);
			MoveWindow(GW::MemoryMgr::GetGWWindowHandle(), size.top, size.left, size.right, size.bottom, TRUE);
		}
	}
}
