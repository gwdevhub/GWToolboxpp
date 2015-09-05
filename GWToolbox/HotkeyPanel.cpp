#include "HotkeyPanel.h"

#include "GWToolbox.h"
#include "Config.h"

HotkeyPanel::HotkeyPanel() {
	clickerTimer = TBTimer::init();
	dropCoinsTimer = TBTimer::init();
	hotkeys = vector<TBHotkey*>();
}

void HotkeyPanel::buildUI() {
	loadIni();

	SetSize(400, 300);

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
			bool active = config->iniReadBool(section.c_str(), L"active", false);
			long key = config->iniReadLong(section.c_str(), L"hotkey", 0);
			TBHotkey* tb_hk = NULL;

			if (type.compare(HotkeySendChat::iniSection()) == 0) {
				wstring msg = config->iniRead(section.c_str(), HotkeySendChat::iniKeyMsg(), L"");
				wchar_t channel = config->iniRead(section.c_str(), HotkeySendChat::iniKeyChannel(), L"")[0];
				if (!msg.empty() && channel) {
					tb_hk = new HotkeySendChat(key, active, msg, channel);
				}

			} else if (type.compare(HotkeyUseItem::iniSection()) == 0) {
				UINT ItemID = config->iniReadLong(section.c_str(), HotkeyUseItem::iniItemIDKey(), 0);
				wstring ItemName = config->iniRead(section.c_str(), HotkeyUseItem::iniItemNameKey(), L"<some item>");
				if (ItemID > 0) {
					tb_hk = new HotkeyUseItem(key, active, ItemID, ItemName);
				}

			} else if (type.compare(HotkeyDropUseBuff::iniSection()) == 0) {
				UINT SkillID = config->iniReadLong(section.c_str(), HotkeyDropUseBuff::iniSkillIDKey(), 0);
				if (SkillID > 0) {
					tb_hk = new HotkeyDropUseBuff(key, active, SkillID);
				}

			} else if (type.compare(HotkeyToggle::iniSection()) == 0) {
				UINT ToggleID = config->iniReadLong(section.c_str(), HotkeyToggle::iniToggleIDKey(), 0);
				if (ToggleID > 0) {
					tb_hk = new HotkeyToggle(key, active, ToggleID);
				}

			} else if (type.compare(HotkeyTarget::iniSection()) == 0) {
				UINT TargetID = config->iniReadLong(section.c_str(), HotkeyTarget::iniTargetID(), 0);
				if (TargetID > 0) {
					tb_hk = new HotkeyTarget(key, active, TargetID);
				}

			} else if (type.compare(HotkeyMove::iniSection()) == 0) {
				float x = (float)config->iniReadDouble(section.c_str(), HotkeyMove::iniXKey(), 0.0);
				float y = (float)config->iniReadDouble(section.c_str(), HotkeyMove::iniYKey(), 0.0);
				if (x != 0.0 || y != 0.0) {
					tb_hk = new HotkeyMove(key, active, x, y);
				}

			} else if (type.compare(HotkeyDialog::iniSection()) == 0) {
				UINT DialogID = config->iniReadLong(section.c_str(), HotkeyDialog::iniDialogIDKey(), 0);
				if (DialogID > 0) {
					tb_hk = new HotkeyDialog(key, active, DialogID);
				}

			} else if (type.compare(HotkeyPingBuild::iniSection()) == 0) {
				UINT index = config->iniReadLong(section.c_str(), HotkeyPingBuild::iniBuildIdxKey(), 0);
				if (index > 0) {
					tb_hk = new HotkeyPingBuild(key, active, index);
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
