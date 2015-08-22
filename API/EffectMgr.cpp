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

	
	for (DWORD i = 0; i < Effects.size(); i++) {
		if (Effects[i].SkillId == SkillID) return Effects[i];
	}

	Effect noEff = { 0, 0, 0, 0, 0, 0 };
	return noEff;
}

GWAPI::EffectMgr::Buff GWAPI::EffectMgr::GetPlayerBuffBySkillId(DWORD SkillID)
{
	BuffArray Buffs = GetPlayerBuffArray();

	for (DWORD i = 0; i < Buffs.size(); i++) {
		if (Buffs[i].SkillId == SkillID) return Buffs[i];
	}

	Buff noBuff = { 0, 0, 0, 0 };
	return noBuff;
}

GWAPI::EffectMgr::EffectArray GWAPI::EffectMgr::GetPlayerEffectArray()
{
	AgentEffectsArray ageffects = GetPartyEffectArray();
	if (ageffects.IsValid()){
		EffectArray ret = ageffects[0].Effects;
		if (ret.IsValid()){
			return ret;
		}
	}

	throw API_EXCEPTION;
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

GWAPI::EffectMgr::AgentEffectsArray GWAPI::EffectMgr::GetPartyEffectArray()
{
	return *MemoryMgr::ReadPtrChain<AgentEffectsArray*>(MemoryMgr::GetContextPtr(), 2, 0x2C, 0x508);
}

GWAPI::EffectMgr::BuffArray GWAPI::EffectMgr::GetPlayerBuffArray()
{
	AgentEffectsArray ageffects = GetPartyEffectArray();
	if (ageffects.IsValid()){
		BuffArray ret = ageffects[0].Buffs;
		if (ret.IsValid()){
			return ret;
		}
	}

	throw API_EXCEPTION;
}

void GWAPI::EffectMgr::DropBuff(DWORD buffId) 
{
	parent->CtoS->SendPacket(0x8, 0x23, buffId);
}
