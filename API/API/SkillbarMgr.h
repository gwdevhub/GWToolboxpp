#pragma once

#include <Windows.h>
#include "GWAPIMgr.h"

namespace GWAPI{

	class SkillbarMgr{	
	public:

		// Get array of skillbars, [0] = player [1-7] = heroes.
		GW::SkillbarArray GetSkillbarArray();

		// Same as GetSkillbarArray()[0] with checks. 
		GW::Skillbar GetPlayerSkillbar();

		// Get the skill slot in the player bar of the player.
		// Returns 0 if the skill is not there
		int getSkillSlot(GwConstants::SkillID SkillID);
		
		// Use Skill in slot (Slot) on (Agent), optionally call that you are using said skill.
		void UseSkill(DWORD Slot, DWORD Target = 0, DWORD CallTarget = 0);

		// Send raw packet to use skill with ID (SkillID). 
		// Same as above except the skillbar client struct will not be registered as casting.
		void UseSkillByID(DWORD SkillID, DWORD Target = 0, DWORD CallTarget = 0);

		// Get skill structure of said id, houses pretty much everything you would want to know about the skill.
		GW::Skill GetSkillConstantData(DWORD SkillID);


		
	private:
		SkillbarMgr(GWAPIMgr* obj);

		typedef void(__fastcall *UseSkill_t)(DWORD, DWORD, DWORD, DWORD);
		friend class GWAPIMgr;

		UseSkill_t UseSkill_;
		GWAPIMgr* parent_;
		GW::Skill* SkillConstants_;
	};
}