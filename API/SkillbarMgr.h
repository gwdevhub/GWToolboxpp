#pragma once

#include <Windows.h>
#include "GWAPIMgr.h"

namespace GWAPI{

	class SkillbarMgr{	
	public:

		SkillbarArray GetSkillbarArray();
		Skillbar GetPlayerSkillbar();

		// Get the skill slot in the player bar of the player.
		// Returns 0 if the skill is not there
		int getSkillSlot(DWORD SkillID);
		
		void UseSkill(DWORD Slot, DWORD Target = 0, DWORD CallTarget = 0);

		void UseSkillByID(DWORD SkillID, DWORD Target = 0, DWORD CallTarget = 0);

		Skill GetSkillConstantData(DWORD SkillID);
		SkillbarMgr(GWAPIMgr* obj);
	private:
		typedef void(__fastcall *UseSkill_t)(DWORD, DWORD, DWORD, DWORD);
		UseSkill_t _UseSkill;
		GWAPIMgr* parent;
		Skill* SkillConstants;
	};
}