#pragma once

#include <Windows.h>
#include "GWAPIMgr.h"

namespace GWAPI {

	class ItemMgr {
		friend class GWAPIMgr;
		typedef void(__fastcall *OpenXunlai_t)(DWORD, DWORD*);
		OpenXunlai_t _OpenXunlai;
		GWAPIMgr* parent;
		ItemMgr(GWAPIMgr* obj);

	public:
	

		ItemArray GetItemArray();

		Bag** GetBagArray();

		void UseItem(Item* item);

		bool UseItemByModelId(DWORD modelid, BYTE bagStart = 1,const BYTE bagEnd = 4);

		DWORD CountItemByModelId(DWORD modelid, BYTE bagStart = 1, const BYTE bagEnd = 4);

		Item* GetItemByModelId(DWORD modelid, BYTE bagStart = 1, const BYTE bagEnd = 4);

		void EquipItem(Item* item);

		void DropItem(Item* item, DWORD quantity);
		
		void PickUpItem(Item* item, DWORD CallTarget = 0);

		void OpenXunlaiWindow();
		
		void DropGold(DWORD Amount = 1);

		DWORD GetGoldAmountOnCharacter();

		DWORD GetGoldAmountInStorage();
	};
}