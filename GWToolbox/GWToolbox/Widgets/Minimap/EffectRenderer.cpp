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


void EffectRenderer::LoadSettings(CSimpleIni* ini, const char* section) {
	Invalidate();
}
void EffectRenderer::SaveSettings(CSimpleIni* ini, const char* section) const {
	ini->SetDoubleValue(section, "maxrange_interp_begin", maxrange_interp_begin);
	ini->SetDoubleValue(section, "maxrange_interp_end", maxrange_interp_end);
}
void EffectRenderer::DrawSettings() {
	/*if (ImGui::SmallButton("Restore Defaults")) {
		// TODO
	}
	Colors::DrawSetting("Drawings", &color_drawings);
	if (Colors::DrawSetting("Pings", &ping_circle.color)) {
		ping_circle.Invalidate();
	}
	if (Colors::DrawSetting("Shadowstep Marker", &marker.color)) {
		marker.Invalidate();
	}
	Colors::DrawSetting("Shadowstep Line", &color_shadowstep_line);
	Colors::DrawSetting("Shadowstep Line (Max range)", &color_shadowstep_line_maxrange);
	if (ImGui::SliderFloat("Max range start", &maxrange_interp_begin, 0.0f, 1.0f)
		&& maxrange_interp_end < maxrange_interp_begin) {
		maxrange_interp_end = maxrange_interp_begin;
	}
	if (ImGui::SliderFloat("Max range end", &maxrange_interp_end, 0.0f, 1.0f)
		&& maxrange_interp_begin > maxrange_interp_end) {
		maxrange_interp_begin = maxrange_interp_end;
	}*/
}

EffectRenderer::EffectRenderer() : vertices(nullptr) {}

void EffectRenderer::PacketCallback(GW::Packet::StoC::GenericValue* pak) {
	if (pak->Value_id != 21) return;
	switch (pak->value) {
	case 347: // Lava font
		break;
	default: return;
	}
	GW::Agent* caster = GW::Agents::GetAgentByID(pak->agent_id);
	if (!caster || caster->allegiance != 0x3) return;
	pings.push_front(new Effect(pak->value, caster->pos.x, caster->pos.y));

}

void EffectRenderer::PacketCallback(GW::Packet::StoC::GenericValueTarget* pak) {
	if (pak->Value_id != 20) return;
	switch (pak->value) {
	case 381: // Maelstrom
	case 346: // Savannah heat
	case 351: // Breath of fire
	case 875: // Bed of coals
		break;
	default: return;
	}
	GW::Agent* caster = GW::Agents::GetAgentByID(pak->caster);
	if (!caster || caster->allegiance != 0x3) return;
	GW::Agent* target = GW::Agents::GetAgentByID(pak->target);
	if (!target) return;
	pings.push_front(new Effect(pak->value, target->pos.x, target->pos.y));

}

void EffectRenderer::PacketCallback(GW::Packet::StoC::PlayEffect* pak) {
	// TODO: Fire storm and Meteor shower have no caster!
	// Need to record GenericValueTarget with value_id matching these skills, then roughly match the coords after.
	switch (pak->effect_id) {
	case 131: // Chaos storm
	case 994: // Churning earth
		break;
	default: return;
	}
	if (pak->agent_id) {
		GW::Agent* a = GW::Agents::GetAgentByID(pak->agent_id);
		if (!a || a->allegiance != 0x3) return;
	}
	pings.push_front(new Effect(pak->effect_id, pak->coords.x, pak->coords.y));
}

void EffectRenderer::Initialize(IDirect3DDevice9* device) {
	type = D3DPT_LINELIST;

	vertices_max = 0x1000; // support for up to 4096 line segments, should be enough

	vertices = nullptr;

	HRESULT hr = device->CreateVertexBuffer(sizeof(D3DVertex) * vertices_max, 0,
		D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer, NULL);
	if (FAILED(hr)) {
		printf("Error setting up PingsLinesRenderer vertex buffer: %d\n", hr);
	}
}

void EffectRenderer::Render(IDirect3DDevice9* device) {
	if (!initialized) {
		initialized = true;
		Initialize(device);
	}

	DrawPings(device);
}

void EffectRenderer::DrawPings(IDirect3DDevice9* device) {
	D3DXMATRIX translate, scale, world;
	D3DXMatrixScaling(&scale, GW::Constants::Range::Nearby, GW::Constants::Range::Nearby, 1.0f);
	for (Effect* ping : pings) {
		if (TIMER_DIFF(ping->start) > ping->duration) continue;

		D3DXMatrixTranslation(&translate, ping->x, ping->y, 0.0f);
		world = scale * translate;
		device->SetTransform(D3DTS_WORLD, &world);
		ping->circle.Render(device);
	}
	for (size_t i = 0; i < pings.size(); i++) {
		Effect* effect = pings[i];
		if (TIMER_DIFF(effect->start) > effect->duration) {
			delete effect;
			pings.erase(pings.begin() + i);
			if (!pings.size()) break;
			i--;
		}
	}
}

void EffectRenderer::EffectCircle ::Initialize(IDirect3DDevice9* device) {
	type = D3DPT_LINESTRIP;
	count = 16; // polycount
	unsigned int vertex_count = count + 1;
	D3DVertex* vertices = nullptr;

	if (buffer) buffer->Release();
	device->CreateVertexBuffer(sizeof(D3DVertex) * vertex_count, 0,
		D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer, NULL);
	buffer->Lock(0, sizeof(D3DVertex) * vertex_count,
		(VOID**)&vertices, D3DLOCK_DISCARD);

	const float PI = 3.1415927f;
	for (size_t i = 0; i < count; ++i) {
		float angle = i * (2 * PI / count);
		vertices[i].x = std::cos(angle);
		vertices[i].y = std::sin(angle);
		vertices[i].z = 0.0f;
		vertices[i].color = color;
	}
	vertices[count] = vertices[0];

	buffer->Unlock();
}
