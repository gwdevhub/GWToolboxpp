#pragma once

#include <Windows.h>
#include "GWAPIMgr.h"

namespace GWAPI{

	class SkillbarMgr{	
	public:

		GW::SkillbarArray GetSkillbarArray();
		GW::Skillbar GetPlayerSkillbar();

		// Get the skill slot in the player bar of the player.
		// Returns 0 if the skill is not there
		int getSkillSlot(GwConstants::SkillID SkillID);
		
		void UseSkill(DWORD Slot, DWORD Target = 0, DWORD CallTarget = 0);

		void UseSkillByID(DWORD SkillID, DWORD Target = 0, DWORD CallTarget = 0);

		GW::Skill GetSkillConstantData(DWORD SkillID);
		SkillbarMgr(GWAPIMgr* obj);
	private:
		typedef void(__fastcall *UseSkill_t)(DWORD, DWORD, DWORD, DWORD);
		UseSkill_t _UseSkill;
		GWAPIMgr* parent;
		GW::Skill* SkillConstants;
	};
}