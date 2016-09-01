#include "RangeRenderer.h"

#include <d3dx9math.h>

#include <GWCA\GWCA.h>
#include <GWCA\Constants\Constants.h>
#include <GWCA\Managers\SkillbarMgr.h>
#include <GWCA\Managers\MapMgr.h>
#include <GWCA\Managers\CameraMgr.h>

#include "Config.h"

RangeRenderer::RangeRenderer() {
	auto IniReadColor = [](wchar_t* key, wchar_t* def) -> DWORD {
		const wchar_t* wc = Config::IniRead(L"minimap", key, def);
		Config::IniWrite(L"minimap", key, wc);
		DWORD c = std::wcstoul(wc, nullptr, 16);
		if (c == LONG_MAX) return D3DCOLOR_ARGB(0xFF, 0x0, 0x0, 0x0);
		return c;
	};

	color_range_hos = IniReadColor(L"color_range_hos", L"0xFF881188");
	color_range_aggro = IniReadColor(L"color_range_aggro", L"0xFF994444");
	color_range_cast = IniReadColor(L"color_range_cast", L"0xFF117777");
	color_range_spirit = IniReadColor(L"color_range_spirit", L"0xFF337733");
	color_range_compass = IniReadColor(L"color_range_compass", L"0xFF666611");
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
	count_ = circle_points * num_circles; // radar range, spirit range, aggro range
	type_ = D3DPT_LINESTRIP;
	float radius;

	checkforhos_ = true;
	havehos_ = false;

	D3DVertex* vertices = nullptr;
	unsigned int vertex_count = count_ + num_circles + 2;

	device->CreateVertexBuffer(sizeof(D3DVertex) * vertex_count, D3DUSAGE_WRITEONLY,
		D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer_, NULL);
	buffer_->Lock(0, sizeof(D3DVertex) * vertex_count,
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

	vertices[0].z = 0.0f;
	vertices[0].x = 260.0f;
	vertices[0].y = 0.0f;
	vertices[0].color = color_range_hos;
	vertices[1].z = 0.0f;
	vertices[1].x = 460.0f;
	vertices[1].y = 0.0f;
	vertices[1].color = color_range_hos;

	buffer_->Unlock();
}

void RangeRenderer::Render(IDirect3DDevice9* device) {
	if (!initialized_) {
		initialized_ = true;
		Initialize(device);
	}

	switch (GW::Map().GetInstanceType()) {
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

	device->SetStreamSource(0, buffer_, 0, sizeof(D3DVertex));
	for (int i = 0; i < num_circles - 1; ++i) {
		device->DrawPrimitive(type_, circle_vertices * i, circle_points);
	}
	if (HaveHos()) {
		device->DrawPrimitive(type_, circle_vertices * (num_circles - 1), circle_points);

		GW::Agent* me = GW::Agents().GetPlayer();
		GW::Agent* tgt = GW::Agents().GetTarget();

		if (me != nullptr
			&& tgt != nullptr
			&& me != tgt
			&& tgt->GetIsLivingType()
			&& !me->GetIsDead()
			&& !tgt->GetIsDead()
			&& GW::Agents().GetSqrDistance(tgt->pos, me->pos) < GW::Constants::SqrRange::Spellcast) {
			GW::Vector2f v = me->pos - tgt->pos;
			float camera_yaw = GW::Cameramgr().GetYaw();
			float rotation = -camera_yaw + (float)M_PI_2;
			float angle = std::atan2(v.y, v.x);

			D3DXMATRIX rotate;
			D3DXMatrixRotationZ(&rotate, rotation + angle);
			device->SetTransform(D3DTS_WORLD, &rotate);
			device->DrawPrimitive(type_, circle_vertices * num_circles, 1);
		}
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
