#include "SkillbarMgr.h"

#include "MemoryMgr.h"
#include "GameThreadMgr.h"
#include "CtoSMgr.h"


GWAPI::GW::Skill GWAPI::SkillbarMgr::GetSkillConstantData(DWORD SkillID)
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
	SkillConstants = (GW::Skill*)MemoryMgr::SkillArray;
	_UseSkill = (UseSkill_t)MemoryMgr::UseSkillFunction;
}

GWAPI::GW::Skillbar GWAPI::SkillbarMgr::GetPlayerSkillbar()
{
	GW::SkillbarArray sb = GetSkillbarArray();

	if (!sb.IsValid()) throw API_EXCEPTION;

	return sb[0];
}

GWAPI::GW::SkillbarArray GWAPI::SkillbarMgr::GetSkillbarArray()
{
	return *MemoryMgr::ReadPtrChain<GW::SkillbarArray*>(MemoryMgr::GetContextPtr(), 2, 0x2C, 0x6F0);
}

void GWAPI::SkillbarMgr::UseSkillByID(DWORD SkillID, DWORD Target /*= 0*/, DWORD CallTarget /*= 0*/)
{
	parent->CtoS->SendPacket(0x14, 0x40, SkillID, 0, Target, CallTarget);
}

int GWAPI::SkillbarMgr::getSkillSlot(DWORD SkillID) {
	GW::Skillbar bar = GetPlayerSkillbar();
	for (int i = 0; i < 8; ++i) {
		if (bar.Skills[i].SkillId == SkillID) {
			return i;
		}
	}
	return 0;
}
