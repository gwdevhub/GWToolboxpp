#pragma  once
#include <Windows.h>
#include "APIMain.h"

namespace GWAPI {

	class GWAPIMgr {

		static GWAPIMgr* instance;

		friend class GameThreadMgr;
		friend class CtoSMgr;
		friend class AgentMgr;
		friend class ItemMgr;
#ifdef GWAPI_USEDIRECTX
		friend class DirectXMgr;
#endif
		friend class SkillbarMgr;
		friend class EffectMgr;
		friend class MapMgr;
		friend class ChatMgr;

		GameThreadMgr* GameThread;
		CtoSMgr* CtoS;
		
		GWAPIMgr();
		~GWAPIMgr();
	public:

		
		AgentMgr* Agents;
		ItemMgr* Items;
		SkillbarMgr* Skillbar;
		EffectMgr* Effects;
		MapMgr* Map;
		ChatMgr* Chat;
#ifdef GWAPI_USEDIRECTX
		DirectXMgr* DirectX;
#endif
		void ToggleRendering();

		static void Initialize();
		static GWAPIMgr* GetInstance();
		static void Destruct();
	};

}