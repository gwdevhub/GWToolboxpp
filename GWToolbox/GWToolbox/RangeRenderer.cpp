#include "RangeRenderer.h"

#include <cmath>
#include <GWCA\GwConstants.h>
#include <GWCA\GWCA.h>
#include <GWCA\SkillbarMgr.h>
#include <GWCA\MapMgr.h>

void RangeRenderer::CreateCircle(D3DVertex* vertices, float radius, DWORD color) {
	for (size_t i = 0; i < circle_vertices; ++i) {
		float angle = i * (2 * static_cast<float>(M_PI) / circle_vertices);
		vertices[i].x = radius * std::cos(angle);
		vertices[i].y = radius * std::sin(angle);
		vertices[i].z = 0.0f;
		vertices[i].color = color; // 0xFF666677;
	}
	vertices[circle_points] = vertices[0];
}

void RangeRenderer::Initialize(IDirect3DDevice9* device) {
	count_ = circle_points * num_circles; // radar range, spirit range, aggro range
	type_ = D3DPT_LINESTRIP;
	float radius;
	
	checkforhos_ = true;
	havehos_ = false;

	D3DVertex* vertices = nullptr;
	unsigned int vertex_count = count_ + num_circles;

	if (buffer_) buffer_->Release();
	device->CreateVertexBuffer(sizeof(D3DVertex) * vertex_count, D3DUSAGE_WRITEONLY,
		D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer_, NULL);
	buffer_->Lock(0, sizeof(D3DVertex) * vertex_count,
		(VOID**)&vertices, D3DLOCK_DISCARD);

	radius = static_cast<float>(GwConstants::Range::Compass);
	CreateCircle(vertices + circle_vertices * 0, radius, 0xFF666611); // 0xFF666677

	radius = static_cast<float>(GwConstants::Range::Spirit);
	CreateCircle(vertices + circle_vertices * 1, radius, 0xFF337733);

	radius = static_cast<float>(GwConstants::Range::Spellcast);
	CreateCircle(vertices + circle_vertices * 2, radius, 0xFF117777);

	radius = static_cast<float>(GwConstants::Range::Earshot);
	CreateCircle(vertices + circle_vertices * 3, radius, 0xFF994444);

	radius = static_cast<float>(360);
	CreateCircle(vertices + circle_vertices * 4, radius, 0xFF881188);

	buffer_->Unlock();
}

void RangeRenderer::Render(IDirect3DDevice9* device) {
	if (!initialized_) {
		initialized_ = true;
		Initialize(device);
	}

	switch (GWCA::Map().GetInstanceType()) {
	case GwConstants::InstanceType::Explorable:
		if (checkforhos_) {
			checkforhos_ = false;
			havehos_ = HaveHos();
		}
		break;
	case GwConstants::InstanceType::Outpost:
		checkforhos_ = true;
		havehos_ = HaveHos();
		break;
	case GwConstants::InstanceType::Loading: 
		havehos_ = false;
		checkforhos_ = true;
		break;
	default:
		break;
	}

	device->SetFVF(D3DFVF_CUSTOMVERTEX);
	device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
	device->SetRenderState(D3DRS_LIGHTING, FALSE);

	device->SetStreamSource(0, buffer_, 0, sizeof(D3DVertex));
	for (int i = 0; i < num_circles - 1; ++i) {
		device->DrawPrimitive(type_, circle_vertices * i, circle_points);
	}
	if (HaveHos()) {
		device->DrawPrimitive(type_, circle_vertices * (num_circles - 1), circle_points);
	}
}

bool RangeRenderer::HaveHos() {
	GWCA::GW::Skillbar skillbar = GWCA::Skillbar().GetPlayerSkillbar();
	if (!skillbar.IsValid()) {
		checkforhos_ = true;
		return false;
	}

	for (int i = 0; i < 8; ++i) {
		GWCA::GW::SkillbarSkill skill = skillbar.Skills[i];
		GwConstants::SkillID id = (GwConstants::SkillID) skill.SkillId;
		if (id == GwConstants::SkillID::Heart_of_Shadow) return true;
		if (id == GwConstants::SkillID::Vipers_Defense) return true;
	}
	return false;
}
