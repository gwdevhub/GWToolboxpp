#pragma once

#include <Windows.h>
#include "GWAPIMgr.h"


namespace GWAPI {

	class EffectMgr {
	public:

		GW::AgentEffectsArray GetPartyEffectArray();
		GW::EffectArray GetPlayerEffectArray();
		GW::BuffArray GetPlayerBuffArray();
		void DropBuff(DWORD buffId);
		GW::Effect GetPlayerEffectById(DWORD SkillID);
		GW::Buff GetPlayerBuffBySkillId(DWORD SkillID);
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