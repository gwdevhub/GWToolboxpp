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

GWAPI::GW::Effect GWAPI::EffectMgr::GetPlayerEffectById(GwConstants::SkillID SkillID)
{
	DWORD id = static_cast<DWORD>(SkillID);
	GW::AgentEffectsArray AgEffects = GetPartyEffectArray();

	if (AgEffects.IsValid()){
		GW::EffectArray Effects = AgEffects[0].Effects;
		if (Effects.IsValid()){
			for (DWORD i = 0; i < Effects.size(); i++) {
				if (Effects[i].SkillId == id) return Effects[i];
			}
		}
	}

	return GW::Effect::Nil();
}

GWAPI::GW::Buff GWAPI::EffectMgr::GetPlayerBuffBySkillId(GwConstants::SkillID SkillID)
{	
	DWORD id = static_cast<DWORD>(SkillID);
	GW::AgentEffectsArray AgEffects = GetPartyEffectArray();

	if (AgEffects.IsValid()){
		GW::BuffArray Buffs = AgEffects[0].Buffs;
		if (Buffs.IsValid()){
			for (DWORD i = 0; i < Buffs.size(); i++) {
				if (Buffs[i].SkillId == id) return Buffs[i];
			}
		}
	}

	return GW::Buff::Nil();
}

GWAPI::GW::EffectArray GWAPI::EffectMgr::GetPlayerEffectArray()
{
	GW::AgentEffectsArray ageffects = GetPartyEffectArray();
	if (ageffects.IsValid()){
		GW::EffectArray ret = ageffects[0].Effects;
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

GWAPI::GW::AgentEffectsArray GWAPI::EffectMgr::GetPartyEffectArray()
{
	return *MemoryMgr::ReadPtrChain<GW::AgentEffectsArray*>(MemoryMgr::GetContextPtr(), 2, 0x2C, 0x508);
}

GWAPI::GW::BuffArray GWAPI::EffectMgr::GetPlayerBuffArray()
{
	GW::AgentEffectsArray ageffects = GetPartyEffectArray();
	if (ageffects.IsValid()){
		GW::BuffArray ret = ageffects[0].Buffs;
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
