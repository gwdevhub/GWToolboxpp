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

	loadIni();

	SetSize(TBHotkey::WIDTH + 2*DefaultBorderPadding, 300);

	for (TBHotkey* hk : hotkeys) {
		AddControl(hk);
	}
}

bool HotkeyPanel::ProcessMessage(LPMSG msg) {
	switch (msg->message) {
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
	{
		bool triggered = false;
		for (TBHotkey* hk : hotkeys) {
			if (!hk->pressed() && msg->wParam == hk->key()) {
				hk->set_pressed(true);
				hk->exec();
				triggered = true;
			}
		}
		return triggered;
		break;
	}

	case WM_KEYUP:
	case WM_SYSKEYUP:
		for (TBHotkey* hk : hotkeys) {
			if (hk->pressed() && msg->wParam == hk->key()) {
				hk->set_pressed(false);
			}
		}
		return false;
		break;

	default:
		return false;
		break;
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
			long key = config->iniReadLong(section.c_str(), TBHotkey::IniKeyHotkey(), 0);
			TBHotkey* tb_hk = NULL;

			if (type.compare(HotkeySendChat::IniSection()) == 0) {
				wstring msg = config->iniRead(section.c_str(), HotkeySendChat::IniKeyMsg(), L"");
				wchar_t channel = config->iniRead(section.c_str(), HotkeySendChat::IniKeyChannel(), L"")[0];
				if (channel) {
					tb_hk = new HotkeySendChat(key, active, section, msg, channel);
				}

			} else if (type.compare(HotkeyUseItem::IniSection()) == 0) {
				UINT ItemID = config->iniReadLong(section.c_str(), HotkeyUseItem::IniItemIDKey(), 0);
				wstring ItemName = config->iniRead(section.c_str(), HotkeyUseItem::IniItemNameKey(), L"<some item>");
				if (ItemID > 0) {
					tb_hk = new HotkeyUseItem(key, active, section, ItemID, ItemName);
				}

			} else if (type.compare(HotkeyDropUseBuff::IniSection()) == 0) {
				UINT SkillID = config->iniReadLong(section.c_str(), HotkeyDropUseBuff::IniSkillIDKey(), 0);
				if (SkillID > 0) {
					tb_hk = new HotkeyDropUseBuff(key, active, section, SkillID);
				}

			} else if (type.compare(HotkeyToggle::IniSection()) == 0) {
				UINT ToggleID = config->iniReadLong(section.c_str(), HotkeyToggle::IniToggleIDKey(), 0);
				if (ToggleID > 0) {
					tb_hk = new HotkeyToggle(key, active, section, ToggleID);
				}

			} else if (type.compare(HotkeyTarget::IniSection()) == 0) {
				UINT TargetID = config->iniReadLong(section.c_str(), HotkeyTarget::IniTargetID(), 0);
				if (TargetID > 0) {
					tb_hk = new HotkeyTarget(key, active, section, TargetID);
				}

			} else if (type.compare(HotkeyMove::IniSection()) == 0) {
				float x = (float)config->iniReadDouble(section.c_str(), HotkeyMove::IniXKey(), 0.0);
				float y = (float)config->iniReadDouble(section.c_str(), HotkeyMove::IniYKey(), 0.0);
				if (x != 0.0 || y != 0.0) {
					tb_hk = new HotkeyMove(key, active, section, x, y);
				}

			} else if (type.compare(HotkeyDialog::IniSection()) == 0) {
				UINT DialogID = config->iniReadLong(section.c_str(), HotkeyDialog::IniDialogIDKey(), 0);
				if (DialogID > 0) {
					tb_hk = new HotkeyDialog(key, active, section, DialogID);
				}

			} else if (type.compare(HotkeyPingBuild::IniSection()) == 0) {
				UINT index = config->iniReadLong(section.c_str(), HotkeyPingBuild::IniBuildIdxKey(), 0);
				if (index > 0) {
					tb_hk = new HotkeyPingBuild(key, active, section, index);
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
