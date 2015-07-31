#include "EffectMgr.h"

DWORD GWAPI::EffectMgr::AlcoholLevel = NULL;
GWAPI::EffectMgr::PPEFunc_t GWAPI::EffectMgr::PPERetourFunc = NULL;


GWAPI::EffectMgr::EffectMgr(GWAPIMgr* obj) :parent(obj)
{
	PPERetourFunc = (PPEFunc_t)MemoryMgr::Detour(MemoryMgr::PostProcessEffectFunction, (BYTE*)AlcoholHandler, 6, &AlcoholHandlerRestore);
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

void __fastcall GWAPI::EffectMgr::AlcoholHandler(DWORD Intensity, DWORD Tint)
{
	AlcoholLevel = Intensity;
	return PPERetourFunc(Intensity, Tint);
}

GWAPI::EffectMgr::~EffectMgr()
{
	MemoryMgr::Retour(MemoryMgr::PostProcessEffectFunction, AlcoholHandlerRestore, 6);
}

void GWAPI::EffectMgr::GetDrunkAf(DWORD Intensity,DWORD Tint)
{
	parent->GameThread->Enqueue(PPERetourFunc, Intensity, Tint);
}
