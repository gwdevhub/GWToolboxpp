#include "ItemMgr.h"

#include "MemoryMgr.h"
#include "GameThreadMgr.h"
#include "CtoSMgr.h"

void GWAPI::ItemMgr::OpenXunlaiWindow()
{
	static DWORD* xunlaibuf = new DWORD[2]{0, 1};
	parent->GameThread->Enqueue(_OpenXunlai,
		*MemoryMgr::ReadPtrChain<DWORD*>((DWORD)MemoryMgr::XunlaiSession, 4, 0x118, 0x10, 0, 0x14), xunlaibuf);
}

void GWAPI::ItemMgr::PickUpItem(Item* item, DWORD CallTarget /*= 0*/)
{
	parent->CtoS->SendPacket(0xC, 0x39, item->AgentId, CallTarget);
}

void GWAPI::ItemMgr::DropItem(Item* item, DWORD quantity)
{
	parent->CtoS->SendPacket(0xC, 0x26, item->ItemId, quantity);
}

void GWAPI::ItemMgr::EquipItem(Item* item)
{
	parent->CtoS->SendPacket(0x8, 0x2A, item->ItemId);
}

void GWAPI::ItemMgr::UseItem(Item* item)
{
	parent->CtoS->SendPacket(0x8, 0x78, item->ItemId);
}

GWAPI::Bag** GWAPI::ItemMgr::GetBagArray()
{
	return *MemoryMgr::ReadPtrChain<Bag***>(MemoryMgr::GetContextPtr(), 2, 0x40, 0xF8);
}

GWAPI::ItemArray GWAPI::ItemMgr::GetItemArray()
{
	return *MemoryMgr::ReadPtrChain<ItemArray*>(MemoryMgr::GetContextPtr(), 2, 0x40, 0xB8);
}

GWAPI::ItemMgr::ItemMgr(GWAPIMgr* obj) : parent(obj)
{
	_OpenXunlai = (OpenXunlai_t)MemoryMgr::OpenXunlaiFunction;
}

bool GWAPI::ItemMgr::UseItemByModelId(DWORD modelid, BYTE bagStart /*= 1*/, const BYTE bagEnd /*= 4*/)
{
	Bag** bags = GetBagArray();
	if (bags == NULL) return false;

	Bag* bag = NULL;

	for (int bagIndex = bagStart; bagIndex <= bagEnd; ++bagIndex) {
		bag = bags[bagIndex];
		if (bag != NULL) {
			ItemArray items = bag->Items;
			if (!items.IsValid()) return false;
			for (size_t i = 0; i < items.size(); i++) {
				if (items[i]) {
					if (items[i]->ModelId == modelid) {
						UseItem(items[i]);
						return true;
					}
				}
			}
		}
	}

	return false;
}

void GWAPI::ItemMgr::DropGold(DWORD Amount /*= 1*/)
{
	parent->CtoS->SendPacket(0x8, 0x29, Amount);
}

DWORD GWAPI::ItemMgr::CountItemByModelId(DWORD modelid, BYTE bagStart /*= 1*/, const BYTE bagEnd /*= 4*/)
{
	DWORD itemcount = 0;
	Bag** bags = GetBagArray();
	Bag* bag = NULL;

	for (int bagIndex = bagStart; bagIndex <= bagEnd; ++bagIndex) {
		bag = bags[bagIndex];
		if (bag != NULL) {
			ItemArray items = bag->Items;
			for (size_t i = 0; i < items.size(); i++) {
				if (items[i]) {
					if (items[i]->ModelId == modelid) {
						itemcount += items[i]->Quantity;
					}
				}
			}
		}
	}

	return itemcount;
}

GWAPI::Item* GWAPI::ItemMgr::GetItemByModelId(DWORD modelid, BYTE bagStart /*= 1*/, const BYTE bagEnd /*= 4*/)
{
	Bag** bags = GetBagArray();
	Bag* bag = NULL;

	for (int bagIndex = bagStart; bagIndex <= bagEnd; ++bagIndex) {
		bag = bags[bagIndex];
		if (bag != NULL) {
			ItemArray items = bag->Items;
			for (size_t i = 0; i < items.size(); i++) {
				if (items[i]) {
					if (items[i]->ModelId == modelid) {
						return items[i];
					}
				}
			}
		}
	}

	return NULL;
}

DWORD GWAPI::ItemMgr::GetGoldAmountOnCharacter()
{
	return *MemoryMgr::ReadPtrChain<DWORD*>(MemoryMgr::GetContextPtr(), 3, 0x40, 0xF8, 0x7C);
}

DWORD GWAPI::ItemMgr::GetGoldAmountInStorage()
{
	return *MemoryMgr::ReadPtrChain<DWORD*>(MemoryMgr::GetContextPtr(), 3, 0x40, 0xF8, 0x80);
}
