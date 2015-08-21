#include "Pcons.h"
#include "../API/GwConstants.h"
#include "../API/APIMain.h"

//#include "Config.h"
#include "GWToolbox.h"

using namespace GWAPI;
using namespace GwConstants;

Pcons::Pcons() {
	/*pconsActive = vector<bool>(Pcons::count, false);

	pconsName = vector<wstring>(Pcons::count);
	pconsName[Pcons::Cons] = L"cons";
	pconsName[Pcons::Alcohol] = L"alcohol";
	pconsName[Pcons::RRC] = L"RRC";
	pconsName[Pcons::BRC] = L"BRC";
	pconsName[Pcons::GRC] = L"GRC";
	pconsName[Pcons::Pie] = L"pie";
	pconsName[Pcons::Cupcake] = L"cupcake";
	pconsName[Pcons::Apple] = L"apple";
	pconsName[Pcons::Corn] = L"corn";
	pconsName[Pcons::Egg] = L"egg";
	pconsName[Pcons::Kabob] = L"kabob";
	pconsName[Pcons::Warsupply] = L"warsupply";
	pconsName[Pcons::Lunars] = L"lunars";
	pconsName[Pcons::Res] = L"res";
	pconsName[Pcons::Skalesoup] = L"skalesoup";
	pconsName[Pcons::Mobstoppers] = L"mobstoppers";
	pconsName[Pcons::Panhai] = L"pahnai";
	pconsName[Pcons::City] = L"city";
	
	pconsItemID = vector<int>(Pcons::count, -1);
	pconsItemID[Pcons::RRC] = GwConstants::ItemID::RRC;
	pconsItemID[Pcons::BRC] = GwConstants::ItemID::BRC;
	pconsItemID[Pcons::GRC] = GwConstants::ItemID::GRC;
	pconsItemID[Pcons::Pie] = GwConstants::ItemID::Pies;
	pconsItemID[Pcons::Cupcake] = GwConstants::ItemID::Cupcakes;
	pconsItemID[Pcons::Apple] = GwConstants::ItemID::Apples;
	pconsItemID[Pcons::Corn] = GwConstants::ItemID::Corns;
	pconsItemID[Pcons::Egg] = GwConstants::ItemID::Eggs;
	pconsItemID[Pcons::Kabob] = GwConstants::ItemID::Kabobs;
	pconsItemID[Pcons::Warsupply] = GwConstants::ItemID::Warsupplies;
	pconsItemID[Pcons::Res] = GwConstants::ItemID::ResScrolls;
	pconsItemID[Pcons::Skalesoup] = GwConstants::ItemID::SkalefinSoup;
	pconsItemID[Pcons::Mobstoppers] = GwConstants::ItemID::Mobstopper;
	pconsItemID[Pcons::Panhai] = GwConstants::ItemID::PahnaiSalad;

	pconsTimer = vector<timer_t>(Pcons::count, TBTimer::init());
*/
}

Pcons::~Pcons() {
}

void Pcons::loadIni() {
	Config* config = GWToolbox::getInstance()->config;
	const wchar_t* section = L"pcons";
	enabled = config->iniReadBool(section, L"active", false);
	//for (size_t i = 0; i < Pcons::count; ++i) {
	//	pconsActive[i] = config->iniReadBool(section, pconsName[i].c_str(), false);
	//}
}

OSHGui::Panel* Pcons::buildUI() {
	Panel* panel = new Panel();
	panel->SetBackColor(Drawing::Color::Empty());
	panel->SetSize(0, 0);
	LOG("building pcons ui\n");
	int row = 0;
	int col = 0;

	essence = new Pcon(L"essence", -5, -5);
	essence->setIcon("Essence_of_Celerity.png");
	essence->setChatName(L"Essence of Celerity");
	essence->setItemID(ItemID::ConsEssence);
	essence->setEffectID(Effect::ConsEssence);
	essence->setThreshold(5);
	essence->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	panel->AddControl(essence);
	++col;

	grail = new Pcon(L"grail", -5, -5);
	grail->setIcon("Grail_of_Might.png");
	grail->setChatName(L"Grail of Might");
	grail->setItemID(ItemID::ConsGrail);
	grail->setEffectID(Effect::ConsGrail);
	grail->setThreshold(5);
	grail->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	panel->AddControl(grail);
	++col;

	armor = new Pcon(L"armor", -5, -5);
	armor->setIcon("Armor_of_Salvation.png");
	armor->setChatName(L"Armor of Salvation");
	armor->setItemID(ItemID::ConsArmor);
	armor->setEffectID(Effect::ConsArmor);
	armor->setThreshold(5);
	armor->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	panel->AddControl(armor);
	++row; col = 0;

	redrock = new Pcon(L"redrock", -5, -5);
	redrock->setIcon("Red_Rock_Candy.png");
	redrock->setChatName(L"Red Rock Candy");
	redrock->setItemID(ItemID::RRC);
	redrock->setEffectID(Effect::Redrock);
	redrock->setThreshold(5);
	redrock->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	panel->AddControl(redrock);
	++col;

	bluerock = new Pcon(L"bluerock", -5, -5);
	bluerock->setIcon("Blue_Rock_Candy.png");
	bluerock->setChatName(L"Blue Rock Candy");
	bluerock->setItemID(ItemID::BRC);
	bluerock->setEffectID(Effect::Bluerock);
	bluerock->setThreshold(10);
	bluerock->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	panel->AddControl(bluerock);
	++col;

	greenrock = new Pcon(L"greenrock", -5, -5);
	greenrock->setIcon("Green_Rock_Candy.png");
	greenrock->setChatName(L"Green Rock Candy");
	greenrock->setItemID(ItemID::GRC);
	greenrock->setEffectID(Effect::Greenrock);
	greenrock->setThreshold(15);
	greenrock->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	panel->AddControl(greenrock);
	++row; col = 0;

	cupcake = new Pcon(L"cupcake", -5, -7);
	cupcake->setIcon("Birthday_Cupcake.png");
	cupcake->setChatName(L"Birthday Cupcake");
	cupcake->setItemID(ItemID::Cupcakes);
	cupcake->setEffectID(Effect::Cupcake);
	cupcake->setThreshold(10);
	cupcake->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	panel->AddControl(cupcake);
	++col;

	apple = new Pcon(L"apple", -5, -10);
	apple->setIcon("Candy_Apple.png");
	apple->setChatName(L"Candy Apple");
	apple->setItemID(ItemID::Apples);
	apple->setEffectID(Effect::Apple);
	apple->setThreshold(10);
	apple->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	panel->AddControl(apple);
	++col;

	corn = new Pcon(L"corn", -5, -5);
	corn->setIcon("Candy_Corn.png");
	corn->setChatName(L"Candy Corn");
	corn->setItemID(ItemID::Corns);
	corn->setEffectID(Effect::Corn);
	corn->setThreshold(10);
	corn->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	panel->AddControl(corn);
	++row; col = 0;

	egg = new Pcon(L"egg", -2, -5);
	egg->setIcon("Golden_Egg.png");
	egg->setChatName(L"Golden Egg");
	egg->setItemID(ItemID::Eggs);
	egg->setEffectID(Effect::Egg);
	egg->setThreshold(20);
	egg->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	panel->AddControl(egg);
	++col;

	pie = new Pcon(L"pie", -2, -5);
	pie->setIcon("Slice_of_Pumpkin_Pie.png");
	pie->setChatName(L"Slice of Pumpkin_Pie");
	pie->setItemID(ItemID::Pies);
	pie->setEffectID(Effect::Pie);
	pie->setThreshold(10);
	pie->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	panel->AddControl(pie);
	++col;

	city = new Pcon(L"city", -5, -5);
	city->setIcon("Sugary_Blue_Drink.png");
	city->setChatName(L"City speedboost");
	city->setThreshold(20);
	city->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	panel->AddControl(city);
	++row; col = 0;

	alcohol = new Pcon(L"alcohol", -5, -5);
	alcohol->setIcon("Dwarven_Ale.png");
	alcohol->setChatName(L"Alcohol");
	alcohol->setThreshold(10);
	alcohol->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	panel->AddControl(alcohol);
	++col;

	lunars = new Pcon(L"lunars", -5, -5);
	lunars->setIcon("Lunar_Fortune.png");
	lunars->setChatName(L"Lunar Fortunes");
	lunars->setEffectID(Effect::Lunars);
	lunars->setThreshold(10);
	lunars->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	panel->AddControl(lunars);
	++col;

	warsupply = new Pcon(L"warsupply", -5, -7);
	warsupply->setIcon("War_Supplies.png");
	warsupply->setChatName(L"War Supplies");
	warsupply->setItemID(ItemID::Warsupplies);
	warsupply->setEffectID(Effect::Warsupplies);
	warsupply->setThreshold(20);
	warsupply->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	panel->AddControl(warsupply);
	++row; col = 0;

	kabob = new Pcon(L"kabob", -5, -5);
	kabob->setIcon("Drake_Kabob.png");
	kabob->setChatName(L"Drake Kabob");
	kabob->setItemID(ItemID::Kabobs);
	kabob->setEffectID(Effect::Kabobs);
	kabob->setThreshold(10);
	kabob->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	panel->AddControl(kabob);
	++col;

	skalesoup = new Pcon(L"skalesoup", -5, -5);
	skalesoup->setIcon("Bowl_of_Skalefin_Soup.png");
	skalesoup->setChatName(L"Bowl of Skalefin Soup");
	skalesoup->setItemID(ItemID::SkalefinSoup);
	skalesoup->setEffectID(Effect::SkaleVigor);
	skalesoup->setThreshold(10);
	skalesoup->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	panel->AddControl(skalesoup);
	++col;

	pahnai = new Pcon(L"pahnai", -5, -5);
	pahnai->setIcon("Pahnai_Salad.png");
	pahnai->setChatName(L"Pahnai Salad");
	pahnai->setItemID(ItemID::PahnaiSalad);
	pahnai->setEffectID(Effect::PahnaiSalad);
	pahnai->setThreshold(10);
	pahnai->SetLocation(6 + col * Pcon::WIDTH, 6 + row * Pcon::HEIGHT);
	panel->AddControl(pahnai);
	++row; col = 0;

	scanInventory();

	return panel;
}

Pcon::Pcon(const wchar_t* ini, int xOffset, int yOffset) {
	Button::Button();
	
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
	
	pic->SetSize(48, 48);
	pic->SetLocation(xOffset, yOffset);
	pic->SetBackColor(Drawing::Color::Empty());
	pic->SetStretch(true);
	pic->SetEnabled(false);
	AddSubControl(pic);

	shadow->SetText(std::to_string(quantity));
	shadow->SetForeColor(Drawing::Color::Black());
	shadow->SetLocation(5, 5);
	AddSubControl(shadow);

	label_->SetText(std::to_string(quantity));
	label_->SetLocation(4, 4);

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
	if (enabled) {
		Drawing::Graphics g(*geometry_);
		g.DrawRectangle(Drawing::Color::Black(), 0.0, 0.0, (float)GetWidth() - 1, (float)GetHeight() - 1);
	}
}

void Pcon::setIcon(const char* icon) {
	pic->SetImage(Drawing::Image::FromFile(GWToolbox::getInstance()->config->getPathA(icon)));
}

void Pcon::checkAndUse() {
	if (enabled
		&& TBTimer::diff(timer) > 5000) {

		GWAPIMgr * API = GWAPIMgr::GetInstance();
		EffectMgr::Effect effect = API->Effects->GetPlayerEffectById(effectID);

		if (effect.SkillId == 0 || effect.GetTimeRemaining() < 1000) {
			if (API->Items->UseItemByModelId(itemID)) {
				timer = TBTimer::init();
			} else {
				scanInventory();
				API->Chat->WriteChatF(L"[WARNING] Cannot find %ls", chatName);
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
	
	label_->SetText(std::to_string(quantity));
	shadow->SetText(std::to_string(quantity));

	if (quantity == 0) {
		label_->SetForeColor(Drawing::Color::Red());
	} else if (quantity < threshold) {
		label_->SetForeColor(Drawing::Color::Yellow());
	} else {
		label_->SetForeColor(Drawing::Color::Green());
	}
}

void Pcons::mainRoutine() {
	if (!enabled) return;

	GWAPIMgr * API = GWAPIMgr::GetInstance();

	switch (API->Map->GetInstanceType()) {
	case GwConstants::InstanceType::Explorable:
		if (API->Agents->GetPlayer()->GetIsDead()) break;

		// use the standard ones
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

		// cons
		//if (pconsActive[Pcons::Cons]
		//	&& API->Map->GetInstanceTime() < (60 * 1000)
		//	&& API->Effects->GetPlayerEffectById(Effect::ConsEssence).SkillId
		//	&& API->Effects->GetPlayerEffectById(Effect::ConsArmor).SkillId
		//	&& API->Effects->GetPlayerEffectById(Effect::ConsGrail).SkillId
		//	&& TBTimer::diff(pconsTimer[Pcons::Cons]) > 5000) {

		//	size_t partySize = API->Agents->GetPartySize();
		//	vector<AgentMgr::Agent*> party = *API->Agents->GetParty();
		//	if (partySize > 0 && party.size() == partySize) {
		//		bool everybodyAliveAndLoaded = true;
		//		for (size_t i = 0; i < party.size(); ++i) {
		//			if (party[i]->HP <= 0) {
		//				everybodyAliveAndLoaded = false;
		//			}
		//		}

		//		// if all is good, use cons
		//		if (everybodyAliveAndLoaded) {
		//			if (   pconsFind(ItemID::ConsEssence)
		//				&& pconsFind(ItemID::ConsArmor)
		//				&& pconsFind(ItemID::ConsGrail)) {

		//				API->Items->UseItemByModelId(ItemID::ConsEssence);
		//				API->Items->UseItemByModelId(ItemID::ConsGrail);
		//				API->Items->UseItemByModelId(ItemID::ConsArmor);
		//					
		//				pconsTimer[Pcons::Cons] = TBTimer::init();
		//			} else {
		//				scanInventory();
		//				API->Chat->WriteChat(L"[WARNING] Cannot find cons");
		//			}
		//		}
		//	}
		//	delete &party;
		//}

		//// alcohol
		//if (pconsActive[Pcons::Alcohol]) {
		//	if (API->Effects->GetAlcoholLevel() <= 1
		//		&& TBTimer::diff(pconsTimer[Pcons::Alcohol]) > 5000) {

		//		// use an alcohol item. Because of logical-OR only the first one will be used
		//		if (   API->Items->UseItemByModelId(ItemID::Eggnog)
		//			|| API->Items->UseItemByModelId(ItemID::DwarvenAle)
		//			|| API->Items->UseItemByModelId(ItemID::HuntersAle)
		//			|| API->Items->UseItemByModelId(ItemID::Absinthe)
		//			|| API->Items->UseItemByModelId(ItemID::WitchsBrew)
		//			|| API->Items->UseItemByModelId(ItemID::Ricewine)
		//			|| API->Items->UseItemByModelId(ItemID::ShamrockAle)
		//			|| API->Items->UseItemByModelId(ItemID::Cider)

		//			|| API->Items->UseItemByModelId(ItemID::Grog)
		//			|| API->Items->UseItemByModelId(ItemID::SpikedEggnog)
		//			|| API->Items->UseItemByModelId(ItemID::AgedDwarvenAle)
		//			|| API->Items->UseItemByModelId(ItemID::AgedHungersAle)
		//			|| API->Items->UseItemByModelId(ItemID::Keg)
		//			|| API->Items->UseItemByModelId(ItemID::FlaskOfFirewater)
		//			|| API->Items->UseItemByModelId(ItemID::KrytanBrandy)) {

		//			pconsTimer[Pcons::Alcohol] = TBTimer::init();
		//		} else {
		//			scanInventory();
		//			API->Chat->WriteChat(L"[WARNING] Cannot find Alcohol");
		//		}
		//	}
		//}

		//// lunars
		//if (pconsActive[Pcons::Lunars]
		//	&& API->Effects->GetPlayerEffectById(Effect::Lunars).SkillId
		//	&& TBTimer::diff(pconsTimer[Pcons::Lunars]) > 500) {

		//	if (   API->Items->UseItemByModelId(ItemID::LunarDragon)
		//		&& API->Items->UseItemByModelId(ItemID::LunarHorse)
		//		&& API->Items->UseItemByModelId(ItemID::LunarRabbit)
		//		&& API->Items->UseItemByModelId(ItemID::LunarSheep)
		//		&& API->Items->UseItemByModelId(ItemID::LunarSnake)) {

		//		pconsTimer[Pcons::Lunars] = TBTimer::init();
		//	} else {
		//		scanInventory();
		//		API->Chat->WriteChat(L"[WARNING] Cannot find Lunar Fortunes");
		//	}
		//}

		//// res scrolls
		//if (pconsActive[Pcons::Res]
		//	&& TBTimer::diff(pconsTimer[Pcons::Res]) > 500) {

		//	vector<AgentMgr::Agent*> party = *API->Agents->GetParty();
		//	for (size_t i = 0; i < party.size(); ++i) {
		//		AgentMgr::Agent * me = API->Agents->GetPlayer();
		//		if (party[i]->GetIsDead()
		//			&& API->Agents->GetSqrDistance(party[i], me) < SqrRange::Earshot) {
		//			if (API->Items->UseItemByModelId(ItemID::ResScrolls)) {
		//				pconsTimer[Pcons::Res] = TBTimer::init();
		//			} else {
		//				scanInventory();
		//				API->Chat->WriteChat(L"[WARNING] Cannot find Res Scrolls");
		//			}

		//		}
		//	}
		//	delete &party;
		//}

		//// mobstoppers
		//if (pconsActive[Pcons::Mobstoppers]
		//	&& API->Map->GetMapID() == MapID::UW
		//	&& API->Agents->GetTarget()->PlayerNumber == ModelID::SkeletonOfDhuum
		//	&& API->Agents->GetTarget()->HP < 0.25
		//	&& API->Agents->GetDistance(API->Agents->GetTarget(), API->Agents->GetPlayer())
		//	&& TBTimer::diff(pconsTimer[Pcons::Mobstoppers]) > 5000) {

		//	if (API->Items->UseItemByModelId(ItemID::Mobstopper)) {
		//		pconsTimer[Pcons::Mobstoppers] = TBTimer::init();
		//	} else {
		//		scanInventory();
		//		API->Chat->WriteChat(L"[WARNING] Cannot find Mobstoppers");
		//	}
		//}
		break;

	case GwConstants::InstanceType::Outpost:
		//if (pconsActive[Pcons::City]
		//	&& pconsTimer[Pcons::City] > 5000) {

		//	if (API->Agents->GetPlayer()->MoveX > 0 || API->Agents->GetPlayer()->MoveY > 0) {
		//		if (API->Effects->GetPlayerEffectById(Effect::CremeBrulee).SkillId
		//			|| API->Effects->GetPlayerEffectById(Effect::BlueDrink).SkillId
		//			|| API->Effects->GetPlayerEffectById(Effect::ChocolateBunny).SkillId
		//			|| API->Effects->GetPlayerEffectById(Effect::RedBeanCake).SkillId) {
		//			// then we have effect on already
		//		} else {
		//			// we should use it
		//			if (API->Items->UseItemByModelId(ItemID::CremeBrulee)
		//				|| API->Items->UseItemByModelId(ItemID::ChocolateBunny)
		//				|| API->Items->UseItemByModelId(ItemID::Fruitcake)
		//				|| API->Items->UseItemByModelId(ItemID::SugaryBlueDrink)
		//				|| API->Items->UseItemByModelId(ItemID::RedBeanCake)
		//				|| API->Items->UseItemByModelId(ItemID::JarOfHoney)) {

		//				pconsTimer[Pcons::City] = TBTimer::init();
		//			} else {
		//				scanInventory();
		//				API->Chat->WriteChat(L"[WARNING] Cannot find a city speedboost");
		//			}
		//		}
		//	}
		//}
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

	//vector<unsigned int> quantity(Pcons::count, 0);
	//unsigned int quantityEssence = 0;
	//unsigned int quantityGrail = 0;
	//unsigned int quantityArmor = 0;

	//ItemMgr::Bag** bags = GWAPIMgr::GetInstance()->Items->GetBagArray();
	//ItemMgr::Bag* curBag = NULL;
	//for (int bagIndex = 1; bagIndex <= 4; ++bagIndex) {
	//	curBag = bags[bagIndex];
	//	if (curBag != NULL) {
	//		ItemMgr::ItemArray curItems = curBag->Items;
	//		for (unsigned int i = 0; i < curItems.size(); i++) {
	//			switch (curItems[i]->ModelId) {
	//				case ItemID::ConsEssence:
	//					quantityEssence += curItems[i]->Quantity;
	//					break;
	//				case ItemID::ConsArmor:
	//					quantityArmor += curItems[i]->Quantity;
	//					break;
	//				case ItemID::ConsGrail:
	//					quantityGrail += curItems[i]->Quantity;
	//					break;

	//				case ItemID::LunarDragon:
	//				case ItemID::LunarHorse:
	//				case ItemID::LunarRabbit:
	//				case ItemID::LunarSheep:
	//				case ItemID::LunarSnake:
	//					quantity[Pcons::Lunars] += curItems[i]->Quantity;
	//					break;
	//				
	//				case ItemID::Eggnog:
	//				case ItemID::DwarvenAle:
	//				case ItemID::HuntersAle:
	//				case ItemID::Absinthe:
	//				case ItemID::WitchsBrew:
	//				case ItemID::Ricewine:
	//				case ItemID::ShamrockAle:
	//				case ItemID::Cider:
	//					quantity[Pcons::Alcohol] += curItems[i]->Quantity;
	//					break;

	//				case ItemID::Grog:
	//				case ItemID::SpikedEggnog:
	//				case ItemID::AgedDwarvenAle:
	//				case ItemID::AgedHungersAle:
	//				case ItemID::Keg:
	//				case ItemID::FlaskOfFirewater:
	//				case ItemID::KrytanBrandy:
	//					quantity[Pcons::Alcohol] += curItems[i]->Quantity * 5;
	//					break;

	//				case ItemID::CremeBrulee:
	//				case ItemID::SugaryBlueDrink:
	//				case ItemID::ChocolateBunny:
	//				case ItemID::RedBeanCake:
	//					quantity[Pcons::City] += curItems[i]->Quantity;
	//					break;

	//				default:
	//					for (unsigned int i = 0; i < Pcons::count; ++i) {
	//						if (curItems[i]->ModelId > 0 && curItems[i]->ModelId == pconsItemID[i]) {
	//							quantity[i] += curItems[i]->Quantity;
	//						}
	//					}
	//					break;
	//			}
	//		}
	//	}
	//}
	//quantity[Pcons::Cons] = min(min(quantityArmor, quantityEssence), quantityGrail);

	//for (unsigned int i = 0; i < Pcons::count; ++i) {
	//	pconsActive[i] = pconsActive[i] && (quantity[i] > 0);
	//	
	//	// TODO: update GUI
	//}
}

