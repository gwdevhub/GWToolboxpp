#include "Pcons.h"

#include "GWCA\APIMain.h"

#include "GWToolbox.h"

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
	enabled = GWToolbox::instance()->config()->iniReadBool(L"pcons", ini, false);;
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
	GWToolbox::instance()->config()->iniWriteBool(L"pcons", iniName, enabled);
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
			GWAPIMgr::instance()->Chat()->WriteChatF(L"[WARNING] Cannot find %ls", chatName);
		}
	}
}

bool Pcon::checkAndUse() {
	CheckUpdateTimer();

	if (enabled && TBTimer::diff(this->timer) > 5000) {

		GWAPIMgr* API = GWAPIMgr::instance();
		try {
			GW::Effect effect = API->Effects()->GetPlayerEffectById(effectID);

			if (effect.SkillId == 0 || effect.GetTimeRemaining() < 1000) {
				bool used = API->Items()->UseItemByModelId(itemID);
				if (used) {
					this->timer = TBTimer::init();
					this->update_timer = TBTimer::init();
				} else {
					// this should never happen, it should be disabled before
					API->Chat()->WriteChatF(L"[WARNING] Cannot find %ls", chatName);
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
		GWAPIMgr* API = GWAPIMgr::instance();
		try {
			GW::Effect effect = API->Effects()->GetPlayerEffectById(effectID);
			if (effect.SkillId == 0 || effect.GetTimeRemaining() < 1000) {

				if (!API->Agents()->GetIsPartyLoaded()) return false;

				GW::MapAgentArray mapAgents = API->Agents()->GetMapAgentArray();
				if (!mapAgents.valid()) return false;
				int n_players = API->Agents()->GetAmountOfPlayersInInstance();
				for (int i = 1; i <= n_players; ++i) {
					DWORD currentPlayerAgID = API->Agents()->GetAgentIdByLoginNumber(i);
					if (currentPlayerAgID <= 0) return false;
					if (currentPlayerAgID >= mapAgents.size()) return false;
					if (mapAgents[currentPlayerAgID].GetIsDead()) return false;
				}

				bool used = API->Items()->UseItemByModelId(itemID);
				if (used) {
					this->timer = TBTimer::init();
					this->update_timer = TBTimer::init();
				} else {
					API->Chat()->WriteChatF(L"[WARNING] Cannot find %ls", chatName);
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
		GWAPIMgr* API = GWAPIMgr::instance();
		try {
			if (API->Agents()->GetPlayer() &&
				(API->Agents()->GetPlayer()->MoveX > 0 || API->Agents()->GetPlayer()->MoveY > 0)) {
				if (   API->Effects()->GetPlayerEffectById(SkillID::Sugar_Rush_short).SkillId
					|| API->Effects()->GetPlayerEffectById(SkillID::Sugar_Rush_long).SkillId
					|| API->Effects()->GetPlayerEffectById(SkillID::Sugar_Jolt_short).SkillId
					|| API->Effects()->GetPlayerEffectById(SkillID::Sugar_Jolt_long).SkillId) {

					// then we have effect on already, do nothing
				} else {
					// we should use it. Because of logical-OR only the first one will be used
					bool used = API->Items()->UseItemByModelId(ItemID::CremeBrulee)
						|| API->Items()->UseItemByModelId(ItemID::ChocolateBunny)
						|| API->Items()->UseItemByModelId(ItemID::Fruitcake)
						|| API->Items()->UseItemByModelId(ItemID::SugaryBlueDrink)
						|| API->Items()->UseItemByModelId(ItemID::RedBeanCake)
						|| API->Items()->UseItemByModelId(ItemID::JarOfHoney);
					if (used) {
						this->timer = TBTimer::init();
						this->update_timer = TBTimer::init();
					} else {
						API->Chat()->WriteChat(L"[WARNING] Cannot find a city speedboost");
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
		GWAPIMgr* API = GWAPIMgr::instance();
		try {
			if (API->Effects()->GetAlcoholLevel() <= 1) {
				// use an alcohol item. Because of logical-OR only the first one will be used
				bool used = API->Items()->UseItemByModelId(ItemID::Eggnog)
					|| API->Items()->UseItemByModelId(ItemID::DwarvenAle)
					|| API->Items()->UseItemByModelId(ItemID::HuntersAle)
					|| API->Items()->UseItemByModelId(ItemID::Absinthe)
					|| API->Items()->UseItemByModelId(ItemID::WitchsBrew)
					|| API->Items()->UseItemByModelId(ItemID::Ricewine)
					|| API->Items()->UseItemByModelId(ItemID::ShamrockAle)
					|| API->Items()->UseItemByModelId(ItemID::Cider)

					|| API->Items()->UseItemByModelId(ItemID::Grog)
					|| API->Items()->UseItemByModelId(ItemID::SpikedEggnog)
					|| API->Items()->UseItemByModelId(ItemID::AgedDwarvenAle)
					|| API->Items()->UseItemByModelId(ItemID::AgedHungersAle)
					|| API->Items()->UseItemByModelId(ItemID::Keg)
					|| API->Items()->UseItemByModelId(ItemID::FlaskOfFirewater)
					|| API->Items()->UseItemByModelId(ItemID::KrytanBrandy);
				if (used) {
					this->timer = TBTimer::init();
					this->update_timer = TBTimer::init();
				} else {
					API->Chat()->WriteChat(L"[WARNING] Cannot find Alcohol");
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
		GWAPIMgr* API = GWAPIMgr::instance();
		try {
			if (API->Effects()->GetPlayerEffectById(SkillID::Lunar_Blessing).SkillId == 0) {
				bool used = API->Items()->UseItemByModelId(ItemID::LunarDragon)
					|| API->Items()->UseItemByModelId(ItemID::LunarHorse)
					|| API->Items()->UseItemByModelId(ItemID::LunarRabbit)
					|| API->Items()->UseItemByModelId(ItemID::LunarSheep)
					|| API->Items()->UseItemByModelId(ItemID::LunarSnake);
				if (used) {
					this->timer = TBTimer::init();
					this->update_timer = TBTimer::init();
				} else {
					API->Chat()->WriteChat(L"[WARNING] Cannot find Lunar Fortunes");
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

	GW::Bag** bags = GWAPIMgr::instance()->Items()->GetBagArray();
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

	GW::Bag** bags = GWAPIMgr::instance()->Items()->GetBagArray();
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
	GW::Bag** bags = GWAPIMgr::instance()->Items()->GetBagArray();
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

	GW::Bag** bags = GWAPIMgr::instance()->Items()->GetBagArray();
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
