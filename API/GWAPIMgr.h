#pragma  once
#include <Windows.h>
#include "APIMain.h"

namespace GWAPI {

	class GWAPIMgr {

		friend class GameThreadMgr;
		friend class CtoSMgr;
		friend class StoCMgr;
		friend class AgentMgr;
		friend class DirectXMgr;

		GameThreadMgr* GameThread;
		CtoSMgr* CtoS;
		StoCMgr* StoC;
	
		

		GWAPIMgr();
	public:
		AgentMgr* Agents;
		DirectXMgr* DirectX;

		static GWAPIMgr* GetInstance();
	};

}