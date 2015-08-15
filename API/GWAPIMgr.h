#pragma  once
#include <Windows.h>
#include "APIMain.h"

namespace GWAPI {

	class GWAPIMgr {

		static GWAPIMgr* instance;

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
		CtoSMgr* CtoS;
		StoCMgr* StoC;
		
		GWAPIMgr();
		~GWAPIMgr();
	public:

		
		AgentMgr* Agents;
		ItemMgr* Items;
		SkillbarMgr* Skillbar;
		EffectMgr* Effects;
		MapMgr* Map;
#ifdef GWAPI_USEDIRECTX
		DirectXMgr* DirectX;
#endif
		void ToggleRendering();

		static void Initialize();
		static GWAPIMgr* GetInstance();
		static void Destruct();
	};

}