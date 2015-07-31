#pragma once

#include <Windows.h>
#include "APIMain.h"


namespace GWAPI {

	class EffectMgr {
	public:

		struct Effect {							// total : 18 BYTEs
			DWORD SkillId;						// 0000						skill id of the effect
			long EffectType;					// 0004						type classifier 0 = condition/shout, 8 = stance, 11 = maintained enchantment, 14 = enchantment/nature ritual
			DWORD EffectId;						// 0008						unique identifier of effect
			DWORD AgentId;						// 000C						non-zero means maintained enchantment - caster id
			float Duration;						// 0010						non-zero if effect has a duration
			DWORD TimeStamp;					// 0014						GW-timestamp of when effect was applied - only with duration

			long GetTimeElapsed() const { return MemoryMgr::GetSkillTimer() - TimeStamp; }
			long GetTimeRemaining() const { return (long)(Duration * 1000) - GetTimeElapsed(); }
		};

		typedef MemoryMgr::gw_array<Effect> EffectArray;
		EffectArray GetPlayerEffectArray();
		Effect GetPlayerEffectById(DWORD SkillID);
		EffectMgr(GWAPIMgr* obj);
		~EffectMgr();
		DWORD GetAlcoholLevel() const { return AlcoholLevel; }
		void GetDrunkAf(DWORD Intensity, DWORD Tint);
		
	private:

		typedef void(__fastcall *PPEFunc_t)(DWORD Intensity, DWORD Tint);
		static PPEFunc_t PPERetourFunc;
		static DWORD AlcoholLevel;
		BYTE* AlcoholHandlerRestore;
		static void __fastcall AlcoholHandler(DWORD Intensity, DWORD Tint);

		GWAPIMgr* parent;
		
	};
}