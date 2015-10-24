#include "PconPanel.h"

#include <string>
#include <functional>

#include "GWCA\APIMain.h"

#include "MainWindow.h"
#include "GWToolbox.h"

using namespace GWAPI;
using namespace OSHGui::Drawing;
using namespace GwConstants;

PconPanel::PconPanel() {
	initialized = false;
	enabled = false;
}

void PconPanel::BuildUI() {
	const int pcon_columns = 3;
	const int pcon_rows = 6;
	const int width = DefaultBorderPadding * 2 + Pcon::WIDTH * pcon_columns;
	const int height = Pcon::HEIGHT * pcon_rows;
	const int space = DefaultBorderPadding;

	SetSize(width, GetHeight());
	
	int row = 0;
	int col = 0;
	const int top = 4;
	pcons = vector<Pcon*>();

	essence = new PconCons(L"essence");
	essence->setIcon("Essence_of_Celerity.png", 0, 0, 60);
	essence->setChatName(L"Essence of Celerity");
	essence->setItemID(ItemID::ConsEssence);
	essence->setEffectID(SkillID::Essence_of_Celerity_item_effect);
	essence->setThreshold(5);
	essence->SetLocation(space + col * Pcon::WIDTH, top + row * Pcon::HEIGHT);
	pcons.push_back(essence);
	++col;

	grail = new PconCons(L"grail");
	grail->setIcon("Grail_of_Might.png", 0, 0, 52);
	grail->setChatName(L"Grail of Might");
	grail->setItemID(ItemID::ConsGrail);
	grail->setEffectID(SkillID::Grail_of_Might_item_effect);
	grail->setThreshold(5);
	grail->SetLocation(space + col * Pcon::WIDTH, top + row * Pcon::HEIGHT);
	pcons.push_back(grail);
	++col;

	armor = new PconCons(L"armor");
	armor->setIcon("Armor_of_Salvation.png", 0, 0, 50);
	armor->setChatName(L"Armor of Salvation");
	armor->setItemID(ItemID::ConsArmor);
	armor->setEffectID(SkillID::Armor_of_Salvation_item_effect);
	armor->setThreshold(5);
	armor->SetLocation(space + col * Pcon::WIDTH, top + row * Pcon::HEIGHT);
	pcons.push_back(armor);
	++row; col = 0;

	redrock = new Pcon(L"redrock");
	redrock->setIcon("Red_Rock_Candy.png", 0, 0, 52);
	redrock->setChatName(L"Red Rock Candy");
	redrock->setItemID(ItemID::RRC);
	redrock->setEffectID(SkillID::Red_Rock_Candy_Rush);
	redrock->setThreshold(5);
	redrock->SetLocation(space + col * Pcon::WIDTH, top + row * Pcon::HEIGHT);
	pcons.push_back(redrock);
	++col;

	bluerock = new Pcon(L"bluerock");
	bluerock->setIcon("Blue_Rock_Candy.png", 0, 0, 52);
	bluerock->setChatName(L"Blue Rock Candy");
	bluerock->setItemID(ItemID::BRC);
	bluerock->setEffectID(SkillID::Blue_Rock_Candy_Rush);
	bluerock->setThreshold(10);
	bluerock->SetLocation(space + col * Pcon::WIDTH, top + row * Pcon::HEIGHT);
	pcons.push_back(bluerock);
	++col;

	greenrock = new Pcon(L"greenrock");
	greenrock->setIcon("Green_Rock_Candy.png", 0, 0, 52);
	greenrock->setChatName(L"Green Rock Candy");
	greenrock->setItemID(ItemID::GRC);
	greenrock->setEffectID(SkillID::Green_Rock_Candy_Rush);
	greenrock->setThreshold(15);
	greenrock->SetLocation(space + col * Pcon::WIDTH, top + row * Pcon::HEIGHT);
	pcons.push_back(greenrock);
	++row; col = 0;

	egg = new Pcon(L"egg");
	egg->setIcon("Golden_Egg.png", 5, 4, 48);
	egg->setChatName(L"Golden Egg");
	egg->setItemID(ItemID::Eggs);
	egg->setEffectID(SkillID::Golden_Egg_skill);
	egg->setThreshold(20);
	egg->SetLocation(space + col * Pcon::WIDTH, top + row * Pcon::HEIGHT);
	pcons.push_back(egg);
	++col;

	apple = new Pcon(L"apple");
	apple->setIcon("Candy_Apple.png", 2, -3, 52);
	apple->setChatName(L"Candy Apple");
	apple->setItemID(ItemID::Apples);
	apple->setEffectID(SkillID::Candy_Apple_skill);
	apple->setThreshold(10);
	apple->SetLocation(space + col * Pcon::WIDTH, top + row * Pcon::HEIGHT);
	pcons.push_back(apple);
	++col;

	corn = new Pcon(L"corn");
	corn->setIcon("Candy_Corn.png", 2, 2, 52);
	corn->setChatName(L"Candy Corn");
	corn->setItemID(ItemID::Corns);
	corn->setEffectID(SkillID::Candy_Corn_skill);
	corn->setThreshold(10);
	corn->SetLocation(space + col * Pcon::WIDTH, top + row * Pcon::HEIGHT);
	pcons.push_back(corn);
	++row; col = 0;

	cupcake = new Pcon(L"cupcake");
	cupcake->setIcon("Birthday_Cupcake.png", 1, 0, 52);
	cupcake->setChatName(L"Birthday Cupcake");
	cupcake->setItemID(ItemID::Cupcakes);
	cupcake->setEffectID(SkillID::Birthday_Cupcake_skill);
	cupcake->setThreshold(10);
	cupcake->SetLocation(space + col * Pcon::WIDTH, top + row * Pcon::HEIGHT);
	pcons.push_back(cupcake);
	++col;

	pie = new Pcon(L"pie");
	pie->setIcon("Slice_of_Pumpkin_Pie.png", 3, 0, 52);
	pie->setChatName(L"Slice of Pumpkin_Pie");
	pie->setItemID(ItemID::Pies);
	pie->setEffectID(SkillID::Pie_Induced_Ecstasy);
	pie->setThreshold(10);
	pie->SetLocation(space + col * Pcon::WIDTH, top + row * Pcon::HEIGHT);
	pcons.push_back(pie);
	++col;

	warsupply = new Pcon(L"warsupply");
	warsupply->setIcon("War_Supplies.png", 1, 0, 44);
	warsupply->setChatName(L"War Supplies");
	warsupply->setItemID(ItemID::Warsupplies);
	warsupply->setEffectID(SkillID::Well_Supplied);
	warsupply->setThreshold(20);
	warsupply->SetLocation(space + col * Pcon::WIDTH, top + row * Pcon::HEIGHT);
	pcons.push_back(warsupply);
	++row; col = 0;

	alcohol = new PconAlcohol(L"alcohol");
	alcohol->setIcon("Dwarven_Ale.png", 4, 0, 46);
	alcohol->setChatName(L"Alcohol");
	alcohol->setThreshold(10);
	alcohol->SetLocation(space + col * Pcon::WIDTH, top + row * Pcon::HEIGHT);
	pcons.push_back(alcohol);
	++col;

	lunars = new PconLunar(L"lunars");
	lunars->setIcon("Lunar_Fortune.png", 0, 0, 52);
	lunars->setChatName(L"Lunar Fortunes");
	lunars->setEffectID(SkillID::Lunar_Blessing);
	lunars->setThreshold(10);
	lunars->SetLocation(space + col * Pcon::WIDTH, top + row * Pcon::HEIGHT);
	pcons.push_back(lunars);
	++col;

	city = new PconCity(L"city");
	city->setIcon("Sugary_Blue_Drink.png", 4, 0, 46);
	city->setChatName(L"City speedboost");
	city->setThreshold(20);
	city->SetLocation(space + col * Pcon::WIDTH, top + row * Pcon::HEIGHT);
	pcons.push_back(city);
	++row; col = 0;

	kabob = new Pcon(L"kabob");
	kabob->setIcon("Drake_Kabob.png", 2, 0, 48);
	kabob->setChatName(L"Drake Kabob");
	kabob->setItemID(ItemID::Kabobs);
	kabob->setEffectID(SkillID::Drake_Skin);
	kabob->setThreshold(10);
	kabob->SetLocation(space + col * Pcon::WIDTH, top + row * Pcon::HEIGHT);
	pcons.push_back(kabob);
	++col;

	skalesoup = new Pcon(L"skalesoup");
	skalesoup->setIcon("Bowl_of_Skalefin_Soup.png", 0, 2, 54);
	skalesoup->setChatName(L"Bowl of Skalefin Soup");
	skalesoup->setItemID(ItemID::SkalefinSoup);
	skalesoup->setEffectID(SkillID::Skale_Vigor);
	skalesoup->setThreshold(10);
	skalesoup->SetLocation(space + col * Pcon::WIDTH, top + row * Pcon::HEIGHT);
	pcons.push_back(skalesoup);
	++col;

	pahnai = new Pcon(L"pahnai");
	pahnai->setIcon("Pahnai_Salad.png", 3, 0, 54);
	pahnai->setChatName(L"Pahnai Salad");
	pahnai->setItemID(ItemID::PahnaiSalad);
	pahnai->setEffectID(SkillID::Pahnai_Salad_item_effect);
	pahnai->setThreshold(10);
	pahnai->SetLocation(space + col * Pcon::WIDTH, top + row * Pcon::HEIGHT);
	pcons.push_back(pahnai);
	++row; col = 0;

	for (Pcon* pcon : pcons) {
		AddControl(pcon);
		pcon->scanInventory();
		pcon->UpdateUI();
	}

	initialized = true;
}

void PconPanel::UpdateUI() {
	if (!initialized) return;

	GWCA api;

	if (current_map_type != api().Map().GetInstanceType()) {
		current_map_type = api().Map().GetInstanceType();
		scan_inventory_timer = TBTimer::init();
	}

	if (scan_inventory_timer > 0 && TBTimer::diff(scan_inventory_timer) > 2000) {
		scan_inventory_timer = 0;

		for (Pcon* pcon : pcons) {
			pcon->scanInventory();
		}
	}

	for (Pcon* pcon : pcons) {
		pcon->UpdateUI();
	}
}

void PconPanel::MainRoutine() {
	if (!initialized) return;

	GWCA api;

	if (!enabled) return;
	InstanceType type;
	try {
		type = api().Map().GetInstanceType();
		if (type == InstanceType::Loading) return;
		if (api().Agents().GetPlayerId() == 0) return;
		if (api().Agents().GetPlayer() == NULL) return;
		if (api().Agents().GetPlayer()->GetIsDead()) return;
	} catch (APIException_t) {
		return;
	}

	if (type == InstanceType::Explorable) {
		essence->checkAndUse();
		grail->checkAndUse();
		armor->checkAndUse();
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
	} else if (type == InstanceType::Outpost) {
		city->checkAndUse();
	}
}

bool PconPanel::SetActive(bool active) {
	enabled = active;
	GWToolbox* tb = GWToolbox::instance();
	tb->main_window().UpdatePconToggleButton(enabled);
	if (tb->main_window().minimized()) {
		GWCA::Api().Chat().WriteChatF(L"asdrofl", L"Pcons %ls", active ? L"enabled" : L"disabled");
	}
	return enabled;
}
