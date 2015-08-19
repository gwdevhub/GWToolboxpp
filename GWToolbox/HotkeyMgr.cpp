#include "HotkeyMgr.h"

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

	GWAPIMgr::GetInstance()->Chat->SendChat(msg.c_str(), channel);
	if (channel == L'/') {
		GWAPIMgr::GetInstance()->Chat->WriteChat((wstring(L"/") + msg).c_str());
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

	// TODO
}

void HotkeyPingBuild::exec() {
	if (isLoading()) return;

	// TODO
}

bool HotkeyMgr::processMessage(LPMSG msg) {
	switch (msg->message) {
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
	{
		bool triggered = false;
		for (TBHotkey* hk : hotkeys) {
			if (!hk->pressed && msg->wParam == hk->key) {
				hk->pressed = true;
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
			if (hk->pressed && msg->wParam == hk->key) {
				hk->pressed = false;
			}
		}
		return false;
		break;

	default:
		return false;
		break;
	}
}
