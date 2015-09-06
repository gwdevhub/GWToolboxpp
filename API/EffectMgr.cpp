#include "EffectMgr.h"

#include "MemoryMgr.h"
#include "GameThreadMgr.h"
#include "CtoSMgr.h"
#include "AgentMgr.h"

DWORD GWAPI::EffectMgr::AlcoholLevel = NULL;
GWAPI::EffectMgr::PPEFunc_t GWAPI::EffectMgr::PPERetourFunc = NULL;


GWAPI::EffectMgr::EffectMgr(GWAPIMgr* obj) :parent(obj)
{
	PPERetourFunc = (PPEFunc_t)MemoryMgr::Detour(MemoryMgr::PostProcessEffectFunction, (BYTE*)AlcoholHandler, 6, &AlcoholHandlerRestore);
}

GWAPI::Effect GWAPI::EffectMgr::GetPlayerEffectById(DWORD SkillID)
{
	AgentEffectsArray AgEffects = GetPartyEffectArray();

	if (AgEffects.IsValid()){
		EffectArray Effects = AgEffects[0].Effects;
		if (Effects.IsValid()){
			for (DWORD i = 0; i < Effects.size(); i++) {
				if (Effects[i].SkillId == SkillID) return Effects[i];
			}
		}
	}

	return Effect::Nil();
}

GWAPI::Buff GWAPI::EffectMgr::GetPlayerBuffBySkillId(DWORD SkillID)
{
	AgentEffectsArray AgEffects = GetPartyEffectArray();

	if (AgEffects.IsValid()){
		BuffArray Buffs = AgEffects[0].Buffs;
		if (Buffs.IsValid()){
			for (DWORD i = 0; i < Buffs.size(); i++) {
				if (Buffs[i].SkillId == SkillID) return Buffs[i];
			}
		}
	}

	return Buff::Nil();
}

GWAPI::EffectArray GWAPI::EffectMgr::GetPlayerEffectArray()
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

GWAPI::AgentEffectsArray GWAPI::EffectMgr::GetPartyEffectArray()
{
	return *MemoryMgr::ReadPtrChain<AgentEffectsArray*>(MemoryMgr::GetContextPtr(), 2, 0x2C, 0x508);
}

GWAPI::BuffArray GWAPI::EffectMgr::GetPlayerBuffArray()
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
