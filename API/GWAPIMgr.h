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
		friend class SkillbarMgr;
		friend class EffectMgr;

		GameThreadMgr* GameThread;
		CtoSMgr* CtoS;
		StoCMgr* StoC;
	
		GWAPIMgr();
	public:
		AgentMgr* Agents;
		SkillbarMgr* Skillbar;
		EffectMgr* Effects;
		DirectXMgr* DirectX;

		static GWAPIMgr* GetInstance();
	};

}