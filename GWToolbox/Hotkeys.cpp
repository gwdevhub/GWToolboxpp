#include "Hotkeys.h"
#include "logger.h"
#include "GWToolbox.h"
#include "Config.h"

using namespace GWAPI;
using namespace OSHGui;

TBHotkey::TBHotkey(string name, long key, bool active, wstring ini_section)
	: active_(active), key_(key), ini_section_(ini_section) {

	pressed_ = false;

	TBHotkey* self = this;
	SetBackColor(Drawing::Color::Empty());
	SetSize(WIDTH, HEIGHT);

	CheckBox* checkbox = new CheckBox();
	checkbox->SetChecked(active);
	checkbox->SetText("Active");
	checkbox->SetLocation(0, 0);
	checkbox->SetSize(CONTROL_WIDTH, CONTROL_HEIGHT);
	checkbox->SetBackColor(Drawing::Color::Black());
	checkbox->GetCheckedChangedEvent() += CheckedChangedEventHandler([self, checkbox, ini_section](Control*) {
		self->set_active(checkbox->GetChecked());
		GWToolbox::instance()->config()->iniWriteBool(ini_section.c_str(), TBHotkey::IniKeyActive(), checkbox->GetChecked());
	});
	AddControl(checkbox);
	
	Button* hotkey_button = new Button();
	hotkey_button->SetText(std::to_string(key));
	hotkey_button->SetSize(CONTROL_WIDTH, CONTROL_HEIGHT);
	hotkey_button->SetLocation(0, CONTROL_HEIGHT + VSPACE);
	hotkey_button->GetClickEvent() += ClickEventHandler([self](Control*) {
		// TODO set new hotkey
	});
	AddControl(hotkey_button);

	Label* action = new Label();
	action->SetText(name);
	action->SetLocation(CONTROL_WIDTH + HSPACE, VSPACE);
	AddControl(action);

	// then there will be hotkey-specific items here

	Button* run_button = new Button();
	run_button->SetText("Run");
	run_button->SetSize(CONTROL_WIDTH, CONTROL_HEIGHT);
	run_button->SetLocation(WIDTH - CONTROL_WIDTH, 0);
	run_button->GetClickEvent() += ClickEventHandler([self](Control*) {
		self->exec();
	});
	AddControl(run_button);
}


HotkeySendChat::HotkeySendChat(long key, bool active, wstring ini_section, 
	wstring msg, wchar_t channel)
	: TBHotkey("Send Chat", key, active, ini_section), msg_(msg), channel_(channel) {
	
	ComboBox* combo = new ComboBox();
	combo->AddItem("/");
	combo->AddItem("!");
	combo->AddItem("@");
	combo->AddItem("#");
	combo->AddItem("$");
	combo->AddItem("%");
	combo->SetSelectedIndex(ChannelToIndex(channel));
	combo->SetSize(30, CONTROL_HEIGHT);
	combo->SetLocation(CONTROL_WIDTH + HSPACE, CONTROL_HEIGHT + VSPACE);
	HotkeySendChat* self = this;
	combo->GetSelectedIndexChangedEvent() += SelectedIndexChangedEventHandler(
		[self, combo, ini_section](Control*) {
		wchar_t channel = self->IndexToChannel(combo->GetSelectedIndex());
		self->set_channel(channel);
		GWToolbox::instance()->config()->iniWrite(ini_section.c_str(), 
			self->IniKeyChannel(), wstring(1, channel).c_str());
	});
	AddControl(combo);
	
	string text = string(msg.begin(), msg.end());
	TextBox* text_box = new TextBox();
	text_box->SetText(text);
	text_box->SetSize(WIDTH - combo->GetRight() - HSPACE / 2, CONTROL_HEIGHT);
	text_box->SetLocation(combo->GetRight() + HSPACE / 2, combo->GetTop());
	text_box->GetTextChangedEvent() += TextChangedEventHandler(
		[self, text_box, ini_section](Control*) {
		string text = text_box->GetText();
		wstring wtext = wstring(text.begin(), text.end());
		self->set_msg(wtext);
		GWToolbox::instance()->config()->iniWrite(ini_section.c_str(),
			self->IniKeyMsg(), wtext.c_str());
	});
	text_box->GetFocusGotEvent() += FocusGotEventHandler([](Control*) {
		GWToolbox::capture_input = true;
	});
	text_box->GetFocusLostEvent() += FocusLostEventHandler([](Control*, Control*) {
		GWToolbox::capture_input = false;
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
		LOG("Warning - bad channel %lc", channel);
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
		LOG("Warning - bad index %d", index);
		return L'/';
	}
}

void HotkeyUseItem::exec() {
	if (!isExplorable()) return;

	if (GWAPI::GWAPIMgr::GetInstance()->Items->UseItemByModelId(ItemID)) {
		LOG("Hotkey fired: used item %s (ID: %u)", ItemName, ItemID);
	} else {
		wstring msg = wstring(L"[Warning] ") + ItemName + L"not found!";
		GWAPI::GWAPIMgr::GetInstance()->Chat->WriteChat(msg.c_str());
		LOGW(msg.c_str());
	}
}

void HotkeySendChat::exec() {
	if (isLoading()) return;

	GWAPIMgr::GetInstance()->Chat->SendChat(msg_.c_str(), channel_);
	if (channel_ == L'/') {
		GWAPIMgr::GetInstance()->Chat->WriteChat((wstring(L"/") + msg_).c_str());
	}
}

void HotkeyDropUseBuff::exec() {
	if (!isExplorable()) return;

	GWAPIMgr* API = GWAPIMgr::GetInstance();
	EffectMgr::Buff buff = API->Effects->GetPlayerBuffBySkillId(SkillID);
	if (buff.SkillId) {
		API->Effects->DropBuff(buff.BuffId);
	} else {
		int slot = API->Skillbar->getSkillSlot(SkillID);
		if (slot > 0 && API->Skillbar->GetPlayerSkillbar().Skills[slot].Recharge == 0) {
			API->Skillbar->UseSkill(slot, API->Agents->GetTargetId());
		}
	}
}

void HotkeyToggle::exec() {
	// TODO
	LOG("hotkey toggle function invoked - doing nothing atm\n");
}

void HotkeyTarget::exec() {
	if (isLoading()) return;

	GWAPIMgr* API = GWAPIMgr::GetInstance();
	AgentMgr::Agent* me = API->Agents->GetPlayer();
	AgentMgr::AgentArray agents = API->Agents->GetAgentArray();

	unsigned long distance = GwConstants::SqrRange::Compass;
	int closest = -1;

	for (size_t i = 0; i < agents.size(); ++i) {
		if (agents[i]->PlayerNumber == TargetID
			&& agents[i]->HP >= 0) {

			unsigned long newDistance = API->Agents->GetSqrDistance(me, agents[i]);
			if (newDistance < distance) {
				closest = i;
				distance = newDistance;
			}
		}
	}
	if (closest > 0) {
		API->Agents->ChangeTarget(agents[closest]);
	}
}

void HotkeyMove::exec() {
	if (!isExplorable()) return;

	GWAPIMgr* API = GWAPIMgr::GetInstance();
	AgentMgr::Agent* me = API->Agents->GetPlayer();
	double sqrDist = (me->X - x) * (me->X - x) + (me->Y - y) * (me->Y - y);
	if (sqrDist < GwConstants::SqrRange::Compass) {
		API->Agents->Move(x, y);
	}
	GWAPIMgr::GetInstance()->Chat->WriteChat(L"Movement macro activated");
}

void HotkeyDialog::exec() {
	if (isLoading()) return;

	GWAPIMgr::GetInstance()->Agents->Dialog(DialogID);
}

void HotkeyPingBuild::exec() {
	if (isLoading()) return;

	// TODO
}
