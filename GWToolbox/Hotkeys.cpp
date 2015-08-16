#include "Hotkeys.h"

using namespace GWAPI;

void Hotkeys::callbackStuck() {
	if (isLoading()) return;

	GWAPIMgr::GetInstance()->Chat->SendChat(L"stuck", L'/');
}

void Hotkeys::callbackRecall() {
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

void Hotkeys::callbackUA() {
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

void Hotkeys::callbackResign() {
	if (isLoading()) return;
	GWAPIMgr::GetInstance()->Chat->SendChat(L"resign", L'/');
	GWAPIMgr::GetInstance()->Chat->WriteToChat(L"/resign");
}

void Hotkeys::callbackTeamResign() {
	if (isLoading()) return;
	GWAPIMgr::GetInstance()->Chat->SendChat(L"[/resign;xx]", L'#');
}

void Hotkeys::callbackClicker() {
	clickerToggle = !clickerToggle;
	if (!isLoading()) {
		GWAPIMgr::GetInstance()->Chat->WriteToChat(clickerToggle ? L"Clicker enabled" : L"Clicker disabled");
	}
}

void Hotkeys::callbackRes() {
	if (!isExplorable()) return;
	if (!GWAPIMgr::GetInstance()->Items->UseItemByModelId(GwConstants::ItemID::ResScrolls)) {
		GWAPIMgr::GetInstance()->Chat->WriteToChat(L"[Warning] Res scroll not found!");
	}
}

void Hotkeys::callbackAge() {
	if (isLoading()) return;
	GWAPIMgr::GetInstance()->Chat->SendChat(L"age", L'/');
}

void Hotkeys::callbackPstone() {
	if (!isExplorable()) return;
	if (!GWAPIMgr::GetInstance()->Items->UseItemByModelId(GwConstants::ItemID::Powerstone)) {
		GWAPIMgr::GetInstance()->Chat->WriteToChat(L"[Warning] Powerstone not found!");
	}
}

void Hotkeys::callbackGhostTarget() {
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

void Hotkeys::callbackGhostPop() {
	if (isLoading()) return;
	if (!GWAPIMgr::GetInstance()->Items->UseItemByModelId(GwConstants::ItemID::GhostInTheBox)) {
		GWAPIMgr::GetInstance()->Chat->WriteToChat(L"[Warning] Ghost-in-the-box not found!");
	}
}

void Hotkeys::callbackGstonePop() {

}

void Hotkeys::callbackLegioPop() {

}

void Hotkeys::callbackRainbowUse() {

}

void Hotkeys::callbackLooter() {

}

void Hotkeys::callbackIdentifier() {

}

void Hotkeys::callbackRupt() {

}

void Hotkeys::callbackMovement() {

}

void Hotkeys::callbackDrop1Coin() {

}

void Hotkeys::callbackDropCoins() {

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
Looter(initializer++),
Identifier(initializer++),
Rupt(initializer++),
Movement(initializer++),
Drop1Coin(initializer++),
DropCoins(initializer++),
count(initializer)
{
	clickerTimer = Timer::init();
	dropCoinsTimer = Timer::init();

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
	hotkeyName[Hotkeys::Looter] = "looter";
	hotkeyName[Hotkeys::Identifier] = "identifier";
	hotkeyName[Hotkeys::Rupt] = "rupt";
	hotkeyName[Hotkeys::Movement] = "movement";
	hotkeyName[Hotkeys::Drop1Coin] = "drop1coin";
	hotkeyName[Hotkeys::DropCoins] = "dropcoins";

	callbacks = vector<void(Hotkeys::*)()>(Hotkeys::count);
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
	callbacks[Hotkeys::Looter] = &Hotkeys::callbackLooter;
	callbacks[Hotkeys::Identifier] = &Hotkeys::callbackIdentifier;
	callbacks[Hotkeys::Rupt] = &Hotkeys::callbackRupt;
	callbacks[Hotkeys::Movement] = &Hotkeys::callbackMovement;
	callbacks[Hotkeys::Drop1Coin] = &Hotkeys::callbackDrop1Coin;
	callbacks[Hotkeys::DropCoins] = &Hotkeys::callbackDropCoins;
}

void Hotkeys::loadIni() {
	// TODO
}

void Hotkeys::buildUI() {
	// TODO
}

void Hotkeys::mainRoutine() {
	// TODO
}