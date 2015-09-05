#include "Hotkeys.h"
#include "logger.h"

using namespace GWAPI;
using namespace OSHGui;

TBHotkey::TBHotkey(string name, long key, bool active)
	: key_(key) {
	pressed_ = false;
	active_ = active;

	TBHotkey* self = this;
	SetBackColor(Drawing::Color::Empty());

	CheckBox* checkbox = new CheckBox();
	checkbox->SetChecked(active);
	checkbox->SetLocation(0, 0);
	checkbox->GetCheckedChangedEvent() += CheckedChangedEventHandler([self, checkbox](Control*) {
		self->set_active(checkbox->GetChecked());
	});
	AddControl(checkbox);

	Button* hotkey_button = new Button();
	hotkey_button->SetText(std::to_string(key));
	hotkey_button->SetSize(60, 20);
	hotkey_button->SetLocation(checkbox->GetRight(), 0);
	hotkey_button->GetClickEvent() += ClickEventHandler([](Control*) {
		// TODO something
	});
	AddControl(hotkey_button);

	Label* action = new Label();
	action->SetText(name);
	action->SetLocation(hotkey_button->GetRight(), 0);
	AddControl(action);

	// then there will be hotkey-specific items here

	Button* run_button = new Button();
	run_button->SetText("Run");
	run_button->SetSize(60, 20);
	run_button->SetLocation(WIDTH - 60, 0);
	run_button->GetClickEvent() += ClickEventHandler([self](Control*) {
		self->exec();
	});
	AddControl(run_button);
}


HotkeySendChat::HotkeySendChat(long key, bool active, wstring msg, wchar_t channel)
	: TBHotkey("Send Chat", key, active), msg_(msg), channel_(channel) {
	
	string text = string(msg.begin(), msg.end());
	Label* label = new Label();
	label->SetText(text);
	label->SetLocation(100, 0);
	AddControl(label);
	
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
