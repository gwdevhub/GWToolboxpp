#pragma  once

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
	public:
		AgentMgr* Agents;
		DirectXMgr* DirectX;

		GWAPIMgr(){
			if (MemoryMgr::Scan()){
				GameThread = new GameThreadMgr(this);
			}
		}
	};

}