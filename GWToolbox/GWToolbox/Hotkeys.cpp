#include "Hotkeys.h"

#include <iostream>
#include <sstream>

#include <GWCA\GWCA.h>
#include <GWCA\Managers\ItemMgr.h>
#include <GWCA\Managers\EffectMgr.h>
#include <GWCA\Managers\PlayerMgr.h>
#include <GWCA\Managers\SkillbarMgr.h>

#include "logger.h"
#include "ChatLogger.h"
#include "GWToolbox.h"
#include "Config.h"
#include "BuildPanel.h"


using namespace GWCA;
using namespace OSHGui;

TBHotkey::TBHotkey(OSHGui::Control* parent, Key key, Key modifier, bool active, wstring ini_section)
	: OSHGui::Panel(parent), active_(active), key_(key), modifier_(modifier), ini_section_(ini_section) {

	pressed_ = false;

	SetBackColor(Drawing::Color::Empty());
	SetSize(SizeI(WIDTH, HEIGHT));

	CheckBox* checkbox = new CheckBox(this);
	checkbox->SetChecked(active);
	checkbox->SetText(L"");
	checkbox->SetLocation(PointI(HOTKEY_X, HOTKEY_Y + 5));
	checkbox->GetCheckedChangedEvent() += CheckedChangedEventHandler([this, checkbox, ini_section](Control*) {
		this->set_active(checkbox->GetChecked());
		Config::IniWriteBool(ini_section.c_str(), TBHotkey::IniKeyActive(), checkbox->GetChecked());
	});
	AddControl(checkbox);

	HotkeyControl* hotkey_button = new HotkeyControl(this);
	hotkey_button->SetSize(SizeI(WIDTH - checkbox->GetRight() - 60 - HSPACE * 2, LINE_HEIGHT));
	hotkey_button->SetLocation(PointI(checkbox->GetRight() + HSPACE, HOTKEY_Y));
	hotkey_button->SetHotkey(key);
	hotkey_button->SetHotkeyModifier(modifier);
	hotkey_button->GetFocusGotEvent() += FocusGotEventHandler([hotkey_button](Control*) {
		GWToolbox::instance().set_capture_input(true);
		hotkey_button->SetBackColor(Drawing::Color::Red());
	});
	hotkey_button->GetFocusLostEvent() += FocusLostEventHandler([hotkey_button, this](Control*, Control*) {
		GWToolbox::instance().set_capture_input(false);
		Misc::AnsiString controltype = Control::ControlTypeToString(GetType());
		Drawing::Theme::ControlTheme theme = Application::Instance().GetTheme().GetControlColorTheme(controltype);
		hotkey_button->SetBackColor(theme.BackColor);
	});
	hotkey_button->GetHotkeyChangedEvent() += HotkeyChangedEventHandler(
		[this, hotkey_button, ini_section](Control*) {
		Key key = hotkey_button->GetHotkey();
		Key modifier = hotkey_button->GetHotkeyModifier();
		this->set_key(key);
		this->set_modifier(modifier);
		Config::IniWriteLong(ini_section.c_str(),
			this->IniKeyHotkey(), (long)key);
		Config::IniWriteLong(ini_section.c_str(),
			this->IniKeyModifier(), (long)modifier);
	});
	AddControl(hotkey_button);

	Button* run_button = new Button(this);
	run_button->SetText(L"Run");
	run_button->SetSize(SizeI(60, LINE_HEIGHT));
	run_button->SetLocation(PointI(WIDTH - 60, HOTKEY_Y));
	run_button->GetClickEvent() += ClickEventHandler([this](Control*) {
		this->exec();
	});
	AddControl(run_button);
}

bool TBHotkey::isLoading() const {
	return GW::Map().GetInstanceType() == GW::Constants::InstanceType::Loading;
}

bool TBHotkey::isExplorable() const {
	return GW::Map().GetInstanceType() == GW::Constants::InstanceType::Explorable;
}
bool TBHotkey::isOutpost() const {
	return GW::Map().GetInstanceType() == GW::Constants::InstanceType::Outpost;
}

void TBHotkey::PopulateGeometry() {
	Panel::PopulateGeometry();
	Graphics g(*geometry_);
	g.DrawLine(GetForeColor(), PointI(0, 1), PointI(WIDTH, 1));
}

HotkeySendChat::HotkeySendChat(OSHGui::Control* parent, Key key, Key modifier, 
	bool active, wstring ini_section, wstring msg, wchar_t channel)
	: TBHotkey(parent, key, modifier, active, ini_section), msg_(msg), channel_(channel) {
	
	Label* label = new Label(this);
	label->SetLocation(PointI(ITEM_X, LABEL_Y));
	label->SetText(L"Send Chat");
	AddControl(label);

	ComboBox* combo = new ComboBox(this);
	combo->AddItem(L"/");
	combo->AddItem(L"!");
	combo->AddItem(L"@");
	combo->AddItem(L"#");
	combo->AddItem(L"$");
	combo->AddItem(L"%");
	combo->SetSelectedIndex(ChannelToIndex(channel));
	combo->SetSize(SizeI(30, LINE_HEIGHT));
	combo->SetLocation(PointI(label->GetRight() + HSPACE, ITEM_Y));
	combo->GetSelectedIndexChangedEvent() += SelectedIndexChangedEventHandler(
		[this, combo, ini_section](Control*) {
		wchar_t channel = this->IndexToChannel(combo->GetSelectedIndex());
		this->set_channel(channel);
		Config::IniWrite(ini_section.c_str(),
			this->IniKeyChannel(), wstring(1, channel).c_str());
		GWToolbox::instance().main_window().hotkey_panel().UpdateDeleteCombo();
	});
	controls_.push_front(combo);
	
	TextBox* text_box = new TextBox(this);
	text_box->SetText(msg);
	text_box->SetSize(SizeI(WIDTH - combo->GetRight() - HSPACE, LINE_HEIGHT));
	text_box->SetLocation(PointI(combo->GetRight() + HSPACE, ITEM_Y));
	text_box->GetTextChangedEvent() += TextChangedEventHandler(
		[this, text_box, ini_section](Control*) {
		wstring text = text_box->GetText();
		this->set_msg(text);
		Config::IniWrite(ini_section.c_str(),
			this->IniKeyMsg(), text.c_str());
		GWToolbox::instance().main_window().hotkey_panel().UpdateDeleteCombo();
	});
	text_box->GetFocusGotEvent() += FocusGotEventHandler([](Control*) {
		GWToolbox::instance().set_capture_input(true);
	});
	text_box->GetFocusLostEvent() += FocusLostEventHandler([](Control*, Control*) {
		GWToolbox::instance().set_capture_input(false);
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

HotkeyUseItem::HotkeyUseItem(OSHGui::Control* parent, Key key, Key modifier, 
	bool active, wstring ini_section, UINT item_id_, wstring item_name_) :
	TBHotkey(parent, key, modifier, active, ini_section), 
	item_id_(item_id_), 
	item_name_(item_name_) {

	Label* label = new Label(this);
	label->SetLocation(PointI(ITEM_X, LABEL_Y));
	label->SetText(L"Use Item");
	AddControl(label);

	Label* label_id = new Label(this);
	label_id->SetLocation(PointI(label->GetRight() + HSPACE, LABEL_Y));
	label_id->SetText(L"ID:");
	AddControl(label_id);

	int width_left = WIDTH - label_id->GetRight();
	TextBox* id_box = new TextBox(this);
	id_box->SetText(to_wstring(item_id_));
	id_box->SetSize(SizeI(width_left / 2 - HSPACE / 2, LINE_HEIGHT));
	id_box->SetLocation(PointI(label_id->GetRight(), ITEM_Y));
	id_box->GetTextChangedEvent() += TextChangedEventHandler(
		[this, id_box, ini_section](Control*) {
		try {
			long id = std::stol(id_box->GetText());
			this->set_item_id((UINT)id);
			Config::IniWriteLong(ini_section.c_str(),
				this->IniKeyItemID(), id);
			GWToolbox::instance().main_window().hotkey_panel().UpdateDeleteCombo();
		} catch (...) {}
	});
	id_box->GetFocusGotEvent() += FocusGotEventHandler([](Control*) {
		GWToolbox::instance().set_capture_input(true);
	});
	id_box->GetFocusLostEvent() += FocusLostEventHandler([id_box](Control*, Control*) {
		GWToolbox::instance().set_capture_input(false);
		try {
			std::stol(id_box->GetText());
			GWToolbox::instance().main_window().hotkey_panel().UpdateDeleteCombo();
		} catch (...) {
			id_box->SetText(L"0");
		}
	});
	AddControl(id_box);

	TextBox* name_box = new TextBox(this);
	name_box->SetText(item_name_.c_str());
	name_box->SetSize(SizeI(width_left / 2 - HSPACE / 2, LINE_HEIGHT));
	name_box->SetLocation(PointI(id_box->GetRight() + HSPACE , ITEM_Y));
	name_box->GetTextChangedEvent() += TextChangedEventHandler(
		[this, name_box, ini_section](Control*) {
		wstring text = name_box->GetText();
		this->set_item_name(text);
		Config::IniWrite(ini_section.c_str(),
			this->IniKeyItemName(), text.c_str());
		GWToolbox::instance().main_window().hotkey_panel().UpdateDeleteCombo();
	});
	name_box->GetFocusGotEvent() += FocusGotEventHandler([](Control*) {
		GWToolbox::instance().set_capture_input(true);
	});
	name_box->GetFocusLostEvent() += FocusLostEventHandler([](Control*, Control*) {
		GWToolbox::instance().set_capture_input(false);
	});
	AddControl(name_box);
}

HotkeyDropUseBuff::HotkeyDropUseBuff(OSHGui::Control* parent, Key key, Key modifier, 
	bool active, wstring ini_section, GW::Constants::SkillID id) :
	TBHotkey(parent, key, modifier, active, ini_section), id_(id) {

	Label* label = new Label(this);
	label->SetLocation(PointI(ITEM_X, LABEL_Y));
	label->SetText(L"Drop / Use Buff");
	AddControl(label);

	ComboBox* combo = new ComboBox(this);
	combo->AddItem(L"Recall");
	combo->AddItem(L"UA");
	combo->SetSize(SizeI(WIDTH - label->GetRight() - HSPACE, LINE_HEIGHT));
	combo->SetLocation(PointI(label->GetRight() + HSPACE, ITEM_Y));
	switch (id) {
	case GW::Constants::SkillID::Recall:
		combo->SetSelectedIndex(0);
		break;
	case GW::Constants::SkillID::Unyielding_Aura:
		combo->SetSelectedIndex(1);
		break;
	default:
		combo->AddItem(to_wstring(static_cast<int>(id)));
		combo->SetSelectedIndex(2);
		break;
	}
	combo->GetSelectedIndexChangedEvent() += SelectedIndexChangedEventHandler(
		[this, combo, ini_section](Control*) {
		GW::Constants::SkillID skillID = this->IndexToSkillID(combo->GetSelectedIndex());
		this->set_id(skillID);
		Config::IniWriteLong(ini_section.c_str(),
			this->IniKeySkillID(), (long)skillID);
		GWToolbox::instance().main_window().hotkey_panel().UpdateDeleteCombo();
	});
	combo_ = combo;
	controls_.push_front(combo);
}

GW::Constants::SkillID HotkeyDropUseBuff::IndexToSkillID(int index) {
	switch (index) {
	case 0: return GW::Constants::SkillID::Recall;
	case 1: return GW::Constants::SkillID::Unyielding_Aura;
	case 2: 
		if (combo_->GetItemsCount() == 3) {
			wstring s = combo_->GetItem(2);
			try {
				int i = std::stoi(s);
				return static_cast<GW::Constants::SkillID>(i);
			} catch (...) {
				return GW::Constants::SkillID::No_Skill;
			}
		}
	default:
		LOG("Warning. bad skill id %d\n", index);
		return GW::Constants::SkillID::Recall;
	}
}

HotkeyToggle::HotkeyToggle(OSHGui::Control* parent, Key key, Key modifier, 
	bool active, wstring ini_section, long toggle_id)
	: TBHotkey(parent, key, modifier, active, ini_section) {

	target_ = static_cast<HotkeyToggle::Toggle>(toggle_id);

	Label* label = new Label(this);
	label->SetLocation(PointI(ITEM_X, LABEL_Y));
	label->SetText(L"Toggle...");
	AddControl(label);

	ComboBox* combo = new ComboBox(this);
	combo->SetSize(SizeI(WIDTH - label->GetRight() - HSPACE, LINE_HEIGHT));
	combo->SetLocation(PointI(label->GetRight() + HSPACE, ITEM_Y));
	combo->AddItem(L"Clicker");
	combo->AddItem(L"Pcons");
	combo->AddItem(L"Coin drop");
	combo->SetSelectedIndex(toggle_id);
	combo->GetSelectedIndexChangedEvent() += SelectedIndexChangedEventHandler(
		[this, combo, ini_section](Control*) {
		int index = combo->GetSelectedIndex();
		Toggle target = static_cast<HotkeyToggle::Toggle>(index);
		this->set_target(target);
		Config::IniWriteLong(ini_section.c_str(),
			this->IniKeyToggleID(), index);
		GWToolbox::instance().main_window().hotkey_panel().UpdateDeleteCombo();
	});
	controls_.push_front(combo);
}

HotkeyAction::HotkeyAction(OSHGui::Control* parent, Key key, Key modifier, 
	bool active, wstring ini_section, long action_id)
	: TBHotkey(parent, key, modifier, active, ini_section) {

	action_ = static_cast<HotkeyAction::Action>(action_id);

	Label* label = new Label(this);
	label->SetLocation(PointI(ITEM_X, LABEL_Y));
	label->SetText(L"Execute...");
	AddControl(label);

	ComboBox* combo = new ComboBox(this);
	combo->SetSize(SizeI(WIDTH - label->GetRight() - HSPACE, LINE_HEIGHT));
	combo->SetLocation(PointI(label->GetRight() + HSPACE, ITEM_Y));
	combo->AddItem(L"Open Xunlai Chest");
	combo->AddItem(L"Open Locked Chest");
	combo->AddItem(L"Drop Gold Coin");
	combo->AddItem(L"Reapply LB Title");
	combo->SetSelectedIndex(action_id);
	combo->GetSelectedIndexChangedEvent() += SelectedIndexChangedEventHandler(
		[this, combo, ini_section](Control*) {
		int index = combo->GetSelectedIndex();
		Action action = static_cast<HotkeyAction::Action>(index);
		this->set_action(action);
		Config::IniWriteLong(ini_section.c_str(),
			this->IniKeyActionID(), index);
		GWToolbox::instance().main_window().hotkey_panel().UpdateDeleteCombo();
	});
	controls_.push_front(combo);
}

HotkeyTarget::HotkeyTarget(OSHGui::Control* parent, Key key, Key modifier, 
	bool active, wstring ini_section, UINT targetID, wstring target_name)
	: TBHotkey(parent, key, modifier, active, ini_section), id_(targetID), name_(target_name) {

	Label* label = new Label(this);
	label->SetLocation(PointI(ITEM_X, LABEL_Y));
	label->SetText(L"Target");
	AddControl(label);

	Label* label_id = new Label(this);
	label_id->SetLocation(PointI(label->GetRight() + HSPACE, LABEL_Y));
	label_id->SetText(L"ID:");
	AddControl(label_id);

	int width_left = WIDTH - label_id->GetRight();
	TextBox* id_box = new TextBox(this);
	id_box->SetText(to_wstring(id_));
	id_box->SetSize(SizeI(width_left / 2 - HSPACE / 2, LINE_HEIGHT));
	id_box->SetLocation(PointI(label_id->GetRight(), ITEM_Y));
	id_box->GetTextChangedEvent() += TextChangedEventHandler(
		[this, id_box, ini_section](Control*) {
		try {
			long id = std::stol(id_box->GetText());
			this->set_id((UINT)id);
			Config::IniWriteLong(ini_section.c_str(),
				this->IniKeyTargetID(), id);
			GWToolbox::instance().main_window().hotkey_panel().UpdateDeleteCombo();
		} catch (...) {}
	});
	id_box->GetFocusGotEvent() += FocusGotEventHandler([](Control*) {
		GWToolbox::instance().set_capture_input(true);
	});
	id_box->GetFocusLostEvent() += FocusLostEventHandler([id_box](Control*, Control*) {
		GWToolbox::instance().set_capture_input(false);
		try {
			std::stol(id_box->GetText());
			GWToolbox::instance().main_window().hotkey_panel().UpdateDeleteCombo();
		} catch (...) {
			id_box->SetText(L"0");
		}
	});
	AddControl(id_box);

	TextBox* name_box = new TextBox(this);
	name_box->SetText(name_.c_str());
	name_box->SetSize(SizeI(width_left / 2 - HSPACE / 2, LINE_HEIGHT));
	name_box->SetLocation(PointI(id_box->GetRight() + HSPACE, ITEM_Y));
	name_box->GetTextChangedEvent() += TextChangedEventHandler(
		[this, name_box, ini_section](Control*) {
		wstring text = name_box->GetText();
		this->set_name(text);
		Config::IniWrite(ini_section.c_str(),
			this->IniKeyTargetName(), text.c_str());
		GWToolbox::instance().main_window().hotkey_panel().UpdateDeleteCombo();
	});
	name_box->GetFocusGotEvent() += FocusGotEventHandler([](Control*) {
		GWToolbox::instance().set_capture_input(true);
	});
	name_box->GetFocusLostEvent() += FocusLostEventHandler([](Control*, Control*) {
		GWToolbox::instance().set_capture_input(false);
	});
	AddControl(name_box);
}

HotkeyMove::HotkeyMove(OSHGui::Control* parent, Key key, Key modifier, 
	bool active, wstring ini_section, float x, float y, wstring name)
	: TBHotkey(parent, key, modifier, active, ini_section), x_(x), y_(y), name_(name) {

	Label* label = new Label(this);
	label->SetLocation(PointI(ITEM_X, LABEL_Y));
	label->SetText(L"Move");
	AddControl(label);

	Label* label_x = new Label(this);
	label_x->SetLocation(PointI(label->GetRight() + HSPACE, LABEL_Y));
	label_x->SetText(L"X");
	AddControl(label_x);

	std::wstringstream ss;
	TextBox* box_x = new TextBox(this);
	ss.clear();
	ss << x;
	box_x->SetText(ss.str());
	box_x->SetSize(SizeI(50, LINE_HEIGHT));
	box_x->SetLocation(PointI(label_x->GetRight(), ITEM_Y));
	box_x->GetTextChangedEvent() += TextChangedEventHandler(
		[this, box_x, ini_section](Control*) {
		try {
			float x = std::stof(box_x->GetText());
			this->set_x(x);
			Config::IniWriteDouble(ini_section.c_str(),
				this->IniKeyX(), x);
			GWToolbox::instance().main_window().hotkey_panel().UpdateDeleteCombo();
		} catch (...) {}
	});
	box_x->GetFocusGotEvent() += FocusGotEventHandler([](Control*) {
		GWToolbox::instance().set_capture_input(true);
	});
	box_x->GetFocusLostEvent() += FocusLostEventHandler([box_x](Control*, Control*) {
		GWToolbox::instance().set_capture_input(false);
		try {
			std::stof(box_x->GetText());
			GWToolbox::instance().main_window().hotkey_panel().UpdateDeleteCombo();
		} catch (...) {
			box_x->SetText(L"0.0");
		}
	});
	AddControl(box_x);

	Label* label_y = new Label(this);
	label_y->SetLocation(PointI(box_x->GetRight() + HSPACE, LABEL_Y));
	label_y->SetText(L"Y");
	AddControl(label_y);

	TextBox* box_y = new TextBox(this);
	ss.clear();
	ss << y;
	box_y->SetText(ss.str());
	box_y->SetSize(SizeI(50, LINE_HEIGHT));
	box_y->SetLocation(PointI(label_y->GetRight(), ITEM_Y));
	box_y->GetTextChangedEvent() += TextChangedEventHandler(
		[this, box_y, ini_section](Control*) {
		try {
			float y = std::stof(box_y->GetText());
			this->set_y(y);
			Config::IniWriteDouble(ini_section.c_str(),
				this->IniKeyY(), y);
			GWToolbox::instance().main_window().hotkey_panel().UpdateDeleteCombo();
		} catch (...) {}
	});
	box_y->GetFocusGotEvent() += FocusGotEventHandler([](Control*) {
		GWToolbox::instance().set_capture_input(true);
	});
	box_y->GetFocusLostEvent() += FocusLostEventHandler([box_y](Control*, Control*) {
		GWToolbox::instance().set_capture_input(false);
		try {
			std::stof(box_y->GetText());
			GWToolbox::instance().main_window().hotkey_panel().UpdateDeleteCombo();
		} catch (...) {
			box_y->SetText(L"0.0");
		}
	});
	AddControl(box_y);

	TextBox* name_box = new TextBox(this);
	name_box->SetText(name_.c_str());
	name_box->SetSize(SizeI(WIDTH - box_y->GetRight() - HSPACE, LINE_HEIGHT));
	name_box->SetLocation(PointI(box_y->GetRight() + HSPACE, ITEM_Y));
	name_box->GetTextChangedEvent() += TextChangedEventHandler(
		[this, name_box, ini_section](Control*) {
		wstring text = name_box->GetText();
		this->set_name(text);
		Config::IniWrite(ini_section.c_str(),
			this->IniKeyName(), text.c_str());
		GWToolbox::instance().main_window().hotkey_panel().UpdateDeleteCombo();
	});
	name_box->GetFocusGotEvent() += FocusGotEventHandler([](Control*) {
		GWToolbox::instance().set_capture_input(true);
	});
	name_box->GetFocusLostEvent() += FocusLostEventHandler([](Control*, Control*) {
		GWToolbox::instance().set_capture_input(false);
	});
	AddControl(name_box);
}

HotkeyDialog::HotkeyDialog(OSHGui::Control* parent, Key key, Key modifier, 
	bool active, wstring ini_section, UINT id, wstring name)
	: TBHotkey(parent, key, modifier, active, ini_section), id_(id), name_(name) {

	Label* label = new Label(this);
	label->SetLocation(PointI(ITEM_X, LABEL_Y));
	label->SetText(L"Dialog");
	AddControl(label);

	Label* label_id = new Label(this);
	label_id->SetLocation(PointI(label->GetRight() + HSPACE, LABEL_Y));
	label_id->SetText(L"ID:");
	AddControl(label_id);

	int width_left = WIDTH - label_id->GetRight();
	TextBox* id_box = new TextBox(this);
	id_box->SetText(to_wstring(id_));
	id_box->SetSize(SizeI(width_left / 2 - HSPACE / 2, LINE_HEIGHT));
	id_box->SetLocation(PointI(label_id->GetRight(), ITEM_Y));
	id_box->GetTextChangedEvent() += TextChangedEventHandler(
		[this, id_box, ini_section](Control*) {
		try {
			long id = std::stol(id_box->GetText(), 0, 0);
			this->set_id((UINT)id);
			Config::IniWriteLong(ini_section.c_str(),
				this->IniKeyDialogID(), id);
			GWToolbox::instance().main_window().hotkey_panel().UpdateDeleteCombo();
		} catch (...) {}
	});
	id_box->GetFocusGotEvent() += FocusGotEventHandler([](Control*) {
		GWToolbox::instance().set_capture_input(true);
	});
	id_box->GetFocusLostEvent() += FocusLostEventHandler([id_box](Control*, Control*) {
		GWToolbox::instance().set_capture_input(false);
		try {
			std::stol(id_box->GetText(), 0, 0);
			GWToolbox::instance().main_window().hotkey_panel().UpdateDeleteCombo();
		} catch (...) {
			id_box->SetText(L"0");
		}
	});
	AddControl(id_box);

	TextBox* name_box = new TextBox(this);
	name_box->SetText(name_.c_str());
	name_box->SetSize(SizeI(width_left / 2 - HSPACE / 2, LINE_HEIGHT));
	name_box->SetLocation(PointI(id_box->GetRight() + HSPACE, ITEM_Y));
	name_box->GetTextChangedEvent() += TextChangedEventHandler(
		[this, name_box, ini_section](Control*) {
		wstring text = name_box->GetText();
		this->set_name(text);
		Config::IniWrite(ini_section.c_str(),
			this->IniKeyDialogName(), text.c_str());
		GWToolbox::instance().main_window().hotkey_panel().UpdateDeleteCombo();
	});
	name_box->GetFocusGotEvent() += FocusGotEventHandler([](Control*) {
		GWToolbox::instance().set_capture_input(true);
	});
	name_box->GetFocusLostEvent() += FocusLostEventHandler([](Control*, Control*) {
		GWToolbox::instance().set_capture_input(false);
	});
	AddControl(name_box);
}

HotkeyPingBuild::HotkeyPingBuild(OSHGui::Control* parent, Key key, Key modifier, 
	bool active, wstring ini_section, long index)
	: TBHotkey(parent, key, modifier, active, ini_section), index_(index) {
	
	Label* label = new Label(this);
	label->SetLocation(PointI(ITEM_X, LABEL_Y));
	label->SetText(L"Ping...");
	AddControl(label);

	ComboBox* combo = new ComboBox(this);
	combo->SetSize(SizeI(WIDTH - label->GetRight() - HSPACE, LINE_HEIGHT));
	combo->SetLocation(PointI(label->GetRight() + HSPACE, ITEM_Y));
	for (int i = 0; i < BuildPanel::N_BUILDS; ++i) {
		int index = i + 1;
		wstring section = wstring(L"builds") + to_wstring(index);
		wstring name = Config::IniRead(section.c_str(), L"buildname", L"");
		if (name.empty()) name = wstring(L"<Build ") + to_wstring(index);
		combo->AddItem(name);
	}
	combo->SetSelectedIndex(index);
	combo->GetSelectedIndexChangedEvent() += SelectedIndexChangedEventHandler(
		[this, combo, ini_section](Control*) {
		long index = combo->GetSelectedIndex();
		this->set_index(index);
		Config::IniWriteLong(ini_section.c_str(),
			this->IniKeyBuildIndex(), index);
		GWToolbox::instance().main_window().hotkey_panel().UpdateDeleteCombo();
	});
	controls_.push_front(combo);
}

void HotkeyUseItem::exec() {
	if (!isExplorable()) return;
	if (item_id_ <= 0) return;
	if (!GW::Items().UseItemByModelId(item_id_)) {
		wstring name = item_name_.empty() ? to_wstring(item_id_) : item_name_;
		ChatLogger::LogF(L"[Warning] %ls not found!", name.c_str());
	}
}

void HotkeySendChat::exec() {
	if (isLoading()) return;
	if (msg_.empty()) return;
	if (channel_ == L'/') {
		//ChatLogger::LogF(L"/%ls", msg_.c_str());
		ChatLogger::Log(L"/" + msg_);
	}
	GW::Chat().SendChat(msg_.c_str(), channel_);
}

void HotkeyDropUseBuff::exec() {
	if (!isExplorable()) return;

	GW::Buff buff = GW::Effects().GetPlayerBuffBySkillId(id_);
	if (buff.SkillId) {
		GW::Effects().DropBuff(buff.BuffId);
	} else {
		int slot = GW::Skillbarmgr().GetSkillSlot(id_);
		if (slot >= 0 && GW::Skillbar::GetPlayerSkillbar().Skills[slot].Recharge == 0) {
			GW::Skillbarmgr().UseSkill(slot, GW::Agents().GetTargetId());
		}
	}
}

void HotkeyToggle::exec() {
	GWToolbox& tb = GWToolbox::instance();
	bool active;
	switch (target_) {
	case HotkeyToggle::Clicker:
		active = tb.main_window().hotkey_panel().ToggleClicker();
		ChatLogger::LogF(L"Clicker is %ls", active ? L"active" : L"disabled");
		break;
	case HotkeyToggle::Pcons:
		tb.main_window().pcon_panel().ToggleActive();
		// writing to chat is done by ToggleActive if needed
		break;
	case HotkeyToggle::CoinDrop:
		active = tb.main_window().hotkey_panel().ToggleCoinDrop();
		ChatLogger::LogF(L"Coin dropper is %ls", active ? L"active" : L"disabled");
		break;
	}
}

void HotkeyAction::exec() {
	if (isLoading()) return;
	switch (action_) {
	case HotkeyAction::OpenXunlaiChest:
		if (isOutpost()) {
			GW::Items().OpenXunlaiWindow();
		}
		break;
	case HotkeyAction::OpenLockedChest: {
		if (isExplorable()) {
			GW::Agent* target = GW::Agents().GetTarget();
			if (target && target->Type == 0x200) {
				GW::Agents().GoSignpost(target);
				GW::Items().OpenLockedChest();
			}
		}
		break;
	}
	case HotkeyAction::DropGoldCoin:
		if (isExplorable()) {
			GW::Items().DropGold(1);
		}
		break;
	case HotkeyAction::ReapplyLBTitle:
		GW::Playermgr().RemoveActiveTitle();
		GW::Playermgr().SetActiveTitle(GW::Constants::TitleID::Lightbringer);
		break;
	}
}

void HotkeyTarget::exec() {
	if (isLoading()) return;
	if (id_ <= 0) return;

	GW::AgentArray agents = GW::Agents().GetAgentArray();
	if (!agents.valid()) {
		return;
	}

	GW::Agent* me = agents[GW::Agents().GetPlayerId()];
	if (me == nullptr) return;

	float distance = GW::Constants::SqrRange::Compass;
	int closest = -1;

	for (size_t i = 0; i < agents.size(); ++i) {
		GW::Agent* agent = agents[i];
		if (agent == nullptr) continue;
		if (agent->PlayerNumber == id_ && agent->HP > 0) {
			float newDistance = GW::Agents().GetSqrDistance(me->pos, agents[i]->pos);
			if (newDistance < distance) {
				closest = i;
				distance = newDistance;
			}
		}
	}
	if (closest > 0) {
		GW::Agents().ChangeTarget(agents[closest]);
	}
}

void HotkeyMove::exec() {
	if (!isExplorable()) return;

	GW::Agent* me = GW::Agents().GetPlayer();
	double sqrDist = (me->X - x_) * (me->X - x_) + (me->Y - y_) * (me->Y - y_);
	if (sqrDist < GW::Constants::SqrRange::Compass) {
		GW::Agents().Move(x_, y_);
		ChatLogger::Log(L"Movement macro activated");
	}
}

void HotkeyDialog::exec() {
	if (isLoading()) return;
	if (id_ <= 0) return;

	GW::Agents().Dialog(id_);
}

void HotkeyPingBuild::exec() {
	if (isLoading()) return;

	if (index_ >= 0 && index_ < BuildPanel::N_BUILDS) {
		GWToolbox::instance().main_window().build_panel().SendTeamBuild(index_);
	}
}

wstring HotkeySendChat::GetDescription() {
	return wstring(L"Send ") + channel_ + msg_;
}

wstring HotkeyUseItem::GetDescription() {
	if (item_name_.empty()) {
		return wstring(L"Use Item #") + to_wstring(item_id_);
	} else {
		return wstring(L"Use ") + item_name_;
	}
}

wstring HotkeyDropUseBuff::GetDescription() {
	switch (id_) {
	case GW::Constants::SkillID::Recall:
		return wstring(L"Drop/Use Recall");
	case GW::Constants::SkillID::Unyielding_Aura:
		return wstring(L"Drop/Use UA");
	default:
		return wstring(L"Drop/Use Skill #") + to_wstring(static_cast<long>(id_));
	}
}

wstring HotkeyToggle::GetDescription() {
	switch (target_) {
	case HotkeyToggle::Clicker:
		return wstring(L"Toggle Clicker");
	case HotkeyToggle::Pcons:
		return wstring(L"Toggle Pcons");
	case HotkeyToggle::CoinDrop:
		return wstring(L"Toggle Coin Drop");
	default:
		return wstring(L"error :(");
	}
}

wstring HotkeyAction::GetDescription() {
	switch (action_) {
	case HotkeyAction::OpenXunlaiChest:
		return wstring(L"Open Xunlai Chest");
	case HotkeyAction::OpenLockedChest:
		return wstring(L"Open Locked Chest");
	case HotkeyAction::DropGoldCoin:
		return wstring(L"Drop Gold Coin");
	case HotkeyAction::ReapplyLBTitle:
		return wstring(L"Reapply LB Title");
	default:
		return wstring(L"<Action Hotkey>");
	}
}

wstring HotkeyTarget::GetDescription() {
	if (name_.empty()) {
		return wstring(L"Target ") + to_wstring(id_);
	} else {
		return wstring(L"Target ") + name_;
	}
}

wstring HotkeyMove::GetDescription() {
	if (name_.empty()) {
		return wstring(L"Move (") + to_wstring(lroundf(x_)) + L", " + to_wstring(lroundf(y_)) + L")";
	} else {
		return wstring(L"Move ") + name_;
	}
}

wstring HotkeyDialog::GetDescription() {
	if (name_.empty()) {
		return wstring(L"Dialog ") + to_wstring(id_);
	} else {
		return wstring(L"Dialog ") + name_;
	}
}

wstring HotkeyPingBuild::GetDescription() {
	return wstring(L"Ping Build #") + to_wstring(index_);
}
