#pragma once

#include <Windows.h>
#include "APIMain.h"

namespace GWAPI {

	class ItemMgr {

		typedef void(__fastcall *OpenXunlai_t)(DWORD, DWORD*);
		OpenXunlai_t _OpenXunlai;
		GWAPIMgr* parent;

	public:
		ItemMgr(GWAPIMgr* obj);

		struct Bag;
		struct Item;

		typedef MemoryMgr::gw_array<Item*> ItemArray;


		struct Bag{							// total : 24 BYTEs
			BYTE unknown1[4];					// 0000	|--4 BYTEs--|
			long index;							// 0004
			DWORD BagId;						// 0008
			DWORD ContainerItem;				// 000C
			DWORD ItemsCount;					// 0010
			Bag* BagArray;						// 0014
			ItemArray Items;						// 0020
		};

		struct Item{							// total : 50 BYTEs
			DWORD ItemId;						// 0000
			DWORD AgentId;						// 0004
			BYTE unknown1[4];					// 0008	|--4 BYTEs--|
			Bag* bag;							// 000C
			DWORD* ModStruct;						// 0010						pointer to an array of mods
			DWORD ModStructSize;				// 0014						size of this array
			wchar_t* Customized;				// 0018
			BYTE unknown2[4];					// 001C	|--4 BYTEs--|
			BYTE Type;							// 0020
			BYTE unknown3;						// 0021	|--1 BYTE---|
			short ExtraId;						// 0022
			short Value;						// 0024
			DWORD Interaction;   				//							ways the player can interact with the item
			DWORD ModelId;						// 002C
			DWORD ModString;					// 0030
			BYTE unknown4[4];					// 0034	|--4 BYTEs--|
			DWORD NameString;					// 0038
			BYTE unknown5[16];					// 003C	|-16 BYTEs--|
			BYTE Quantity;						// 004C
			BYTE Equipped;						// 004D
			BYTE unknown6;						// 004E	|--1 BYTE---|
			BYTE Slot;							// 004F
		};

		ItemArray GetItemArray();

		Bag** GetBagArray();

		void UseItem(Item* item);

		bool UseItemByModelId(DWORD modelid, BYTE bagStart = 1,const BYTE bagEnd = 4);

		void EquipItem(Item* item);

		void DropItem(Item* item, DWORD quantity);

		void PickUpItem(Item* item, DWORD CallTarget = 0);

		void OpenXunlaiWindow();
		
		void DropGold(DWORD Amount = 1);
	};
}