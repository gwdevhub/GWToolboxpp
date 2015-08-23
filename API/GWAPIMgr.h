#pragma  once
#include <Windows.h>
#include "APIMain.h"
#include <exception>

namespace GWAPI {

	struct MemoryMgr;
	class GameThreadMgr;

	class CtoSMgr;

	class AgentMgr;
	class ItemMgr;
	class SkillbarMgr;
	class EffectMgr;
	class MapMgr;

	class MerchantMgr;

#ifdef GWAPI_USEDIRECTX
	class DirectXMgr;
#endif


	typedef BYTE APIException_t;
	const APIException_t API_EXCEPTION = 1;

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
		friend class MerchantMgr;

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
		MerchantMgr* Merchant;
#ifdef GWAPI_USEDIRECTX
		DirectXMgr* DirectX;
#endif
		void ToggleRendering();

		static void Initialize();
		static GWAPIMgr* GetInstance();
		static void Destruct();
	};

}