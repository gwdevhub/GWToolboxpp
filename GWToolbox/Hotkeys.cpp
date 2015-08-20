#include "Hotkeys.h"
#include "GWToolbox.h"

using namespace GWAPI;

//void Hotkeys::callbackStuck(Hotkeys * self) {
//	if (self->isLoading()) return;
//
//	GWAPIMgr::GetInstance()->Chat->SendChat(L"stuck", L'/');
//}
//
//void Hotkeys::callbackRecall(Hotkeys * self) {
//	if (!isExplorable()) return;
//
//	GWAPIMgr* API = GWAPIMgr::GetInstance();
//	EffectMgr::Buff recall = API->Effects->GetPlayerBuffBySkillId(GwConstants::SkillID::Recall);
//	if (recall.SkillId) {
//		API->Effects->DropBuff(recall.BuffId);
//	} else {
//		int slot = API->Skillbar->getSkillSlot(GwConstants::SkillID::Recall);
//		if (slot > 0 && API->Skillbar->GetPlayerSkillbar().Skills[slot].Recharge == 0) {
//			API->Skillbar->UseSkill(slot, API->Agents->GetTargetId());
//		}
//	}
//}
//
//void Hotkeys::callbackUA(Hotkeys * self) {
//	if (!isExplorable()) return;
//
//	GWAPIMgr* API = GWAPIMgr::GetInstance();
//	EffectMgr::Buff ua = API->Effects->GetPlayerBuffBySkillId(GwConstants::SkillID::UA);
//	if (ua.SkillId) {
//		API->Effects->DropBuff(ua.BuffId);
//	} else {
//		int slot = API->Skillbar->getSkillSlot(GwConstants::SkillID::UA);
//		if (slot > 0 && API->Skillbar->GetPlayerSkillbar().Skills[slot].Recharge == 0) {
//			API->Skillbar->UseSkill(slot, API->Agents->GetTargetId());
//		}
//	}
//}
//
//void Hotkeys::callbackResign(Hotkeys * self) {
//	if (isLoading()) return;
//	GWAPIMgr::GetInstance()->Chat->SendChat(L"resign", L'/');
//	GWAPIMgr::GetInstance()->Chat->WriteChat(L"/resign");
//}
//
//void Hotkeys::callbackTeamResign(Hotkeys * self) {
//	if (isLoading()) return;
//	GWAPIMgr::GetInstance()->Chat->SendChat(L"[/resign;xx]", L'#');
//}
//
//void Hotkeys::callbackClicker(Hotkeys * self) {
//	bool status = GWToolbox::getInstance()->hotkeys->toggleClicker();
//
//	if (!isLoading()) {
//		GWAPIMgr::GetInstance()->Chat->WriteChat(status ? L"Clicker enabled" : L"Clicker disabled");
//	}
//}
//
//void Hotkeys::callbackRes(Hotkeys * self) {
//	if (isExplorable()
//		&& !GWAPIMgr::GetInstance()->Items->UseItemByModelId(GwConstants::ItemID::ResScrolls)) {
//		GWAPIMgr::GetInstance()->Chat->WriteChat(L"[Warning] Res scroll not found!");
//	}
//}
//
//void Hotkeys::callbackAge(Hotkeys * self) {
//	if (isLoading()) return;
//	GWAPIMgr::GetInstance()->Chat->SendChat(L"age", L'/');
//}
//
//void Hotkeys::callbackPstone(Hotkeys * self) {
//	if (isExplorable()
//		&& !GWAPIMgr::GetInstance()->Items->UseItemByModelId(GwConstants::ItemID::Powerstone)) {
//		GWAPIMgr::GetInstance()->Chat->WriteChat(L"[Warning] Powerstone not found!");
//	}
//}
//
//void Hotkeys::callbackGhostTarget(Hotkeys * self) {
//	if (isLoading()) return;
//
//	GWAPIMgr* API = GWAPIMgr::GetInstance();
//	AgentMgr::Agent* me = API->Agents->GetPlayer();
//	AgentMgr::AgentArray agents = API->Agents->GetAgentArray();
//
//	unsigned long distance = GwConstants::SqrRange::Compass;
//	int closest = -1;
//
//	for (size_t i = 0; i < agents.size(); ++i) {
//		if (agents[i]->PlayerNumber == GwConstants::ModelID::Boo
//			&& agents[i]->HP >= 0) {
//
//			unsigned long newDistance = API->Agents->GetSqrDistance(me, agents[i]);
//			if (newDistance < distance) {
//				closest = i;
//				distance = newDistance;
//			}
//		}
//	}
//	if (closest > 0) {
//		API->Agents->ChangeTarget(agents[closest]);
//	}
//}
//
//void Hotkeys::callbackGhostPop(Hotkeys * self) {
//	if (!isLoading()
//		&& !GWAPIMgr::GetInstance()->Items->UseItemByModelId(GwConstants::ItemID::GhostInTheBox)) {
//		GWAPIMgr::GetInstance()->Chat->WriteChat(L"[Warning] Ghost-in-the-box not found!");
//	}
//}
//
//void Hotkeys::callbackGstonePop(Hotkeys * self) {
//	if (isExplorable()
//		&& !GWAPIMgr::GetInstance()->Items->UseItemByModelId(GwConstants::ItemID::GhastlyStone)) {
//		GWAPIMgr::GetInstance()->Chat->WriteChat(L"[Warning] Ghastly Summoning Stone not found!");
//	}
//}
//
//void Hotkeys::callbackLegioPop(Hotkeys * self) {
//	if (isExplorable()
//		&& !GWAPIMgr::GetInstance()->Items->UseItemByModelId(GwConstants::ItemID::LegionnaireStone)) {
//		GWAPIMgr::GetInstance()->Chat->WriteChat(L"[Warning] Legionnaire Summoning Crystal not found!");
//	}
//}
//
//void Hotkeys::callbackRainbowUse(Hotkeys * self) {
//	if (!isExplorable()) return;
//
//	GWAPIMgr* API = GWAPIMgr::GetInstance();
//
//	if (API->Effects->GetPlayerEffectById(GwConstants::Effect::Redrock).SkillId == 0) {
//		if (!API->Items->UseItemByModelId(GwConstants::ItemID::RRC)) {
//			API->Chat->WriteChat(L"[Warning] Red Rock Candy not found!");
//		}
//	}
//
//	if (API->Effects->GetPlayerEffectById(GwConstants::Effect::Bluerock).SkillId == 0) {
//		if (!API->Items->UseItemByModelId(GwConstants::ItemID::BRC)) {
//			API->Chat->WriteChat(L"[Warning] Blue Rock Candy not found!");
//		}
//	}
//
//	if (API->Effects->GetPlayerEffectById(GwConstants::Effect::Greenrock).SkillId == 0) {
//		if (!API->Items->UseItemByModelId(GwConstants::ItemID::GRC)) {
//			API->Chat->WriteChat(L"[Warning] Green Rock Candy not found!");
//		}
//	}
//}
//
//void Hotkeys::callbackIdentifier(Hotkeys * self) {
//	// TODO
//}
//
//void Hotkeys::callbackRupt(Hotkeys * self) {
//	bool status = GWToolbox::getInstance()->hotkeys->toggleRupt();
//
//	if (!isLoading()) {
//		GWAPIMgr::GetInstance()->Chat->WriteChat(status ? L"Rupt enabled" : L"Rupt disabled");
//	}
//}
//
//void Hotkeys::callbackMovement(Hotkeys * self) {
//	if (isLoading()) return;
//	if (self->movementX == 0 && self->movementY == 0) return;
//
//	GWAPIMgr::GetInstance()->Agents->Move(self->movementX, self->movementY);
//	GWAPIMgr::GetInstance()->Chat->WriteChat(L"Movement macro activated");
//}
//
//void Hotkeys::callbackDrop1Coin(Hotkeys * self) {
//	if (!isExplorable()) return;
//
//	GWAPIMgr::GetInstance()->Items->DropGold();
//}
//
//void Hotkeys::callbackDropCoins(Hotkeys * self) {
//	bool status = GWToolbox::getInstance()->hotkeys->toggleCoinDrop();
//
//	if (!isLoading()) {
//		GWAPIMgr::GetInstance()->Chat->WriteChat(status ? L"Coin dropper enabled" : L"Coin dropper disabled");
//	}
//}

Hotkeys::Hotkeys() {
	clickerTimer = TBTimer::init();
	dropCoinsTimer = TBTimer::init();
	hotkeys = vector<UIHotkey>();
}

Hotkeys::~Hotkeys() {
}

void Hotkeys::loadIni() {
	Config* config = GWToolbox::getInstance()->config;

	list<wstring> sections = config->iniReadSections();
	for (wstring section : sections) {
		if (section.compare(0, 6, L"hotkey") == 0) {
			size_t first_sep = 6;
			size_t second_sep = section.find(L':', first_sep);

			wstring id = section.substr(first_sep + 1, second_sep - first_sep - 1);
			wstring type = section.substr(second_sep + 1);
			wstring name = config->iniRead(section.c_str(), L"name", L"");
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
				UINT ItemID = config->iniReadLong(section.c_str(), HotkeyUseItem::iniItemIDKey() , 0);
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
				GWToolbox::getInstance()->hotkeyMgr->add(tb_hk);
				hotkeys.push_back(UIHotkey(name, type, id, tb_hk));
			}
		}
	}
}

OSHGui::Panel* Hotkeys::buildUI() {
	OSHGui::Panel* p = new Panel();
	p->SetBackColor(Drawing::Color(1, 0, 1, 1));
	return p;
}

void Hotkeys::mainRoutine() {

}
