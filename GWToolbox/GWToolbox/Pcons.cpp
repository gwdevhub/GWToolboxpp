#include "Pcons.h"

#include "GWCA\APIMain.h"

#include "GWToolbox.h"
#include "ChatLogger.h"

using namespace OSHGui;
using namespace OSHGui::Drawing;
using namespace GwConstants;
using namespace GWAPI;

Pcon::Pcon(const wchar_t* ini)
	: Button() {

	pic = new PictureBox();
	tick = new PictureBox();
	shadow = new Label();
	quantity = -1; // to force a redraw when first created
	enabled = GWToolbox::instance().config().IniReadBool(L"pcons", ini, false);;
	iniName = ini;
	chatName = ini; // will be set later, but its a good temporary value
	itemID = 0;
	effectID = SkillID::No_Skill;
	threshold = 0;
	timer = TBTimer::init();
	update_timer = 0;

	tick->SetBackColor(Drawing::Color::Empty());
	tick->SetStretch(true);
	tick->SetEnabled(false);
	tick->SetLocation(0, 0);
	tick->SetSize(WIDTH, HEIGHT);
	tick->SetImage(Drawing::Image::FromFile(GuiUtils::getSubPathA("Tick.png", "img")));
	AddSubControl(tick);

	pic->SetBackColor(Drawing::Color::Empty());
	pic->SetStretch(true);
	pic->SetEnabled(false);
	AddSubControl(pic);

	int text_x = 5;
	int text_y = 3;
	shadow->SetText(std::to_wstring(quantity));
	shadow->SetFont(GuiUtils::getTBFont(11.0f, true));
	shadow->SetForeColor(Drawing::Color::Black());
	shadow->SetLocation(text_x + 1, text_y + 1);
	AddSubControl(shadow);

	label_->SetText(std::to_wstring(quantity));
	label_->SetFont(GuiUtils::getTBFont(11.0f, true));
	label_->SetLocation(text_x, text_y);

	SetSize(WIDTH, HEIGHT);
	SetBackColor(Drawing::Color::Empty());
	SetMouseOverFocusColor(GuiUtils::getMouseOverColor());

	GetClickEvent() += ClickEventHandler([this](Control*) { this->toggleActive(); });
}

void Pcon::toggleActive() {
	enabled = !enabled;
	scanInventory();
	update_ui = true;
	GWToolbox::instance().config().IniWriteBool(L"pcons", iniName, enabled);
}

void Pcon::DrawSelf(Drawing::RenderContext &context) {
	BufferGeometry(context);
	QueueGeometry(context);
	pic->Render();
	shadow->Render();
	label_->Render();
	if (enabled) tick->Render();
}

void Pcon::setIcon(const char* icon, int xOff, int yOff, int size) {
	pic->SetSize(size, size);
	pic->SetLocation(xOff, yOff);
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
		update_ui = false;
	}
}

void Pcon::CheckUpdateTimer() {
	if (update_timer != 0 && TBTimer::diff(update_timer) > 2000) {
		bool old_enabled = enabled;
		this->scanInventory();
		update_timer = 0;
		if (old_enabled != enabled) {
			ChatLogger::LogF(L"[WARNING] Cannot find %ls", chatName);
		}
	}
}

bool Pcon::checkAndUse() {
	CheckUpdateTimer();

	if (enabled && TBTimer::diff(this->timer) > 5000) {

		GWCA api;
		try {
			GW::Effect effect = api().Effects().GetPlayerEffectById(effectID);

			if (effect.SkillId == 0 || effect.GetTimeRemaining() < 1000) {
				bool used = api().Items().UseItemByModelId(itemID);
				if (used) {
					this->timer = TBTimer::init();
					this->update_timer = TBTimer::init();
				} else {
					// this should never happen, it should be disabled before
					ChatLogger::LogF(L"[WARNING] Cannot find %ls", chatName);
					this->scanInventory();
				}
				return used;
			}
		} catch (APIException_t) {}
	}
	return false;
}

bool PconCons::checkAndUse() {
	CheckUpdateTimer();

	if (enabled && TBTimer::diff(this->timer) > 5000) {
		GWCA api;
		try {
			GW::Effect effect = api().Effects().GetPlayerEffectById(effectID);
			if (effect.SkillId == 0 || effect.GetTimeRemaining() < 1000) {

				if (!api().Agents().GetIsPartyLoaded()) return false;

				GW::MapAgentArray mapAgents = api().Agents().GetMapAgentArray();
				if (!mapAgents.valid()) return false;
				int n_players = api().Agents().GetAmountOfPlayersInInstance();
				for (int i = 1; i <= n_players; ++i) {
					DWORD currentPlayerAgID = api().Agents().GetAgentIdByLoginNumber(i);
					if (currentPlayerAgID <= 0) return false;
					if (currentPlayerAgID >= mapAgents.size()) return false;
					if (mapAgents[currentPlayerAgID].GetIsDead()) return false;
				}

				bool used = api().Items().UseItemByModelId(itemID);
				if (used) {
					this->timer = TBTimer::init();
					this->update_timer = TBTimer::init();
				} else {
					ChatLogger::LogF(L"[WARNING] Cannot find %ls", chatName);
					this->scanInventory();
				}
				return used;
			}
		} catch (APIException_t) {}
	}
	return false;
}

bool PconCity::checkAndUse() {
	CheckUpdateTimer();

	if (enabled	&& TBTimer::diff(this->timer) > 5000) {
		GWCA api;
		try {
			if (api().Agents().GetPlayer() &&
				(api().Agents().GetPlayer()->MoveX > 0 || api().Agents().GetPlayer()->MoveY > 0)) {
				if (api().Effects().GetPlayerEffectById(SkillID::Sugar_Rush_short).SkillId
					|| api().Effects().GetPlayerEffectById(SkillID::Sugar_Rush_long).SkillId
					|| api().Effects().GetPlayerEffectById(SkillID::Sugar_Jolt_short).SkillId
					|| api().Effects().GetPlayerEffectById(SkillID::Sugar_Jolt_long).SkillId) {

					// then we have effect on already, do nothing
				} else {
					// we should use it. Because of logical-OR only the first one will be used
					bool used = api().Items().UseItemByModelId(ItemID::CremeBrulee)
						|| api().Items().UseItemByModelId(ItemID::ChocolateBunny)
						|| api().Items().UseItemByModelId(ItemID::Fruitcake)
						|| api().Items().UseItemByModelId(ItemID::SugaryBlueDrink)
						|| api().Items().UseItemByModelId(ItemID::RedBeanCake)
						|| api().Items().UseItemByModelId(ItemID::JarOfHoney);
					if (used) {
						this->timer = TBTimer::init();
						this->update_timer = TBTimer::init();
					} else {
						ChatLogger::Log(L"[WARNING] Cannot find a city speedboost");
						this->scanInventory();
					}
					return used;
				}
			}
		} catch (APIException_t) {}
	}
	return false;
}

bool PconAlcohol::checkAndUse() {
	CheckUpdateTimer();

	if (enabled && TBTimer::diff(this->timer) > 5000) {
		GWCA api;
		try {
			if (api().Effects().GetAlcoholLevel() <= 1) {
				// use an alcohol item. Because of logical-OR only the first one will be used
				bool used = api().Items().UseItemByModelId(ItemID::Eggnog)
					|| api().Items().UseItemByModelId(ItemID::DwarvenAle)
					|| api().Items().UseItemByModelId(ItemID::HuntersAle)
					|| api().Items().UseItemByModelId(ItemID::Absinthe)
					|| api().Items().UseItemByModelId(ItemID::WitchsBrew)
					|| api().Items().UseItemByModelId(ItemID::Ricewine)
					|| api().Items().UseItemByModelId(ItemID::ShamrockAle)
					|| api().Items().UseItemByModelId(ItemID::Cider)

					|| api().Items().UseItemByModelId(ItemID::Grog)
					|| api().Items().UseItemByModelId(ItemID::SpikedEggnog)
					|| api().Items().UseItemByModelId(ItemID::AgedDwarvenAle)
					|| api().Items().UseItemByModelId(ItemID::AgedHungersAle)
					|| api().Items().UseItemByModelId(ItemID::Keg)
					|| api().Items().UseItemByModelId(ItemID::FlaskOfFirewater)
					|| api().Items().UseItemByModelId(ItemID::KrytanBrandy);
				if (used) {
					this->timer = TBTimer::init();
					this->update_timer = TBTimer::init();
				} else {
					ChatLogger::Log(L"[WARNING] Cannot find Alcohol");
					this->scanInventory();
				}
				return used;
			}
		} catch (APIException_t) {}
	}
	return false;
}

bool PconLunar::checkAndUse() {
	CheckUpdateTimer();

	if (enabled	&& TBTimer::diff(this->timer) > 500) {
		GWCA api;
		try {
			if (api().Effects().GetPlayerEffectById(SkillID::Lunar_Blessing).SkillId == 0) {
				bool used = api().Items().UseItemByModelId(ItemID::LunarDragon)
					|| api().Items().UseItemByModelId(ItemID::LunarHorse)
					|| api().Items().UseItemByModelId(ItemID::LunarRabbit)
					|| api().Items().UseItemByModelId(ItemID::LunarSheep)
					|| api().Items().UseItemByModelId(ItemID::LunarSnake);
				if (used) {
					this->timer = TBTimer::init();
					this->update_timer = TBTimer::init();
				} else {
					ChatLogger::Log(L"[WARNING] Cannot find Lunar Fortunes");
					this->scanInventory();
				}
				return used;
			}
		} catch (APIException_t) {}
	}
	return false;
}

void Pcon::scanInventory() {
	int old_quantity = quantity;
	bool old_enabled = enabled;

	quantity = 0;
	GWCA api;
	GW::Bag** bags = api().Items().GetBagArray();
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
	GWCA api;
	GW::Bag** bags = api().Items().GetBagArray();
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
	GWCA api;
	GW::Bag** bags = api().Items().GetBagArray();
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
	GWCA api;
	GW::Bag** bags = api().Items().GetBagArray();
	if (bags != nullptr) {
		GW::Bag* bag = NULL;
		for (int bagIndex = 1; bagIndex <= 4; ++bagIndex) {
			bag = bags[bagIndex];
			if (bag != NULL) {
				GW::ItemArray items = bag->Items;
				for (size_t i = 0; i < items.size(); i++) {
					if (items[i]) {
						if (items[i]->ModelId == ItemID::LunarDragon
							|| items[i]->ModelId == ItemID::LunarHorse
							|| items[i]->ModelId == ItemID::LunarRabbit
							|| items[i]->ModelId == ItemID::LunarSheep
							|| items[i]->ModelId == ItemID::LunarSnake) {

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
