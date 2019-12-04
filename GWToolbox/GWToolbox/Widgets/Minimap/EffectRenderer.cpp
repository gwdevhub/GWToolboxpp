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
		Bed_of_coals = 875,
		Churning_earth = 994
	};
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

void EffectRenderer::PacketCallback(GW::Packet::StoC::GenericValue* pak) {
	if (!initialized) return;
	uint32_t duration = 10000;
	switch (pak->Value_id) {
	case 21: // Effect on agent
		switch (pak->value) {
		case SkillEffect::Lava_font:
			duration = 5000;
			break;
		default: return;
		}
	default:return;
	}
	GW::Agent* caster = GW::Agents::GetAgentByID(pak->agent_id);
	if (!caster || caster->allegiance != 0x3) return;
	Effect* e = new Effect(pak->value, caster->pos.x, caster->pos.y, duration);
    e->circle.color = &aoe_color;
	aoe_effects.push_front(e);
}
void EffectRenderer::PacketCallback(GW::Packet::StoC::GenericValueTarget* pak) {
	if (!initialized) return;
	uint32_t duration = 10000;
	switch (pak->Value_id) {
		case 20: // Effect on target
			switch (pak->value) {
			case SkillEffect::Maelstrom:
				duration = 10000;
				break;
			case SkillEffect::Savannah_heat:
			case SkillEffect::Breath_of_fire:
			case SkillEffect::Bed_of_coals:
				duration = 5000;
				break;
			default: return;
			}
			break;
		default: return;
	}
	if (pak->caster == pak->target) return;
	GW::Agent* caster = GW::Agents::GetAgentByID(pak->caster);
	if (!caster || caster->allegiance != 0x3) return;
	GW::Agent* target = GW::Agents::GetAgentByID(pak->target);
	if (!target) return;
	Effect* e = new Effect(pak->value, target->pos.x, target->pos.y, duration);
	e->circle.color = &aoe_color;
	aoe_effects.push_front(e);
}
void EffectRenderer::PacketCallback(GW::Packet::StoC::PlayEffect* pak) {
	if (!initialized) return;
	// TODO: Fire storm and Meteor shower have no caster!
	// Need to record GenericValueTarget with value_id matching these skills, then roughly match the coords after.
	uint32_t duration = 10000;
	float range = GW::Constants::Range::Adjacent;
	switch (pak->effect_id) {
	case SkillEffect::Chaos_storm:
		duration = 10000;
		break;
	case SkillEffect::Churning_earth:
		duration = 5000;
		range = GW::Constants::Range::Nearby;
		break;
	default: return;
	}
	if (pak->agent_id) {
		GW::Agent* a = GW::Agents::GetAgentByID(pak->agent_id);
		if (!a || a->allegiance != 0x3) return;
	}
	Effect* e = new Effect(pak->effect_id, pak->coords.x, pak->coords.y, duration, range);
	e->circle.color = &aoe_color;
	aoe_effects.push_front(e);
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
}
void EffectRenderer::Render(IDirect3DDevice9* device) {
	Initialize(device);
	DrawAoeEffects(device);
}
void EffectRenderer::DrawAoeEffects(IDirect3DDevice9* device) {
    if (aoe_effects.empty())
        return;
	D3DXMATRIX translate, scale, world;
	D3DXMatrixScaling(&scale, GW::Constants::Range::Adjacent, GW::Constants::Range::Adjacent, 1.0f);
	for (size_t i = 0; i < aoe_effects.size(); i++) {
		Effect* effect = aoe_effects[i];
		if (TIMER_DIFF(effect->start) > effect->duration) {
			delete effect;
			aoe_effects.erase(aoe_effects.begin() + i);
			if (!aoe_effects.size()) break;
			i--;
            continue;
		}
        D3DXMatrixTranslation(&translate, effect->x, effect->y, 0.0f);
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
