#include "Pcons.h"
#include "../API/GwConstants.h"
#include "../API/APIMain.h"

using namespace GWAPI;
using namespace GwConstants;

Pcons::Pcons() {
	pconsActive = vector<bool>(pconsCount, false);

	pconsName = vector<string>(pconsCount);
	pconsName[pcons::Cons] = "cons";
	pconsName[pcons::Alcohol] = "alcohol";
	pconsName[pcons::RRC] = "RRC";
	pconsName[pcons::BRC] = "BRC";
	pconsName[pcons::GRC] = "GRC";
	pconsName[pcons::Pie] = "pie";
	pconsName[pcons::Cupcake] = "cupcake";
	pconsName[pcons::Apple] = "apple";
	pconsName[pcons::Corn] = "corn";
	pconsName[pcons::Egg] = "egg";
	pconsName[pcons::Kabob] = "kabob";
	pconsName[pcons::Warsupply] = "warsupply";
	pconsName[pcons::Lunars] = "lunars";
	pconsName[pcons::Res] = "res";
	pconsName[pcons::Skalesoup] = "skalesoup";
	pconsName[pcons::Mobstoppers] = "mobstoppers";
	pconsName[pcons::Panhai] = "panhai";
	pconsName[pcons::City] = "city";
	
	pconsItemID = vector<int>(pconsCount, -1);
	pconsItemID[pcons::RRC] = GwConstants::ItemID::RRC;
	pconsItemID[pcons::BRC] = GwConstants::ItemID::BRC;
	pconsItemID[pcons::GRC] = GwConstants::ItemID::GRC;
	pconsItemID[pcons::Pie] = GwConstants::ItemID::Pies;
	pconsItemID[pcons::Cupcake] = GwConstants::ItemID::Cupcakes;
	pconsItemID[pcons::Apple] = GwConstants::ItemID::Apples;
	pconsItemID[pcons::Corn] = GwConstants::ItemID::Corns;
	pconsItemID[pcons::Egg] = GwConstants::ItemID::Eggs;
	pconsItemID[pcons::Kabob] = GwConstants::ItemID::Kabobs;
	pconsItemID[pcons::Warsupply] = GwConstants::ItemID::Warsupplies;
	pconsItemID[pcons::Res] = GwConstants::ItemID::ResScrolls;
	pconsItemID[pcons::Skalesoup] = GwConstants::ItemID::SkalefinSoup;
	pconsItemID[pcons::Mobstoppers] = GwConstants::ItemID::Mobstopper;
	pconsItemID[pcons::Panhai] = GwConstants::ItemID::PahnaiSalad;

	pconsTimer = vector<timer_t>(pconsCount, Timer::init());

	pconsEffect = vector<int>(pconsCount, -1);
	pconsEffect[pcons::RRC] = Effect::Redrock;
	pconsEffect[pcons::BRC] = Effect::Bluerock;
	pconsEffect[pcons::GRC] = Effect::Greenrock;
	pconsEffect[pcons::Pie] = Effect::Pie;
	pconsEffect[pcons::Cupcake] = Effect::Cupcake;
	pconsEffect[pcons::Apple] = Effect::Apple;
	pconsEffect[pcons::Corn] = Effect::Corn;
	pconsEffect[pcons::Egg] = Effect::Egg;
	pconsEffect[pcons::Kabob] = Effect::Kabobs;
	pconsEffect[pcons::Warsupply] = Effect::Warsupplies;
	pconsEffect[pcons::Skalesoup] = Effect::SkaleVigor;
	pconsEffect[pcons::Panhai] = Effect::PahnaiSalad;
}

Pcons::~Pcons() {
}

void Pcons::loadIni() {
	// TODO
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
		checkAndUsePcon(pcons::RRC);
		checkAndUsePcon(pcons::BRC);
		checkAndUsePcon(pcons::GRC);
		checkAndUsePcon(pcons::Pie);
		checkAndUsePcon(pcons::Cupcake);
		checkAndUsePcon(pcons::Apple);
		checkAndUsePcon(pcons::Corn);
		checkAndUsePcon(pcons::Egg);
		checkAndUsePcon(pcons::Kabob);
		checkAndUsePcon(pcons::Warsupply);
		checkAndUsePcon(pcons::Skalesoup);
		checkAndUsePcon(pcons::Panhai);

		// cons
		if (pconsActive[pcons::Cons] 
			&& API->Map->GetInstanceTime() < (60 * 1000)
			&& API->Effects->GetPlayerEffectById(Effect::ConsEssence).SkillId
			&& API->Effects->GetPlayerEffectById(Effect::ConsArmor).SkillId
			&& API->Effects->GetPlayerEffectById(Effect::ConsGrail).SkillId
			&& Timer::diff(pconsTimer[pcons::Cons]) > 5000) {

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
							
						pconsTimer[pcons::Cons] = Timer::init();
					} else {
						scanInventory();
						// TODO WriteChat("[WARNING] Cannot find cons");
					}
				}
			}
			delete &party;
		}

		// alcohol
		if (pconsActive[pcons::Alcohol]) {
			if (API->Effects->GetAlcoholLevel() <= 1
				&& Timer::diff(pconsTimer[pcons::Alcohol]) > 5000) {

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

					pconsTimer[pcons::Alcohol] = Timer::init();
				} else {
					scanInventory();
					// TODO WriteChat("[WARNING] Cannot find Alcohol");
				}
			}
		}

		// lunars
		if (pconsActive[pcons::Lunars]
			&& API->Effects->GetPlayerEffectById(Effect::Lunars).SkillId
			&& Timer::diff(pconsTimer[pcons::Lunars]) > 500) {

			if (   API->Items->UseItemByModelId(ItemID::LunarDragon)
				&& API->Items->UseItemByModelId(ItemID::LunarHorse)
				&& API->Items->UseItemByModelId(ItemID::LunarRabbit)
				&& API->Items->UseItemByModelId(ItemID::LunarSheep)
				&& API->Items->UseItemByModelId(ItemID::LunarSnake)) {

				pconsTimer[pcons::Lunars] = Timer::init();
			} else {
				scanInventory();
				// TODO WriteChat("[WARNING] Cannot find Lunar Fortunes");
			}
		}

		// res scrolls
		if (pconsActive[pcons::Res]
			&& Timer::diff(pconsTimer[pcons::Res]) > 500) {

			vector<AgentMgr::Agent*> party = *API->Agents->GetParty();
			for (size_t i = 0; i < party.size(); ++i) {
				AgentMgr::Agent * me = API->Agents->GetPlayer();
				if (party[i]->GetIsDead()
					&& API->Agents->GetSqrDistance(party[i], me) < SqrRange::Earshot) {
					if (API->Items->UseItemByModelId(ItemID::ResScrolls)) {
						pconsTimer[pcons::Res] = Timer::init();
					} else {
						scanInventory();
						// TODO WriteChat("[WARNING] Cannot find Res Scrolls");
					}

				}
			}
			delete &party;
		}

		// mobstoppers
		if (pconsActive[pcons::Mobstoppers]
			&& API->Map->GetMapID() == MapID::UW
			&& API->Agents->GetTarget()->PlayerNumber == ModelID::SkeletonOfDhuum
			&& API->Agents->GetTarget()->HP < 0.25
			&& API->Agents->GetDistance(API->Agents->GetTarget(), API->Agents->GetPlayer())
			&& Timer::diff(pconsTimer[pcons::Mobstoppers]) > 5000) {

			if (API->Items->UseItemByModelId(ItemID::Mobstopper)) {
				pconsTimer[pcons::Mobstoppers] = Timer::init();
			} else {
				scanInventory();
				// TODO WriteChat("[WARNING] Cannot find Mobstoppers");
			}
		}

		// TODO all others
		break;

	case GwConstants::InstanceType::Outpost:
		if (pconsActive[pcons::City]
			&& pconsTimer[pcons::City] > 5000) {

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

						pconsTimer[pcons::City] = Timer::init();
					} else {
						scanInventory();
						// TODO WriteChat("[WARNING] Cannot find a city speedboost");
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
		&& Timer::diff(pconsTimer[PconID]) > 5000) {

		GWAPIMgr * API = GWAPIMgr::GetInstance();
		EffectMgr::Effect effect = API->Effects->GetPlayerEffectById(pconsEffect[PconID]);

		if (effect.SkillId == 0 || effect.GetTimeRemaining() < 1000) {
			if (API->Items->UseItemByModelId(pconsItemID[PconID])) {
				pconsTimer[PconID] = Timer::init();
			} else {
				scanInventory();
				// TODO WriteChat("[WARNING] Cannot find a something");
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
	vector<unsigned int> quantity(pconsCount, 0);
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
						quantity[pcons::Lunars] += curItems[i]->Quantity;
						break;
					
					case ItemID::Eggnog:
					case ItemID::DwarvenAle:
					case ItemID::HuntersAle:
					case ItemID::Absinthe:
					case ItemID::WitchsBrew:
					case ItemID::Ricewine:
					case ItemID::ShamrockAle:
					case ItemID::Cider:
						quantity[pcons::Alcohol] += curItems[i]->Quantity;
						break;

					case ItemID::Grog:
					case ItemID::SpikedEggnog:
					case ItemID::AgedDwarvenAle:
					case ItemID::AgedHungersAle:
					case ItemID::Keg:
					case ItemID::FlaskOfFirewater:
					case ItemID::KrytanBrandy:
						quantity[pcons::Alcohol] += curItems[i]->Quantity * 5;
						break;

					case ItemID::CremeBrulee:
					case ItemID::SugaryBlueDrink:
					case ItemID::ChocolateBunny:
					case ItemID::RedBeanCake:
						quantity[pcons::City] += curItems[i]->Quantity;
						break;

					default:
						for (unsigned int i = 0; i < pconsCount; ++i) {
							if (curItems[i]->ModelId > 0 && curItems[i]->ModelId == pconsItemID[i]) {
								quantity[i] += curItems[i]->Quantity;
							}
						}
						break;
				}
			}
		}
	}
	quantity[pcons::Cons] = min(min(quantityArmor, quantityEssence), quantityGrail);

	for (unsigned int i = 0; i < pconsCount; ++i) {
		pconsActive[i] = pconsActive[i] && (quantity[i] > 0);
		
		// TODO: update GUI
	}
}