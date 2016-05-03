#pragma once

#include <string>
#include <OSHGui\OSHGui.hpp>
#include <GWCA\ChatMgr.h>
#include <GWCA\MemoryMgr.h>
#include <GWCA\MemoryPatcher.h>

#include "GWToolbox.h"
#include "MainWindow.h"

class BoolSetting : public OSHGui::CheckBox {
public:
	BoolSetting(OSHGui::Control* parent, bool default_value, const wchar_t* inikey) 
		: CheckBox(parent), default_value_(default_value), inikey_(inikey) {}

	virtual void ApplySetting(bool value) = 0;

	bool ReadSetting() const {
		return GWToolbox::instance().config().IniReadBool(MainWindow::IniSection(), inikey_, default_value_);
	}

	void WriteSetting(bool value) {
		GWToolbox::instance().config().IniWriteBool(MainWindow::IniSection(), inikey_, value);
	}

protected:
	void Initialize(std::wstring text) {
		SetText(text);
		SetChecked(ReadSetting());
		GetCheckedChangedEvent() += CheckedChangedEventHandler([&](Control*) {
			WriteSetting(GetChecked());
			ApplySetting(GetChecked());
		});
	}

private:
	const bool default_value_;
	const wchar_t* inikey_;
};

class OpenTabsLeft : public BoolSetting {
public:
	OpenTabsLeft(OSHGui::Control* parent) : BoolSetting(parent, false, L"tabsleft") {
		Initialize(L"Open tabs on the left"); }

	void ApplySetting(bool value) override {
		GWToolbox::instance().main_window().SetPanelPositions(value);
	}
};

class FreezeWidgets : public BoolSetting {
public:
	FreezeWidgets(OSHGui::Control* parent) : BoolSetting(parent, false, L"freeze_widgets") {
		Initialize(L"Freeze info widget positions"); }

	void ApplySetting(bool value) override {
		GWToolbox::instance().timer_window().SetFreeze(value);
		GWToolbox::instance().bonds_window().SetFreze(value);
		GWToolbox::instance().health_window().SetFreeze(value);
		GWToolbox::instance().distance_window().SetFreeze(value);
		GWToolbox::instance().party_damage().SetFreeze(value);
	}
};

class HideTargetWidgets : public BoolSetting {
public:
	HideTargetWidgets(OSHGui::Control* parent) : BoolSetting(parent, true, L"hide_target") {
		Initialize(L"Hide target windows"); }

	void ApplySetting(bool value) override {
		GWToolbox::instance().health_window().SetHideTarget(value);
		GWToolbox::instance().distance_window().SetHideTarget(value);
	}
};

class MinimizeToAltPos : public BoolSetting {
public:
	MinimizeToAltPos(OSHGui::Control* parent) : BoolSetting(parent, false, L"minimize_alt_position") {
		Initialize(L"Minimize to different position"); }

	void ApplySetting(bool value) override {
		GWToolbox::instance().main_window().set_use_minimized_alt_pos(value);
	}
};

class TickWithPcons : public BoolSetting {
public:
	TickWithPcons(OSHGui::Control* parent) : BoolSetting(parent, true, L"minimize_alt_position") {
		Initialize(L"Tick with pcon status");
	}

	void ApplySetting(bool value) override {
		GWToolbox::instance().main_window().set_tick_with_pcons(value);
	}
};

class SaveLocationData : public BoolSetting {
public:
	SaveLocationData(OSHGui::Control* parent) : BoolSetting(parent, false, L"save_location") {
		Initialize(L"Save location data");
	}

	void ApplySetting(bool value) override {
		GWToolbox::instance().main_window().settings_panel().SetSaveLocationData(value);
	}
};

class OpenTemplateLinks : public BoolSetting {
public:
	OpenTemplateLinks(OSHGui::Control* parent) : BoolSetting(parent, true, L"openlinks") {
		Initialize(L"Open web links from templates");
	}

	void ApplySetting(bool value) override {
		GWCA::Chat().SetOpenLinks(value);
	}
};

class AdjustOnResize : public BoolSetting {
public:
	AdjustOnResize(OSHGui::Control* parent) : BoolSetting(parent, false, L"adjustresize") {
		Initialize(L"Adjust positions on window resize");
	}

	void ApplySetting(bool value) override {
		GWToolbox::instance().set_adjust_on_resize(value);
	}
};

class SuppressMessages : public BoolSetting {
public:
	SuppressMessages(OSHGui::Control* parent) : BoolSetting(parent, false, L"suppressmessages") {
		Initialize(L"Suppress drop messages");
	}

	void ApplySetting(bool value) override {
		GWToolbox::instance().chat_commands().SetSuppressMessages(value);
	}
};

class BorderlessWindow : public BoolSetting {
public:
	BorderlessWindow(OSHGui::Control* parent) : BoolSetting(parent, false, L"borderlesswindow") {
		Initialize(L"Borderless fullscreen window");

		patches.push_back(new GWCA::MemoryPatcher((void*)0x0067D7C8, 
			(BYTE*)"\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 16));
		patches.push_back(new GWCA::MemoryPatcher((void*)0x0067D320, (BYTE*)"\xEB", 1));
		patches.push_back(new GWCA::MemoryPatcher((void*)0x0067D33D, (BYTE*)"\xEB", 1));
		
		BYTE* a = (BYTE*)"\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90";
		patches.push_back(new GWCA::MemoryPatcher((void*)0x00669846, a, 10));
		patches.push_back(new GWCA::MemoryPatcher((void*)0x00669892, a, 10));
		patches.push_back(new GWCA::MemoryPatcher((void*)0x006698CE, a, 10));
		patches.push_back(new GWCA::MemoryPatcher((void*)0x0067D5D6, a, 10));
		patches.push_back(new GWCA::MemoryPatcher((void*)0x0067D622, a, 10));
		patches.push_back(new GWCA::MemoryPatcher((void*)0x0067D65E, a, 10));
	}
	void ApplySetting(bool value) {
		for (GWCA::MemoryPatcher* patch : patches) patch->TooglePatch(value);

		DWORD style;
		if (value) style = WS_POPUP | WS_VISIBLE | WS_MINIMIZEBOX;
		else style = WS_SIZEBOX | WS_SYSMENU | WS_CAPTION | WS_VISIBLE | WS_MAXIMIZEBOX | WS_MINIMIZEBOX;
		SetWindowLong(GWCA::MemoryMgr::GetGWWindowHandle(), GWL_STYLE, style);

		if (value) {
			int width = GetSystemMetrics(SM_CXSCREEN);
			int height = GetSystemMetrics(SM_CYSCREEN);
			MoveWindow(GWCA::MemoryMgr::GetGWWindowHandle(), 0, 0, width, height, TRUE);
		} else {
			RECT size;
			SystemParametersInfoW(SPI_GETWORKAREA, 0, &size, 0);
			MoveWindow(GWCA::MemoryMgr::GetGWWindowHandle(), size.top, size.left, size.right, size.bottom, TRUE);
		}
	}

private:
	std::vector<GWCA::MemoryPatcher*> patches;
};
