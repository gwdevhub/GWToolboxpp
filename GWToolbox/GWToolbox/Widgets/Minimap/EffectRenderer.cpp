#include "stdafx.h"
#include "EffectRenderer.h"

#include <d3d9.h>
#include <d3dx9math.h>

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/Packets/StoC.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/CtoSMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/EffectMgr.h>

#include "GuiUtils.h"

namespace {
	enum SkillEffect {
		Chaos_storm = 131,
		Meteor_Shower = 341,
		Savannah_heat = 346,
		Lava_font = 347,
		Breath_of_fire = 351,
		Maelstrom = 381,
		Barbed_Trap = 772,
		Barbed_Trap_Activate = 773,
		Flame_Trap = 774,
		Flame_Trap_Activate = 775,
		Spike_Trap = 777,
		Spike_Trap_Activate = 778,
		Bed_of_coals = 875,
		Churning_earth = 994
	};

	static bool GetRangeAndDuration(uint32_t effect_id, float* range, int* duration, uint32_t header) {
		*range = GW::Constants::Range::Adjacent;
		*duration = 10000;
		switch (effect_id) {
		case SkillEffect::Maelstrom:
		case SkillEffect::Chaos_storm:
			*duration = 10000;
			break;
		case SkillEffect::Savannah_heat:
		case SkillEffect::Breath_of_fire:
		case SkillEffect::Bed_of_coals:
		case SkillEffect::Lava_font:
			*duration = 5000;
			break;
		case SkillEffect::Barbed_Trap:
		case SkillEffect::Flame_Trap:
		case SkillEffect::Spike_Trap:
			// For traps, GW sometimes sends GenericValueTarget AND GenericValue - we only care about the latter.
			if (header != GW::Packet::StoC::GenericValue::STATIC_HEADER)
				return false; 
			*duration = 90000;
			*range = GW::Constants::Range::Nearby;
			break;
		case SkillEffect::Churning_earth:
			*duration = 5000;
			*range = GW::Constants::Range::Nearby;
			break;
		default: return false;
		}
		return true;
	}
}
void EffectRenderer::LoadSettings(CSimpleIni* ini, const char* section) {
	Invalidate();
	Colors::Load(ini, section, VAR_NAME(aoe_color), aoe_color);
}
void EffectRenderer::SaveSettings(CSimpleIni* ini, const char* section) const {
	Colors::Save(ini, section, VAR_NAME(aoe_color), aoe_color);
}
void EffectRenderer::DrawSettings() {
	Colors::DrawSetting("AoE Circle Color", &aoe_color);
}
void EffectRenderer::RemoveTriggeredEffect(uint32_t effect_id, GW::Vec2f* pos) {
	uint32_t effect_to_check = 0;
	switch (effect_id) {
	case SkillEffect::Barbed_Trap_Activate:
		effect_to_check = Barbed_Trap;
		break;
	case SkillEffect::Flame_Trap_Activate:
		effect_to_check = Flame_Trap;
		break;
	case SkillEffect::Spike_Trap_Activate:
		effect_to_check = Spike_Trap;
		break;
	}
	if (!effect_to_check)
		return;
	std::pair<float, float> posp = { pos->x,pos->y };
	auto trap_handled = trap_triggers_handled.find(posp);
	if (trap_handled != trap_triggers_handled.end() && TIMER_DIFF(trap_handled->second) < 5000) {
		return; // Already handled this trap, e.g. Spike Trap triggers 3 times over 2 seconds; we only care about the first.
	}
	trap_triggers_handled.emplace(posp, TIMER_INIT());
	std::lock_guard<std::recursive_mutex> lock(effects_mutex);
	Effect* closest = nullptr;
	float closestDistance = GW::Constants::SqrRange::Nearby;
	uint32_t closest_idx = 0;
	for (size_t i = 0; i < aoe_effects.size();i++) {
		Effect* effect = aoe_effects[i];
		if (!effect || effect->effect_id != effect_to_check)
			continue;
		// Need to estimate position; player may have moved on cast slightly.
		float newDistance = GW::GetSquareDistance(*pos,effect->pos);
		if (newDistance > closestDistance)
			continue;
		closest_idx = i;
		closest = effect;
		closestDistance = newDistance;
	}
	if (closest) {
		// Trigger this trap to time out in 2 seconds' time.
		closest->start = TIMER_INIT();
		closest->duration = 2000;
	}
}

void EffectRenderer::PacketCallback(GW::Packet::StoC::GenericValue* pak) {
	if (!initialized) return;
	int duration = 10000;
	float range = 0;
	if (pak->Value_id != 21) // Effect on agent
		return;
	if (!GetRangeAndDuration(pak->value, &range, &duration, pak->header))
		return;
	GW::Agent* caster = GW::Agents::GetAgentByID(pak->agent_id);
	//if (!caster || caster->allegiance != 0x3) return;
	Effect* e = new Effect(pak->value, caster->pos.x, caster->pos.y, duration, range);
    e->circle.color = &aoe_color;
	aoe_effects.push_back(e);
}
void EffectRenderer::PacketCallback(GW::Packet::StoC::GenericValueTarget* pak) {
	if (!initialized) return;
	int duration = 10000;
	float range = 0;
	if (pak->Value_id != 20) // Effect on target
		return;
	if (!GetRangeAndDuration(pak->value, &range, &duration, pak->header))
		return;
	if (pak->caster == pak->target) return;
	GW::Agent* caster = GW::Agents::GetAgentByID(pak->caster);
	if (!caster || caster->allegiance != 0x3) return;
	GW::Agent* target = GW::Agents::GetAgentByID(pak->target);
	if (!target) return;
	Effect* e = new Effect(pak->value, target->pos.x, target->pos.y, duration, range);
	e->circle.color = &aoe_color;
	aoe_effects.push_back(e);
}
void EffectRenderer::PacketCallback(GW::Packet::StoC::PlayEffect* pak) {
	if (!initialized) return;
	// TODO: Fire storm and Meteor shower have no caster!
	// Need to record GenericValueTarget with value_id matching these skills, then roughly match the coords after.
	int duration = 10000;
	float range = 0;
	RemoveTriggeredEffect(pak->effect_id, &pak->coords);
	if (!GetRangeAndDuration(pak->effect_id, &range, &duration, pak->header))
		return;
	GW::Agent* a = GW::Agents::GetAgentByID(pak->agent_id);
	if (!a || a->allegiance != 0x3) return;
	Effect* e = new Effect(pak->effect_id, pak->coords.x, pak->coords.y, duration, range);
	e->circle.color = &aoe_color;
	aoe_effects.push_back(e);
}
void EffectRenderer::Initialize(IDirect3DDevice9* device) {
	if (initialized)
		return;
	initialized = true;
	type = D3DPT_LINELIST;

	HRESULT hr = device->CreateVertexBuffer(sizeof(D3DVertex) * vertices_max, 0,
		D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer, NULL);
	if (FAILED(hr)) {
		printf("Error setting up PingsLinesRenderer vertex buffer: %d\n", hr);
	}
	GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GameSrvTransfer>(&StoC_Hook, [&](GW::HookStatus*, GW::Packet::StoC::GameSrvTransfer*) {
		need_to_clear_effects = true;
		});
}
void EffectRenderer::Render(IDirect3DDevice9* device) {
	Initialize(device);
	DrawAoeEffects(device);
}
void EffectRenderer::DrawAoeEffects(IDirect3DDevice9* device) {
	if (need_to_clear_effects) {
		Invalidate();
		need_to_clear_effects = false;
	}
    if (aoe_effects.empty())
        return;
	D3DXMATRIX translate, scale, world;
	std::lock_guard<std::recursive_mutex> lock(effects_mutex);
	int effect_size = aoe_effects.size();
	for (int i = 0; i < effect_size; i++) {
		Effect* effect = aoe_effects[i];
		if (!effect)
			continue;
		if (TIMER_DIFF(effect->start) > effect->duration) {
			aoe_effects.erase(aoe_effects.begin() + i);
			delete effect;
			i--;
			effect_size--;
			continue;
		}
		D3DXMatrixScaling(&scale, effect->circle.range, effect->circle.range, 1.0f);
        D3DXMatrixTranslation(&translate, effect->pos.x, effect->pos.y, 0.0f);
        world = scale * translate;
        device->SetTransform(D3DTS_WORLD, &world);
        effect->circle.Render(device);
	}
}

void EffectRenderer::EffectCircle::Initialize(IDirect3DDevice9* device) {
	type = D3DPT_LINESTRIP;
	count = 16; // polycount
	unsigned int vertex_count = count + 1;
	D3DVertex* vertices = nullptr;

	if (buffer) buffer->Release();
	device->CreateVertexBuffer(sizeof(D3DVertex) * vertex_count, 0,
		D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer, NULL);
	buffer->Lock(0, sizeof(D3DVertex) * vertex_count,
		(VOID**)&vertices, D3DLOCK_DISCARD);

	for (size_t i = 0; i < count; ++i) {
		float angle = i * (2 * (float)M_PI / count);
		vertices[i].x = std::cos(angle);
		vertices[i].y = std::sin(angle);
		vertices[i].z = 0.0f;
		vertices[i].color = *color;
	}
	vertices[count] = vertices[0];

	buffer->Unlock();
}
