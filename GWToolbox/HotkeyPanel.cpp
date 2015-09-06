#include "HotkeyPanel.h"
#include "logger.h"
#include "GWToolbox.h"
#include "Config.h"

HotkeyPanel::HotkeyPanel() {
	clickerTimer = TBTimer::init();
	dropCoinsTimer = TBTimer::init();
	hotkeys = vector<TBHotkey*>();
}

void HotkeyPanel::buildUI() {
	LOG("Building Hotkey Panel\n");
	const int height = 300;
	loadIni();

	ScrollBar* scrollbar = new ScrollBar();
	scrollbar->SetLocation(TBHotkey::WIDTH + 2 * DefaultBorderPadding, 0);
	scrollbar->SetSize(scrollbar->GetWidth(), height);
	AddControl(scrollbar);

	SetSize(TBHotkey::WIDTH + 2 * DefaultBorderPadding + scrollbar->GetWidth(), height);

	for (size_t i = 0; i < hotkeys.size(); ++i) {
		hotkeys[i]->SetLocation(DefaultBorderPadding, 
			DefaultBorderPadding + i * (TBHotkey::HEIGHT + 10));
		AddControl(hotkeys[i]);
	}
}

bool HotkeyPanel::ProcessMessage(LPMSG msg) {
	switch (msg->message) {
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN: {
		Key modifier = Key::None;
		if (GetKeyState(static_cast<int>(Key::ControlKey)) < 0)
			modifier |= Key::Control;
		if (GetKeyState(static_cast<int>(Key::ShiftKey)) < 0)
			modifier |= Key::Shift;
		if (GetKeyState(static_cast<int>(Key::Menu)) < 0)
			modifier |= Key::Alt;

		Key keyData = (Key)msg->wParam;

		bool triggered = false;
		for (TBHotkey* hk : hotkeys) {
			if (!hk->pressed() && keyData == hk->key() && modifier == hk->modifier()) {
				hk->set_pressed(true);
				hk->exec();
				triggered = true;
			}
		}
		return triggered;
	}

	case WM_KEYUP:
	case WM_SYSKEYUP: {
		Key keyData = (Key)msg->wParam;
		for (TBHotkey* hk : hotkeys) {
			if (hk->pressed() && keyData == hk->key()) {
				hk->set_pressed(false);
			}
		}
		return false;
	}

	default:
		return false;
	}
}


void HotkeyPanel::loadIni() {
	Config* config = GWToolbox::instance()->config();

	list<wstring> sections = config->iniReadSections();
	for (wstring section : sections) {
		if (section.compare(0, 6, L"hotkey") == 0) {
			size_t first_sep = 6;
			size_t second_sep = section.find(L':', first_sep);

			wstring id = section.substr(first_sep + 1, second_sep - first_sep - 1);
			wstring type = section.substr(second_sep + 1);
			//wstring wname = config->iniRead(section.c_str(), L"name", L"");
			//string name = string(wname.begin(), wname.end()); // transform wstring in string
			bool active = config->iniReadBool(section.c_str(), TBHotkey::IniKeyActive(), false);
			Key key = (Key)config->iniReadLong(section.c_str(), TBHotkey::IniKeyHotkey(), 0);
			Key modifier = (Key)config->iniReadLong(section.c_str(), TBHotkey::IniKeyModifier(), 0);
			TBHotkey* tb_hk = NULL;

			if (type.compare(HotkeySendChat::IniSection()) == 0) {
				wstring msg = config->iniRead(section.c_str(), HotkeySendChat::IniKeyMsg(), L"");
				wchar_t channel = config->iniRead(section.c_str(), HotkeySendChat::IniKeyChannel(), L"")[0];
				if (channel) {
					tb_hk = new HotkeySendChat(key, modifier, active, section, msg, channel);
				}

			} else if (type.compare(HotkeyUseItem::IniSection()) == 0) {
				UINT ItemID = (UINT)config->iniReadLong(section.c_str(), HotkeyUseItem::IniItemIDKey(), 0);
				wstring ItemName = config->iniRead(section.c_str(), HotkeyUseItem::IniItemNameKey(), L"<some item>");
				if (ItemID > 0) {
					tb_hk = new HotkeyUseItem(key, modifier, active, section, ItemID, ItemName);
				}

			} else if (type.compare(HotkeyDropUseBuff::IniSection()) == 0) {
				UINT SkillID = (UINT)config->iniReadLong(section.c_str(), HotkeyDropUseBuff::IniSkillIDKey(), 0);
				if (SkillID > 0) {
					tb_hk = new HotkeyDropUseBuff(key, modifier, active, section, SkillID);
				}

			} else if (type.compare(HotkeyToggle::IniSection()) == 0) {
				int ToggleID = (int)config->iniReadLong(section.c_str(), HotkeyToggle::IniToggleIDKey(), 0);
				if (ToggleID > 0) {
					tb_hk = new HotkeyToggle(key, modifier, active, section, ToggleID);
				}

			} else if (type.compare(HotkeyTarget::IniSection()) == 0) {
				UINT TargetID = (UINT)config->iniReadLong(section.c_str(), HotkeyTarget::IniTargetID(), 0);
				if (TargetID > 0) {
					tb_hk = new HotkeyTarget(key, modifier, active, section, TargetID);
				}

			} else if (type.compare(HotkeyMove::IniSection()) == 0) {
				float x = (float)config->iniReadDouble(section.c_str(), HotkeyMove::IniXKey(), 0.0);
				float y = (float)config->iniReadDouble(section.c_str(), HotkeyMove::IniYKey(), 0.0);
				if (x != 0.0 || y != 0.0) {
					tb_hk = new HotkeyMove(key, modifier, active, section, x, y);
				}

			} else if (type.compare(HotkeyDialog::IniSection()) == 0) {
				UINT DialogID = (UINT)config->iniReadLong(section.c_str(), HotkeyDialog::IniDialogIDKey(), 0);
				if (DialogID > 0) {
					tb_hk = new HotkeyDialog(key, modifier, active, section, DialogID);
				}

			} else if (type.compare(HotkeyPingBuild::IniSection()) == 0) {
				UINT index = (UINT)config->iniReadLong(section.c_str(), HotkeyPingBuild::IniBuildIdxKey(), 0);
				if (index > 0) {
					tb_hk = new HotkeyPingBuild(key, modifier, active, section, index);
				}
			} else {
				LOG("WARNING hotkey detected, but could not match any type!\n");
			}

			if (tb_hk) {
				hotkeys.push_back(tb_hk);
			}
		}
	}
}

void HotkeyPanel::mainRoutine() {

}
