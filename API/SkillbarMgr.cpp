#include "SkillbarMgr.h"


GWAPI::SkillbarMgr::Skill GWAPI::SkillbarMgr::GetSkillConstantData(DWORD SkillID)
{
	return SkillConstants[SkillID];
}

void GWAPI::SkillbarMgr::UseSkill(DWORD Slot, DWORD Target /*= 0*/, DWORD CallTarget /*= 0*/)
{
	Slot--;
	parent->GameThread->Enqueue(_UseSkill, parent->Agents->GetPlayerId(), Slot, Target, CallTarget);
}

GWAPI::SkillbarMgr::SkillbarMgr(GWAPIMgr* obj) : parent(obj)
{
	SkillConstants = (Skill*)MemoryMgr::SkillArray;
	_UseSkill = (UseSkill_t)MemoryMgr::UseSkillFunction;
}

GWAPI::SkillbarMgr::Skillbar GWAPI::SkillbarMgr::GetPlayerSkillbar()
{
	return GetSkillbarArray()[0];
}

GWAPI::SkillbarMgr::SkillbarArray GWAPI::SkillbarMgr::GetSkillbarArray()
{
	return *MemoryMgr::ReadPtrChain<SkillbarArray*>(MemoryMgr::GetContextPtr(), 2, 0x2C, 0x6F0);
}

void GWAPI::SkillbarMgr::UseSkillByID(DWORD SkillID, DWORD Target /*= 0*/, DWORD CallTarget /*= 0*/)
{
	parent->CtoS->SendPacket(0x14, 0x40, SkillID, 0, Target, CallTarget);
}

int GWAPI::SkillbarMgr::getSkillSlot(DWORD SkillID) {
	Skillbar bar = GetPlayerSkillbar();
	for (int i = 0; i < 8; ++i) {
		if (bar.Skills[i].SkillId == SkillID) {
			return i;
		}
	}
	return 0;
}
