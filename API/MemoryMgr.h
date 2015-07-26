#pragma once

#include <Windows.h>

namespace GWAPI{

	struct MemoryMgr{

		template <typename T>
		class gw_array {
		protected:
			T* m_array;
			DWORD m_allocatedsize;
			DWORD m_currentsize;
		public:
			T GetIndex(DWORD index);
			T operator[](DWORD index);
			DWORD size() const { return m_currentsize; }
		};

		static bool scanCompleted;

		// Agent shit
		static BYTE* agArrayPtr;
		static BYTE* PlayerAgentIDPtr;
		static BYTE* TargetAgentIDPtr;

		// Map Id
		static BYTE* MapIDPtr;


		// Send Packet Shit
		static BYTE* CtoGSObjectPtr;
		static BYTE* CtoGSSendFunction;

		// Base ptr to get context ptr for gameworld
		static BYTE* BasePointerLocation;

		// Disable/Enable Rendering
		static BYTE* RenderLoopLocation;

		// For gamethread calls
		static BYTE* GameLoopLocation;
		static BYTE* GameLoopReturn;
		static BYTE* GameLoopRestore;

		// For writing PM's in chat.
		static BYTE* WriteChatFunction;

		// Skill timer for effects.
		static BYTE* SkillTimerPtr;

		// Skill structure array.

		static BYTE* SkillArray;

		// Basics
		static bool Scan();
		static void *Detour(BYTE *src, const BYTE *dst, const int len, BYTE** restore = NULL);
		static void Retour(BYTE *src, BYTE *restore, const int len);
		template <typename T> static T ReadPtrChain(DWORD pBase, long pOffset1 = 0, long pOffset2 = 0, long pOffset3 = 0, long pOffset4 = 0, long pOffset5 = 0);


		// Memory Reads.
		static DWORD GetContextPtr(){ return *(DWORD*)((*(BYTE**)BasePointerLocation) + 0x18); }
	};
}