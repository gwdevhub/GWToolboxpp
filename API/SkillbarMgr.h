#pragma once

#include <Windows.h>
#include "APIMain.h"

namespace GWAPI{

	class SkillbarMgr{	
	public:

		struct Skill{							// total : A0 BYTEs
			DWORD SkillId;						// 0000
			BYTE Unknown1[4];					// 0004
			long Campaign;						// 0008	
			long Type;							// 000C
			DWORD Special;
			long ComboReq;						// 0014
			DWORD Effect1;
			DWORD Condition;
			DWORD Effect2;
			DWORD WeaponReq;
			BYTE Profession;					// 0028
			BYTE Attribute;						// 0029
			BYTE Unknown2[2];					// 002A
			long SkillId_PvP;					// 002C
			BYTE Combo;							// 0030
			BYTE Target;						// 0031
			BYTE unknown3;						// 0032
			BYTE SkillEquipType;				// 0033
			BYTE Unknown4;						// 0034
			BYTE EnergyCost;
			BYTE HealthCost;
			BYTE Unknown7;
			DWORD Adrenaline;					// 0038
			float Activation;					// 003C
			float Aftercast;					// 0040
			long Duration0;						// 0044
			long Duration15;					// 0048
			long Recharge;						// 004C
			BYTE Unknown5[12];					// 0050
			long Scale0;						// 005C
			long Scale15;						// 0060
			long BonusScale0;					// 0064
			long BonusScale15;					// 0068
			float AoERange;						// 006C
			float ConstEffect;					// 0070
			BYTE unknown6[44];					// 0074

			BYTE GetEnergyCost()
			{
				switch (EnergyCost){
				case 11: return 15;
				case 12: return 25;
				default: return EnergyCost;
				}
			}
		};

		struct Skillbar {						// total : BC BYTEs
			DWORD AgentId;						// 0000						id of the agent whose skillbar this is
			struct {
				long AdrenalineA;					// 0000					
				long AdrenalineB;					// 0004					
				DWORD Recharge;						// 0008					
				DWORD SkillId;						// 000C						see GWConst::SkillIds
				DWORD Event;						// 0010	s
			}Skills[8];			// 0004
			DWORD Disabled;
			BYTE unknown1[8];					// 00A8	|--8 BYTEs--|
			DWORD Casting;						// 00B0
			BYTE unknown2[8];					// 00B4	|--8 BYTEs--|
		};

		typedef MemoryMgr::gw_array<Skillbar> SkillbarArray;

		SkillbarArray GetSkillbarArray();
		Skillbar GetPlayerSkillbar();
		
		void UseSkill(DWORD Slot, DWORD Target = 0, DWORD CallTarget = 0);
		Skill GetSkillConstantData(DWORD SkillID);
		SkillbarMgr(GWAPIMgr* obj);
	private:
		typedef void(__fastcall *UseSkill_t)(DWORD, DWORD, DWORD, DWORD);
		UseSkill_t _UseSkill;
		GWAPIMgr* parent;
		Skill* SkillConstants;
	};
}