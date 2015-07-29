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

	private:
		GWAPIMgr* parent;
	};
}