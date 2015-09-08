#include "MerchantMgr.h"

#include "MemoryMgr.h"
#include "GameThreadMgr.h"
#include "ItemMgr.h"

BYTE* GWAPI::MerchantMgr::TraderSellClass = NULL;
BYTE* GWAPI::MerchantMgr::TraderBuyClass = NULL;
BYTE* GWAPI::MerchantMgr::TraderSellClassHookRestore = NULL;
BYTE* GWAPI::MerchantMgr::TraderSellClassHookRet = NULL;
BYTE* GWAPI::MerchantMgr::TraderBuyClassHookRestore = NULL;
BYTE* GWAPI::MerchantMgr::TraderBuyClassHookRet = NULL;

void GWAPI::MerchantMgr::CommandTraderSell()
{
	if (!((long*)TraderSellClass)[1] || !((long*)TraderSellClass)[2]){ return; }

	static long* info = new long[2];
	info[0] = ((long*)TraderSellClass)[1];
	info[1] = ((long*)TraderSellClass)[2];

	long price = ((long*)TraderSellClass)[2];

	_asm {
		PUSH 0
		PUSH 0
		PUSH 0
		PUSH price
		PUSH 0
		PUSH info
		PUSH 1
		MOV ECX, 0xD
		MOV EDX, 0
		CALL MemoryMgr::TraderFunction
	}
}

void GWAPI::MerchantMgr::CommandTraderBuy()
{
	if (!((long*)TraderBuyClass)[1] || !((long*)TraderBuyClass)[2]){ return; }

	static long* info = new long[2];
	info[0] = ((long*)TraderBuyClass)[1];
	info[1] = ((long*)TraderBuyClass)[2];

	long price = ((long*)TraderBuyClass)[2];

	_asm {
		PUSH 0
		PUSH info
		PUSH 1
		PUSH 0
		PUSH 0
		PUSH 0
		PUSH 0
		MOV ECX, 0xC
		MOV EDX, price
		CALL MemoryMgr::TraderFunction
	}
}

void GWAPI::MerchantMgr::CommandCollectItem(long* ItemtoGivePtr, long AmountPerCollect, long* ItemtoRecvPtr)
{
	__asm{
		PUSH 0
		PUSH ItemtoGivePtr
		PUSH 1
		PUSH 0
		PUSH AmountPerCollect
		PUSH ItemtoRecvPtr
		PUSH 1
		MOV ECX, 2
		MOV EDX, 0
		CALL MemoryMgr::BuyItemFunction
	}
}

void GWAPI::MerchantMgr::CommandCraftItem(long* ItemRowPtr, long* QuantityPtr, long* MaterialPtr, long MaterialCount, long GoldTotal)
{
	__asm{
		PUSH QuantityPtr
			PUSH ItemRowPtr
			PUSH 1
			PUSH 0
			PUSH 0
			PUSH MaterialPtr
			PUSH MaterialCount
			MOV EDX, ESP
			MOV ECX, DWORD PTR DS : [MemoryMgr::CraftitemObj]
			MOV DWORD PTR DS : [EDX - 0x70], ECX
			MOV ECX, DWORD PTR DS : [EDX + 0x1C]
			MOV DWORD PTR DS : [EDX + 0x54], ECX
			MOV ECX, DWORD PTR DS : [EDX + 0x4]
			MOV DWORD PTR DS : [EDX - 0x14], ECX
			MOV ECX, 3
			MOV EBX, MaterialCount
			MOV EDX, GoldTotal
			CALL MemoryMgr::BuyItemFunction
	}
}

void GWAPI::MerchantMgr::CommandRequestTraderSellQuote(long* itemptr)
{
	_asm {
		PUSH 0
			PUSH 0
			PUSH 0
			PUSH itemptr
			PUSH 1
			PUSH 0
			MOV ECX, 0xD
			MOV EDX, 0
			CALL MemoryMgr::RequestQuoteFunction
	}
	((long*)TraderSellClass)[1] = *itemptr;
	((long*)TraderSellClass)[9] = 2;
}

void GWAPI::MerchantMgr::CommandRequestTraderBuyQuote(long* itemptr)
{
	_asm {
		PUSH itemptr
			PUSH 1
			PUSH 0
			PUSH 0
			PUSH 0
			PUSH 0
			MOV ECX, 0xC
			MOV EDX, 0
			CALL MemoryMgr::RequestQuoteFunction
	}
	((long*)TraderBuyClass)[1] = *itemptr;
	((long*)TraderBuyClass)[5] = 3;
}

void GWAPI::MerchantMgr::CommandSellMerchantItem(long* itemIdPtr, long itemValue)
{
	_asm {
		PUSH 0
		PUSH 0
		PUSH 0
		PUSH itemValue
		PUSH 0
		PUSH itemIdPtr
		PUSH 1
		MOV ECX, 0xB
		MOV EDX, 0
		CALL MemoryMgr::SellItemFunction
	}
}

void GWAPI::MerchantMgr::CommandBuyMerchantItem(long* idptr, long* quantityptr, long value)
{
	_asm {
		    PUSH quantityptr
			PUSH idptr
			PUSH 1
			PUSH 0
			PUSH 0
			PUSH 0
			PUSH 0
			MOV ECX, 1
			MOV EDX, value
			CALL MemoryMgr::BuyItemFunction
	}
}

void __declspec(naked) GWAPI::MerchantMgr::TraderSellClassHook()
{
	_asm{
		MOV TraderSellClass, ECX
		JMP TraderSellClassHookRet
	}
}

void __declspec(naked) GWAPI::MerchantMgr::TraderBuyClassHook()
{
	_asm{
		MOV TraderBuyClass, ECX
		JMP TraderBuyClassHookRet
	}
}

GWAPI::MerchantMgr::~MerchantMgr()
{
	MemoryMgr::Retour(MemoryMgr::TraderBuyClassHook, TraderBuyClassHookRestore, 6);
	MemoryMgr::Retour(MemoryMgr::TraderSellClassHook, TraderSellClassHookRestore, 6);
}

GWAPI::MerchantMgr::MerchantMgr(GWAPIMgr* obj) : parent(obj)
{
	TraderBuyClassHookRet = (BYTE*)MemoryMgr::Detour(MemoryMgr::TraderBuyClassHook, (BYTE*)TraderBuyClassHook, 6, &TraderBuyClassHookRestore);
	TraderSellClassHookRet = (BYTE*)MemoryMgr::Detour(MemoryMgr::TraderSellClassHook, (BYTE*)TraderSellClassHook, 6, &TraderSellClassHookRestore);
}

long* GWAPI::MerchantMgr::GetCraftItemArray(long aQuantity, long aMatCount, CraftMaterial* mats)
{
	long retArrSize = NULL;
	long* TotalMats = new long[aMatCount];
	long* retArr = 0;

	for (int i = 0; i < aMatCount; i++){
		TotalMats[i] = parent->Items->CountItemByModelId(mats[i].ModelID);
		if (TotalMats[i] < mats[i].Quantity * aQuantity) return NULL;
		TotalMats[i] = (long)((TotalMats[i] / 250) + 1);
		retArrSize += TotalMats[i];
	}

	retArr = new long[retArrSize];
	retArrSize = NULL;

	GW::Bag** curBags = parent->Items->GetBagArray();
	if (curBags == NULL) return NULL;

	for (int i = 0; i < aMatCount; i++){
		for (int curBag = 1; curBag <= 4; curBag++){
			GW::Bag* curBagPtr = curBags[curBag];
			if (!curBagPtr) continue;

			GW::ItemArray curItemArr = curBagPtr->Items;
			if (!curItemArr.IsValid()) continue;

			for (DWORD j = 0; j < curItemArr.size(); j++){
				if (!curItemArr[j]) continue;

				if (curItemArr[j]->ModelId == mats[i].ModelID)
				{
					retArr[retArrSize] = curItemArr[j]->ItemId;
					retArrSize++;
					TotalMats[i]--;
				}
				if (TotalMats[i] == NULL) break;
			}
			if (TotalMats[i] == NULL) break;
		}

	}
	delete[] TotalMats;
	return retArr;
}

void GWAPI::MerchantMgr::SellQuotedItem()
{
	parent->GameThread->Enqueue(CommandTraderSell);
}

void GWAPI::MerchantMgr::BuyQuotedItem()
{
	parent->GameThread->Enqueue(CommandTraderBuy);
}

void GWAPI::MerchantMgr::RequestSellQuote(DWORD itemtorequest)
{
	if (!itemtorequest) return;
	parent->GameThread->Enqueue(CommandRequestTraderSellQuote, (long*)itemtorequest);
}

void GWAPI::MerchantMgr::RequestBuyQuote(DWORD ModelIDToRequest)
{
	DWORD itemtorequest = GetMerchantItemByModelId(ModelIDToRequest);
	if (!itemtorequest) return;

	parent->GameThread->Enqueue(CommandRequestTraderBuyQuote, (long*)itemtorequest);
}

void GWAPI::MerchantMgr::SellItemToMerch(DWORD ItemToSell, DWORD AmountToSell /*= 1*/)
{
	GW::ItemArray items = parent->Items->GetItemArray();
	long amount = AmountToSell * items[ItemToSell]->value;

	parent->GameThread->Enqueue(CommandSellMerchantItem, (long*)ItemToSell, amount);
}

void GWAPI::MerchantMgr::BuyMerchItem(DWORD ModelId, DWORD AmountToBuy)
{
	GW::ItemArray items = parent->Items->GetItemArray();
	static long* amountptr = new long;
	*amountptr = AmountToBuy;
	static long* itemidptr = new long;

	*itemidptr = GetMerchantItemByModelId(ModelId);
	if (!(*itemidptr)) return;

	parent->GameThread->Enqueue(CommandBuyMerchantItem, itemidptr, amountptr, items[*itemidptr]->value);
}

void GWAPI::MerchantMgr::CollectItem(int modelIDToGive, int AmountPerCollect, int modelIDtoRecieve)
{
	GW::Item* itemGiving = parent->Items->GetItemByModelId(modelIDToGive);
	if (!itemGiving || itemGiving->Quantity < AmountPerCollect) return;

	static long* itemidptr = new long;

	*itemidptr = GetMerchantItemByModelId(modelIDtoRecieve);
	if (!(*itemidptr)) return;

	parent->GameThread->Enqueue(CommandCollectItem, (long*)itemGiving, AmountPerCollect, itemidptr);
}

void GWAPI::MerchantMgr::CraftGrail(int amount)
{
	CraftMaterial* matsGrail = new CraftMaterial[2];
	matsGrail[0] = CraftMaterial(948, 50);
	matsGrail[1] = CraftMaterial(929, 50);
	CraftItem(24861, amount, 250, 2, matsGrail);
}

void GWAPI::MerchantMgr::CraftArmor(int amount)
{
	CraftMaterial* matsArmor = new CraftMaterial[2];
	matsArmor[0] = CraftMaterial(921, 50);
	matsArmor[1] = CraftMaterial(948, 50);
	CraftItem(24860, amount, 250, 2, matsArmor);
}

void GWAPI::MerchantMgr::CraftEssence(int amount)
{
	CraftMaterial* matsEssence = new CraftMaterial[2]; // Amount of materials taken to craft item.
	matsEssence[0] = CraftMaterial(933, 50); // Material ModelID, then Amount needed to craft one of the item.
	matsEssence[1] = CraftMaterial(929, 50); // Same. Make sure modelid's are in the order seen on mat screen.
	CraftItem(24859, amount, 250, 2, matsEssence); // Modelid, amount to craft, gold needed per item, amount of mats, craftmaterial array.
}

void GWAPI::MerchantMgr::CraftItem(long ModelId, long Quantity, long value, long matcount, CraftMaterial* Materials)
{
	static long* MaterialPtr = NULL;
	static long* QuantityPtr = new long;
	long GoldTotal = Quantity * value;

	if (MaterialPtr != NULL) delete[] MaterialPtr;

	*QuantityPtr = Quantity;

	static long* itemtobuyptr = new long;
	*itemtobuyptr = GetMerchantItemByModelId(ModelId);
	if (!(*itemtobuyptr)) return;

	MaterialPtr = GetCraftItemArray(Quantity, matcount, Materials);

	delete[] Materials;

	parent->GameThread->Enqueue(CommandCraftItem, itemtobuyptr, QuantityPtr, MaterialPtr, matcount, GoldTotal);
}

DWORD GWAPI::MerchantMgr::GetMerchantItemByModelId(DWORD modelid)
{
	try{
		GW::ItemArray itemstructs = parent->Items->GetItemArray();
		if (!itemstructs.IsValid()) throw API_EXCEPTION;

		ItemRowArray merchitems = GetMerchantItemsArray();
		if (!merchitems.IsValid()) throw API_EXCEPTION;

		for (DWORD i = 0; i < merchitems.size(); i++){
			if (itemstructs[merchitems[i]]->ModelId == modelid){
				return merchitems[i];
			}
		}
	}
	catch (APIException_t){
		return NULL;
	}
	return NULL;
}

GWAPI::MerchantMgr::ItemRowArray GWAPI::MerchantMgr::GetMerchantItemsArray()
{
	ItemRowArray ret = *MemoryMgr::ReadPtrChain<ItemRowArray*>(MemoryMgr::GetContextPtr(), 2, 0x2C, 0x24);

	if (!ret.IsValid()) throw API_EXCEPTION;

	return ret;
}

