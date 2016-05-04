#include <algorithm>
#include <Windows.h>

#include <GWCA\ItemMgr.h>

#include "HotkeyPanel.h"
#include "logger.h"
#include "GWToolbox.h"
#include "Config.h"

HotkeyPanel::HotkeyPanel(OSHGui::Control* parent) : ToolboxPanel(parent) {
	clickerTimer = TBTimer::init();
	dropCoinsTimer = TBTimer::init();
	hotkeys = vector<TBHotkey*>();
}

void HotkeyPanel::BuildUI() {
	Config& config = GWToolbox::instance().config();
	
	create_combo_ = new ComboBox(this);
	create_combo_->SetText(L"Create Hotkey");
	create_combo_->AddItem(L"Send Chat");		// 0
	create_combo_->AddItem(L"Use Item");			// 1
	create_combo_->AddItem(L"Drop or Use Buff");	// 2
	create_combo_->AddItem(L"Toggle...");		// 3
	create_combo_->AddItem(L"Execute...");		// 4
	create_combo_->AddItem(L"Target");			// 5
	create_combo_->AddItem(L"Move to");			// 6
	create_combo_->AddItem(L"Dialog");			// 7
	create_combo_->AddItem(L"Ping Build");		// 8
	create_combo_->SetMaxShowItems(create_combo_->GetItemsCount());
	create_combo_->SetLocation(PointI(Padding, Padding));
	create_combo_->SetSize(SizeI(GuiUtils::ComputeWidth(GetWidth(), 2), GuiUtils::BUTTON_HEIGHT));
	create_combo_->GetSelectedIndexChangedEvent() += SelectedIndexChangedEventHandler(
		[&](Control*) {
		if (create_combo_->GetSelectedIndex() < 0) return;
		wstring ini = L"hotkey-";
		ini += to_wstring(this->NewID());
		ini += L":";
		switch (create_combo_->GetSelectedIndex()) {
		case 0:
			ini += HotkeySendChat::IniSection();
			AddHotkey(new HotkeySendChat(scroll_panel_->GetContainer(), Key::None, Key::None, true, ini, L"", L'/'));
			break;
		case 1:
			ini += HotkeyUseItem::IniSection();
			AddHotkey(new HotkeyUseItem(scroll_panel_->GetContainer(), Key::None, Key::None, true, ini, 0, L""));
			break;
		case 2:
			ini += HotkeyDropUseBuff::IniSection();
			AddHotkey(new HotkeyDropUseBuff(scroll_panel_->GetContainer(), Key::None, Key::None, true, ini, GwConstants::SkillID::Recall));
			break;
		case 3:
			ini += HotkeyToggle::IniSection();
			AddHotkey(new HotkeyToggle(scroll_panel_->GetContainer(), Key::None, Key::None, true, ini, 1));
			break;
		case 4:
			ini += HotkeyAction::IniSection();
			AddHotkey(new HotkeyAction(scroll_panel_->GetContainer(), Key::None, Key::None, true, ini, 0));
			break;
		case 5:
			ini += HotkeyTarget::IniSection();
			AddHotkey(new HotkeyTarget(scroll_panel_->GetContainer(), Key::None, Key::None, true, ini, 0, L""));
			break;
		case 6:
			ini += HotkeyMove::IniSection();
			AddHotkey(new HotkeyMove(scroll_panel_->GetContainer(), Key::None, Key::None, true, ini, 0.0, 0.0, L""));
			break;
		case 7:
			ini += HotkeyDialog::IniSection();
			AddHotkey(new HotkeyDialog(scroll_panel_->GetContainer(), Key::None, Key::None, true, ini, 0, L""));
			break;
		case 8:
			ini += HotkeyPingBuild::IniSection();
			AddHotkey(new HotkeyPingBuild(scroll_panel_->GetContainer(), Key::None, Key::None, true, ini, 0));
			break;
		default:
			break;
		}
		create_combo_->SetText(L"Create Hotkey");
		create_combo_->SetSelectedIndex(-1);
	});
	AddControl(create_combo_);

	delete_combo_ = new ComboBox(this);
	delete_combo_->SetText(L"Delete Hotkey");
	delete_combo_->SetSize(SizeI(GuiUtils::ComputeWidth(GetWidth(), 2), GuiUtils::BUTTON_HEIGHT));
	delete_combo_->SetLocation(PointI(create_combo_->GetRight() + Padding, Padding));
	delete_combo_->GetSelectedIndexChangedEvent() += SelectedIndexChangedEventHandler(
		[this](Control*) {
		int index = delete_combo_->GetSelectedIndex();
		if (index < 0) return;
		DeleteHotkey(index);
	});
	AddControl(delete_combo_);

	scroll_panel_ = new ScrollPanel(this);
	scroll_panel_->SetLocation(PointI(0, create_combo_->GetBottom() + Padding));
	scroll_panel_->SetSize(SizeI(GetWidth(), GetHeight() - create_combo_->GetHeight() - 2 * Padding));
	scroll_panel_->GetContainer()->SetBackColor(Color::Empty());
	AddControl(scroll_panel_);

	max_id_ = 0;
	list<wstring> sections = config.IniReadSections();
	for (wstring section : sections) {
		if (section.compare(0, 6, L"hotkey") == 0) {
			size_t first_sep = 6;
			size_t second_sep = section.find(L':', first_sep);

			wstring id = section.substr(first_sep + 1, second_sep - first_sep - 1);
			try {
				long long_id = stol(id);
				if (long_id > max_id_) max_id_ = long_id;
			} catch (...) {}
			wstring type = section.substr(second_sep + 1);
			bool active = config.IniReadBool(section.c_str(), TBHotkey::IniKeyActive(), true);
			Key key = (Key)config.IniReadLong(section.c_str(), TBHotkey::IniKeyHotkey(), 0);
			Key modifier = (Key)config.IniReadLong(section.c_str(), TBHotkey::IniKeyModifier(), 0);
			TBHotkey* tb_hk = NULL;

			if (type.compare(HotkeySendChat::IniSection()) == 0) {
				wstring msg = config.IniRead(section.c_str(), HotkeySendChat::IniKeyMsg(), L"");
				wchar_t channel = config.IniRead(section.c_str(), HotkeySendChat::IniKeyChannel(), L"/")[0];
				tb_hk = new HotkeySendChat(scroll_panel_->GetContainer(), key, modifier, active, section, msg, channel);

			} else if (type.compare(HotkeyUseItem::IniSection()) == 0) {
				UINT itemID = (UINT)config.IniReadLong(section.c_str(), HotkeyUseItem::IniKeyItemID(), 0);
				wstring item_name = config.IniRead(section.c_str(), HotkeyUseItem::IniKeyItemName(), L"");
				tb_hk = new HotkeyUseItem(scroll_panel_->GetContainer(), key, modifier, active, section, itemID, item_name);

			} else if (type.compare(HotkeyDropUseBuff::IniSection()) == 0) {
				long skillID = config.IniReadLong(section.c_str(), HotkeyDropUseBuff::IniKeySkillID(),
					static_cast<long>(GwConstants::SkillID::Recall));
				GwConstants::SkillID id = static_cast<GwConstants::SkillID>(skillID);
				tb_hk = new HotkeyDropUseBuff(scroll_panel_->GetContainer(), key, modifier, active, section, id);

			} else if (type.compare(HotkeyToggle::IniSection()) == 0) {
				long toggleID = config.IniReadLong(section.c_str(), HotkeyToggle::IniKeyToggleID(), 0);
				tb_hk = new HotkeyToggle(scroll_panel_->GetContainer(), key, modifier, active, section, toggleID);

			} else if (type.compare(HotkeyAction::IniSection()) == 0) {
				long actionID = config.IniReadLong(section.c_str(), HotkeyAction::IniKeyActionID(), 0);
				tb_hk = new HotkeyAction(scroll_panel_->GetContainer(), key, modifier, active, section, actionID);

			} else if (type.compare(HotkeyTarget::IniSection()) == 0) {
				UINT targetID = (UINT)config.IniReadLong(section.c_str(), HotkeyTarget::IniKeyTargetID(), 0);
				wstring target_name = config.IniRead(section.c_str(), HotkeyTarget::IniKeyTargetName(), L"");
				tb_hk = new HotkeyTarget(scroll_panel_->GetContainer(), key, modifier, active, section, targetID, target_name);

			} else if (type.compare(HotkeyMove::IniSection()) == 0) {
				float x = (float)config.IniReadDouble(section.c_str(), HotkeyMove::IniKeyX(), 0.0);
				float y = (float)config.IniReadDouble(section.c_str(), HotkeyMove::IniKeyY(), 0.0);
				wstring name = config.IniRead(section.c_str(), HotkeyMove::IniKeyName(), L"");
				tb_hk = new HotkeyMove(scroll_panel_->GetContainer(), key, modifier, active, section, x, y, name);

			} else if (type.compare(HotkeyDialog::IniSection()) == 0) {
				UINT dialogID = (UINT)config.IniReadLong(section.c_str(), HotkeyDialog::IniKeyDialogID(), 0);
				wstring dialog_name = config.IniRead(section.c_str(), HotkeyDialog::IniKeyDialogName(), L"");
				tb_hk = new HotkeyDialog(scroll_panel_->GetContainer(), key, modifier, active, section, dialogID, dialog_name);

			} else if (type.compare(HotkeyPingBuild::IniSection()) == 0) {
				UINT index = (UINT)config.IniReadLong(section.c_str(), HotkeyPingBuild::IniKeyBuildIndex(), 0);
				tb_hk = new HotkeyPingBuild(scroll_panel_->GetContainer(), key, modifier, active, section, index);

			} else {
				LOG("WARNING hotkey detected, but could not match any type!\n");
			}

			if (tb_hk) {
				hotkeys.push_back(tb_hk);
				scroll_panel_->AddControl(tb_hk);
			}
		}
	}

	
	UpdateScrollBarMax();
	CalculateHotkeyPositions();
	UpdateDeleteCombo();
}

void HotkeyPanel::UpdateDeleteCombo() {
	delete_combo_->SetSelectedIndex(-1);
	delete_combo_->Clear();
	delete_combo_->SetText(L"Delete Hotkey");
	for (int i = 0; i < (int)hotkeys.size(); ++i) {
		delete_combo_->AddItem(hotkeys[i]->GetDescription());
	}
	delete_combo_->SetSelectedIndex(-1);
}

void HotkeyPanel::UpdateScrollBarMax() {
	scroll_panel_->SetInternalHeight((TBHotkey::HEIGHT + Padding / 2) * hotkeys.size());
}

void HotkeyPanel::DeleteHotkey(int index) {
	if (index < 0 || index >= (int)hotkeys.size()) return;

	GWToolbox::instance().config().IniDeleteSection(hotkeys[index]->ini_section().c_str());
	scroll_panel_->RemoveControl(hotkeys[index]);
	hotkeys.erase(hotkeys.begin() + index);
	UpdateScrollBarMax();
	CalculateHotkeyPositions();
	UpdateDeleteCombo();
}

void HotkeyPanel::AddHotkey(TBHotkey* hotkey) {
	hotkeys.push_back(hotkey);
	scroll_panel_->AddControl(hotkey);
	UpdateScrollBarMax();
	scroll_panel_->ScrollToBottom();
	CalculateHotkeyPositions();
	UpdateDeleteCombo();
}

void HotkeyPanel::CalculateHotkeyPositions() {
	for (int i = 0; i < static_cast<int>(hotkeys.size()); ++i) {
		hotkeys[i]->SetLocation(PointI(Padding, i * (TBHotkey::HEIGHT + Padding / 2)));
	}
}

bool HotkeyPanel::ProcessMessage(LPMSG msg) {
	Key keyData = Key::None;
	switch (msg->message) {
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
	case WM_KEYUP:
	case WM_SYSKEYUP:
		keyData = (Key)msg->wParam;
		break;
	case WM_XBUTTONDOWN:
	case WM_MBUTTONDOWN:
		if (LOWORD(msg->wParam) & MK_MBUTTON) keyData = Key::MButton;
		if (LOWORD(msg->wParam) & MK_XBUTTON1) keyData = Key::XButton1;
		if (LOWORD(msg->wParam) & MK_XBUTTON2) keyData = Key::XButton2;
		break;
	case WM_XBUTTONUP:
	case WM_MBUTTONUP:
		// leave keydata to none, need to handle special case below
		break;
	default:
		break;
	}

	switch (msg->message) {
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
	case WM_XBUTTONDOWN:
	case WM_MBUTTONDOWN: {
		Key modifier = Key::None;
		if (GetKeyState(static_cast<int>(Key::ControlKey)) < 0)
			modifier |= Key::Control;
		if (GetKeyState(static_cast<int>(Key::ShiftKey)) < 0)
			modifier |= Key::Shift;
		if (GetKeyState(static_cast<int>(Key::Menu)) < 0)
			modifier |= Key::Alt;

		bool triggered = false;
		for (TBHotkey* hk : hotkeys) {
			if (hk->active() 
				&& !hk->pressed() && keyData == hk->key() 
				&& modifier == hk->modifier()) {

				hk->set_pressed(true);
				hk->exec();
				triggered = true;
			}
		}
		return triggered;
	}

	case WM_KEYUP:
	case WM_SYSKEYUP:
		for (TBHotkey* hk : hotkeys) {
			if (hk->pressed() && keyData == hk->key()) {
				hk->set_pressed(false);
			}
		}
		return false;

	case WM_XBUTTONUP:
		for (TBHotkey* hk : hotkeys) {
			if (hk->pressed() && (hk->key() == Key::XButton1 || hk->key() == Key::XButton2)) {
				hk->set_pressed(false);
			}
		}
		return false;
	case WM_MBUTTONUP:
		for (TBHotkey* hk : hotkeys) {
			if (hk->pressed() && hk->key() == Key::MButton) {
				hk->set_pressed(false);
			}
		}
	default:
		return false;
	}
}


void HotkeyPanel::MainRoutine() {
	if (clickerActive && TBTimer::diff(clickerTimer) > 20) {
		clickerTimer = TBTimer::init();
		INPUT input;
		input.type = INPUT_MOUSE;
		input.mi.dx = 0;
		input.mi.dy = 0;
		input.mi.mouseData = 0;
		input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP;
		input.mi.time = 0;
		input.mi.dwExtraInfo = NULL;

		SendInput(1, &input, sizeof(INPUT));
	}

	if (dropCoinsActive && TBTimer::diff(dropCoinsTimer) > 500) {
		if (GWCA::Map().GetInstanceType() == GwConstants::InstanceType::Explorable) {
			dropCoinsTimer = TBTimer::init();
			GWCA::Items().DropGold(1);
		}
	}

	// TODO rupt?
}
