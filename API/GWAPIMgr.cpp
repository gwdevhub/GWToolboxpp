#include "GWAPIMgr.h"

#include "MemoryMgr.h"
#include "GameThreadMgr.h"

#include "CtoSMgr.h"
#include "AgentMgr.h"
#include "ItemMgr.h"

#include "MerchantMgr.h"

#ifdef GWAPI_USEDIRECTX
#include "DirectXMgr.h"
#endif

#include "SkillbarMgr.h"
#include "EffectMgr.h"

#include "MapMgr.h"

#include "ChatMgr.h"

GWAPI::GWAPIMgr* GWAPI::GWAPIMgr::instance = NULL;

GWAPI::GWAPIMgr* GWAPI::GWAPIMgr::GetInstance()
{
	return instance ? instance : NULL;
}

GWAPI::GWAPIMgr::GWAPIMgr()
{
	if (MemoryMgr::Scan()){
		GameThread = new GameThreadMgr(this);
		CtoS = new CtoSMgr(this);
		Agents = new AgentMgr(this);
		Items = new ItemMgr(this);
#ifdef GWAPI_USEDIRECTX
		DirectX = new DirectXMgr(this);
#endif
		Skillbar = new SkillbarMgr(this);
		Effects = new EffectMgr(this);
		Map = new MapMgr(this);
		Chat = new ChatMgr(this);
		Merchant = new MerchantMgr(this);
		Guild = new GuildMgr(this);
	}
	else{
		MessageBoxA(0, "Initialize Failed at finding all addresses, contact Developers about this.", "GWToolbox++ API Error", 0);
	}
}

GWAPI::GWAPIMgr::~GWAPIMgr()
{

	if (Map) delete Map;
	if (Guild) delete Guild;

	
	if (Skillbar) delete Skillbar;

	if (Items) delete Items;
	if (Agents) delete Agents;
	
	if (CtoS) delete CtoS;

	if (Effects) delete Effects;
	if (Merchant) delete Merchant;
	if (GameThread) delete GameThread;
#ifdef GWAPI_USEDIRECTX
	if (DirectX) delete DirectX;
#endif

}

void GWAPI::GWAPIMgr::Initialize()
{
	if (!instance) instance = new GWAPIMgr();
}

void GWAPI::GWAPIMgr::Destruct()
{
	if (instance){
		delete instance;
	}
}

