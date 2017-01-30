#include "Settings.h"

#include <GWCA\Utilities\MemoryPatcher.h>
#include <GWCA\Managers\ChatMgr.h>

#include "ChatLogger.h"
#include "GWToolbox.h"
#include "SettingManager.h"

Setting::Setting(const wchar_t* inisection_,
	const wchar_t* inikey_,
	const char* text_,
	const char* tooltip_) :
	inisection(inisection_),
	inikey(inikey_),
	text(text_),
	tooltip(tooltip_) {
	SettingManager::Register(this);
}

void SettingBool::Load() {
	value = Config::IniReadBool(MainWindow::IniSection(), inikey, default_value);
}
void SettingBool::Save() const {
	Config::IniWriteBool(MainWindow::IniSection(), inikey, value);
}

void FreezeWidgets::Apply() {
	GWToolbox::instance().bonds_window().SetFreze(value);
	GWToolbox::instance().party_damage().SetFreeze(value);
	GWToolbox::instance().minimap().SetFreeze(value);
}

void AdjustOnResize::Apply() {
	GWToolbox::instance().set_adjust_on_resize(value);
}

void NoBackgroundWidgets::Apply() {
	GWToolbox::instance().bonds_window().SetTransparentBackColor(value);
	GWToolbox::instance().party_damage().SetTransparentBackColor(value);
}

void OpenTemplateLinks::Apply() {
	GW::Chat().SetOpenLinks(value);
}

BorderlessWindow::BorderlessWindow(const wchar_t* inisection)
	: SettingBool(false, inisection, L"borderlesswindow") {
	text = "Borderless window mode";
	tooltip = "Shows fullscreen but acts as a window";

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

void BorderlessWindow::Apply() {
	DWORD current_style = GetWindowLong(GW::MemoryMgr::GetGWWindowHandle(), GWL_STYLE);

	if ((current_style & WS_POPUP) != 0) { // borderless or fullscreen
		if ((current_style & WS_MAXIMIZEBOX) == 0) { // fullscreen
			if (value) {
				ChatLogger::Log(L"Please enable Borderless while in Windowed mode");
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
