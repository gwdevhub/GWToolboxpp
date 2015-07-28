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
			T GetIndex(DWORD index)
			{
				if (index > m_currentsize || index < 0) throw 1;
				return m_array[index];
			}

			T operator[](DWORD index)
			{
				return GetIndex(index);
			}

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
		template <typename T> static T ReadPtrChain(DWORD pBase, long pOffset1 = -1, long pOffset2 = -1, long pOffset3 = -1, long pOffset4 = -1, long pOffset5 = -1)
		{
			DWORD pRead = pBase;
			if (pRead == NULL){ return 0; }

			if (pOffset1 != -1){ pRead = *(DWORD*)(pRead + pOffset1); }
			if (pRead == NULL){ return 0; }

			if (pOffset2 != -1){ pRead = *(DWORD*)(pRead + pOffset2); }
			if (pRead == NULL){ return 0; }

			if (pOffset3 != -1){ pRead = *(DWORD*)(pRead + pOffset3); }
			if (pRead == NULL){ return 0; }

			if (pOffset4 != -1){ pRead = *(DWORD*)(pRead + pOffset4); }
			if (pRead == NULL){ return 0; }

			if (pOffset5 != -1){ pRead = *(DWORD*)(pRead + pOffset5); }
			if (pRead == NULL){ return 0; }

			return (T)(pRead);
		}



		// Memory Reads.
		static DWORD GetContextPtr(){ return *(DWORD*)((*(BYTE**)BasePointerLocation) + 0x18); }
	};
}