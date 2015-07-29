#include "EffectMgr.h"

GWAPI::EffectMgr::EffectMgr(GWAPIMgr* obj) :parent(obj)
{

}

GWAPI::EffectMgr::Effect GWAPI::EffectMgr::GetPlayerEffectById(DWORD SkillID)
{
	EffectArray Effects = GetPlayerEffectArray();

	for (DWORD i = 1; i < Effects.size(); i++)
		if (Effects[i].SkillId == SkillID) return Effects[i];

	throw 1;
}

GWAPI::EffectMgr::EffectArray GWAPI::EffectMgr::GetPlayerEffectArray()
{
	return *MemoryMgr::ReadPtrChain<EffectArray*>(MemoryMgr::GetContextPtr(), 3, 0x2C, 0x508, 0x14);
}
