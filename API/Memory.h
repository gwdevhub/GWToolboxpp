#pragma once

#include "APIMain.h"

namespace GWAPI{



	struct CMemory{

		// Agent shit
		AgentArray* agArrayPtr;
		DWORD* PlayerAgentIDPtr;
		DWORD* TargetAgentIDPtr;

		// Map Id
		DWORD* MapIDPtr;


		// Send Packet Shit
		DWORD* CtoGSObjectPtr;
		SendCtoGSPacket_t CtoGSSendFunction;

		// Base ptr to get context ptr for gameworld
		BYTE* BasePointerLocation;

		// Disable/Enable Rendering
		BYTE* RenderLoopLocation;

		// For gamethread calls
		BYTE* GameLoopLocation;
		BYTE* GameLoopReturn;
		BYTE* GameLoopRestore;

		// Basics
		bool Scan();
		void *Detour(BYTE *src, const BYTE *dst, const int len, BYTE** restore = NULL);
		void Retour(BYTE *src, BYTE *restore, const int len);
		template <typename T>  T ReadPtrChain(DWORD pBase, long pOffset1 = 0, long pOffset2 = 0, long pOffset3 = 0, long pOffset4 = 0, long pOffset5 = 0);


		// Memory Reads.

		inline DWORD GetContextPtr(){ return *(DWORD*)((*(BYTE**)BasePointerLocation) + 0x18); }
		AgentArray& GetAgentArray()
		{
			return *agArrayPtr;
		}


	}Memory;

}