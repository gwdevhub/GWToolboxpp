#include "PconPanel.h"
#include "../API/GwConstants.h"
#include "../API/APIMain.h"
#include "MainWindow.h"

//#include "Config.h"
#include "GWToolbox.h"

using namespace GWAPI;
using namespace OSHGui::Drawing;
using namespace GwConstants;

PconPanel::PconPanel() {
	initialized = false;
	enabled = false;
}

void PconPanel::buildUI() {
	SetSize(6 * 2 + Pcon::WIDTH * 3, 6 * 2 + Pcon::HEIGHT * 6);
	LOG("building pcons ui\n");
	int row = 0;
	int col = 0;

	essence = new PconCons(L"essence");
	essence->setIcon("Essence_of_Celerity.png", 0, 0, 64);
	essence->setChatName(L"Essence of Celerity");
	essence->setItemID(ItemID::ConsEssence);
	essence->setEffectID(GwConstants::EffectID::ConsEssence);
	essence->setThreshold(5);
	essence->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	AddControl(essence);
	++col;

	grail = new PconCons(L"grail");
	grail->setIcon("Grail_of_Might.png", 0, 0, 56);
	grail->setChatName(L"Grail of Might");
	grail->setItemID(ItemID::ConsGrail);
	grail->setEffectID(GwConstants::EffectID::ConsGrail);
	grail->setThreshold(5);
	grail->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	AddControl(grail);
	++col;

	armor = new PconCons(L"armor");
	armor->setIcon("Armor_of_Salvation.png", 0, 0, 54);
	armor->setChatName(L"Armor of Salvation");
	armor->setItemID(ItemID::ConsArmor);
	armor->setEffectID(GwConstants::EffectID::ConsArmor);
	armor->setThreshold(5);
	armor->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	AddControl(armor);
	++row; col = 0;

	redrock = new Pcon(L"redrock");
	redrock->setIcon("Red_Rock_Candy.png", 0, 0, 56);
	redrock->setChatName(L"Red Rock Candy");
	redrock->setItemID(ItemID::RRC);
	redrock->setEffectID(GwConstants::EffectID::Redrock);
	redrock->setThreshold(5);
	redrock->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	AddControl(redrock);
	++col;

	bluerock = new Pcon(L"bluerock");
	bluerock->setIcon("Blue_Rock_Candy.png", 0, 0, 56);
	bluerock->setChatName(L"Blue Rock Candy");
	bluerock->setItemID(ItemID::BRC);
	bluerock->setEffectID(GwConstants::EffectID::Bluerock);
	bluerock->setThreshold(10);
	bluerock->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	AddControl(bluerock);
	++col;

	greenrock = new Pcon(L"greenrock");
	greenrock->setIcon("Green_Rock_Candy.png", 0, 0, 56);
	greenrock->setChatName(L"Green Rock Candy");
	greenrock->setItemID(ItemID::GRC);
	greenrock->setEffectID(GwConstants::EffectID::Greenrock);
	greenrock->setThreshold(15);
	greenrock->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	AddControl(greenrock);
	++row; col = 0;

	cupcake = new Pcon(L"cupcake");
	cupcake->setIcon("Birthday_Cupcake.png", 0, 0, 56);
	cupcake->setChatName(L"Birthday Cupcake");
	cupcake->setItemID(ItemID::Cupcakes);
	cupcake->setEffectID(GwConstants::EffectID::Cupcake);
	cupcake->setThreshold(10);
	cupcake->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	AddControl(cupcake);
	++col;

	apple = new Pcon(L"apple");
	apple->setIcon("Candy_Apple.png", 0, 0, 56);
	apple->setChatName(L"Candy Apple");
	apple->setItemID(ItemID::Apples);
	apple->setEffectID(GwConstants::EffectID::Apple);
	apple->setThreshold(10);
	apple->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	AddControl(apple);
	++col;

	corn = new Pcon(L"corn");
	corn->setIcon("Candy_Corn.png", 0, 0, 56);
	corn->setChatName(L"Candy Corn");
	corn->setItemID(ItemID::Corns);
	corn->setEffectID(GwConstants::EffectID::Corn);
	corn->setThreshold(10);
	corn->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	AddControl(corn);
	++row; col = 0;

	egg = new Pcon(L"egg");
	egg->setIcon("Golden_Egg.png", 0, 0, 52);
	egg->setChatName(L"Golden Egg");
	egg->setItemID(ItemID::Eggs);
	egg->setEffectID(GwConstants::EffectID::Egg);
	egg->setThreshold(20);
	egg->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	AddControl(egg);
	++col;

	pie = new Pcon(L"pie");
	pie->setIcon("Slice_of_Pumpkin_Pie.png", 0, 0, 56);
	pie->setChatName(L"Slice of Pumpkin_Pie");
	pie->setItemID(ItemID::Pies);
	pie->setEffectID(GwConstants::EffectID::Pie);
	pie->setThreshold(10);
	pie->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	AddControl(pie);
	++col;

	city = new PconCity(L"city");
	city->setIcon("Sugary_Blue_Drink.png", 0, 0, 50);
	city->setChatName(L"City speedboost");
	city->setThreshold(20);
	city->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	AddControl(city);
	++row; col = 0;

	alcohol = new PconAlcohol(L"alcohol");
	alcohol->setIcon("Dwarven_Ale.png", 0, 0, 50);
	alcohol->setChatName(L"Alcohol");
	alcohol->setThreshold(10);
	alcohol->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	AddControl(alcohol);
	++col;

	lunars = new PconLunar(L"lunars");
	lunars->setIcon("Lunar_Fortune.png", 0, 0, 56);
	lunars->setChatName(L"Lunar Fortunes");
	lunars->setEffectID(GwConstants::EffectID::Lunars);
	lunars->setThreshold(10);
	lunars->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	AddControl(lunars);
	++col;

	warsupply = new Pcon(L"warsupply");
	warsupply->setIcon("War_Supplies.png", 0, 0, 48);
	warsupply->setChatName(L"War Supplies");
	warsupply->setItemID(ItemID::Warsupplies);
	warsupply->setEffectID(GwConstants::EffectID::Warsupplies);
	warsupply->setThreshold(20);
	warsupply->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	AddControl(warsupply);
	++row; col = 0;

	kabob = new Pcon(L"kabob");
	kabob->setIcon("Drake_Kabob.png", 0, 0, 52);
	kabob->setChatName(L"Drake Kabob");
	kabob->setItemID(ItemID::Kabobs);
	kabob->setEffectID(GwConstants::EffectID::Kabobs);
	kabob->setThreshold(10);
	kabob->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	AddControl(kabob);
	++col;

	skalesoup = new Pcon(L"skalesoup");
	skalesoup->setIcon("Bowl_of_Skalefin_Soup.png", 0, 0, 56);
	skalesoup->setChatName(L"Bowl of Skalefin Soup");
	skalesoup->setItemID(ItemID::SkalefinSoup);
	skalesoup->setEffectID(GwConstants::EffectID::SkaleVigor);
	skalesoup->setThreshold(10);
	skalesoup->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	AddControl(skalesoup);
	++col;

	pahnai = new Pcon(L"pahnai");
	pahnai->setIcon("Pahnai_Salad.png", 0, 0, 56);
	pahnai->setChatName(L"Pahnai Salad");
	pahnai->setItemID(ItemID::PahnaiSalad);
	pahnai->setEffectID(GwConstants::EffectID::PahnaiSalad);
	pahnai->setThreshold(10);
	pahnai->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	AddControl(pahnai);
	++row; col = 0;

	this->scanInventory();

	initialized = true;
}

void PconPanel::mainRoutine() {
	if (!enabled || !initialized) return;

	GWAPIMgr * API = GWAPIMgr::GetInstance();
	InstanceType type;
	try {
		type = API->Map->GetInstanceType();
		if (type == InstanceType::Loading) return;
		if (API->Agents->GetPlayerId() == 0) return;
		if (API->Agents->GetPlayer() == NULL) return;
		if (API->Agents->GetPlayer()->GetIsDead()) return;
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

void PconPanel::scanInventory() {
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

