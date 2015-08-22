#include "Pcons.h"
#include "../API/GwConstants.h"
#include "../API/APIMain.h"
#include "TBMainWindow.h"

//#include "Config.h"
#include "GWToolbox.h"

using namespace GWAPI;
using namespace OSHGui::Drawing;
using namespace GwConstants;

Pcons::Pcons() {
	initialized = false;
	enabled = false;
}

Pcons::~Pcons() {}

void Pcons::loadIni() {
	//enabled = GWToolbox::getInstance()->config->iniReadBool(L"pcons", L"active", false);
}

OSHGui::Panel* Pcons::buildUI() {
	Panel* panel = new Panel();
	panel->SetSize(6 * 2 + Pcon::WIDTH * 3, 6 * 2 + Pcon::HEIGHT * 6);
	LOG("building pcons ui\n");
	int row = 0;
	int col = 0;

	essence = new PconCons(L"essence");
	essence->setIcon("Essence_of_Celerity.png", 0, 0, 64);
	essence->setChatName(L"Essence of Celerity");
	essence->setItemID(ItemID::ConsEssence);
	essence->setEffectID(Effect::ConsEssence);
	essence->setThreshold(5);
	essence->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	panel->AddControl(essence);
	++col;

	grail = new PconCons(L"grail");
	grail->setIcon("Grail_of_Might.png", 0, 0, 56);
	grail->setChatName(L"Grail of Might");
	grail->setItemID(ItemID::ConsGrail);
	grail->setEffectID(Effect::ConsGrail);
	grail->setThreshold(5);
	grail->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	panel->AddControl(grail);
	++col;

	armor = new PconCons(L"armor");
	armor->setIcon("Armor_of_Salvation.png", 0, 0, 54);
	armor->setChatName(L"Armor of Salvation");
	armor->setItemID(ItemID::ConsArmor);
	armor->setEffectID(Effect::ConsArmor);
	armor->setThreshold(5);
	armor->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	panel->AddControl(armor);
	++row; col = 0;

	redrock = new Pcon(L"redrock");
	redrock->setIcon("Red_Rock_Candy.png", 0, 0, 56);
	redrock->setChatName(L"Red Rock Candy");
	redrock->setItemID(ItemID::RRC);
	redrock->setEffectID(Effect::Redrock);
	redrock->setThreshold(5);
	redrock->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	panel->AddControl(redrock);
	++col;

	bluerock = new Pcon(L"bluerock");
	bluerock->setIcon("Blue_Rock_Candy.png", 0, 0, 56);
	bluerock->setChatName(L"Blue Rock Candy");
	bluerock->setItemID(ItemID::BRC);
	bluerock->setEffectID(Effect::Bluerock);
	bluerock->setThreshold(10);
	bluerock->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	panel->AddControl(bluerock);
	++col;

	greenrock = new Pcon(L"greenrock");
	greenrock->setIcon("Green_Rock_Candy.png", 0, 0, 56);
	greenrock->setChatName(L"Green Rock Candy");
	greenrock->setItemID(ItemID::GRC);
	greenrock->setEffectID(Effect::Greenrock);
	greenrock->setThreshold(15);
	greenrock->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	panel->AddControl(greenrock);
	++row; col = 0;

	cupcake = new Pcon(L"cupcake");
	cupcake->setIcon("Birthday_Cupcake.png", 0, 0, 56);
	cupcake->setChatName(L"Birthday Cupcake");
	cupcake->setItemID(ItemID::Cupcakes);
	cupcake->setEffectID(Effect::Cupcake);
	cupcake->setThreshold(10);
	cupcake->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	panel->AddControl(cupcake);
	++col;

	apple = new Pcon(L"apple");
	apple->setIcon("Candy_Apple.png", 0, 0, 56);
	apple->setChatName(L"Candy Apple");
	apple->setItemID(ItemID::Apples);
	apple->setEffectID(Effect::Apple);
	apple->setThreshold(10);
	apple->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	panel->AddControl(apple);
	++col;

	corn = new Pcon(L"corn");
	corn->setIcon("Candy_Corn.png", 0, 0, 56);
	corn->setChatName(L"Candy Corn");
	corn->setItemID(ItemID::Corns);
	corn->setEffectID(Effect::Corn);
	corn->setThreshold(10);
	corn->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	panel->AddControl(corn);
	++row; col = 0;

	egg = new Pcon(L"egg");
	egg->setIcon("Golden_Egg.png", 0, 0, 52);
	egg->setChatName(L"Golden Egg");
	egg->setItemID(ItemID::Eggs);
	egg->setEffectID(Effect::Egg);
	egg->setThreshold(20);
	egg->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	panel->AddControl(egg);
	++col;

	pie = new Pcon(L"pie");
	pie->setIcon("Slice_of_Pumpkin_Pie.png", 0, 0, 56);
	pie->setChatName(L"Slice of Pumpkin_Pie");
	pie->setItemID(ItemID::Pies);
	pie->setEffectID(Effect::Pie);
	pie->setThreshold(10);
	pie->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	panel->AddControl(pie);
	++col;

	city = new PconCity(L"city");
	city->setIcon("Sugary_Blue_Drink.png", 0, 0, 50);
	city->setChatName(L"City speedboost");
	city->setThreshold(20);
	city->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	panel->AddControl(city);
	++row; col = 0;

	alcohol = new PconAlcohol(L"alcohol");
	alcohol->setIcon("Dwarven_Ale.png", 0, 0, 50);
	alcohol->setChatName(L"Alcohol");
	alcohol->setThreshold(10);
	alcohol->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	panel->AddControl(alcohol);
	++col;

	lunars = new PconLunar(L"lunars");
	lunars->setIcon("Lunar_Fortune.png", 0, 0, 56);
	lunars->setChatName(L"Lunar Fortunes");
	lunars->setEffectID(Effect::Lunars);
	lunars->setThreshold(10);
	lunars->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	panel->AddControl(lunars);
	++col;

	warsupply = new Pcon(L"warsupply");
	warsupply->setIcon("War_Supplies.png", 0, 0, 48);
	warsupply->setChatName(L"War Supplies");
	warsupply->setItemID(ItemID::Warsupplies);
	warsupply->setEffectID(Effect::Warsupplies);
	warsupply->setThreshold(20);
	warsupply->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	panel->AddControl(warsupply);
	++row; col = 0;

	kabob = new Pcon(L"kabob");
	kabob->setIcon("Drake_Kabob.png", 0, 0, 52);
	kabob->setChatName(L"Drake Kabob");
	kabob->setItemID(ItemID::Kabobs);
	kabob->setEffectID(Effect::Kabobs);
	kabob->setThreshold(10);
	kabob->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	panel->AddControl(kabob);
	++col;

	skalesoup = new Pcon(L"skalesoup");
	skalesoup->setIcon("Bowl_of_Skalefin_Soup.png", 0, 0, 56);
	skalesoup->setChatName(L"Bowl of Skalefin Soup");
	skalesoup->setItemID(ItemID::SkalefinSoup);
	skalesoup->setEffectID(Effect::SkaleVigor);
	skalesoup->setThreshold(10);
	skalesoup->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	panel->AddControl(skalesoup);
	++col;

	pahnai = new Pcon(L"pahnai");
	pahnai->setIcon("Pahnai_Salad.png", 0, 0, 56);
	pahnai->setChatName(L"Pahnai Salad");
	pahnai->setItemID(ItemID::PahnaiSalad);
	pahnai->setEffectID(Effect::PahnaiSalad);
	pahnai->setThreshold(10);
	pahnai->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	panel->AddControl(pahnai);
	++row; col = 0;

	Pcons::scanInventory();

	initialized = true;

	return panel;
}

Pcon::Pcon(const wchar_t* ini)
: Button() {
	
	pic = new PictureBox();
	shadow = new Label();
	quantity = 0;
	enabled = GWToolbox::getInstance()->config->iniReadBool(L"pcons", ini, false);;
	iniName = ini;
	chatName = ini; // should be set later, but its a good temporary value
	itemID = 0;
	effectID = 0;
	threshold = 0;
	timer = TBTimer::init();
	
	pic->SetBackColor(Drawing::Color::Empty());
	pic->SetStretch(true);
	pic->SetEnabled(false);
	AddSubControl(pic);

	shadow->SetText(std::to_string(quantity));
	shadow->SetFont(TBMainWindow::getTBFont(11.0f, true));
	shadow->SetForeColor(Drawing::Color::Black());
	shadow->SetLocation(1, 1);
	AddSubControl(shadow);

	label_->SetText(std::to_string(quantity));
	label_->SetLocation(0, 0);
	label_->SetFont(TBMainWindow::getTBFont(11.0f, true));

	SetSize(WIDTH, HEIGHT);
	SetBackColor(Drawing::Color::Empty());

	GetClickEvent() += ClickEventHandler([this](Control*) { 
		LOG("click event handler\n");
		this->toggleActive();
	});
}

void Pcon::toggleActive() {
	enabled = !enabled;
	GWToolbox::getInstance()->config->iniWriteBool(L"pcons", iniName, enabled);
}

void Pcon::DrawSelf(Drawing::RenderContext &context) {
	pic->Render();
	shadow->Render();
	Button::DrawSelf(context);
}

void Pcon::PopulateGeometry() {
	Button::PopulateGeometry();
	Drawing::Graphics g(*geometry_);
	if (enabled) {
		g.DrawRectangle(Drawing::Color::Red(), 0.0, 0.0, (float)GetWidth() - 1, (float)GetHeight() - 1);
	}
}

void Pcon::setIcon(const char* icon, int xOff, int yOff, int size) {
	pic->SetSize(size, size);
	pic->SetLocation(xOff, yOff);
	pic->SetImage(Drawing::Image::FromFile(GWToolbox::getInstance()->config->getPathA(icon)));
}

void Pcon::checkAndUse() {
	if (enabled && TBTimer::diff(timer) > 5000) {

		GWAPIMgr* API = GWAPIMgr::GetInstance();
		EffectMgr::Effect effect = API->Effects->GetPlayerEffectById(effectID);

		if (effect.SkillId == 0 || effect.GetTimeRemaining() < 1000) {
			if (API->Items->UseItemByModelId(itemID)) {
				timer = TBTimer::init();
			} else {
				Pcon::scanInventory();
				API->Chat->WriteChatF(L"[WARNING] Cannot find %ls", chatName);
			}
		}
	}
}

void PconCons::checkAndUse() {
	if (enabled && TBTimer::diff(timer) > 5000) {
		GWAPIMgr* API = GWAPIMgr::GetInstance();
		EffectMgr::Effect effect = API->Effects->GetPlayerEffectById(effectID);
		if (effect.SkillId == 0 || effect.GetTimeRemaining() < 1000) {
			if (!API->Agents->GetIsPartyLoaded()) return;

			AgentMgr::MapAgentArray mapAgents = API->Agents->GetMapAgentArray();
			for (size_t i = 0; i < mapAgents.size(); ++i) {
				if (mapAgents[i].curHealth == 0) return;
			}
			
			if (API->Items->UseItemByModelId(itemID)) {
				timer = TBTimer::init();
			} else {
				scanInventory();
				API->Chat->WriteChatF(L"[WARNING] Cannot find %ls", chatName);
			}
		}
	}
}

void PconCity::checkAndUse() {
	if (enabled	&& TBTimer::diff(timer) > 5000) {
		GWAPIMgr* API = GWAPIMgr::GetInstance();
		if (API->Agents->GetPlayer()->MoveX > 0 || API->Agents->GetPlayer()->MoveY > 0) {
			if (API->Effects->GetPlayerEffectById(Effect::CremeBrulee).SkillId
				|| API->Effects->GetPlayerEffectById(Effect::BlueDrink).SkillId
				|| API->Effects->GetPlayerEffectById(Effect::ChocolateBunny).SkillId
				|| API->Effects->GetPlayerEffectById(Effect::RedBeanCake).SkillId) {

				// then we have effect on already, do nothing
			} else {
				// we should use it. Because of logical-OR only the first one will be used
				if (API->Items->UseItemByModelId(ItemID::CremeBrulee)
					|| API->Items->UseItemByModelId(ItemID::ChocolateBunny)
					|| API->Items->UseItemByModelId(ItemID::Fruitcake)
					|| API->Items->UseItemByModelId(ItemID::SugaryBlueDrink)
					|| API->Items->UseItemByModelId(ItemID::RedBeanCake)
					|| API->Items->UseItemByModelId(ItemID::JarOfHoney)) {

					timer = TBTimer::init();
				} else {
					scanInventory();
					API->Chat->WriteChat(L"[WARNING] Cannot find a city speedboost");
				}
			}
		}
	}
}

void PconAlcohol::checkAndUse() {
	if (enabled && TBTimer::diff(timer) > 5000) {
		GWAPIMgr* API = GWAPIMgr::GetInstance();
		if (API->Effects->GetAlcoholLevel() <= 1) {
			// use an alcohol item. Because of logical-OR only the first one will be used
			if (   API->Items->UseItemByModelId(ItemID::Eggnog)
				|| API->Items->UseItemByModelId(ItemID::DwarvenAle)
				|| API->Items->UseItemByModelId(ItemID::HuntersAle)
				|| API->Items->UseItemByModelId(ItemID::Absinthe)
				|| API->Items->UseItemByModelId(ItemID::WitchsBrew)
				|| API->Items->UseItemByModelId(ItemID::Ricewine)
				|| API->Items->UseItemByModelId(ItemID::ShamrockAle)
				|| API->Items->UseItemByModelId(ItemID::Cider)

				|| API->Items->UseItemByModelId(ItemID::Grog)
				|| API->Items->UseItemByModelId(ItemID::SpikedEggnog)
				|| API->Items->UseItemByModelId(ItemID::AgedDwarvenAle)
				|| API->Items->UseItemByModelId(ItemID::AgedHungersAle)
				|| API->Items->UseItemByModelId(ItemID::Keg)
				|| API->Items->UseItemByModelId(ItemID::FlaskOfFirewater)
				|| API->Items->UseItemByModelId(ItemID::KrytanBrandy)) {

				timer = TBTimer::init();
			} else {
				scanInventory();
				API->Chat->WriteChat(L"[WARNING] Cannot find Alcohol");
			}
		}
	}
}

void PconLunar::checkAndUse() {
	if (enabled	&& TBTimer::diff(timer) > 500) {
		GWAPIMgr* API = GWAPIMgr::GetInstance();
		if (API->Effects->GetPlayerEffectById(Effect::Lunars).SkillId == 0) {
			if (   API->Items->UseItemByModelId(ItemID::LunarDragon)
				|| API->Items->UseItemByModelId(ItemID::LunarHorse)
				|| API->Items->UseItemByModelId(ItemID::LunarRabbit)
				|| API->Items->UseItemByModelId(ItemID::LunarSheep)
				|| API->Items->UseItemByModelId(ItemID::LunarSnake)) {

				timer = TBTimer::init();
			} else {
				scanInventory();
				API->Chat->WriteChat(L"[WARNING] Cannot find Lunar Fortunes");
			}
		}
	}
}

void Pcon::scanInventory() {
	quantity = 0;

	ItemMgr::Bag** bags = GWAPIMgr::GetInstance()->Items->GetBagArray();
	ItemMgr::Bag* bag = NULL;
	for (int bagIndex = 1; bagIndex <= 4; ++bagIndex) {
		bag = bags[bagIndex];
		if (bag != NULL) {
			ItemMgr::ItemArray items = bag->Items;
			for (size_t i = 0; i < items.size(); i++) {
				if (items[i]) {
					if (items[i]->ModelId == itemID) {
						quantity += items[i]->Quantity;
					}
				}
			}
		}
	}

	enabled = enabled && quantity > 0;
	updateLabel();
}

void PconCity::scanInventory() {
	quantity = 0;

	ItemMgr::Bag** bags = GWAPIMgr::GetInstance()->Items->GetBagArray();
	ItemMgr::Bag* bag = NULL;
	for (int bagIndex = 1; bagIndex <= 4; ++bagIndex) {
		bag = bags[bagIndex];
		if (bag != NULL) {
			ItemMgr::ItemArray items = bag->Items;
			for (size_t i = 0; i < items.size(); i++) {
				if (items[i]) {
					if (   items[i]->ModelId == ItemID::CremeBrulee
						|| items[i]->ModelId == ItemID::SugaryBlueDrink
						|| items[i]->ModelId == ItemID::ChocolateBunny
						|| items[i]->ModelId == ItemID::RedBeanCake) {

						quantity += items[i]->Quantity;
					}
				}
			}
		}
	}

	enabled = enabled && quantity > 0;
	updateLabel();
}

void PconAlcohol::scanInventory() {
	quantity = 0;
	LOG("looking for alcohol\n");
	ItemMgr::Bag** bags = GWAPIMgr::GetInstance()->Items->GetBagArray();
	ItemMgr::Bag* bag = NULL;
	for (int bagIndex = 1; bagIndex <= 4; ++bagIndex) {
		bag = bags[bagIndex];
		if (bag != NULL) {
			ItemMgr::ItemArray items = bag->Items;
			for (size_t i = 0; i < items.size(); i++) {
				if (items[i]) {
					if (	   items[i]->ModelId == ItemID::Eggnog
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

	enabled = enabled && quantity > 0;
	updateLabel();
}

void PconLunar::scanInventory() {
	quantity = 0;

	ItemMgr::Bag** bags = GWAPIMgr::GetInstance()->Items->GetBagArray();
	ItemMgr::Bag* bag = NULL;
	for (int bagIndex = 1; bagIndex <= 4; ++bagIndex) {
		bag = bags[bagIndex];
		if (bag != NULL) {
			ItemMgr::ItemArray items = bag->Items;
			for (size_t i = 0; i < items.size(); i++) {
				if (items[i]) {
					if (   items[i]->ModelId == ItemID::LunarDragon
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

	enabled = enabled && quantity > 0;
	updateLabel();
}

void Pcon::updateLabel() {
	label_->SetText(std::to_string(quantity));
	shadow->SetText(std::to_string(quantity));

	if (quantity == 0) {
		label_->SetForeColor(Color(1.0, 1.0, 0.0, 0.0));
	} else if (quantity < threshold) {
		label_->SetForeColor(Color(1.0, 1.0, 1.0, 0.0));
	} else {
		label_->SetForeColor(Color(1.0, 0.0, 1.0, 0.0));
	}
}

void Pcons::mainRoutine() {
	if (!enabled || !initialized) return;

	GWAPIMgr * API = GWAPIMgr::GetInstance();

	switch (API->Map->GetInstanceType()) {
	case GwConstants::InstanceType::Explorable:
		if (API->Agents->GetPlayer()->GetIsDead()) break;
		redrock->checkAndUse();
		bluerock->checkAndUse();
		greenrock->checkAndUse();
		pie->checkAndUse();
		cupcake->checkAndUse();
		apple->checkAndUse();
		corn->checkAndUse();
		egg->checkAndUse();
		kabob->checkAndUse();
		warsupply->checkAndUse();
		skalesoup->checkAndUse();
		pahnai->checkAndUse();
		alcohol->checkAndUse();
		lunars->checkAndUse();
		break;

	case GwConstants::InstanceType::Outpost:
		city->checkAndUse();
		break;

	case GwConstants::InstanceType::Loading:
		break;
	}
}

void Pcons::scanInventory() {
	essence->scanInventory();
	grail->scanInventory();
	armor->scanInventory();
	alcohol->scanInventory();
	redrock->scanInventory();
	bluerock->scanInventory();
	greenrock->scanInventory();
	pie->scanInventory();
	cupcake->scanInventory();
	apple->scanInventory();
	corn->scanInventory();
	egg->scanInventory();
	kabob->scanInventory();
	warsupply->scanInventory();
	lunars->scanInventory();
	skalesoup->scanInventory();
	pahnai->scanInventory();
	city->scanInventory();
}

