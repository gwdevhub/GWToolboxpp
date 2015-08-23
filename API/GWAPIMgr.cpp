#include "GWAPIMgr.h"

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
	}
	else{
		MessageBoxA(0, "Initialize Failed at finding all addresses, contact Developers about this.", "GWToolbox++ API Error", 0);
	}
}

void GWAPI::GWAPIMgr::ToggleRendering()
{
	GameThread->ToggleRenderHook();
}

GWAPI::GWAPIMgr::~GWAPIMgr()
{
#ifdef GWAPI_USEDIRECTX
	if (DirectX) delete DirectX;
#endif
	if (Map) delete Map;

	if (Effects) delete Effects;
	if (Skillbar) delete Skillbar;

	if (Items) delete Items;
	if (Agents) delete Agents;
	
	if (CtoS) delete CtoS;
	if (GameThread) delete GameThread;
	if (Merchant) delete Merchant;
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

