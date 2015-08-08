#pragma  once
#include <Windows.h>
#include "APIMain.h"

namespace GWAPI {

	class GWAPIMgr {

		friend class GameThreadMgr;
		friend class CtoSMgr;
		friend class StoCMgr;
		friend class AgentMgr;
		friend class ItemMgr;
#ifdef GWAPI_USEDIRECTX
		friend class DirectXMgr;
#endif
		friend class SkillbarMgr;
		friend class EffectMgr;
		friend class MapMgr;

		GameThreadMgr* GameThread;
		
		GWAPIMgr();
	public:

		CtoSMgr* CtoS;
		StoCMgr* StoC;
		AgentMgr* Agents;
		ItemMgr* Items;
		SkillbarMgr* Skillbar;
		EffectMgr* Effects;
		MapMgr* Map;
#ifdef GWAPI_USEDIRECTX
		DirectXMgr* DirectX;
#endif
		void ToggleRendering();
		static GWAPIMgr* GetInstance();
	};

}