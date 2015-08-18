#include "Pcons.h"
#include "../API/GwConstants.h"
#include "../API/APIMain.h"

//#include "Config.h"
#include "GWToolbox.h"

using namespace GWAPI;
using namespace GwConstants;

Pcons::Pcons() :
initializer(0),
Cons(initializer++),
Alcohol(initializer++),
RRC(initializer++),
BRC(initializer++),
GRC(initializer++),
Pie(initializer++),
Cupcake(initializer++),
Apple(initializer++),
Corn(initializer++),
Egg(initializer++),
Kabob(initializer++),
Warsupply(initializer++),
Lunars(initializer++),
Res(initializer++),
Skalesoup(initializer++),
Mobstoppers(initializer++),
Panhai(initializer++),
City(initializer++),
count(initializer)
{

	pconsActive = vector<bool>(Pcons::count, false);

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
	pconsName[Pcons::Panhai] = L"panhai";
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

	pconsEffect = vector<int>(Pcons::count, -1);
	pconsEffect[Pcons::RRC] = Effect::Redrock;
	pconsEffect[Pcons::BRC] = Effect::Bluerock;
	pconsEffect[Pcons::GRC] = Effect::Greenrock;
	pconsEffect[Pcons::Pie] = Effect::Pie;
	pconsEffect[Pcons::Cupcake] = Effect::Cupcake;
	pconsEffect[Pcons::Apple] = Effect::Apple;
	pconsEffect[Pcons::Corn] = Effect::Corn;
	pconsEffect[Pcons::Egg] = Effect::Egg;
	pconsEffect[Pcons::Kabob] = Effect::Kabobs;
	pconsEffect[Pcons::Warsupply] = Effect::Warsupplies;
	pconsEffect[Pcons::Skalesoup] = Effect::SkaleVigor;
	pconsEffect[Pcons::Panhai] = Effect::PahnaiSalad;

	pconsChatName = vector<wstring>(Pcons::count, L"");
	pconsChatName[Pcons::RRC] = L"Red Rocks";
	pconsChatName[Pcons::BRC] = L"Blue Rocks";
	pconsChatName[Pcons::GRC] = L"Green Rocks";
	pconsChatName[Pcons::Pie] = L"Pies";
	pconsChatName[Pcons::Cupcake] = L"Cupcakes";
	pconsChatName[Pcons::Apple] = L"Apples";
	pconsChatName[Pcons::Corn] = L"Corns";
	pconsChatName[Pcons::Egg] = L"Eggs";
	pconsChatName[Pcons::Kabob] = L"Kabobs";
	pconsChatName[Pcons::Warsupply] = L"War Supplies";
	pconsChatName[Pcons::Skalesoup] = L"Skalefin Soup";
	pconsChatName[Pcons::Panhai] = L"Panhai Salad";
}

Pcons::~Pcons() {
}

void Pcons::loadIni() {
	Config* config = GWToolbox::getInstance()->config;
	const wchar_t* section = L"pcons";
	enabled = config->iniReadBool(section, L"active", false);
	for (size_t i = 0; i < Pcons::count; ++i) {
		pconsActive[i] = config->iniReadBool(section, pconsName[i].c_str(), false);
	}
}

void Pcons::buildUI() {
	// TODO
}

void Pcons::mainRoutine() {
	if (!enabled) return;

	GWAPIMgr * API = GWAPIMgr::GetInstance();

	switch (API->Map->GetInstanceType()) {
	case GwConstants::InstanceType::Explorable:
		if (API->Agents->GetPlayer()->GetIsDead()) break;

		// use the standard ones
		checkAndUsePcon(Pcons::RRC);
		checkAndUsePcon(Pcons::BRC);
		checkAndUsePcon(Pcons::GRC);
		checkAndUsePcon(Pcons::Pie);
		checkAndUsePcon(Pcons::Cupcake);
		checkAndUsePcon(Pcons::Apple);
		checkAndUsePcon(Pcons::Corn);
		checkAndUsePcon(Pcons::Egg);
		checkAndUsePcon(Pcons::Kabob);
		checkAndUsePcon(Pcons::Warsupply);
		checkAndUsePcon(Pcons::Skalesoup);
		checkAndUsePcon(Pcons::Panhai);

		// cons
		if (pconsActive[Pcons::Cons]
			&& API->Map->GetInstanceTime() < (60 * 1000)
			&& API->Effects->GetPlayerEffectById(Effect::ConsEssence).SkillId
			&& API->Effects->GetPlayerEffectById(Effect::ConsArmor).SkillId
			&& API->Effects->GetPlayerEffectById(Effect::ConsGrail).SkillId
			&& TBTimer::diff(pconsTimer[Pcons::Cons]) > 5000) {

			size_t partySize = API->Agents->GetPartySize();
			vector<AgentMgr::Agent*> party = *API->Agents->GetParty();
			if (partySize > 0 && party.size() == partySize) {
				bool everybodyAliveAndLoaded = true;
				for (size_t i = 0; i < party.size(); ++i) {
					if (party[i]->HP <= 0) {
						everybodyAliveAndLoaded = false;
					}
				}

				// if all is good, use cons
				if (everybodyAliveAndLoaded) {
					if (   pconsFind(ItemID::ConsEssence)
						&& pconsFind(ItemID::ConsArmor)
						&& pconsFind(ItemID::ConsGrail)) {

						API->Items->UseItemByModelId(ItemID::ConsEssence);
						API->Items->UseItemByModelId(ItemID::ConsGrail);
						API->Items->UseItemByModelId(ItemID::ConsArmor);
							
						pconsTimer[Pcons::Cons] = TBTimer::init();
					} else {
						scanInventory();
						API->Chat->WriteChat(L"[WARNING] Cannot find cons");
					}
				}
			}
			delete &party;
		}

		// alcohol
		if (pconsActive[Pcons::Alcohol]) {
			if (API->Effects->GetAlcoholLevel() <= 1
				&& TBTimer::diff(pconsTimer[Pcons::Alcohol]) > 5000) {

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

					pconsTimer[Pcons::Alcohol] = TBTimer::init();
				} else {
					scanInventory();
					API->Chat->WriteChat(L"[WARNING] Cannot find Alcohol");
				}
			}
		}

		// lunars
		if (pconsActive[Pcons::Lunars]
			&& API->Effects->GetPlayerEffectById(Effect::Lunars).SkillId
			&& TBTimer::diff(pconsTimer[Pcons::Lunars]) > 500) {

			if (   API->Items->UseItemByModelId(ItemID::LunarDragon)
				&& API->Items->UseItemByModelId(ItemID::LunarHorse)
				&& API->Items->UseItemByModelId(ItemID::LunarRabbit)
				&& API->Items->UseItemByModelId(ItemID::LunarSheep)
				&& API->Items->UseItemByModelId(ItemID::LunarSnake)) {

				pconsTimer[Pcons::Lunars] = TBTimer::init();
			} else {
				scanInventory();
				API->Chat->WriteChat(L"[WARNING] Cannot find Lunar Fortunes");
			}
		}

		// res scrolls
		if (pconsActive[Pcons::Res]
			&& TBTimer::diff(pconsTimer[Pcons::Res]) > 500) {

			vector<AgentMgr::Agent*> party = *API->Agents->GetParty();
			for (size_t i = 0; i < party.size(); ++i) {
				AgentMgr::Agent * me = API->Agents->GetPlayer();
				if (party[i]->GetIsDead()
					&& API->Agents->GetSqrDistance(party[i], me) < SqrRange::Earshot) {
					if (API->Items->UseItemByModelId(ItemID::ResScrolls)) {
						pconsTimer[Pcons::Res] = TBTimer::init();
					} else {
						scanInventory();
						API->Chat->WriteChat(L"[WARNING] Cannot find Res Scrolls");
					}

				}
			}
			delete &party;
		}

		// mobstoppers
		if (pconsActive[Pcons::Mobstoppers]
			&& API->Map->GetMapID() == MapID::UW
			&& API->Agents->GetTarget()->PlayerNumber == ModelID::SkeletonOfDhuum
			&& API->Agents->GetTarget()->HP < 0.25
			&& API->Agents->GetDistance(API->Agents->GetTarget(), API->Agents->GetPlayer())
			&& TBTimer::diff(pconsTimer[Pcons::Mobstoppers]) > 5000) {

			if (API->Items->UseItemByModelId(ItemID::Mobstopper)) {
				pconsTimer[Pcons::Mobstoppers] = TBTimer::init();
			} else {
				scanInventory();
				API->Chat->WriteChat(L"[WARNING] Cannot find Mobstoppers");
			}
		}
		break;

	case GwConstants::InstanceType::Outpost:
		if (pconsActive[Pcons::City]
			&& pconsTimer[Pcons::City] > 5000) {

			if (API->Agents->GetPlayer()->MoveX > 0 || API->Agents->GetPlayer()->MoveY > 0) {
				if (API->Effects->GetPlayerEffectById(Effect::CremeBrulee).SkillId
					|| API->Effects->GetPlayerEffectById(Effect::BlueDrink).SkillId
					|| API->Effects->GetPlayerEffectById(Effect::ChocolateBunny).SkillId
					|| API->Effects->GetPlayerEffectById(Effect::RedBeanCake).SkillId) {
					// then we have effect on already
				} else {
					// we should use it
					if (API->Items->UseItemByModelId(ItemID::CremeBrulee)
						|| API->Items->UseItemByModelId(ItemID::ChocolateBunny)
						|| API->Items->UseItemByModelId(ItemID::Fruitcake)
						|| API->Items->UseItemByModelId(ItemID::SugaryBlueDrink)
						|| API->Items->UseItemByModelId(ItemID::RedBeanCake)
						|| API->Items->UseItemByModelId(ItemID::JarOfHoney)) {

						pconsTimer[Pcons::City] = TBTimer::init();
					} else {
						scanInventory();
						API->Chat->WriteChat(L"[WARNING] Cannot find a city speedboost");
					}
				}
			}
		}
		break;

	case GwConstants::InstanceType::Loading:
		break;
	}
}

void Pcons::checkAndUsePcon(int PconID) {
	if (pconsActive[PconID]
		&& TBTimer::diff(pconsTimer[PconID]) > 5000) {

		GWAPIMgr * API = GWAPIMgr::GetInstance();
		EffectMgr::Effect effect = API->Effects->GetPlayerEffectById(pconsEffect[PconID]);

		if (effect.SkillId == 0 || effect.GetTimeRemaining() < 1000) {
			if (API->Items->UseItemByModelId(pconsItemID[PconID])) {
				pconsTimer[PconID] = TBTimer::init();
			} else {
				scanInventory();
				wstring msg(L"[WARNING] Cannot find ");
				msg.append(pconsChatName[PconID]);
				API->Chat->WriteChat(msg.c_str());
			}
		}
	}
}

bool Pcons::pconsFind(unsigned int ModelID) {
	ItemMgr::Bag** bags = GWAPIMgr::GetInstance()->Items->GetBagArray();
	ItemMgr::Bag* curBag = NULL;

	for (int bagIndex = 1; bagIndex <= 4; ++bagIndex) {
		curBag = bags[bagIndex];
		if (curBag != NULL) {
			ItemMgr::ItemArray curItems = curBag->Items;
			for (DWORD i = 0; i < curItems.size(); i++) {
				if (curItems[i]->ModelId = ModelID) {
					return true;
				}
			}
		}
	}
	return false;
}

void Pcons::scanInventory() {
	vector<unsigned int> quantity(Pcons::count, 0);
	unsigned int quantityEssence = 0;
	unsigned int quantityGrail = 0;
	unsigned int quantityArmor = 0;

	ItemMgr::Bag** bags = GWAPIMgr::GetInstance()->Items->GetBagArray();
	ItemMgr::Bag* curBag = NULL;
	for (int bagIndex = 1; bagIndex <= 4; ++bagIndex) {
		curBag = bags[bagIndex];
		if (curBag != NULL) {
			ItemMgr::ItemArray curItems = curBag->Items;
			for (unsigned int i = 0; i < curItems.size(); i++) {
				switch (curItems[i]->ModelId) {
					case ItemID::ConsEssence:
						quantityEssence += curItems[i]->Quantity;
						break;
					case ItemID::ConsArmor:
						quantityArmor += curItems[i]->Quantity;
						break;
					case ItemID::ConsGrail:
						quantityGrail += curItems[i]->Quantity;
						break;

					case ItemID::LunarDragon:
					case ItemID::LunarHorse:
					case ItemID::LunarRabbit:
					case ItemID::LunarSheep:
					case ItemID::LunarSnake:
						quantity[Pcons::Lunars] += curItems[i]->Quantity;
						break;
					
					case ItemID::Eggnog:
					case ItemID::DwarvenAle:
					case ItemID::HuntersAle:
					case ItemID::Absinthe:
					case ItemID::WitchsBrew:
					case ItemID::Ricewine:
					case ItemID::ShamrockAle:
					case ItemID::Cider:
						quantity[Pcons::Alcohol] += curItems[i]->Quantity;
						break;

					case ItemID::Grog:
					case ItemID::SpikedEggnog:
					case ItemID::AgedDwarvenAle:
					case ItemID::AgedHungersAle:
					case ItemID::Keg:
					case ItemID::FlaskOfFirewater:
					case ItemID::KrytanBrandy:
						quantity[Pcons::Alcohol] += curItems[i]->Quantity * 5;
						break;

					case ItemID::CremeBrulee:
					case ItemID::SugaryBlueDrink:
					case ItemID::ChocolateBunny:
					case ItemID::RedBeanCake:
						quantity[Pcons::City] += curItems[i]->Quantity;
						break;

					default:
						for (unsigned int i = 0; i < Pcons::count; ++i) {
							if (curItems[i]->ModelId > 0 && curItems[i]->ModelId == pconsItemID[i]) {
								quantity[i] += curItems[i]->Quantity;
							}
						}
						break;
				}
			}
		}
	}
	quantity[Pcons::Cons] = min(min(quantityArmor, quantityEssence), quantityGrail);

	for (unsigned int i = 0; i < Pcons::count; ++i) {
		pconsActive[i] = pconsActive[i] && (quantity[i] > 0);
		
		// TODO: update GUI
	}
}

