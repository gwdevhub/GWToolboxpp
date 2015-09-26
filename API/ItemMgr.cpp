#include "ItemMgr.h"

#include "MemoryMgr.h"
#include "GameThreadMgr.h"
#include "CtoSMgr.h"

void GWAPI::ItemMgr::OpenXunlaiWindow()
{
	static DWORD* xunlaibuf = new DWORD[2]{0, 1};
	parent_->Gamethread()->Enqueue(open_xunlai_function_,
		*MemoryMgr::ReadPtrChain<DWORD*>((DWORD)MemoryMgr::XunlaiSession, 4, 0x118, 0x10, 0, 0x14), xunlaibuf);
}

void GWAPI::ItemMgr::PickUpItem(GW::Item* item, DWORD CallTarget /*= 0*/)
{
	parent_->CtoS()->SendPacket(0xC, 0x39, item->AgentId, CallTarget);
}

void GWAPI::ItemMgr::DropItem(GW::Item* item, DWORD quantity)
{
	parent_->CtoS()->SendPacket(0xC, 0x26, item->ItemId, quantity);
}

void GWAPI::ItemMgr::EquipItem(GW::Item* item)
{
	parent_->CtoS()->SendPacket(0x8, 0x2A, item->ItemId);
}

void GWAPI::ItemMgr::UseItem(GW::Item* item)
{
	parent_->CtoS()->SendPacket(0x8, 0x78, item->ItemId);
}

GWAPI::GW::Bag** GWAPI::ItemMgr::GetBagArray()
{
	return *MemoryMgr::ReadPtrChain<GW::Bag***>(MemoryMgr::GetContextPtr(), 2, 0x40, 0xF8);
}

GWAPI::GW::ItemArray GWAPI::ItemMgr::GetItemArray()
{
	return *MemoryMgr::ReadPtrChain<GW::ItemArray*>(MemoryMgr::GetContextPtr(), 2, 0x40, 0xB8);
}

GWAPI::ItemMgr::ItemMgr(GWAPIMgr* obj) : parent_(obj)
{
	open_xunlai_function_ = (OpenXunlai_t)MemoryMgr::OpenXunlaiFunction;
}

bool GWAPI::ItemMgr::UseItemByModelId(DWORD modelid, BYTE bagStart /*= 1*/, const BYTE bagEnd /*= 4*/)
{

	if (parent_->Map()->GetInstanceType() == GwConstants::InstanceType::Loading) return false;

	GW::Bag** bags = GetBagArray();
	if (bags == NULL) return false;

	GW::Bag* bag = NULL;
	GW::Item* item = NULL;

	for (int bagIndex = bagStart; bagIndex <= bagEnd; ++bagIndex) {
		bag = bags[bagIndex];
		if (bag != NULL) {
			GW::ItemArray items = bag->Items;
			if (!items.valid()) return false;
			for (size_t i = 0; i < items.size(); i++) {
				item = items[i];
				if (item != NULL) {
					if (item->ModelId == modelid) {
						UseItem(item);
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
	parent_->CtoS()->SendPacket(0x8, 0x29, Amount);
}

DWORD GWAPI::ItemMgr::CountItemByModelId(DWORD modelid, BYTE bagStart /*= 1*/, const BYTE bagEnd /*= 4*/)
{
	DWORD itemcount = 0;
	GW::Bag** bags = GetBagArray();
	GW::Bag* bag = NULL;

	for (int bagIndex = bagStart; bagIndex <= bagEnd; ++bagIndex) {
		bag = bags[bagIndex];
		if (bag != NULL) {
			GW::ItemArray items = bag->Items;
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

GWAPI::GW::Item* GWAPI::ItemMgr::GetItemByModelId(DWORD modelid, BYTE bagStart /*= 1*/, const BYTE bagEnd /*= 4*/)
{
	GW::Bag** bags = GetBagArray();
	GW::Bag* bag = NULL;

	for (int bagIndex = bagStart; bagIndex <= bagEnd; ++bagIndex) {
		bag = bags[bagIndex];
		if (bag != NULL) {
			GW::ItemArray items = bag->Items;
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

void GWAPI::ItemMgr::OpenLockedChest()
{
	return parent_->CtoS()->SendPacket(0x8, 0x4D, 0x2);
}
