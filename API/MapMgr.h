#pragma once

#include <Windows.h>
#include "APIMain.h"


namespace GWAPI{
	class MapMgr{
		GWAPIMgr* parent;
	public:

		MapMgr(GWAPIMgr* obj);

		DWORD GetMapID();

		DWORD GetInstanceTime();

		// Get the instance type (Outpost, Explorable or Loading)
		GwConstants::InstanceType GetInstanceType();

		void Travel(DWORD MapID, DWORD District = 0, DWORD Region = 0, DWORD Language = 0);


	};
}