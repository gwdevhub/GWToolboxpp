#pragma once

#include <Windows.h>
#include "ItemMgr.h"
#include "GWAPIMgr.h"


namespace GWAPI {

	class MerchantMgr {
	public:
		typedef MemoryMgr::gw_array<DWORD> ItemRowArray;
		struct CraftMaterial{
			CraftMaterial(){}
			CraftMaterial(long aMid, long aQuantity) :
				ModelID(aMid), Quantity(aQuantity){}

			long ModelID;
			long Quantity;
		};
	private:
		friend class GWAPIMgr;
		GWAPIMgr* const parent_;

		static BYTE* trader_buy_class_hook_return_;
		Hook hk_trader_buy_class_;

		static BYTE* trader_sell_class_hook_return_;
		Hook hk_trader_sell_class_;

		void RestoreHooks();

		MerchantMgr(GWAPIMgr* obj);
		~MerchantMgr();

		static BYTE* trader_buy_class_;
		static BYTE* trader_sell_class_;

		static void TraderBuyClassHook();
		static void TraderSellClassHook();

		// Static asm calls to be used within gameloop.
		static void CommandBuyMerchantItem(long* idptr, long* quantityptr, long value);
		static void CommandSellMerchantItem(long* itemIdPtr, long itemValue);
		static void CommandRequestTraderBuyQuote(long* itemptr);
		static void CommandRequestTraderSellQuote(long* itemptr);
		static void CommandCraftItem(long* ItemRowPtr, long* QuantityPtr, long* MaterialPtr, long MaterialCount, long GoldTotal);
		static void CommandCollectItem(long* ItemtoGivePtr, long AmountPerCollect, long* ItemtoRecvPtr);
		static void CommandTraderBuy();
		static void CommandTraderSell();

		// Internal use for CraftItem.
		long* GetCraftItemArray(long aQuantity, long aMatCount, CraftMaterial* mats);

	public:

		// Returns the merchant array in memory, holds ItemID's that can be used to grab structs from.
		ItemRowArray GetMerchantItemsArray();

		GW::Item* GetMerchantItemByModelId(DWORD modelid);

		// Craft Item, use this to create items that require materials (Cons,Armor,etc.)
		void CraftItem(long ModelId, long Quantity, long value, long matcount, CraftMaterial* Materials);
		
		// Prebuilt functions using craftitem for consets, Essence is commented to explain how to make your own calls in cpp.
		void CraftEssence(int amount);
		void CraftArmor(int amount);
		void CraftGrail(int amount);

		// Used for collectors (people who want 3 of that for this)
		// Modelid of item the guy wants, amount needed for one collect (big ass number in window), item modelid you want.
		void CollectItem(int modelIDToGive, int AmountPerCollect, int modelIDtoRecieve);

		// Buy from general merchant.
		void BuyMerchItem(DWORD ModelIdToBuy, DWORD AmountToBuy);

		// Sell to general merchant.
		void SellItemToMerch(GW::Item* ItemToSell, DWORD AmountToSell = 1);

		// Request quote of item from a trader.
		void RequestBuyQuote(DWORD ModelIDToRequest);

		// Request quote to sell one of your items to a trader.
		void RequestSellQuote(GW::Item* itemtorequest);

		// These are pretty self explanatory.
		void BuyQuotedItem();
		void SellQuotedItem();
	};
}