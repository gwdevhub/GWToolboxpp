#include "GameSettings.h"

#include <GWCA\Managers\ChatMgr.h>
#include <GWCA\Managers\PartyMgr.h>
#include <GWCA\Managers\AgentMgr.h>
#include <GWCA\Managers\StoCMgr.h>
#include <GWCA\Managers\FriendListMgr.h>
#include <GWCA\Managers\Render.h>

#include <GWCA\Context\GameContext.h>

#include <logger.h>
#include "GuiUtils.h"
#include <GWToolbox.h>
#include <Timer.h>
#include <Color.h>

namespace {
	void ChatEventCallback(DWORD id, DWORD type, wchar_t* info, void* unk) {
		if (type == 0x29 && GameSettings::Instance().select_with_chat_doubleclick) {
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

	void SendChatCallback(GW::Chat::Channel chan, wchar_t msg[120]) {
		if (!GameSettings::Instance().auto_url || !msg) return;
		size_t len = wcslen(msg);
		size_t max_len = 120;

		if (chan == GW::Chat::CHANNEL_WHISPER) {
			// msg == "Whisper Target Name,msg"
			size_t i;
			for (i = 0; i < len; i++)
				if (msg[i] == ',')
					break;

			if (i < len) {
				msg += i + 1;
				len -= i + 1;
				max_len -= i + 1;
			}
		}

		if (wcsncmp(msg, L"http://", 7) && wcsncmp(msg, L"https://", 8)) return;

		if (len + 5 < max_len) {
			for (int i = len; i > 0; i--)
				msg[i] = msg[i - 1];
			msg[0] = '[';
			msg[len + 1] = ';';
			msg[len + 2] = 'x';
			msg[len + 3] = 'x';
			msg[len + 4] = ']';
			msg[len + 5] = 0;
		}
	}

	void FlashWindow() {
		FLASHWINFO flashInfo = { 0 };
		flashInfo.cbSize = sizeof(FLASHWINFO);
		flashInfo.hwnd = GW::MemoryMgr::GetGWWindowHandle();
		flashInfo.dwFlags = FLASHW_TIMER | FLASHW_TRAY | FLASHW_TIMERNOFG;
		flashInfo.uCount = 0;
		flashInfo.dwTimeout = 0;
		FlashWindowEx(&flashInfo);
	}

	void WhisperCallback(const wchar_t from[20], const wchar_t msg[140]) {
		if (GameSettings::Instance().flash_window_on_pm) FlashWindow();
	}
}

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

	GW::Chat::CreateCommand(L"borderless",
		[&](int argc, LPWSTR *argv) {
		if (argc <= 1) {
			ApplyBorderless(!borderlesswindow);
		} else {
			std::wstring arg1 = GuiUtils::ToLower(argv[1]);
			if (arg1 == L"on") {
				ApplyBorderless(true);
			} else if (arg1 == L"off") {
				ApplyBorderless(false);
			} else {
				Log::Error("Invalid argument '%ls', please use /borderless [|on|off]", argv[1]);
			}
		}
	});

	GW::StoC::AddCallback<GW::Packet::StoC::P444>(
		[](GW::Packet::StoC::P444*) -> bool {
		if (GameSettings::Instance().flash_window_on_party_invite) FlashWindow();
		return false;
	});

	GW::StoC::AddCallback<GW::Packet::StoC::P406_GameSrvTransfer>(
		[](GW::Packet::StoC::P406_GameSrvTransfer *pak) -> bool {

		GW::CharContext *ctx = GW::GameContext::instance()->character;
		if (GameSettings::Instance().flash_window_on_zoning) FlashWindow();
		if (GameSettings::Instance().focus_window_on_zoning && pak->is_explorable) {
			HWND hwnd = GW::MemoryMgr::GetGWWindowHandle();
			SetForegroundWindow(hwnd);
			ShowWindow(hwnd, SW_RESTORE);
		}

		return false;
	});
}

void GameSettings::LoadSettings(CSimpleIni* ini) {
	ToolboxModule::LoadSettings(ini);
	borderlesswindow = ini->GetBoolValue(Name(), VAR_NAME(borderlesswindow), false);
	tick_is_toggle = ini->GetBoolValue(Name(), VAR_NAME(tick_is_toggle), true);

	GW::Chat::ShowTimestamps = ini->GetBoolValue(Name(), "show_timestamps", false);
	GW::Chat::TimestampsColor = Colors::Load(ini, Name(), "timestamps_color", Colors::White());
	GW::Chat::KeepChatHistory = ini->GetBoolValue(Name(), "keep_chat_history", true);

	openlinks = ini->GetBoolValue(Name(), VAR_NAME(openlinks), true);
	auto_url = ini->GetBoolValue(Name(), VAR_NAME(auto_url), true);
	select_with_chat_doubleclick = ini->GetBoolValue(Name(), VAR_NAME(select_with_chat_doubleclick), true);

	flash_window_on_pm = ini->GetBoolValue(Name(), VAR_NAME(flash_window_on_pm), true);
	flash_window_on_party_invite = ini->GetBoolValue(Name(), VAR_NAME(flash_window_on_party_invite), true);
	flash_window_on_zoning = ini->GetBoolValue(Name(), VAR_NAME(flash_window_on_zoning), true);
	focus_window_on_zoning = ini->GetBoolValue(Name(), VAR_NAME(focus_window_on_zoning), false);

	auto_set_away = ini->GetBoolValue(Name(), VAR_NAME(auto_set_away), false);
	auto_set_away_delay = ini->GetLongValue(Name(), VAR_NAME(auto_set_away_delay), 10);
	auto_set_online = ini->GetBoolValue(Name(), VAR_NAME(auto_set_online), false);

	if (borderlesswindow) ApplyBorderless(borderlesswindow);
	if (openlinks) GW::Chat::SetOpenLinks(openlinks);
	if (tick_is_toggle) GW::PartyMgr::SetTickToggle();
	if (select_with_chat_doubleclick) GW::Chat::SetChatEventCallback(&ChatEventCallback);
	if (auto_url) GW::Chat::SetSendChatCallback(&SendChatCallback);
	if (flash_window_on_pm) GW::Chat::SetWhisperCallback(&WhisperCallback);
}

void GameSettings::SaveSettings(CSimpleIni* ini) {
	ToolboxModule::SaveSettings(ini);
	ini->SetBoolValue(Name(), VAR_NAME(borderlesswindow), borderlesswindow);
	ini->SetBoolValue(Name(), VAR_NAME(tick_is_toggle), tick_is_toggle);

	ini->SetBoolValue(Name(), "show_timestamps", GW::Chat::ShowTimestamps);
	Colors::Save(ini, Name(), "timestamps_color", GW::Chat::TimestampsColor);
	ini->SetBoolValue(Name(), "keep_chat_history", GW::Chat::KeepChatHistory);

	ini->SetBoolValue(Name(), VAR_NAME(openlinks), openlinks);
	ini->SetBoolValue(Name(), VAR_NAME(auto_url), auto_url);
	ini->SetBoolValue(Name(), VAR_NAME(select_with_chat_doubleclick), select_with_chat_doubleclick);

	ini->SetBoolValue(Name(), VAR_NAME(flash_window_on_pm), flash_window_on_pm);
	ini->SetBoolValue(Name(), VAR_NAME(flash_window_on_party_invite), flash_window_on_party_invite);
	ini->SetBoolValue(Name(), VAR_NAME(flash_window_on_zoning), flash_window_on_zoning);
	ini->SetBoolValue(Name(), VAR_NAME(focus_window_on_zoning), focus_window_on_zoning);

	ini->SetBoolValue(Name(), VAR_NAME(auto_set_away), auto_set_away);
	ini->SetLongValue(Name(), VAR_NAME(auto_set_away_delay), auto_set_away_delay);
	ini->SetBoolValue(Name(), VAR_NAME(auto_set_online), auto_set_online);
}

void GameSettings::DrawSettingInternal() {
	DrawBorderlessSetting();

	ImGui::Checkbox("Show chat messages timestamp. Color:", &GW::Chat::ShowTimestamps);
	ImGui::SameLine();
	
	ImVec4 col = ImGui::ColorConvertU32ToFloat4(GW::Chat::TimestampsColor);
	if (ImGui::ColorEdit4("Color:", &col.x, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_PickerHueWheel)) {
		GW::Chat::TimestampsColor = ImGui::ColorConvertFloat4ToU32(col);
	}
	ImGui::ShowHelp("Show timestamps in message history.");

	ImGui::Checkbox("Keep chat history.", &GW::Chat::KeepChatHistory);
	ImGui::ShowHelp("Messages in the chat do not disappear on character change.");

	if (ImGui::Checkbox("Open web links from templates", &openlinks)) {
		GW::Chat::SetOpenLinks(openlinks);
	}
	ImGui::ShowHelp("Clicking on template that has a URL as name will open that URL in your browser");

	if (ImGui::Checkbox("Automatically change urls into build templates.", &openlinks)) {
		GW::Chat::SetSendChatCallback(&SendChatCallback);
	}
	ImGui::ShowHelp("When you write a message starting with 'http://' or 'https://', it will be converted in template format");

	if (ImGui::Checkbox("Tick is a toggle", &tick_is_toggle)) {
		if (tick_is_toggle) {
			GW::PartyMgr::SetTickToggle();
		} else {
			GW::PartyMgr::RestoreTickToggle();
		}
	}
	ImGui::ShowHelp("Ticking in party window will work as a toggle instead of opening the menu");

	if (ImGui::Checkbox("Target with double-click on message author", &select_with_chat_doubleclick)) {
		GW::Chat::SetChatEventCallback(&ChatEventCallback);
	}
	ImGui::ShowHelp("Double clicking on the author of a message in chat will target the author");

	ImGui::Text("Flash Guild Wars taskbar icon when:");
	ImGui::Indent();
	ImGui::ShowHelp("Only triggers when Guild Wars is not the active window");
	if (ImGui::Checkbox("Receiving a private message", &flash_window_on_pm)) {
		GW::Chat::SetWhisperCallback(&WhisperCallback);
	}
	ImGui::Checkbox("Receiving a party invite", &flash_window_on_party_invite);
	ImGui::Checkbox("Zoning in a new map", &flash_window_on_zoning);
	ImGui::Unindent();

	ImGui::Checkbox("Allow window restore", &focus_window_on_zoning);
	ImGui::ShowHelp("When enabled, GWToolbox++ can automatically restore\n"
					"the window from a minimized state when important events\n"
					"occur, such as entering instances.");

	ImGui::Checkbox("Automatically set 'Away' after ", &auto_set_away);
	ImGui::SameLine();
	ImGui::PushItemWidth(50);
	ImGui::InputInt("##awaydelay", &auto_set_away_delay, 0);
	ImGui::PopItemWidth();
	ImGui::SameLine();
	ImGui::Text("minutes of inactivity");
	ImGui::ShowHelp("Only if you were 'Online'");

	ImGui::Checkbox("Automatically set 'Online' after an input to Guild Wars", &auto_set_online);
	ImGui::ShowHelp("Only if you were 'Away'");
}

void GameSettings::DrawBorderlessSetting() {
	if (ImGui::Checkbox("Borderless Window", &borderlesswindow)) {
		ApplyBorderless(borderlesswindow);
	}
}

void GameSettings::ApplyBorderless(bool borderless) {
	if (borderless) {
		borderless_status = WantBorderless;
	} else {
		borderless_status = WantWindowed;
	}
}

void GameSettings::Update(float delta) {
	if (auto_set_away 
		&& TIMER_DIFF(activity_timer) > auto_set_away_delay * 60000
		&& GW::FriendListMgr::GetMyStatus() == (DWORD)GW::Constants::OnlineStatus::ONLINE) {
		GW::FriendListMgr::SetFriendListStatus(GW::Constants::OnlineStatus::AWAY);
		activity_timer = TIMER_INIT(); // refresh the timer to avoid spamming in case the set status call fails
	}

	UpdateBorderless();
}

void GameSettings::UpdateBorderless() {

	// ==== Check if we have something to do ====
	bool borderless = false;
	switch (borderless_status) {
	case GameSettings::Ok:										return; // nothing to do
	case GameSettings::WantBorderless:	borderless = true;		break;
	case GameSettings::WantWindowed:	borderless = false;		break;
	}

	// ==== Check fullscreen status allows it ====
	switch (GW::Render::GetIsFullscreen()) {
	case -1:
		// Unknown status. Probably has been minimized since toolbox launch. Wait.
		// leave borderless_status as-is
		return;

	case 0:
		// windowed or borderless windowed
		break; // proceed

	case 1:
		// fullscreen
		Log::Info("Please enable Borderless while in Windowed mode");
		borderless_status = Ok;
		return;

	default:
		return; // should never happen
	}

	// ==== Either borderless or windowed, find out which and warn the user if it's a useless operation ====
	RECT windowRect, desktopRect;
	if (GetWindowRect(GW::MemoryMgr::GetGWWindowHandle(), &windowRect)
		&& GetWindowRect(GetDesktopWindow(), &desktopRect)) {

		if (windowRect.left == desktopRect.left
			&& windowRect.right == desktopRect.right
			&& windowRect.top == desktopRect.top
			&& windowRect.bottom == desktopRect.bottom) {

			//borderless
			if (borderless_status == WantBorderless) {
				//trying to activate borderless while already in borderless
				Log::Info("Already in Borderless mode");
			}
		} else if (windowRect.left >= desktopRect.left
			&& windowRect.right <= desktopRect.right
			&& windowRect.top >= desktopRect.top
			&& windowRect.bottom <= desktopRect.bottom) {

			// windowed
			if (borderless_status == WantWindowed) {
				//trying to deactivate borderless in window mode
				Log::Info("Already in Window mode");
			}
		}
	}

	DWORD current_style = GetWindowLong(GW::MemoryMgr::GetGWWindowHandle(), GWL_STYLE);
	DWORD new_style = borderless ? WS_POPUP | WS_VISIBLE | WS_MAXIMIZEBOX |WS_MINIMIZEBOX | WS_CLIPSIBLINGS
		: WS_SIZEBOX | WS_SYSMENU | WS_CAPTION | WS_VISIBLE | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_CLIPSIBLINGS;

	//printf("old 0x%X, new 0x%X\n", current_style, new_style);
	//printf("popup: old=%X, new=%X\n", current_style & WS_POPUP, new_style & WS_POPUP);

	for (GW::MemoryPatcher* patch : patches) patch->TooglePatch(borderless);

	SetWindowLong(GW::MemoryMgr::GetGWWindowHandle(), GWL_STYLE, new_style);

	if (borderless) {
		int width = GetSystemMetrics(SM_CXSCREEN);
		int height = GetSystemMetrics(SM_CYSCREEN);
		MoveWindow(GW::MemoryMgr::GetGWWindowHandle(), 0, 0, width, height, TRUE);
	} else {
		RECT size;
		SystemParametersInfoW(SPI_GETWORKAREA, 0, &size, 0);
		MoveWindow(GW::MemoryMgr::GetGWWindowHandle(), size.top, size.left, size.right, size.bottom, TRUE);
	}

	borderlesswindow = borderless;
	borderless_status = Ok;
}

bool GameSettings::WndProc(UINT Message, WPARAM wParam, LPARAM lParam) {

	activity_timer = TIMER_INIT();
	static clock_t set_online_timer = TIMER_INIT();
	if (auto_set_online
		&& TIMER_DIFF(set_online_timer) > 5000 // to avoid spamming in case of failure
		&& GW::FriendListMgr::GetMyStatus() == (DWORD)GW::Constants::OnlineStatus::AWAY) {
		GW::FriendListMgr::SetFriendListStatus(GW::Constants::OnlineStatus::ONLINE);
		set_online_timer = TIMER_INIT();
	}

	return false;
}
