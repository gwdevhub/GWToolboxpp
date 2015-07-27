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
	
		

		GWAPIMgr(){
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
	public:
		AgentMgr* Agents;
		DirectXMgr* DirectX;

		static GWAPIMgr* GetInstance();
	};

}