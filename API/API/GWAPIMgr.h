#pragma  once
#include <Windows.h>
#include "APIMain.h"
#include <exception>

namespace GWAPI {

	typedef BYTE APIException_t;
	const APIException_t API_EXCEPTION = 1;

	class GWAPIMgr {

		static bool init_sucessful_;
		static GWAPIMgr* instance_;

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
		friend class GuildMgr;

		GameThreadMgr* gamethread_;
		CtoSMgr* ctos_;
		AgentMgr* agents_;
		ItemMgr* items_;
		SkillbarMgr* skillbar_;
		EffectMgr* effects_;
		MapMgr* map_;
		ChatMgr* chat_;
		MerchantMgr* merchant_;
		GuildMgr* guild_;
#ifdef GWAPI_USEDIRECTX
		DirectXMgr* directx_;

#endif
		
		GWAPIMgr();
		~GWAPIMgr();
	public:

		inline GameThreadMgr* Gamethread() const { return gamethread_; }
		inline CtoSMgr* CtoS() const { return ctos_; }
		inline AgentMgr* Agents() const { return agents_; }
		inline ItemMgr* Items() const { return items_; }
		inline SkillbarMgr* Skillbar() const { return skillbar_; }
		inline EffectMgr* Effects() const { return effects_; }
		inline ChatMgr* Chat() const { return chat_; }
		inline MerchantMgr* Merchant() const { return merchant_; }
		inline GuildMgr* Guild() const { return guild_; }
		inline MapMgr* Map() const { return map_; }
#ifdef GWAPI_USEDIRECTX
		inline DirectXMgr* DirectX() const { return directx_; }
#endif

		static bool Initialize();
		static GWAPIMgr* instance();
		static void Destruct();
	};

}