#pragma once

#include <Windows.h>
#include "GWAPIMgr.h"
#include <vector>

namespace GWAPI {

	class AgentMgr {
		GWAPIMgr* const parent;
		typedef void(__fastcall *ChangeTarget_t)(DWORD AgentID);
		ChangeTarget_t _ChangeTarget;
		friend class GWAPIMgr;

		struct MovePosition {
			float X;
			float Y;
			DWORD ZPlane; // Ground in gwapi.
		};

		typedef void(__fastcall *Move_t)(MovePosition* Pos);
		Move_t _Move;
	public:

		Agent* GetPlayer();
		Agent* GetTarget();

		inline DWORD GetPlayerId() { return *(DWORD*)MemoryMgr::PlayerAgentIDPtr; }
		inline DWORD GetTargetId() { return *(DWORD*)MemoryMgr::TargetAgentIDPtr; }

		// Returns array of alternate agent array that can be read beyond compass range.
		// Holds limited info and needs to be explored more.
		MapAgentArray GetMapAgentArray();

		// Returns Agentstruct Array of agents in compass range, full structs.
		AgentArray GetAgentArray();

		// Returns the party member array, used in GetIsPartyLoaded().
		PartyMemberArray GetPartyMemberArray();

		// Returns whether your party is loaded currently.
		bool GetIsPartyLoaded();

		// Returns a vector of agents in the party. 
		// YOU SHALL DELETE THIS VECTOR AFTER YOU'RE DONE.
		std::vector<Agent*>* GetParty();

		// Returns the size of the party
		size_t GetPartySize();

		// Computes distance between the two agents in game units
		DWORD GetDistance(Agent* a, Agent* b);

		// Computes squared distance between the two agents in game units
		DWORD GetSqrDistance(Agent* a, Agent* b);

		void ChangeTarget(Agent* Agent);

		void Move(float X, float Y, DWORD ZPlane = 0);

		void Dialog(DWORD id);

		AgentMgr(GWAPIMgr* obj);
	};

}