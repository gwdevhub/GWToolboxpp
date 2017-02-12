#include "SymbolsRenderer.h"

#include <d3dx9math.h>

#include <GWCA\GWCA.h>
#include <GWCA\Managers\AgentMgr.h>
#include <GWCA\Context\GameContext.h>
#include <GWCA\Context\WorldContext.h>

#include "MinimapUtils.h"

SymbolsRenderer::SymbolsRenderer() {
	color_quest = MinimapUtils::IniReadColor2("color_quest", "0xFF22EF22");
	color_north = MinimapUtils::IniReadColor2("color_north", "0xFFFF8000");
}

void SymbolsRenderer::Initialize(IDirect3DDevice9* device) {
	type_ = D3DPT_TRIANGLELIST;

	D3DVertex* vertices = nullptr;
	DWORD vertex_count = (star_ntriangles + arrow_ntriangles + north_ntriangles) * 3;
	DWORD offset = 0;

	device->CreateVertexBuffer(sizeof(D3DVertex) * vertex_count, D3DUSAGE_WRITEONLY,
		D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer_, NULL);
	buffer_->Lock(0, sizeof(D3DVertex) * vertex_count,
		(VOID**)&vertices, D3DLOCK_DISCARD);

	auto AddVertex = [&vertices, &offset](float x, float y, MinimapUtils::Color color) -> void {
		vertices[0].x = x;
		vertices[0].y = y;
		vertices[0].z = 0.0f;
		vertices[0].color = color.GetDXColor();
		++vertices;
		++offset;
	};

	MinimapUtils::Color color_outside = MinimapUtils::Color(0, -30, -30, -30);
	MinimapUtils::Color color_middle = MinimapUtils::Color(0, 0, 0, 0);
	MinimapUtils::Color color_center = MinimapUtils::Color(0, 30, 30, 30);

	// === Star ===
	star_offset = offset;
	float star_size_big = 300.0f;
	float star_size_small = 150.0f;
	for (unsigned int i = 0; i < star_ntriangles; ++i) {
		float angle1 = 2 * (i + 0) * fPI / star_ntriangles;
		float angle2 = 2 * (i + 1) * fPI / star_ntriangles;
		float size1 = ((i + 0) % 2 == 0 ? star_size_small : star_size_big);
		float size2 = ((i + 1) % 2 == 0 ? star_size_small : star_size_big);
		MinimapUtils::Color c1 = ((i + 0) % 2 == 0 ? color_middle : color_outside);
		MinimapUtils::Color c2 = ((i + 1) % 2 == 0 ? color_middle : color_outside);
		AddVertex(std::cos(angle1) * size1, std::sin(angle1) * size1, c1 + color_quest);
		AddVertex(std::cos(angle2) * size2, std::sin(angle2) * size2, c2 + color_quest);
		AddVertex(0.0f, 0.0f, color_center + color_quest);
	}

	// === Arrow (quest) ===
	arrow_offset = offset;
	AddVertex(   0.0f, -125.0f, color_center + color_quest);
	AddVertex( 250.0f, -250.0f, color_middle + color_quest);
	AddVertex(   0.0f,  250.0f, color_middle + color_quest);
	AddVertex(   0.0f,  250.0f, color_middle + color_quest);
	AddVertex(-250.0f, -250.0f, color_middle + color_quest);
	AddVertex(   0.0f, -125.0f, color_center + color_quest);


	// === Arrow (north) ===
	north_offset = offset;
	AddVertex(   0.0f, -375.0f, color_center + color_north);
	AddVertex( 250.0f, -500.0f, color_middle + color_north);
	AddVertex(   0.0f,    0.0f, color_middle + color_north);
	AddVertex(   0.0f,    0.0f, color_middle + color_north);
	AddVertex(-250.0f, -500.0f, color_middle + color_north);
	AddVertex(   0.0f, -375.0f, color_center + color_north);

	buffer_->Unlock();
}

void SymbolsRenderer::Render(IDirect3DDevice9* device) {
	if (!initialized_) {
		initialized_ = true;
		Initialize(device);
	}

	GW::Agent* me = GW::Agents().GetPlayer();
	if (me == nullptr) return;

	device->SetFVF(D3DFVF_CUSTOMVERTEX);
	device->SetStreamSource(0, buffer_, 0, sizeof(D3DVertex));

	static float tau = 0.0f;
	tau += 0.05f;
	if (tau > 10 * fPI) tau -= 10 * fPI;
	D3DXMATRIX translate, scale, rotate, world;

	
	GW::QuestLog qlog = GW::GameContext::instance()->world->questlog;
	DWORD qid = GW::GameContext::instance()->world->activequestid;
	if (qlog.valid() && qid > 0) {
		GW::Vector2f qpos(0, 0);
		bool qfound = false;
		for (unsigned int i = 0; i < qlog.size(); ++i) {
			GW::Quest q = qlog[i];
			if (q.questid == qid) {
				qpos = q.marker;
				qfound = true;
				break;
			}
		}

		if (qfound) {
			D3DXMatrixRotationZ(&rotate, -tau / 5);
			D3DXMatrixScaling(&scale, 1.0f + std::sin(tau) * 0.3f, 1.0f + std::sin(tau) * 0.3f, 1.0f);
			D3DXMatrixTranslation(&translate, qpos.x, qpos.y, 0);
			world = rotate * scale * translate;
			device->SetTransform(D3DTS_WORLD, &world);
			device->DrawPrimitive(type_, star_offset, star_ntriangles);

			GW::Vector2f mypos = me->pos;
			GW::Vector2f v = qpos - mypos;
			const float max_quest_range = GW::Constants::Range::Compass - 250.0f;
			const float max_quest_range_sqr = max_quest_range * max_quest_range;
			if (v.SquaredNorm() > max_quest_range_sqr) {
				v = v.Normalized() * max_quest_range;
				float angle = std::atan2(v.y, v.x);
				D3DXMatrixRotationZ(&rotate, angle - (float)M_PI_2);
				D3DXMatrixScaling(&scale, 1.0f + std::sin(tau) * 0.3f, 1.0f + std::sin(tau) * 0.3f, 1.0f);
				D3DXMatrixTranslation(&translate, me->X + v.x, me->Y + v.y, 0);
				world = rotate * scale * translate;
				device->SetTransform(D3DTS_WORLD, &world);
				device->DrawPrimitive(type_, arrow_offset, arrow_ntriangles);
			}
		}
	}

	D3DXMatrixTranslation(&translate, me->X, me->Y + 5000.0f, 0);
	world = translate;
	device->SetTransform(D3DTS_WORLD, &world);
	device->DrawPrimitive(type_, north_offset, north_ntriangles);
}
