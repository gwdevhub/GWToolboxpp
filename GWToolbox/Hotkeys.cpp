#include "Hotkeys.h"

#include <iostream>
#include <sstream>

#include "logger.h"
#include "GWToolbox.h"
#include "Config.h"
#include "BuildPanel.h"

using namespace GWAPI;
using namespace OSHGui;

TBHotkey::TBHotkey(Key key, Key modifier, bool active, wstring ini_section)
	: active_(active), key_(key), modifier_(modifier), ini_section_(ini_section) {

	pressed_ = false;

	SetBackColor(Drawing::Color::Empty());
	SetSize(WIDTH, HEIGHT);

	CheckBox* checkbox = new CheckBox();
	checkbox->SetChecked(active);
	checkbox->SetText("");
	checkbox->SetLocation(HOTKEY_X, HOTKEY_Y + 5);
	checkbox->GetCheckedChangedEvent() += CheckedChangedEventHandler([this, checkbox, ini_section](Control*) {
		this->set_active(checkbox->GetChecked());
		GWToolbox::instance()->config()->iniWriteBool(ini_section.c_str(), TBHotkey::IniKeyActive(), checkbox->GetChecked());
	});
	AddControl(checkbox);

	HotkeyControl* hotkey_button = new HotkeyControl();
	hotkey_button->SetSize(WIDTH - checkbox->GetRight() - 60 - HSPACE * 2, LINE_HEIGHT);
	hotkey_button->SetLocation(checkbox->GetRight() + HSPACE, HOTKEY_Y);
	hotkey_button->SetHotkey(key);
	hotkey_button->SetHotkeyModifier(modifier);
	hotkey_button->GetFocusGotEvent() += FocusGotEventHandler([](Control*) {
		GWToolbox::instance()->set_capture_input(true);
	});
	hotkey_button->GetFocusLostEvent() += FocusLostEventHandler([](Control*, Control*) {
		GWToolbox::instance()->set_capture_input(false);
	});
	hotkey_button->GetHotkeyChangedEvent() += HotkeyChangedEventHandler(
		[this, hotkey_button, ini_section](Control*) {
		Key key = hotkey_button->GetHotkey();
		Key modifier = hotkey_button->GetHotkeyModifier();
		this->set_key(key);
		this->set_modifier(modifier);
		GWToolbox::instance()->config()->iniWriteLong(ini_section.c_str(),
			this->IniKeyHotkey(), (long)key);
		GWToolbox::instance()->config()->iniWriteLong(ini_section.c_str(),
			this->IniKeyModifier(), (long)modifier);
	});
	AddControl(hotkey_button);

	Button* run_button = new Button();
	run_button->SetText("Run");
	run_button->SetSize(60, LINE_HEIGHT);
	run_button->SetLocation(WIDTH - 60, HOTKEY_Y);
	run_button->GetClickEvent() += ClickEventHandler([this](Control*) {
		this->exec();
	});
	AddControl(run_button);
}

void TBHotkey::PopulateGeometry() {
	Panel::PopulateGeometry();
	Graphics g(*geometry_);
	g.DrawLine(GetForeColor(), PointF(0.0f, -3.0f), PointF((float)WIDTH, -3.0f));
}

HotkeySendChat::HotkeySendChat(Key key, Key modifier, bool active, wstring ini_section,
	wstring msg, wchar_t channel)
	: TBHotkey(key, modifier, active, ini_section), msg_(msg), channel_(channel) {
	
	Label* label = new Label();
	label->SetLocation(ITEM_X, LABEL_Y);
	label->SetText("Send Chat");
	AddControl(label);

	ComboBox* combo = new ComboBox();
	combo->AddItem("/");
	combo->AddItem("!");
	combo->AddItem("@");
	combo->AddItem("#");
	combo->AddItem("$");
	combo->AddItem("%");
	combo->SetSelectedIndex(ChannelToIndex(channel));
	combo->SetSize(30, LINE_HEIGHT);
	combo->SetLocation(label->GetRight() + HSPACE, ITEM_Y);
	combo->GetSelectedIndexChangedEvent() += SelectedIndexChangedEventHandler(
		[this, combo, ini_section](Control*) {
		wchar_t channel = this->IndexToChannel(combo->GetSelectedIndex());
		this->set_channel(channel);
		GWToolbox::instance()->config()->iniWrite(ini_section.c_str(), 
			this->IniKeyChannel(), wstring(1, channel).c_str());
		GWToolbox::instance()->main_window()->hotkey_panel()->UpdateDeleteCombo();
	});
	AddControl(combo);
	
	string text = string(msg.begin(), msg.end());
	TextBox* text_box = new TextBox();
	text_box->SetText(text);
	text_box->SetSize(WIDTH - combo->GetRight() - HSPACE, LINE_HEIGHT);
	text_box->SetLocation(combo->GetRight() + HSPACE, ITEM_Y);
	text_box->GetTextChangedEvent() += TextChangedEventHandler(
		[this, text_box, ini_section](Control*) {
		string text = text_box->GetText();
		wstring wtext = wstring(text.begin(), text.end());
		this->set_msg(wtext);
		GWToolbox::instance()->config()->iniWrite(ini_section.c_str(),
			this->IniKeyMsg(), wtext.c_str());
		GWToolbox::instance()->main_window()->hotkey_panel()->UpdateDeleteCombo();
	});
	text_box->GetFocusGotEvent() += FocusGotEventHandler([](Control*) {
		GWToolbox::instance()->set_capture_input(true);
	});
	text_box->GetFocusLostEvent() += FocusLostEventHandler([](Control*, Control*) {
		GWToolbox::instance()->set_capture_input(false);
	});
	AddControl(text_box);
}

int HotkeySendChat::ChannelToIndex(wchar_t channel) {
	switch (channel) {
	case L'/': return 0;
	case L'!': return 1;
	case L'@': return 2;
	case L'#': return 3;
	case L'$': return 4;
	case L'%': return 5;
	default:
		LOG("Warning - bad channel %lc\n", channel);
		return 0;
	}
}

wchar_t HotkeySendChat::IndexToChannel(int index) {
	switch (index) {
	case 0: return L'/';
	case 1: return L'!';
	case 2: return L'@';
	case 3: return L'#';
	case 4: return L'$';
	case 5: return L'%';
	default:
		LOG("Warning - bad index %d\n", index);
		return L'/';
	}
}

HotkeyUseItem::HotkeyUseItem(Key key, Key modifier, bool active, wstring ini_section,
	UINT item_id_, wstring item_name_) :
	TBHotkey(key, modifier, active, ini_section), 
	item_id_(item_id_), 
	item_name_(item_name_) {

	Label* label = new Label();
	label->SetLocation(ITEM_X, LABEL_Y);
	label->SetText("Use Item");
	AddControl(label);

	Label* label_id = new Label();
	label_id->SetLocation(label->GetRight() + HSPACE, LABEL_Y);
	label_id->SetText("ID:");
	AddControl(label_id);

	int width_left = WIDTH - label_id->GetRight();
	TextBox* id_box = new TextBox();
	id_box->SetText(to_string(item_id_));
	id_box->SetSize(width_left / 2 - HSPACE / 2, LINE_HEIGHT);
	id_box->SetLocation(label_id->GetRight(), ITEM_Y);
	id_box->GetTextChangedEvent() += TextChangedEventHandler(
		[this, id_box, ini_section](Control*) {
		try {
			long id = std::stol(id_box->GetText());
			this->set_item_id((UINT)id);
			GWToolbox::instance()->config()->iniWriteLong(ini_section.c_str(),
				this->IniKeyItemID(), id);
			GWToolbox::instance()->main_window()->hotkey_panel()->UpdateDeleteCombo();
		} catch (...) {}
	});
	id_box->GetFocusGotEvent() += FocusGotEventHandler([](Control*) {
		GWToolbox::instance()->set_capture_input(true);
	});
	id_box->GetFocusLostEvent() += FocusLostEventHandler([id_box](Control*, Control*) {
		GWToolbox::instance()->set_capture_input(false);
		try {
			std::stol(id_box->GetText());
			GWToolbox::instance()->main_window()->hotkey_panel()->UpdateDeleteCombo();
		} catch (...) {
			id_box->SetText("0");
		}
	});
	AddControl(id_box);

	TextBox* name_box = new TextBox();
	string text = string(item_name_.begin(), item_name_.end());
	name_box->SetText(text.c_str());
	name_box->SetSize(width_left / 2 - HSPACE / 2, LINE_HEIGHT);
	name_box->SetLocation(id_box->GetRight() + HSPACE , ITEM_Y);
	name_box->GetTextChangedEvent() += TextChangedEventHandler(
		[this, name_box, ini_section](Control*) {
		string text = name_box->GetText();
		wstring wtext = wstring(text.begin(), text.end());
		this->set_item_name(wtext);
		GWToolbox::instance()->config()->iniWrite(ini_section.c_str(),
			this->IniKeyItemName(), wtext.c_str());
		GWToolbox::instance()->main_window()->hotkey_panel()->UpdateDeleteCombo();
	});
	name_box->GetFocusGotEvent() += FocusGotEventHandler([](Control*) {
		GWToolbox::instance()->set_capture_input(true);
	});
	name_box->GetFocusLostEvent() += FocusLostEventHandler([](Control*, Control*) {
		GWToolbox::instance()->set_capture_input(false);
	});
	AddControl(name_box);
}

HotkeyDropUseBuff::HotkeyDropUseBuff(Key key, Key modifier, bool active, 
	wstring ini_section, GwConstants::SkillID id) :
TBHotkey(key, modifier, active, ini_section), id_(id) {

	Label* label = new Label();
	label->SetLocation(ITEM_X, LABEL_Y);
	label->SetText("Drop / Use Buff");
	AddControl(label);

	ComboBox* combo = new ComboBox();
	combo->AddItem("Recall");
	combo->AddItem("UA");
	combo->SetSize(WIDTH - label->GetRight() - HSPACE, LINE_HEIGHT);
	combo->SetLocation(label->GetRight() + HSPACE, ITEM_Y);
	switch (id) {
	case GwConstants::SkillID::Recall:
		combo->SetSelectedIndex(0);
		break;
	case GwConstants::SkillID::Unyielding_Aura:
		combo->SetSelectedIndex(1);
		break;
	default:
		combo->AddItem(to_string(static_cast<int>(id)));
		combo->SetSelectedIndex(2);
		break;
	}
	combo->GetSelectedIndexChangedEvent() += SelectedIndexChangedEventHandler(
		[this, combo, ini_section](Control*) {
		GwConstants::SkillID skillID = this->IndexToSkillID(combo->GetSelectedIndex());
		this->set_id(skillID);
		GWToolbox::instance()->config()->iniWriteLong(ini_section.c_str(),
			this->IniKeySkillID(), (long)skillID);
		GWToolbox::instance()->main_window()->hotkey_panel()->UpdateDeleteCombo();
	});
	combo_ = combo;
	AddControl(combo);
}

GwConstants::SkillID HotkeyDropUseBuff::IndexToSkillID(int index) {
	switch (index) {
	case 0: return GwConstants::SkillID::Recall;
	case 1: return GwConstants::SkillID::Unyielding_Aura;
	case 2: 
		if (combo_->GetItemsCount() == 3) {
			string s = combo_->GetItem(2);
			try {
				int i = std::stoi(s);
				return static_cast<GwConstants::SkillID>(i);
			} catch (...) {
				return GwConstants::SkillID::No_Skill;
			}
		}
	default:
		LOG("Warning. bad skill id %d\n", index);
		return GwConstants::SkillID::Recall;
	}
}

HotkeyToggle::HotkeyToggle(Key key, Key modifier, bool active, wstring ini_section, long toggle_id)
	: TBHotkey(key, modifier, active, ini_section) {

	target_ = static_cast<HotkeyToggle::Toggle>(toggle_id);

	Label* label = new Label();
	label->SetLocation(ITEM_X, LABEL_Y);
	label->SetText("Toggle...");
	AddControl(label);

	ComboBox* combo = new ComboBox();
	combo->SetSize(WIDTH - label->GetRight() - HSPACE, LINE_HEIGHT);
	combo->SetLocation(label->GetRight() + HSPACE, ITEM_Y);
	combo->AddItem("Clicker");
	combo->AddItem("Pcons");
	combo->AddItem("Coin drop");
	combo->SetSelectedIndex(toggle_id);
	combo->GetSelectedIndexChangedEvent() += SelectedIndexChangedEventHandler(
		[this, combo, ini_section](Control*) {
		int index = combo->GetSelectedIndex();
		Toggle target = static_cast<HotkeyToggle::Toggle>(index);
		this->set_target(target);
		GWToolbox::instance()->config()->iniWriteLong(ini_section.c_str(), 
			this->IniKeyToggleID(), index);
		GWToolbox::instance()->main_window()->hotkey_panel()->UpdateDeleteCombo();
	});
	AddControl(combo);
}

HotkeyAction::HotkeyAction(Key key, Key modifier, bool active, wstring ini_section, long action_id)
	: TBHotkey(key, modifier, active, ini_section) {

	action_ = static_cast<HotkeyAction::Action>(action_id);

	Label* label = new Label();
	label->SetLocation(ITEM_X, LABEL_Y);
	label->SetText("Execute...");
	AddControl(label);

	ComboBox* combo = new ComboBox();
	combo->SetSize(WIDTH - label->GetRight() - HSPACE, LINE_HEIGHT);
	combo->SetLocation(label->GetRight() + HSPACE, ITEM_Y);
	combo->AddItem("Open Xunlai Chest");
	combo->AddItem("Open Locked Chest");
	combo->AddItem("Drop Gold Coin");
	combo->SetSelectedIndex(action_id);
	combo->GetSelectedIndexChangedEvent() += SelectedIndexChangedEventHandler(
		[this, combo, ini_section](Control*) {
		int index = combo->GetSelectedIndex();
		Action action = static_cast<HotkeyAction::Action>(index);
		this->set_action(action);
		GWToolbox::instance()->config()->iniWriteLong(ini_section.c_str(),
			this->IniKeyActionID(), index);
		GWToolbox::instance()->main_window()->hotkey_panel()->UpdateDeleteCombo();
	});
	AddControl(combo);
}

HotkeyTarget::HotkeyTarget(Key key, Key modifier, bool active, wstring ini_section, 
	UINT targetID, wstring target_name)
	: TBHotkey(key, modifier, active, ini_section), id_(targetID), name_(target_name) {

	Label* label = new Label();
	label->SetLocation(ITEM_X, LABEL_Y);
	label->SetText("Target");
	AddControl(label);

	Label* label_id = new Label();
	label_id->SetLocation(label->GetRight() + HSPACE, LABEL_Y);
	label_id->SetText("ID:");
	AddControl(label_id);

	int width_left = WIDTH - label_id->GetRight();
	TextBox* id_box = new TextBox();
	id_box->SetText(to_string(id_));
	id_box->SetSize(width_left / 2 - HSPACE / 2, LINE_HEIGHT);
	id_box->SetLocation(label_id->GetRight(), ITEM_Y);
	id_box->GetTextChangedEvent() += TextChangedEventHandler(
		[this, id_box, ini_section](Control*) {
		try {
			long id = std::stol(id_box->GetText());
			this->set_id((UINT)id);
			GWToolbox::instance()->config()->iniWriteLong(ini_section.c_str(),
				this->IniKeyTargetID(), id);
			GWToolbox::instance()->main_window()->hotkey_panel()->UpdateDeleteCombo();
		} catch (...) {}
	});
	id_box->GetFocusGotEvent() += FocusGotEventHandler([](Control*) {
		GWToolbox::instance()->set_capture_input(true);
	});
	id_box->GetFocusLostEvent() += FocusLostEventHandler([id_box](Control*, Control*) {
		GWToolbox::instance()->set_capture_input(false);
		try {
			std::stol(id_box->GetText());
			GWToolbox::instance()->main_window()->hotkey_panel()->UpdateDeleteCombo();
		} catch (...) {
			id_box->SetText("0");
		}
	});
	AddControl(id_box);

	TextBox* name_box = new TextBox();
	string text = string(name_.begin(), name_.end());
	name_box->SetText(text.c_str());
	name_box->SetSize(width_left / 2 - HSPACE / 2, LINE_HEIGHT);
	name_box->SetLocation(id_box->GetRight() + HSPACE, ITEM_Y);
	name_box->GetTextChangedEvent() += TextChangedEventHandler(
		[this, name_box, ini_section](Control*) {
		string text = name_box->GetText();
		wstring wtext = wstring(text.begin(), text.end());
		this->set_name(wtext);
		GWToolbox::instance()->config()->iniWrite(ini_section.c_str(),
			this->IniKeyTargetName(), wtext.c_str());
		GWToolbox::instance()->main_window()->hotkey_panel()->UpdateDeleteCombo();
	});
	name_box->GetFocusGotEvent() += FocusGotEventHandler([](Control*) {
		GWToolbox::instance()->set_capture_input(true);
	});
	name_box->GetFocusLostEvent() += FocusLostEventHandler([](Control*, Control*) {
		GWToolbox::instance()->set_capture_input(false);
	});
	AddControl(name_box);
}

HotkeyMove::HotkeyMove(Key key, Key modifier, bool active, wstring ini_section,
	float x, float y, wstring name)
	: TBHotkey(key, modifier, active, ini_section), x_(x), y_(y), name_(name) {

	Label* label = new Label();
	label->SetLocation(ITEM_X, LABEL_Y);
	label->SetText("Move");
	AddControl(label);

	Label* label_x = new Label();
	label_x->SetLocation(label->GetRight() + HSPACE, LABEL_Y);
	label_x->SetText("X");
	AddControl(label_x);

	std::stringstream ss;
	TextBox* box_x = new TextBox();
	ss.clear();
	ss << x;
	box_x->SetText(ss.str());
	box_x->SetSize(50, LINE_HEIGHT);
	box_x->SetLocation(label_x->GetRight(), ITEM_Y);
	box_x->GetTextChangedEvent() += TextChangedEventHandler(
		[this, box_x, ini_section](Control*) {
		try {
			float x = std::stof(box_x->GetText());
			this->set_x(x);
			GWToolbox::instance()->config()->iniWriteDouble(ini_section.c_str(), 
				this->IniKeyX(), x);
			GWToolbox::instance()->main_window()->hotkey_panel()->UpdateDeleteCombo();
		} catch (...) {}
	});
	box_x->GetFocusGotEvent() += FocusGotEventHandler([](Control*) {
		GWToolbox::instance()->set_capture_input(true);
	});
	box_x->GetFocusLostEvent() += FocusLostEventHandler([box_x](Control*, Control*) {
		GWToolbox::instance()->set_capture_input(false);
		try {
			std::stof(box_x->GetText());
			GWToolbox::instance()->main_window()->hotkey_panel()->UpdateDeleteCombo();
		} catch (...) {
			box_x->SetText("0.0");
		}
	});
	AddControl(box_x);

	Label* label_y = new Label();
	label_y->SetLocation(box_x->GetRight() + HSPACE, LABEL_Y);
	label_y->SetText("Y");
	AddControl(label_y);

	TextBox* box_y = new TextBox();
	ss.clear();
	ss << y;
	box_y->SetText(ss.str());
	box_y->SetSize(50, LINE_HEIGHT);
	box_y->SetLocation(label_y->GetRight(), ITEM_Y);
	box_y->GetTextChangedEvent() += TextChangedEventHandler(
		[this, box_y, ini_section](Control*) {
		try {
			float y = std::stof(box_y->GetText());
			this->set_y(y);
			GWToolbox::instance()->config()->iniWriteDouble(ini_section.c_str(), 
				this->IniKeyY(), y);
			GWToolbox::instance()->main_window()->hotkey_panel()->UpdateDeleteCombo();
		} catch (...) {}
	});
	box_y->GetFocusGotEvent() += FocusGotEventHandler([](Control*) {
		GWToolbox::instance()->set_capture_input(true);
	});
	box_y->GetFocusLostEvent() += FocusLostEventHandler([box_y](Control*, Control*) {
		GWToolbox::instance()->set_capture_input(false);
		try {
			std::stof(box_y->GetText());
			GWToolbox::instance()->main_window()->hotkey_panel()->UpdateDeleteCombo();
		} catch (...) {
			box_y->SetText("0.0");
		}
	});
	AddControl(box_y);

	TextBox* name_box = new TextBox();
	string text = string(name_.begin(), name_.end());
	name_box->SetText(text.c_str());
	name_box->SetSize(WIDTH - box_y->GetRight() - HSPACE, LINE_HEIGHT);
	name_box->SetLocation(box_y->GetRight() + HSPACE, ITEM_Y);
	name_box->GetTextChangedEvent() += TextChangedEventHandler(
		[this, name_box, ini_section](Control*) {
		string text = name_box->GetText();
		wstring wtext = wstring(text.begin(), text.end());
		this->set_name(wtext);
		GWToolbox::instance()->config()->iniWrite(ini_section.c_str(),
			this->IniKeyName(), wtext.c_str());
		GWToolbox::instance()->main_window()->hotkey_panel()->UpdateDeleteCombo();
	});
	name_box->GetFocusGotEvent() += FocusGotEventHandler([](Control*) {
		GWToolbox::instance()->set_capture_input(true);
	});
	name_box->GetFocusLostEvent() += FocusLostEventHandler([](Control*, Control*) {
		GWToolbox::instance()->set_capture_input(false);
	});
	AddControl(name_box);
}

HotkeyDialog::HotkeyDialog(Key key, Key modifier, bool active, wstring ini_section, UINT id, wstring name)
	: TBHotkey(key, modifier, active, ini_section), id_(id), name_(name) {

	Label* label = new Label();
	label->SetLocation(ITEM_X, LABEL_Y);
	label->SetText("Dialog");
	AddControl(label);

	Label* label_id = new Label();
	label_id->SetLocation(label->GetRight() + HSPACE, LABEL_Y);
	label_id->SetText("ID:");
	AddControl(label_id);

	int width_left = WIDTH - label_id->GetRight();
	TextBox* id_box = new TextBox();
	id_box->SetText(to_string(id_));
	id_box->SetSize(width_left / 2 - HSPACE / 2, LINE_HEIGHT);
	id_box->SetLocation(label_id->GetRight(), ITEM_Y);
	id_box->GetTextChangedEvent() += TextChangedEventHandler(
		[this, id_box, ini_section](Control*) {
		try {
			long id = std::stol(id_box->GetText(), 0, 0);
			this->set_id((UINT)id);
			GWToolbox::instance()->config()->iniWriteLong(ini_section.c_str(),
				this->IniKeyDialogID(), id);
			GWToolbox::instance()->main_window()->hotkey_panel()->UpdateDeleteCombo();
		} catch (...) {}
	});
	id_box->GetFocusGotEvent() += FocusGotEventHandler([](Control*) {
		GWToolbox::instance()->set_capture_input(true);
	});
	id_box->GetFocusLostEvent() += FocusLostEventHandler([id_box](Control*, Control*) {
		GWToolbox::instance()->set_capture_input(false);
		try {
			std::stol(id_box->GetText(), 0, 0);
			GWToolbox::instance()->main_window()->hotkey_panel()->UpdateDeleteCombo();
		} catch (...) {
			id_box->SetText("0");
		}
	});
	AddControl(id_box);

	TextBox* name_box = new TextBox();
	string text = string(name_.begin(), name_.end());
	name_box->SetText(text.c_str());
	name_box->SetSize(width_left / 2 - HSPACE / 2, LINE_HEIGHT);
	name_box->SetLocation(id_box->GetRight() + HSPACE, ITEM_Y);
	name_box->GetTextChangedEvent() += TextChangedEventHandler(
		[this, name_box, ini_section](Control*) {
		string text = name_box->GetText();
		wstring wtext = wstring(text.begin(), text.end());
		this->set_name(wtext);
		GWToolbox::instance()->config()->iniWrite(ini_section.c_str(),
			this->IniKeyDialogName(), wtext.c_str());
		GWToolbox::instance()->main_window()->hotkey_panel()->UpdateDeleteCombo();
	});
	name_box->GetFocusGotEvent() += FocusGotEventHandler([](Control*) {
		GWToolbox::instance()->set_capture_input(true);
	});
	name_box->GetFocusLostEvent() += FocusLostEventHandler([](Control*, Control*) {
		GWToolbox::instance()->set_capture_input(false);
	});
	AddControl(name_box);
}

HotkeyPingBuild::HotkeyPingBuild(Key key, Key modifier, bool active, wstring ini_section, long index)
	: TBHotkey(key, modifier, active, ini_section), index_(index) {
	
	Label* label = new Label();
	label->SetLocation(ITEM_X, LABEL_Y);
	label->SetText("Ping...");
	AddControl(label);

	ComboBox* combo = new ComboBox();
	combo->SetSize(WIDTH - label->GetRight() - HSPACE, LINE_HEIGHT);
	combo->SetLocation(label->GetRight() + HSPACE, ITEM_Y);
	for (int i = 0; i < BuildPanel::N_BUILDS; ++i) {
		int index = i + 1;
		wstring section = wstring(L"builds") + to_wstring(index);
		wstring wname = GWToolbox::instance()->config()->iniRead(section.c_str(), L"buildname", L"");
		if (wname.empty()) wname = wstring(L"<Build ") + to_wstring(index);
		string name = string(wname.begin(), wname.end());
		combo->AddItem(name);
	}
	combo->SetSelectedIndex(index);
	combo->GetSelectedIndexChangedEvent() += SelectedIndexChangedEventHandler(
		[this, combo, ini_section](Control*) {
		long index = combo->GetSelectedIndex();
		this->set_index(index);
		GWToolbox::instance()->config()->iniWriteLong(ini_section.c_str(),
			this->IniKeyBuildIndex(), index);
		GWToolbox::instance()->main_window()->hotkey_panel()->UpdateDeleteCombo();
	});
	AddControl(combo);
}

void HotkeyUseItem::exec() {
	if (!isExplorable()) return;
	if (item_id_ <= 0) return;

	if (!GWAPI::GWAPIMgr::instance()->Items()->UseItemByModelId(item_id_)) {
		wstring name = item_name_.empty() ? to_wstring(item_id_) : item_name_;
		wstring msg = wstring(L"[Warning] ") + item_name_ + L"not found!";
		GWAPI::GWAPIMgr::instance()->Chat()->WriteChat(msg.c_str());
		LOGW(msg.c_str());
	}
}

void HotkeySendChat::exec() {
	if (isLoading()) return;
	if (msg_.empty()) return;

	GWAPIMgr::instance()->Chat()->SendChat(msg_.c_str(), channel_);
	if (channel_ == L'/') {
		GWAPIMgr::instance()->Chat()->WriteChat((wstring(L"/") + msg_).c_str());
	}
}

void HotkeyDropUseBuff::exec() {
	if (!isExplorable()) return;

	GWAPIMgr* API = GWAPIMgr::instance();
	GW::Buff buff = API->Effects()->GetPlayerBuffBySkillId(id_);
	if (buff.SkillId) {
		API->Effects()->DropBuff(buff.BuffId);
	} else {
		int slot = API->Skillbar()->getSkillSlot(id_);
		if (slot > 0 && API->Skillbar()->GetPlayerSkillbar().Skills[slot].Recharge == 0) {
			API->Skillbar()->UseSkill(slot, API->Agents()->GetTargetId());
		}
	}
}

void HotkeyToggle::exec() {
	GWToolbox* tb = GWToolbox::instance();
	bool active;
	switch (target_) {
	case HotkeyToggle::Clicker:
		active = tb->main_window()->hotkey_panel()->ToggleClicker();
		GWAPIMgr::instance()->Chat()->WriteChat(
			active ? L"Clicker is active" : L"Clicker is disabled");
		break;
	case HotkeyToggle::Pcons:
		active = tb->main_window()->pcon_panel()->ToggleActive();
		tb->main_window()->UpdatePconToggleButton(active);
		break;
	case HotkeyToggle::CoinDrop:
		active = tb->main_window()->hotkey_panel()->ToggleCoinDrop();
		GWAPIMgr::instance()->Chat()->WriteChat(
			active ? L"Coin dropper is active" : L"Coin dropper is disabled");
		break;
	}
}

void HotkeyAction::exec() {
	GWAPIMgr* api = GWAPIMgr::instance();

	switch (action_) {
	case HotkeyAction::OpenXunlaiChest:
		if (api->Map()->GetInstanceType() == GwConstants::InstanceType::Outpost) {
			api->Items()->OpenXunlaiWindow();
		}
		break;
	case HotkeyAction::OpenLockedChest: {
		GW::Agent* target = api->Agents()->GetTarget();
		if (target && target->Type == 0x200) {
			api->Agents()->GoSignpost(target);
			api->Items()->OpenLockedChest();
		}
		break;
	}
	case HotkeyAction::DropGoldCoin:
		api->Items()->DropGold(1);
		break;
	}
}

void HotkeyTarget::exec() {
	if (isLoading()) return;
	if (id_ <= 0) return;
	GWAPIMgr* API = GWAPIMgr::instance();

	GW::AgentArray agents = API->Agents()->GetAgentArray();
	if (!agents.valid()) {
		return;
	}

	GW::Agent* me = agents[API->Agents()->GetPlayerId()];
	if (me == nullptr) return;

	unsigned long distance = GwConstants::SqrRange::Compass;
	int closest = -1;

	for (size_t i = 0; i < agents.size(); ++i) {
		GW::Agent* agent = agents[i];
		if (agent == nullptr) continue;
		if (agent->PlayerNumber == id_ && agent->HP >= 0) {
			unsigned long newDistance = API->Agents()->GetSqrDistance(me, agents[i]);
			if (newDistance < distance) {
				closest = i;
				distance = newDistance;
			}
		}
	}
	if (closest > 0) {
		API->Agents()->ChangeTarget(agents[closest]);
	}
}

void HotkeyMove::exec() {
	if (!isExplorable()) return;

	GWAPIMgr* API = GWAPIMgr::instance();
	GW::Agent* me = API->Agents()->GetPlayer();
	double sqrDist = (me->X - x_) * (me->X - x_) + (me->Y - y_) * (me->Y - y_);
	if (sqrDist < GwConstants::SqrRange::Compass) {
		API->Agents()->Move(x_, y_);
	}
	GWAPIMgr::instance()->Chat()->WriteChat(L"Movement macro activated");
}

void HotkeyDialog::exec() {
	if (isLoading()) return;
	if (id_ <= 0) return;

	GWAPIMgr::instance()->Agents()->Dialog(id_);
}

void HotkeyPingBuild::exec() {
	if (isLoading()) return;

	if (index_ >= 0 && index_ < BuildPanel::N_BUILDS) {
		GWToolbox::instance()->main_window()->build_panel()->SendTeamBuild(index_);
	}
}

string HotkeySendChat::GetDescription() {
	return string("Send ") + static_cast<char>(channel_) + string(msg_.begin(), msg_.end());
}

string HotkeyUseItem::GetDescription() {
	if (item_name_.empty()) {
		return string("Use Item #") + to_string(item_id_);
	} else {
		return string("Use ") + string(item_name_.begin(), item_name_.end());
	}
}

string HotkeyDropUseBuff::GetDescription() {
	switch (id_) {
	case GwConstants::SkillID::Recall:
		return string("Drop/Use Recall");
	case GwConstants::SkillID::Unyielding_Aura:
		return string("Drop/Use UA");
	default:
		return string("Drop/Use Skill #") + to_string(static_cast<long>(id_));
	}
}

string HotkeyToggle::GetDescription() {
	switch (target_) {
	case HotkeyToggle::Clicker:
		return string("Toggle Clicker");
	case HotkeyToggle::Pcons:
		return string("Toggle Pcons");
	case HotkeyToggle::CoinDrop:
		return string("Toggle Coin Drop");
	default:
		return string("error :(");
	}
}

string HotkeyAction::GetDescription() {
	switch (action_) {
	case HotkeyAction::OpenXunlaiChest:
		return string("Open Xunlai Chest");
	case HotkeyAction::OpenLockedChest:
		return string("Open Locked Chest");
	case HotkeyAction::DropGoldCoin:
		return string("Drop Gold Coin");
	default:
		return string("error :(");
	}
}

string HotkeyTarget::GetDescription() {
	if (name_.empty()) {
		return string("Target ") + to_string(id_);
	} else {
		return string("Target ") + string(name_.begin(), name_.end());
	}
}

string HotkeyMove::GetDescription() {
	if (name_.empty()) {
		return string("Move (") + to_string(lroundf(x_)) + ", " + to_string(lroundf(y_)) + ")";
	} else {
		return string("Move ") + string(name_.begin(), name_.end());
	}
}

string HotkeyDialog::GetDescription() {
	if (name_.empty()) {
		return string("Dialog ") + to_string(id_);
	} else {
		return string("Dialog ") + string(name_.begin(), name_.end());
	}
}

string HotkeyPingBuild::GetDescription() {
	return string("Ping Build #") + to_string(index_);
}
