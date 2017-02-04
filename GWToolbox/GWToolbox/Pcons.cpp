#include "Pcons.h"

#include <GWCA\GWCA.h>
#include <GWCA\Managers\EffectMgr.h>
#include <GWCA\Managers\ItemMgr.h>
#include <GWCA\Managers\PartyMgr.h>

#include "ChatLogger.h"
#include "Config.h"
#include "GuiUtils.h"

using namespace OSHGui;
using namespace OSHGui::Drawing;
using namespace GW::Constants;

Pcon::Pcon(OSHGui::Control* parent, const wchar_t* ini) : Button(parent),
	pic(new PictureBox(this)),
	tick(new PictureBox(this)),
	shadow(new Label(this)),
	itemID(0),
	effectID(SkillID::No_Skill),
	threshold(0),
	timer(TBTimer::init()),
	update_timer(0),
	quantity(-1), /* to force a redraw when first created */
	chatName(ini), // will be set later, but its a good temporary value
	iniName(ini) {

	enabled = Config::IniReadBool(L"pcons", ini, false);;

	tick->SetBackColor(Drawing::Color::Empty());
	tick->SetStretch(true);
	tick->SetEnabled(false);
	tick->SetLocation(PointI(0, 0));
	tick->SetSize(SizeI(WIDTH, HEIGHT));
	tick->SetImage(Drawing::Image::FromFile(GuiUtils::getSubPathA("Tick.png", "img")));
	AddControl(tick);

	pic->SetBackColor(Drawing::Color::Empty());
	pic->SetStretch(true);
	pic->SetEnabled(false);
	AddControl(pic);

	int text_x = 5;
	int text_y = 3;
	shadow->SetText(std::to_wstring(quantity));
	shadow->SetFont(GuiUtils::getTBFont(11.0f, true));
	shadow->SetForeColor(Drawing::Color::Black());
	shadow->SetLocation(PointI(text_x + 1, text_y + 1));
	AddControl(shadow);

	label_->SetText(std::to_wstring(quantity));
	label_->SetFont(GuiUtils::getTBFont(11.0f, true));
	label_->SetLocation(PointI(text_x, text_y));
	AddControl(label_);

	SetSize(SizeI(WIDTH, HEIGHT));
	SetBackColor(Drawing::Color::Empty());
	SetMouseOverFocusColor(GuiUtils::getMouseOverColor());

	GetClickEvent() += ClickEventHandler([this](Control*) { this->toggleActive(); });
}

void Pcon::toggleActive() {
	enabled = !enabled;
	scanInventory();
	update_ui = true;
	Config::IniWriteBool(L"pcons", iniName, enabled);
}

void Pcon::setIcon(const char* icon, int xOff, int yOff, int size) {
	pic->SetSize(SizeI(size, size));
	pic->SetLocation(PointI(xOff, yOff));
	pic->SetImage(Drawing::Image::FromFile(GuiUtils::getSubPathA(icon, "img")));
}

void Pcon::UpdateUI() {
	if (update_ui) {
		label_->SetText(std::to_wstring(quantity));
		shadow->SetText(std::to_wstring(quantity));

		if (quantity == 0) {
			label_->SetForeColor(Color(1.0, 1.0, 0.0, 0.0));
		} else if (quantity < threshold) {
			label_->SetForeColor(Color(1.0, 1.0, 1.0, 0.0));
		} else {
			label_->SetForeColor(Color(1.0, 0.0, 1.0, 0.0));
		}

		tick->SetVisible(enabled);

		update_ui = false;
	}
}

void Pcon::CheckUpdateTimer() {
	if (update_timer != 0 && TBTimer::diff(update_timer) > 2000) {
		bool old_enabled = enabled;
		this->scanInventory();
		update_timer = 0;
		if (old_enabled != enabled) {
			ChatLogger::ErrF(L"Cannot find %ls", chatName);
		}
	}
}

bool Pcon::checkAndUse() {
	CheckUpdateTimer();

	if (enabled && TBTimer::diff(this->timer) > 5000) {

		GW::Effect effect = GW::Effects().GetPlayerEffectById(effectID);

		if (effect.SkillId == 0 || effect.GetTimeRemaining() < 1000) {
			bool used = GW::Items().UseItemByModelId(itemID);
			if (used) {
				this->timer = TBTimer::init();
				this->update_timer = TBTimer::init();
			} else {
				// this should never happen, it should be disabled before
				ChatLogger::ErrF(L"Cannot find %ls", chatName);
				this->scanInventory();
			}
			return used;
		}
	}
	return false;
}

bool PconCons::checkAndUse() {
	CheckUpdateTimer();

	if (enabled && TBTimer::diff(this->timer) > 5000) {
		GW::Effect effect = GW::Effects().GetPlayerEffectById(effectID);
		if (effect.SkillId == 0 || effect.GetTimeRemaining() < 1000) {

			if (!GW::Partymgr().GetIsPartyLoaded()) return false;

			GW::MapAgentArray mapAgents = GW::Agents().GetMapAgentArray();
			if (!mapAgents.valid()) return false;
			int n_players = GW::Agents().GetAmountOfPlayersInInstance();
			for (int i = 1; i <= n_players; ++i) {
				DWORD currentPlayerAgID = GW::Agents().GetAgentIdByLoginNumber(i);
				if (currentPlayerAgID <= 0) return false;
				if (currentPlayerAgID >= mapAgents.size()) return false;
				if (mapAgents[currentPlayerAgID].GetIsDead()) return false;
			}

			bool used = GW::Items().UseItemByModelId(itemID);
			if (used) {
				this->timer = TBTimer::init();
				this->update_timer = TBTimer::init();
			} else {
				ChatLogger::ErrF(L"Cannot find %ls", chatName);
				this->scanInventory();
			}
			return used;
		}
	}
	return false;
}

bool PconCity::checkAndUse() {
	CheckUpdateTimer();

	if (enabled	&& TBTimer::diff(this->timer) > 5000) {
		if (GW::Agents().GetPlayer() &&
			(GW::Agents().GetPlayer()->MoveX > 0 || GW::Agents().GetPlayer()->MoveY > 0)) {
			if (GW::Effects().GetPlayerEffectById(SkillID::Sugar_Rush_short).SkillId
				|| GW::Effects().GetPlayerEffectById(SkillID::Sugar_Rush_long).SkillId
				|| GW::Effects().GetPlayerEffectById(SkillID::Sugar_Jolt_short).SkillId
				|| GW::Effects().GetPlayerEffectById(SkillID::Sugar_Jolt_long).SkillId) {

				// then we have effect on already, do nothing
			} else {
				// we should use it. Because of logical-OR only the first one will be used
				bool used = GW::Items().UseItemByModelId(ItemID::CremeBrulee)
					|| GW::Items().UseItemByModelId(ItemID::ChocolateBunny)
					|| GW::Items().UseItemByModelId(ItemID::Fruitcake)
					|| GW::Items().UseItemByModelId(ItemID::SugaryBlueDrink)
					|| GW::Items().UseItemByModelId(ItemID::RedBeanCake)
					|| GW::Items().UseItemByModelId(ItemID::JarOfHoney);
				if (used) {
					this->timer = TBTimer::init();
					this->update_timer = TBTimer::init();
				} else {
					ChatLogger::Err(L"Cannot find a city speedboost");
					this->scanInventory();
				}
				return used;
			}
		}
	}
	return false;
}

bool PconAlcohol::checkAndUse() {
	CheckUpdateTimer();

	if (enabled && TBTimer::diff(this->timer) > 5000) {
		if (GW::Effects().GetAlcoholLevel() <= 1) {
			// use an alcohol item. Because of logical-OR only the first one will be used
			bool used = GW::Items().UseItemByModelId(ItemID::Eggnog)
				|| GW::Items().UseItemByModelId(ItemID::DwarvenAle)
				|| GW::Items().UseItemByModelId(ItemID::HuntersAle)
				|| GW::Items().UseItemByModelId(ItemID::Absinthe)
				|| GW::Items().UseItemByModelId(ItemID::WitchsBrew)
				|| GW::Items().UseItemByModelId(ItemID::Ricewine)
				|| GW::Items().UseItemByModelId(ItemID::ShamrockAle)
				|| GW::Items().UseItemByModelId(ItemID::Cider)

				|| GW::Items().UseItemByModelId(ItemID::Grog)
				|| GW::Items().UseItemByModelId(ItemID::SpikedEggnog)
				|| GW::Items().UseItemByModelId(ItemID::AgedDwarvenAle)
				|| GW::Items().UseItemByModelId(ItemID::AgedHungersAle)
				|| GW::Items().UseItemByModelId(ItemID::Keg)
				|| GW::Items().UseItemByModelId(ItemID::FlaskOfFirewater)
				|| GW::Items().UseItemByModelId(ItemID::KrytanBrandy);
			if (used) {
				this->timer = TBTimer::init();
				this->update_timer = TBTimer::init();
			} else {
				ChatLogger::Err(L"Cannot find Alcohol");
				this->scanInventory();
			}
			return used;
		}
	}
	return false;
}

bool PconLunar::checkAndUse() {
	CheckUpdateTimer();

	if (enabled	&& TBTimer::diff(this->timer) > 500) {
		if (GW::Effects().GetPlayerEffectById(SkillID::Lunar_Blessing).SkillId == 0) {
			bool used = GW::Items().UseItemByModelId(ItemID::LunarRat)
				|| GW::Items().UseItemByModelId(ItemID::LunarOx)
				|| GW::Items().UseItemByModelId(ItemID::LunarTiger)
				|| GW::Items().UseItemByModelId(ItemID::LunarDragon)
				|| GW::Items().UseItemByModelId(ItemID::LunarHorse)
				|| GW::Items().UseItemByModelId(ItemID::LunarRabbit)
				|| GW::Items().UseItemByModelId(ItemID::LunarSheep)
				|| GW::Items().UseItemByModelId(ItemID::LunarSnake)
				|| GW::Items().UseItemByModelId(ItemID::LunarMonkey)
				|| GW::Items().UseItemByModelId(ItemID::LunarRooster);
			if (used) {
				this->timer = TBTimer::init();
				this->update_timer = TBTimer::init();
			} else {
				ChatLogger::Err(L"Cannot find Lunar Fortunes");
				this->scanInventory();
			}
			return used;
		}
	}
	return false;
}

void Pcon::scanInventory() {
	int old_quantity = quantity;
	bool old_enabled = enabled;

	quantity = 0;
	GW::Bag** bags = GW::Items().GetBagArray();
	if (bags != nullptr) {
		GW::Bag* bag = NULL;
		for (int bagIndex = 1; bagIndex <= 4; ++bagIndex) {
			bag = bags[bagIndex];
			if (bag != NULL) {
				GW::ItemArray items = bag->Items;
				for (size_t i = 0; i < items.size(); i++) {
					if (items[i]) {
						if (items[i]->ModelId == itemID) {
							quantity += items[i]->Quantity;
						}
					}
				}
			}
		}
	}

	enabled = enabled && quantity > 0;
	
	if (old_quantity != quantity || old_enabled != enabled) update_ui = true;
}

void PconCity::scanInventory() {
	int old_quantity = quantity;
	bool old_enabled = enabled;

	quantity = 0;
	GW::Bag** bags = GW::Items().GetBagArray();
	if (bags != nullptr) {
		GW::Bag* bag = NULL;
		for (int bagIndex = 1; bagIndex <= 4; ++bagIndex) {
			bag = bags[bagIndex];
			if (bag != NULL) {
				GW::ItemArray items = bag->Items;
				for (size_t i = 0; i < items.size(); i++) {
					if (items[i]) {
						if (items[i]->ModelId == ItemID::CremeBrulee
							|| items[i]->ModelId == ItemID::SugaryBlueDrink
							|| items[i]->ModelId == ItemID::ChocolateBunny
							|| items[i]->ModelId == ItemID::RedBeanCake) {

							quantity += items[i]->Quantity;
						}
					}
				}
			}
		}
	}

	enabled = enabled && quantity > 0;

	if (old_quantity != quantity || old_enabled != enabled) update_ui = true;
}

void PconAlcohol::scanInventory() {
	int old_quantity = quantity;
	bool old_enabled = enabled;

	quantity = 0;
	GW::Bag** bags = GW::Items().GetBagArray();
	if (bags != nullptr) {
		GW::Bag* bag = NULL;
		for (int bagIndex = 1; bagIndex <= 4; ++bagIndex) {
			bag = bags[bagIndex];
			if (bag != NULL) {
				GW::ItemArray items = bag->Items;
				for (size_t i = 0; i < items.size(); i++) {
					if (items[i]) {
						if (items[i]->ModelId == ItemID::Eggnog
							|| items[i]->ModelId == ItemID::DwarvenAle
							|| items[i]->ModelId == ItemID::HuntersAle
							|| items[i]->ModelId == ItemID::Absinthe
							|| items[i]->ModelId == ItemID::WitchsBrew
							|| items[i]->ModelId == ItemID::Ricewine
							|| items[i]->ModelId == ItemID::ShamrockAle
							|| items[i]->ModelId == ItemID::Cider) {

							quantity += items[i]->Quantity;
						} else if (items[i]->ModelId == ItemID::Grog
							|| items[i]->ModelId == ItemID::SpikedEggnog
							|| items[i]->ModelId == ItemID::AgedDwarvenAle
							|| items[i]->ModelId == ItemID::AgedHungersAle
							|| items[i]->ModelId == ItemID::Keg
							|| items[i]->ModelId == ItemID::FlaskOfFirewater
							|| items[i]->ModelId == ItemID::KrytanBrandy) {

							quantity += items[i]->Quantity * 5;
						}
					}
				}
			}
		}
	}

	enabled = enabled && quantity > 0;
	
	if (old_quantity != quantity || old_enabled != enabled) update_ui = true;
}

void PconLunar::scanInventory() {
	int old_quantity = quantity;
	bool old_enabled = enabled;

	quantity = 0;
	GW::Bag** bags = GW::Items().GetBagArray();
	if (bags != nullptr) {
		GW::Bag* bag = NULL;
		for (int bagIndex = 1; bagIndex <= 4; ++bagIndex) {
			bag = bags[bagIndex];
			if (bag != NULL) {
				GW::ItemArray items = bag->Items;
				for (size_t i = 0; i < items.size(); i++) {
					if (items[i]) {
						if (items[i]->ModelId == ItemID::LunarRat
							|| items[i]->ModelId == ItemID::LunarOx
							|| items[i]->ModelId == ItemID::LunarTiger
							|| items[i]->ModelId == ItemID::LunarDragon
							|| items[i]->ModelId == ItemID::LunarHorse
							|| items[i]->ModelId == ItemID::LunarRabbit
							|| items[i]->ModelId == ItemID::LunarSheep
							|| items[i]->ModelId == ItemID::LunarSnake
							|| items[i]->ModelId == ItemID::LunarMonkey
							|| items[i]->ModelId == ItemID::LunarRooster) {

							quantity += items[i]->Quantity;
						}
					}
				}
			}
		}
	}

	enabled = enabled && quantity > 0;
	
	if (old_quantity != quantity || old_enabled != enabled) update_ui = true;
}
