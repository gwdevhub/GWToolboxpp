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
	

		GW::ItemArray GetItemArray();

		GW::Bag** GetBagArray();

		void UseItem(GW::Item* item);

		bool UseItemByModelId(DWORD modelid, BYTE bagStart = 1,const BYTE bagEnd = 4);

		DWORD CountItemByModelId(DWORD modelid, BYTE bagStart = 1, const BYTE bagEnd = 4);

		GW::Item* GetItemByModelId(DWORD modelid, BYTE bagStart = 1, const BYTE bagEnd = 4);

		void EquipItem(GW::Item* item);

		void DropItem(GW::Item* item, DWORD quantity);
		
		void PickUpItem(GW::Item* item, DWORD CallTarget = 0);

		void OpenXunlaiWindow();
		
		void DropGold(DWORD Amount = 1);

		DWORD GetGoldAmountOnCharacter();

		DWORD GetGoldAmountInStorage();
	};
}