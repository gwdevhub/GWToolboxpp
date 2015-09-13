#pragma once

#include <Windows.h>
#include <stdio.h>

namespace GWAPI{

	struct MemoryMgr{

		template <typename T>
		class gw_array {
		protected:
			T* m_array;
			DWORD m_allocatedsize;
			DWORD m_currentsize;
			DWORD m_unk1;
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
			bool IsValid(){
				return m_array != NULL;
			}

			DWORD size() const { return m_currentsize; }
		};

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
		
		// To extract alcohol level.
		static BYTE* PostProcessEffectFunction;

		// To change target.

		static BYTE* ChangeTargetFunction;

		// Skill structure array.

		static BYTE* SkillArray;
		static BYTE* UseSkillFunction;

		// Addresses used for opening xunlai window.
		static BYTE* OpenXunlaiFunction;
		static BYTE* XunlaiSession;

		// For move calls that dont glitch your char around.
		static BYTE* MoveFunction;

		static BYTE* WinHandlePtr;

		// For buying/selling items
		static BYTE* BuyItemFunction;
		static BYTE* SellItemFunction;

		static BYTE* TraderBuyClassHook;
		static BYTE* TraderSellClassHook;
		static BYTE* RequestQuoteFunction;

		static BYTE* TraderFunction;
		static BYTE* CraftitemObj;

		static BYTE* MapInfoPtr;

		static BYTE* DialogFunc;

		// Basics
		static bool Scan();
		template <typename T> static T ReadPtrChain(DWORD pBase,DWORD AmountofOffsets,...)
		{
			va_list vl;

			va_start(vl, AmountofOffsets);
			while (AmountofOffsets--)
			{
				pBase = (*(DWORD*)pBase);
					if (pBase){
						pBase += va_arg(vl, DWORD);
					}
					else{
						return NULL;
					}
			}
			va_end(vl);

			return (T)pBase;
		}

		// Memory Reads.
		inline static DWORD GetContextPtr(){ return (*(DWORD*)BasePointerLocation) + 0x18; }
		inline static DWORD GetSkillTimer(){ return *(DWORD*)SkillTimerPtr; }
		inline static HWND GetGWWindowHandle(){ return *(HWND*)WinHandlePtr; }
	};
}