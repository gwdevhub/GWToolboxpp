#pragma once

#include <Windows.h>
#include "GWAPIMgr.h"


namespace GWAPI {

	class EffectMgr {
	public:
		
		// Get full array of effects and buffs for player and heroes.
		GW::AgentEffectsArray GetPartyEffectArray();

		// Get array of effects on the player.
		GW::EffectArray GetPlayerEffectArray();

		// Get array of buffs on the player.
		GW::BuffArray GetPlayerBuffArray();

		// Drop buffid buff.
		void DropBuff(DWORD buffId);

		// Gets effect struct of effect on player with SkillID, returns Effect::Nil() if no match.
		GW::Effect GetPlayerEffectById(DWORD SkillID);

		// Gets Buff struct of Buff on player with SkillID, returns Buff::Nil() if no match.
		GW::Buff GetPlayerBuffBySkillId(DWORD SkillID);

		// Returns current level of intoxication, 0-5 scale.
		// If > 0 then skills that benefit from drunk will work.
		DWORD GetAlcoholLevel() const { return AlcoholLevel; }

		// Have fun with this ;))))))))))
		void GetDrunkAf(DWORD Intensity, DWORD Tint);
		
	private:

		EffectMgr(GWAPIMgr* obj);
		~EffectMgr();

		typedef void(__fastcall *PPEFunc_t)(DWORD Intensity, DWORD Tint);
		static PPEFunc_t PPERetourFunc;
		static DWORD AlcoholLevel;
		BYTE* AlcoholHandlerRestore;
		static void __fastcall AlcoholHandler(DWORD Intensity, DWORD Tint);
		friend class GWAPIMgr;
		GWAPIMgr* parent;
		
	};
}