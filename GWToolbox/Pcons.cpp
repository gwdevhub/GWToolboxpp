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
}


void Pcons::pconsLoadIni() {
	// TODO
}

void Pcons::pconsBuildUI() {
	// TODO
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
					
					// TODO alcohol

					case ItemID::CremeBrulee:
					case ItemID::SugaryBlueDrink:
					case ItemID::ChocolateBunny:
					case ItemID::RedBeanCake:
						quantity[pcons::City] += curItems[i]->Quantity;
						break;

					default:
						for (int i = 0; i < pconsCount; ++i) {
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

	for (int i = 0; i < pconsCount; ++i) {
		pconsActive[i] = pconsActive[i] && (quantity[i] > 0);
		
		// TODO: update GUI
	}
}