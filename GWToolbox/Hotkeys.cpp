#include "Hotkeys.h"

using namespace GWAPI;

void Hotkeys::callbackStuck(Hotkeys * self) {
	if (self->isLoading()) return;

	GWAPIMgr::GetInstance()->Chat->SendChat(L"stuck", L'/');
}

void Hotkeys::callbackRecall(Hotkeys * self) {
	if (!isExplorable()) return;

	GWAPIMgr* API = GWAPIMgr::GetInstance();
	EffectMgr::Buff recall = API->Effects->GetPlayerBuffBySkillId(GwConstants::SkillID::Recall);
	if (recall.SkillId) {
		API->Effects->DropBuff(recall.BuffId);
	} else {
		int slot = API->Skillbar->getSkillSlot(GwConstants::SkillID::Recall);
		if (slot > 0 && API->Skillbar->GetPlayerSkillbar().Skills[slot].Recharge == 0) {
			API->Skillbar->UseSkill(slot, API->Agents->GetTargetId());
		}
	}
}

void Hotkeys::callbackUA(Hotkeys * self) {
	if (!isExplorable()) return;

	GWAPIMgr* API = GWAPIMgr::GetInstance();
	EffectMgr::Buff ua = API->Effects->GetPlayerBuffBySkillId(GwConstants::SkillID::UA);
	if (ua.SkillId) {
		API->Effects->DropBuff(ua.BuffId);
	} else {
		int slot = API->Skillbar->getSkillSlot(GwConstants::SkillID::UA);
		if (slot > 0 && API->Skillbar->GetPlayerSkillbar().Skills[slot].Recharge == 0) {
			API->Skillbar->UseSkill(slot, API->Agents->GetTargetId());
		}
	}
}

void Hotkeys::callbackResign(Hotkeys * self) {
	if (isLoading()) return;
	GWAPIMgr::GetInstance()->Chat->SendChat(L"resign", L'/');
	GWAPIMgr::GetInstance()->Chat->WriteChat(L"/resign");
}

void Hotkeys::callbackTeamResign(Hotkeys * self) {
	if (isLoading()) return;
	GWAPIMgr::GetInstance()->Chat->SendChat(L"[/resign;xx]", L'#');
}

void Hotkeys::callbackClicker(Hotkeys * self) {
	self->clickerToggle = !self->clickerToggle;
	if (!isLoading()) {
		GWAPIMgr::GetInstance()->Chat->WriteChat(self->clickerToggle ? L"Clicker enabled" : L"Clicker disabled");
	}
}

void Hotkeys::callbackRes(Hotkeys * self) {
	if (isExplorable()
		&& !GWAPIMgr::GetInstance()->Items->UseItemByModelId(GwConstants::ItemID::ResScrolls)) {
		GWAPIMgr::GetInstance()->Chat->WriteChat(L"[Warning] Res scroll not found!");
	}
}

void Hotkeys::callbackAge(Hotkeys * self) {
	if (isLoading()) return;
	GWAPIMgr::GetInstance()->Chat->SendChat(L"age", L'/');
}

void Hotkeys::callbackPstone(Hotkeys * self) {
	if (isExplorable()
		&& !GWAPIMgr::GetInstance()->Items->UseItemByModelId(GwConstants::ItemID::Powerstone)) {
		GWAPIMgr::GetInstance()->Chat->WriteChat(L"[Warning] Powerstone not found!");
	}
}

void Hotkeys::callbackGhostTarget(Hotkeys * self) {
	if (isLoading()) return;

	GWAPIMgr* API = GWAPIMgr::GetInstance();
	AgentMgr::Agent* me = API->Agents->GetPlayer();
	AgentMgr::AgentArray agents = API->Agents->GetAgentArray();

	unsigned long distance = GwConstants::SqrRange::Compass;
	int closest = -1;

	for (size_t i = 0; i < agents.size(); ++i) {
		if (agents[i]->PlayerNumber == GwConstants::ModelID::Boo
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

void Hotkeys::callbackGhostPop(Hotkeys * self) {
	if (!isLoading()
		&& !GWAPIMgr::GetInstance()->Items->UseItemByModelId(GwConstants::ItemID::GhostInTheBox)) {
		GWAPIMgr::GetInstance()->Chat->WriteChat(L"[Warning] Ghost-in-the-box not found!");
	}
}

void Hotkeys::callbackGstonePop(Hotkeys * self) {
	if (isExplorable()
		&& !GWAPIMgr::GetInstance()->Items->UseItemByModelId(GwConstants::ItemID::GhastlyStone)) {
		GWAPIMgr::GetInstance()->Chat->WriteChat(L"[Warning] Ghastly Summoning Stone not found!");
	}
}

void Hotkeys::callbackLegioPop(Hotkeys * self) {
	if (isExplorable()
		&& !GWAPIMgr::GetInstance()->Items->UseItemByModelId(GwConstants::ItemID::LegionnaireStone)) {
		GWAPIMgr::GetInstance()->Chat->WriteChat(L"[Warning] Legionnaire Summoning Crystal not found!");
	}
}

void Hotkeys::callbackRainbowUse(Hotkeys * self) {
	if (!isExplorable()) return;

	GWAPIMgr* API = GWAPIMgr::GetInstance();

	if (API->Effects->GetPlayerEffectById(GwConstants::Effect::Redrock).SkillId == 0) {
		if (!API->Items->UseItemByModelId(GwConstants::ItemID::RRC)) {
			API->Chat->WriteChat(L"[Warning] Red Rock Candy not found!");
		}
	}

	if (API->Effects->GetPlayerEffectById(GwConstants::Effect::Bluerock).SkillId == 0) {
		if (!API->Items->UseItemByModelId(GwConstants::ItemID::BRC)) {
			API->Chat->WriteChat(L"[Warning] Blue Rock Candy not found!");
		}
	}

	if (API->Effects->GetPlayerEffectById(GwConstants::Effect::Greenrock).SkillId == 0) {
		if (!API->Items->UseItemByModelId(GwConstants::ItemID::GRC)) {
			API->Chat->WriteChat(L"[Warning] Green Rock Candy not found!");
		}
	}
}

void Hotkeys::callbackIdentifier(Hotkeys * self) {
	// TODO
}

void Hotkeys::callbackRupt(Hotkeys * self) {
	self->ruptToggle = !self->ruptToggle;

	if (!isLoading()) {
		GWAPIMgr::GetInstance()->Chat->WriteChat(self->clickerToggle ? L"Rupt enabled" : L"Rupt disabled");
	}
}

void Hotkeys::callbackMovement(Hotkeys * self) {
	if (isLoading()) return;
	if (self->movementX == 0 && self->movementY == 0) return;

	GWAPIMgr::GetInstance()->Agents->Move(self->movementX, self->movementY);
	GWAPIMgr::GetInstance()->Chat->WriteChat(L"Movement macro activated");
}

void Hotkeys::callbackDrop1Coin(Hotkeys * self) {
	if (!isExplorable()) return;

	GWAPIMgr::GetInstance()->Items->DropGold();
}

void Hotkeys::callbackDropCoins(Hotkeys * self) {
	self->dropCoinsToggle = !self->dropCoinsToggle;

	if (!isLoading()) {
		GWAPIMgr::GetInstance()->Chat->WriteChat(self->clickerToggle ? L"Coin dropper enabled" : L"Coin dropper disabled");
	}
}

Hotkeys::Hotkeys() :
initializer(0),
Stuck(initializer++),
Recall(initializer++),
UA(initializer++),
Resign(initializer++),
TeamResign(initializer++),
Clicker(initializer++),
Res(initializer++),
Age(initializer++),
Pstone(initializer++),
GhostTarget(initializer++),
GhostPop(initializer++),
GstonePop(initializer++),
LegioPop(initializer++),
RainbowUse(initializer++),
Identifier(initializer++),
Rupt(initializer++),
Movement(initializer++),
Drop1Coin(initializer++),
DropCoins(initializer++),
count(initializer)
{
	clickerTimer = TBTimer::init();
	dropCoinsTimer = TBTimer::init();

	hotkeyName = vector<string>(Hotkeys::count);
	hotkeyName[Hotkeys::Stuck] = "stuck";
	hotkeyName[Hotkeys::Recall] = "recall";
	hotkeyName[Hotkeys::UA] = "ua";
	hotkeyName[Hotkeys::Resign] = "resign";
	hotkeyName[Hotkeys::TeamResign] = "teamresign";
	hotkeyName[Hotkeys::Clicker] = "clicker";
	hotkeyName[Hotkeys::Res] = "res";
	hotkeyName[Hotkeys::Age] = "age";
	hotkeyName[Hotkeys::Pstone] = "pstone";
	hotkeyName[Hotkeys::GhostTarget] = "ghosttarget";
	hotkeyName[Hotkeys::GhostPop] = "ghostpop";
	hotkeyName[Hotkeys::GstonePop] = "gstonepop";
	hotkeyName[Hotkeys::LegioPop] = "legiopop";
	hotkeyName[Hotkeys::RainbowUse] = "rainbowpop";
	hotkeyName[Hotkeys::Identifier] = "identifier";
	hotkeyName[Hotkeys::Rupt] = "rupt";
	hotkeyName[Hotkeys::Movement] = "movement";
	hotkeyName[Hotkeys::Drop1Coin] = "drop1coin";
	hotkeyName[Hotkeys::DropCoins] = "dropcoins";

	callbacks = std::vector<void(*)(Hotkeys*)>(Hotkeys::count);
	callbacks[Hotkeys::Stuck] = &Hotkeys::callbackStuck;
	callbacks[Hotkeys::Recall] = &Hotkeys::callbackRecall;
	callbacks[Hotkeys::UA] = &Hotkeys::callbackUA;
	callbacks[Hotkeys::Resign] = &Hotkeys::callbackResign;
	callbacks[Hotkeys::TeamResign] = &Hotkeys::callbackTeamResign;
	callbacks[Hotkeys::Clicker] = &Hotkeys::callbackClicker;
	callbacks[Hotkeys::Res] = &Hotkeys::callbackRes;
	callbacks[Hotkeys::Age] = &Hotkeys::callbackAge;
	callbacks[Hotkeys::Pstone] = &Hotkeys::callbackPstone;
	callbacks[Hotkeys::GhostTarget] = &Hotkeys::callbackGhostTarget;
	callbacks[Hotkeys::GhostPop] = &Hotkeys::callbackGhostPop;
	callbacks[Hotkeys::GstonePop] = &Hotkeys::callbackGstonePop;
	callbacks[Hotkeys::LegioPop] = &Hotkeys::callbackLegioPop;
	callbacks[Hotkeys::RainbowUse] = &Hotkeys::callbackRainbowUse;
	callbacks[Hotkeys::Identifier] = &Hotkeys::callbackIdentifier;
	callbacks[Hotkeys::Rupt] = &Hotkeys::callbackRupt;
	callbacks[Hotkeys::Movement] = &Hotkeys::callbackMovement;
	callbacks[Hotkeys::Drop1Coin] = &Hotkeys::callbackDrop1Coin;
	callbacks[Hotkeys::DropCoins] = &Hotkeys::callbackDropCoins;
}

Hotkeys::~Hotkeys() {
}

void Hotkeys::loadIni() {
	// TODO
}

void Hotkeys::buildUI() {
	// TODO
}

void Hotkeys::mainRoutine() {
	if (GetAsyncKeyState(VK_INSERT) & 1) {
		callbacks[Hotkeys::Age](this);
	}
}
