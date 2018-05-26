#include "RangeRenderer.h"

#include <d3dx9math.h>

#include <imgui.h>

#include <GWCA\Constants\Constants.h>
#include <GWCA\Managers\AgentMgr.h>
#include <GWCA\Managers\SkillbarMgr.h>
#include <GWCA\Managers\MapMgr.h>
#include <GWCA\Managers\CameraMgr.h>

#include "GuiUtils.h"

void RangeRenderer::LoadSettings(CSimpleIni* ini, const char* section) {
	color_range_hos = Colors::Load(ini, section, "color_range_hos", 0xFF881188);
	color_range_aggro = Colors::Load(ini, section, "color_range_aggro", 0xFF994444);
	color_range_cast = Colors::Load(ini, section, "color_range_cast", 0xFF117777);
	color_range_spirit = Colors::Load(ini, section, "color_range_spirit", 0xFF337733);
	color_range_compass = Colors::Load(ini, section, "color_range_compass", 0xFF666611);
	Invalidate();
}
void RangeRenderer::SaveSettings(CSimpleIni* ini, const char* section) const {
	Colors::Save(ini, section, "color_range_hos", color_range_hos);
	Colors::Save(ini, section, "color_range_aggro", color_range_aggro);
	Colors::Save(ini, section, "color_range_cast", color_range_cast);
	Colors::Save(ini, section, "color_range_spirit", color_range_spirit);
	Colors::Save(ini, section, "color_range_compass", color_range_compass);
}
void RangeRenderer::DrawSettings() {
	bool changed = false;
	if (ImGui::SmallButton("Restore Defaults")) {
		changed = true;
		color_range_hos = 0xFF881188;
		color_range_aggro = 0xFF994444;
		color_range_cast = 0xFF117777;
		color_range_spirit = 0xFF337733;
		color_range_compass = 0xFF666611;
	}
	changed |= Colors::DrawSetting("HoS range", &color_range_hos);
	changed |= Colors::DrawSetting("Aggro range", &color_range_aggro);
	changed |= Colors::DrawSetting("Cast range", &color_range_cast);
	changed |= Colors::DrawSetting("Spirit range", &color_range_spirit);
	changed |= Colors::DrawSetting("Compass range", &color_range_compass);
	if (changed) Invalidate();
}

void RangeRenderer::CreateCircle(D3DVertex* vertices, float radius, DWORD color) {
	for (size_t i = 0; i < circle_vertices - 1; ++i) {
		float angle = i * (2 * static_cast<float>(M_PI) / circle_vertices);
		vertices[i].x = radius * std::cos(angle);
		vertices[i].y = radius * std::sin(angle);
		vertices[i].z = 0.0f;
		vertices[i].color = color; // 0xFF666677;
	}
	vertices[circle_points] = vertices[0];
}

void RangeRenderer::Initialize(IDirect3DDevice9* device) {
	count = circle_points * num_circles; // radar range, spirit range, aggro range
	type = D3DPT_LINESTRIP;
	float radius;

	checkforhos_ = true;
	havehos_ = false;

	D3DVertex* vertices = nullptr;
	unsigned int vertex_count = count + num_circles + num_circles + 1;

	device->CreateVertexBuffer(sizeof(D3DVertex) * vertex_count, D3DUSAGE_WRITEONLY,
		D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer, NULL);
	buffer->Lock(0, sizeof(D3DVertex) * vertex_count,
		(VOID**)&vertices, D3DLOCK_DISCARD);

	radius = GW::Constants::Range::Compass;
	CreateCircle(vertices, radius, color_range_compass);
	vertices += circle_vertices;

	radius = GW::Constants::Range::Spirit;
	CreateCircle(vertices, radius, color_range_spirit);
	vertices += circle_vertices;

	radius = GW::Constants::Range::Spellcast;
	CreateCircle(vertices, radius, color_range_cast);
	vertices += circle_vertices;

	radius = GW::Constants::Range::Earshot;
	CreateCircle(vertices, radius, color_range_aggro);
	vertices += circle_vertices;

	radius = 360.0f;
	CreateCircle(vertices, radius, color_range_hos);
	vertices += circle_vertices;

	for (int i = 0; i < 6; ++i) {
		vertices[i].z = 0.0f;
		vertices[i].color = color_range_hos;
	}
	vertices[0].x = 260.0f;
	vertices[0].y = 0.0f;
	vertices[1].x = 460.0f;
	vertices[1].y = 0.0f;

	vertices[2].x = -150.0f;
	vertices[2].y = 0.0f;
	vertices[3].x = 150.0f;
	vertices[3].y = 0.0f;
	vertices[4].x = 0.0f;
	vertices[4].y = -150.0f;
	vertices[5].x = 0.0f;
	vertices[5].y = 150.0f;

	buffer->Unlock();
}

void RangeRenderer::Render(IDirect3DDevice9* device) {
	if (!initialized) {
		initialized = true;
		Initialize(device);
	}

	switch (GW::Map::GetInstanceType()) {
	case GW::Constants::InstanceType::Explorable:
		if (checkforhos_) {
			checkforhos_ = false;
			havehos_ = HaveHos();
		}
		break;
	case GW::Constants::InstanceType::Outpost:
		checkforhos_ = true;
		havehos_ = HaveHos();
		break;
	case GW::Constants::InstanceType::Loading:
		havehos_ = false;
		checkforhos_ = true;
		break;
	default:
		break;
	}

	device->SetFVF(D3DFVF_CUSTOMVERTEX);
	device->SetStreamSource(0, buffer, 0, sizeof(D3DVertex));
	for (int i = 0; i < num_circles - 1; ++i) {
		device->DrawPrimitive(type, circle_vertices * i, circle_points);
	}

	if (HaveHos()) {
		device->DrawPrimitive(type, circle_vertices * (num_circles - 1), circle_points);

		GW::Agent* me = GW::Agents::GetPlayer();
		GW::Agent* tgt = GW::Agents::GetTarget();

		if (!draw_center_
			&& me != nullptr
			&& tgt != nullptr
			&& me != tgt
			&& tgt->GetIsCharacterType()
			&& !me->GetIsDead()
			&& !tgt->GetIsDead()
			&& GW::Agents::GetSqrDistance(tgt->pos, me->pos) < GW::Constants::SqrRange::Spellcast) {
			
			GW::Vector2f v = me->pos - tgt->pos;
			float angle = std::atan2(v.y, v.x);

			D3DXMATRIX oldworld, rotate, newworld;
			device->GetTransform(D3DTS_WORLD, &oldworld);
			D3DXMatrixRotationZ(&rotate, angle);
			newworld = rotate * oldworld;
			device->SetTransform(D3DTS_WORLD, &newworld);
			device->DrawPrimitive(type, circle_vertices * num_circles, 1);
			device->SetTransform(D3DTS_WORLD, &oldworld);
		}
	}

	if (draw_center_) {
		device->DrawPrimitive(type, circle_vertices * num_circles + 2, 1);
		device->DrawPrimitive(type, circle_vertices * num_circles + 4, 1);
	}
}

bool RangeRenderer::HaveHos() {
	GW::Skillbar skillbar = GW::Skillbar::GetPlayerSkillbar();
	if (!skillbar.IsValid()) {
		checkforhos_ = true;
		return false;
	}

	for (int i = 0; i < 8; ++i) {
		GW::SkillbarSkill skill = skillbar.Skills[i];
		GW::Constants::SkillID id = (GW::Constants::SkillID) skill.SkillId;
		if (id == GW::Constants::SkillID::Heart_of_Shadow) return true;
		if (id == GW::Constants::SkillID::Vipers_Defense) return true;
	}
	return false;
}
