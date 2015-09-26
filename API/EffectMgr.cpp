#include "EffectMgr.h"

#include "MemoryMgr.h"
#include "GameThreadMgr.h"
#include "CtoSMgr.h"
#include "AgentMgr.h"

DWORD GWAPI::EffectMgr::alcohol_level_ = NULL;
GWAPI::EffectMgr::PPEFunc_t GWAPI::EffectMgr::ppe_retour_func_ = NULL;


GWAPI::EffectMgr::EffectMgr(GWAPIMgr* obj) :parent_(obj)
{
	ppe_retour_func_ = (PPEFunc_t)hk_post_process_effect_.Detour(MemoryMgr::PostProcessEffectFunction, (BYTE*)AlcoholHandler, 6);
}

GWAPI::GW::Effect GWAPI::EffectMgr::GetPlayerEffectById(GwConstants::SkillID SkillID)
{
	DWORD id = static_cast<DWORD>(SkillID);
	GW::AgentEffectsArray AgEffects = GetPartyEffectArray();

	if (AgEffects.valid()){
		GW::EffectArray Effects = AgEffects[0].Effects;
		if (Effects.valid()){
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

	if (AgEffects.valid()){
		GW::BuffArray Buffs = AgEffects[0].Buffs;
		if (Buffs.valid()){
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
	if (ageffects.valid()){
		GW::EffectArray ret = ageffects[0].Effects;
		if (ret.valid()){
			return ret;
		}
	}

	throw API_EXCEPTION;
}

void __fastcall GWAPI::EffectMgr::AlcoholHandler(DWORD Intensity, DWORD Tint)
{
	alcohol_level_ = Intensity;
	return ppe_retour_func_(Intensity, Tint);
}

GWAPI::EffectMgr::~EffectMgr()
{
}

void GWAPI::EffectMgr::GetDrunkAf(DWORD Intensity,DWORD Tint)
{
	parent_->Gamethread()->Enqueue(ppe_retour_func_, Intensity, Tint);
}

GWAPI::GW::AgentEffectsArray GWAPI::EffectMgr::GetPartyEffectArray()
{
	return *MemoryMgr::ReadPtrChain<GW::AgentEffectsArray*>(MemoryMgr::GetContextPtr(), 2, 0x2C, 0x508);
}

GWAPI::GW::BuffArray GWAPI::EffectMgr::GetPlayerBuffArray()
{
	GW::AgentEffectsArray ageffects = GetPartyEffectArray();
	if (ageffects.valid()){
		GW::BuffArray ret = ageffects[0].Buffs;
		if (ret.valid()){
			return ret;
		}
	}

	throw API_EXCEPTION;
}

void GWAPI::EffectMgr::DropBuff(DWORD buffId) 
{
	parent_->CtoS()->SendPacket(0x8, 0x23, buffId);
}

void GWAPI::EffectMgr::RestoreHooks()
{
	hk_post_process_effect_.Retour();
}
