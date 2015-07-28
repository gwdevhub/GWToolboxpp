#include "GWAPIMgr.h"


GWAPI::GWAPIMgr* GWAPI::GWAPIMgr::GetInstance()
{
	static GWAPIMgr* inst = new GWAPIMgr();
	return inst;
}

GWAPI::GWAPIMgr::GWAPIMgr()
{
	if (MemoryMgr::Scan()){
		GameThread = new GameThreadMgr(this);
		CtoS = new CtoSMgr(this);
		StoC = new StoCMgr(this);
		Agents = new AgentMgr(this);
		DirectX = new DirectXMgr(this);
	}
	else{
		MessageBoxA(0, "Initialize Failed at finding all addresses, contact Developers about this.", "GWToolbox++ API Error", 0);
	}
}

