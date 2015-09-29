#pragma once

#include <Windows.h>
#include "GWAPIMgr.h"
#include <vector>

namespace GWAPI {

	class AgentMgr {
		AgentMgr(GWAPIMgr* obj);
		~AgentMgr();

		friend class GWAPIMgr;
		typedef void(__fastcall *ChangeTarget_t)(DWORD AgentID,DWORD smth);
		struct MovePosition {
			float X;
			float Y;
			DWORD ZPlane; // Ground in gwapi.
		};
		typedef void(__fastcall *Move_t)(MovePosition* Pos);

		GWAPIMgr* const parent_;

		ChangeTarget_t change_target_;

		
		Hook hk_dialog_log_;

		void RestoreHooks();

		static BYTE* dialog_log_ret_;
		static DWORD last_dialog_id_;
		static void detourDialogLog();

		Move_t move_;


	public:

		// Get AgentArray Structures of player or target.
		GW::Agent* GetPlayer();
		GW::Agent* GetTarget();

		// Get Current AgentID's of player or target.
		inline DWORD GetPlayerId() { return *(DWORD*)MemoryMgr::PlayerAgentIDPtr; }
		inline DWORD GetTargetId() { return *(DWORD*)MemoryMgr::TargetAgentIDPtr; }

		// Returns array of alternate agent array that can be read beyond compass range.
		// Holds limited info and needs to be explored more.
		GW::MapAgentArray GetMapAgentArray();

		// Returns Agentstruct Array of agents in compass range, full structs.
		GW::AgentArray GetAgentArray();

		// Returns the party member array, used in GetIsPartyLoaded().
		GW::PartyMemberArray GetPartyMemberArray();

		// Returns whether your party is loaded currently.
		bool GetIsPartyLoaded();

		// Checks if the whole party is ticked
		bool GetPartyTicked();

		// Get party member ready status
		bool GetTicked(DWORD index);

		// Get player ready status
		bool GetTicked();

		// Set party ready status.
		void Tick(bool flag);

		// Ticks
		inline void Tick() { Tick(true); }

		// Returns a vector of agents in the party. 
		// YOU SHALL DELETE THIS VECTOR AFTER YOU'RE DONE.
		std::vector<GW::Agent*>* GetParty();

		// Returns the size of the party
		size_t GetPartySize();

		// Computes distance between the two agents in game units
		DWORD GetDistance(GW::Agent* a, GW::Agent* b);

		// Computes squared distance between the two agents in game units
		DWORD GetSqrDistance(GW::Agent* a, GW::Agent* b);

		// Change targeted agent to (Agent)
		void ChangeTarget(GW::Agent* Agent);

		// Move to specified coordinates.
		void Move(float X, float Y, DWORD ZPlane = 0);

		// Same as pressing button (id) while talking to an NPC.
		void Dialog(DWORD id);

		// Go to an NPC and begin interaction.
		void GoNPC(GW::Agent* Agent, DWORD CallTarget = 0);

		// Walk to a player.
		void GoPlayer(GW::Agent* Agent);

		// Go to a chest/signpost (yellow nametag) specified by (Agent).
		// Also sets agent as your open chest target.
		void GoSignpost(GW::Agent* Agent, BOOL CallTarget = 0);

		// Call target of specified agent without interacting with the agent.
		void CallTarget(GW::Agent* Agent);

		// Returns last dialog id sent to the server.
		DWORD GetLastDialogId() const { return last_dialog_id_; }

		// Uses size of player array. Needs testing.
		DWORD GetAmountOfPlayersInInstance();

		// Returns name of player with selected loginnumber.
		wchar_t* GetPlayerNameByLoginNumber(DWORD loginnumber);

		// Returns AgentID of player with selected loginnumber.
		DWORD GetAgentIdByLoginNumber(DWORD loginnumber);

		// Returns the profession as short string, e.g. "W" for warrior
		const char* GetProfessionAcronym(GwConstants::Profession profession);
	};

}