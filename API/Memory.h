#pragma once

#include "APIncludes.h"
#include "Structures.h"
#include "GameLoop.h"

namespace GWAPI{

	

	class AgentArray;

	typedef  void(__fastcall *SendCtoGSPacket_t)(DWORD PacketObj,DWORD Size,DWORD* packet);
	typedef void(__fastcall *WriteChat_t)(DWORD Unk, wchar_t* Name, wchar_t* Message);

	struct MemoryMgr{

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

		// For writing PM's in chat.
		WriteChat_t WriteChatFunction;

		// Skill timer for effects.
		DWORD* SkillTimerPtr;

		// Skill structure array.

		Skill* SkillArray;

		// Basics
		bool Scan();
		void *Detour(BYTE *src, const BYTE *dst, const int len, BYTE** restore = NULL);
		void Retour(BYTE *src, BYTE *restore, const int len);
		template <typename T> T ReadPtrChain(DWORD pBase, long pOffset1 = 0, long pOffset2 = 0, long pOffset3 = 0, long pOffset4 = 0, long pOffset5 = 0);


		// Memory Reads.
		 DWORD GetContextPtr(){ return *(DWORD*)((*(BYTE**)BasePointerLocation) + 0x18); }
		 DWORD GetCtoSObj(){ return **(DWORD**)(CtoGSObjectPtr - 4); }
		 DWORD GetSkillTimer(){ return *SkillTimerPtr; }
		 DWORD GetMapID(){ return *MapIDPtr; }

		AgentArray GetAgentArray();
		SkillbarArray GetSkillbarArray();
		Skillbar GetPlayerSkillbar();
		EffectArray GetPlayerEffectArray();


		static MemoryMgr* GetInstance();

	private:
		MemoryMgr();
		~MemoryMgr();
	};
}