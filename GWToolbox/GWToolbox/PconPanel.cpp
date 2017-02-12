#include "PconPanel.h"

#include <string>
#include <functional>

#include <GWCA\GWCA.h>
#include <GWCA\Managers\MapMgr.h>

#include "MainWindow.h"
#include "GWToolbox.h"
#include "ChatLogger.h"

using namespace OSHGui::Drawing;
using namespace GW::Constants;

PconPanel::PconPanel(OSHGui::Control* parent) : ToolboxPanel(parent) {
	initialized = false;
	enabled = false;
}

void PconPanel::BuildUI() {
	const int pcon_columns = 3;
	const int pcon_rows = 6;
	const int width = Padding * 2 + Pcon::WIDTH * pcon_columns;
	const int height = Pcon::HEIGHT * pcon_rows;

	SetSize(SizeI(width, GetHeight()));
	
	int row = 0;
	int col = 0;
	const int top = 4;
	pcons = std::vector<Pcon*>();

	essence = new PconCons(this, "essence");
	essence->setIcon("Essence_of_Celerity.png", 0, 0, 60);
	essence->setChatName("Essence of Celerity");
	essence->setItemID(ItemID::ConsEssence);
	essence->setEffectID(SkillID::Essence_of_Celerity_item_effect);
	essence->setThreshold(5);
	essence->SetLocation(PointI(Padding + col * Pcon::WIDTH, top + row * Pcon::HEIGHT));
	pcons.push_back(essence);
	++col;

	grail = new PconCons(this, "grail");
	grail->setIcon("Grail_of_Might.png", 0, 0, 52);
	grail->setChatName("Grail of Might");
	grail->setItemID(ItemID::ConsGrail);
	grail->setEffectID(SkillID::Grail_of_Might_item_effect);
	grail->setThreshold(5);
	grail->SetLocation(PointI(Padding + col * Pcon::WIDTH, top + row * Pcon::HEIGHT));
	pcons.push_back(grail);
	++col;

	armor = new PconCons(this, "armor");
	armor->setIcon("Armor_of_Salvation.png", 0, 0, 50);
	armor->setChatName("Armor of Salvation");
	armor->setItemID(ItemID::ConsArmor);
	armor->setEffectID(SkillID::Armor_of_Salvation_item_effect);
	armor->setThreshold(5);
	armor->SetLocation(PointI(Padding + col * Pcon::WIDTH, top + row * Pcon::HEIGHT));
	pcons.push_back(armor);
	++row; col = 0;

	redrock = new Pcon(this, "redrock");
	redrock->setIcon("Red_Rock_Candy.png", 0, 0, 52);
	redrock->setChatName("Red Rock Candy");
	redrock->setItemID(ItemID::RRC);
	redrock->setEffectID(SkillID::Red_Rock_Candy_Rush);
	redrock->setThreshold(5);
	redrock->SetLocation(PointI(Padding + col * Pcon::WIDTH, top + row * Pcon::HEIGHT));
	pcons.push_back(redrock);
	++col;

	bluerock = new Pcon(this, "bluerock");
	bluerock->setIcon("Blue_Rock_Candy.png", 0, 0, 52);
	bluerock->setChatName("Blue Rock Candy");
	bluerock->setItemID(ItemID::BRC);
	bluerock->setEffectID(SkillID::Blue_Rock_Candy_Rush);
	bluerock->setThreshold(10);
	bluerock->SetLocation(PointI(Padding + col * Pcon::WIDTH, top + row * Pcon::HEIGHT));
	pcons.push_back(bluerock);
	++col;

	greenrock = new Pcon(this, "greenrock");
	greenrock->setIcon("Green_Rock_Candy.png", 0, 0, 52);
	greenrock->setChatName("Green Rock Candy");
	greenrock->setItemID(ItemID::GRC);
	greenrock->setEffectID(SkillID::Green_Rock_Candy_Rush);
	greenrock->setThreshold(15);
	greenrock->SetLocation(PointI(Padding + col * Pcon::WIDTH, top + row * Pcon::HEIGHT));
	pcons.push_back(greenrock);
	++row; col = 0;

	egg = new Pcon(this, "egg");
	egg->setIcon("Golden_Egg.png", 5, 4, 48);
	egg->setChatName("Golden Egg");
	egg->setItemID(ItemID::Eggs);
	egg->setEffectID(SkillID::Golden_Egg_skill);
	egg->setThreshold(20);
	egg->SetLocation(PointI(Padding + col * Pcon::WIDTH, top + row * Pcon::HEIGHT));
	pcons.push_back(egg);
	++col;

	apple = new Pcon(this, "apple");
	apple->setIcon("Candy_Apple.png", 2, -3, 52);
	apple->setChatName("Candy Apple");
	apple->setItemID(ItemID::Apples);
	apple->setEffectID(SkillID::Candy_Apple_skill);
	apple->setThreshold(10);
	apple->SetLocation(PointI(Padding + col * Pcon::WIDTH, top + row * Pcon::HEIGHT));
	pcons.push_back(apple);
	++col;

	corn = new Pcon(this, "corn");
	corn->setIcon("Candy_Corn.png", 2, 2, 52);
	corn->setChatName("Candy Corn");
	corn->setItemID(ItemID::Corns);
	corn->setEffectID(SkillID::Candy_Corn_skill);
	corn->setThreshold(10);
	corn->SetLocation(PointI(Padding + col * Pcon::WIDTH, top + row * Pcon::HEIGHT));
	pcons.push_back(corn);
	++row; col = 0;

	cupcake = new Pcon(this, "cupcake");
	cupcake->setIcon("Birthday_Cupcake.png", 1, 0, 52);
	cupcake->setChatName("Birthday Cupcake");
	cupcake->setItemID(ItemID::Cupcakes);
	cupcake->setEffectID(SkillID::Birthday_Cupcake_skill);
	cupcake->setThreshold(10);
	cupcake->SetLocation(PointI(Padding + col * Pcon::WIDTH, top + row * Pcon::HEIGHT));
	pcons.push_back(cupcake);
	++col;

	pie = new Pcon(this, "pie");
	pie->setIcon("Slice_of_Pumpkin_Pie.png", 3, 0, 52);
	pie->setChatName("Slice of Pumpkin_Pie");
	pie->setItemID(ItemID::Pies);
	pie->setEffectID(SkillID::Pie_Induced_Ecstasy);
	pie->setThreshold(10);
	pie->SetLocation(PointI(Padding + col * Pcon::WIDTH, top + row * Pcon::HEIGHT));
	pcons.push_back(pie);
	++col;

	warsupply = new Pcon(this, "warsupply");
	warsupply->setIcon("War_Supplies.png", 1, 0, 44);
	warsupply->setChatName("War Supplies");
	warsupply->setItemID(ItemID::Warsupplies);
	warsupply->setEffectID(SkillID::Well_Supplied);
	warsupply->setThreshold(20);
	warsupply->SetLocation(PointI(Padding + col * Pcon::WIDTH, top + row * Pcon::HEIGHT));
	pcons.push_back(warsupply);
	++row; col = 0;

	alcohol = new PconAlcohol(this, "alcohol");
	alcohol->setIcon("Dwarven_Ale.png", 4, 0, 46);
	alcohol->setChatName("Alcohol");
	alcohol->setThreshold(10);
	alcohol->SetLocation(PointI(Padding + col * Pcon::WIDTH, top + row * Pcon::HEIGHT));
	pcons.push_back(alcohol);
	++col;

	lunars = new PconLunar(this, "lunars");
	lunars->setIcon("Lunar_Fortune.png", 0, 0, 52);
	lunars->setChatName("Lunar Fortunes");
	lunars->setEffectID(SkillID::Lunar_Blessing);
	lunars->setThreshold(10);
	lunars->SetLocation(PointI(Padding + col * Pcon::WIDTH, top + row * Pcon::HEIGHT));
	pcons.push_back(lunars);
	++col;

	city = new PconCity(this, "city");
	city->setIcon("Sugary_Blue_Drink.png", 4, 0, 46);
	city->setChatName("City speedboost");
	city->setThreshold(20);
	city->SetLocation(PointI(Padding + col * Pcon::WIDTH, top + row * Pcon::HEIGHT));
	pcons.push_back(city);
	++row; col = 0;

	kabob = new Pcon(this, "kabob");
	kabob->setIcon("Drake_Kabob.png", 2, 0, 48);
	kabob->setChatName("Drake Kabob");
	kabob->setItemID(ItemID::Kabobs);
	kabob->setEffectID(SkillID::Drake_Skin);
	kabob->setThreshold(10);
	kabob->SetLocation(PointI(Padding + col * Pcon::WIDTH, top + row * Pcon::HEIGHT));
	pcons.push_back(kabob);
	++col;

	skalesoup = new Pcon(this, "skalesoup");
	skalesoup->setIcon("Bowl_of_Skalefin_Soup.png", 0, 2, 54);
	skalesoup->setChatName("Bowl of Skalefin Soup");
	skalesoup->setItemID(ItemID::SkalefinSoup);
	skalesoup->setEffectID(SkillID::Skale_Vigor);
	skalesoup->setThreshold(10);
	skalesoup->SetLocation(PointI(Padding + col * Pcon::WIDTH, top + row * Pcon::HEIGHT));
	pcons.push_back(skalesoup);
	++col;

	pahnai = new Pcon(this, "pahnai");
	pahnai->setIcon("Pahnai_Salad.png", 3, 0, 54);
	pahnai->setChatName("Pahnai Salad");
	pahnai->setItemID(ItemID::PahnaiSalad);
	pahnai->setEffectID(SkillID::Pahnai_Salad_item_effect);
	pahnai->setThreshold(10);
	pahnai->SetLocation(PointI(Padding + col * Pcon::WIDTH, top + row * Pcon::HEIGHT));
	pcons.push_back(pahnai);
	++row; col = 0;

	for (Pcon* pcon : pcons) {
		AddControl(pcon);
		pcon->scanInventory();
		pcon->UpdateUI();
	}

	initialized = true;
}

void PconPanel::Draw(IDirect3DDevice9* pDevice) {
	if (!initialized) return;

	if (current_map_type != GW::Map().GetInstanceType()) {
		current_map_type = GW::Map().GetInstanceType();
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

void PconPanel::Update() {
	if (!initialized) return;

	if (!enabled) return;
	
	InstanceType type = GW::Map().GetInstanceType();
	if (type == InstanceType::Loading) return;
	if (GW::Agents().GetPlayerId() == 0) return;
	if (GW::Agents().GetPlayer() == NULL) return;
	if (GW::Agents().GetPlayer()->GetIsDead()) return;

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
	GWToolbox& tb = GWToolbox::instance();
	tb.main_window->UpdatePconToggleButton(enabled);
	if (tb.main_window->minimized() || !tb.main_window->GetVisible()) {
		ChatLogger::LogF(L"Pcons %ls", active ? L"enabled" : L"disabled");
	}
	return enabled;
}
